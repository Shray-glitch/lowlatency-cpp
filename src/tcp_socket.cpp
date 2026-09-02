#include "tcp_socket.hpp"

#include <cstring>

#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>


TCPSocket::TCPSocket(Logger& logger)
    : logger_(logger)
{
    // Allocate both buffers once. They are reused for the socket's lifetime,
    // so the send/receive path performs no repeated buffer allocation.
    send_buffer_ =
        new char[TCP_BUFFER_SIZE];

    rcv_buffer_ =
        new char[TCP_BUFFER_SIZE];


    // Applications normally replace this callback. The default callback
    // logs the amount of data and then discards it by resetting the index.
    recv_callback_ =
        [this](
            TCPSocket* socket,
            Nanos rx_time)
        {
            logger_.log(
                "Default recv callback socket:% len:% rx:%\n",
                socket->fd_,
                socket->next_rcv_valid_index_,
                rx_time
            );

            // The default callback has finished with all received bytes.
            socket->next_rcv_valid_index_ = 0;
        };
}


TCPSocket::~TCPSocket()
{
    destroy();

    delete[] send_buffer_;
    send_buffer_ = nullptr;

    delete[] rcv_buffer_;
    rcv_buffer_ = nullptr;
}


void TCPSocket::destroy() noexcept
{
    if (fd_ >= 0)
    {
        ::close(fd_);
        fd_ = -1;
    }

    // A reused TCPSocket must not keep data or failure flags from the
    // previous connection. The allocated buffers themselves are reused.
    next_send_valid_index_ = 0;
    next_rcv_valid_index_ = 0;

    send_disconnected_ = false;
    recv_disconnected_ = false;

    in_addr_ = {};
}


int TCPSocket::connect(
    const std::string& ip,
    const std::string& iface,
    int port,
    bool is_listening)
{
    // Close and clear an earlier connection before reusing this object.
    destroy();

    fd_ = createSocket(
        logger_,
        ip,
        iface,
        port,

        false,          // TCP, not UDP
        false,          // non-blocking
        is_listening,
        true            // enable kernel receive timestamps
    );

    // recvmsg() can optionally write source-address information here.
    in_addr_.sin_addr.s_addr =
        INADDR_ANY;

    in_addr_.sin_port =
        htons(port);

    in_addr_.sin_family =
        AF_INET;

    return fd_;
}


bool TCPSocket::send(
    const void* data,
    std::size_t len) noexcept
{
    // An empty message needs no buffer space and is considered successful.
    if (len == 0) {
        return true;
    }

    // Check the available space explicitly. Unlike assert(), this protection
    // remains active in optimized Release builds.
    if (
        data == nullptr ||
        next_send_valid_index_ > TCP_BUFFER_SIZE ||
        len > TCP_BUFFER_SIZE - next_send_valid_index_
    )
    {
        return false;
    }

    // Append the new bytes after any data already waiting to be sent.
    std::memcpy(
        send_buffer_
            + next_send_valid_index_,
        data,
        len
    );

    next_send_valid_index_ += len;

    return true;
}


bool TCPSocket::sendAndRecv() noexcept
{
    if (fd_ < 0) {
        return false;
    }

    // The callback must consume data before all 64 KB are occupied.
    // Calling recvmsg() with zero available bytes could look like an orderly
    // TCP shutdown, so handle a full buffer explicitly instead.
    if (next_rcv_valid_index_ >= TCP_BUFFER_SIZE)
    {
        logger_.log(
            "Receive buffer full socket:%\n",
            fd_
        );

        recv_disconnected_ = true;
        return false;
    }


    // recvmsg() stores optional SO_TIMESTAMP control data in this array.
    char control[
        CMSG_SPACE(sizeof(timeval))
    ]{};

    // iovec tells recvmsg() where ordinary network bytes should be appended.
    iovec io{};

    io.iov_base =
        rcv_buffer_
        + next_rcv_valid_index_;

    io.iov_len =
        TCP_BUFFER_SIZE
        - next_rcv_valid_index_;


    // msghdr groups the data buffer, optional address, and control data.
    msghdr message{};

    message.msg_name =
        &in_addr_;

    message.msg_namelen =
        sizeof(in_addr_);

    message.msg_iov =
        &io;

    message.msg_iovlen =
        1;

    message.msg_control =
        control;

    message.msg_controllen =
        sizeof(control);


    // MSG_DONTWAIT makes this receive attempt return immediately.
    const ssize_t received =
        ::recvmsg(
            fd_,
            &message,
            MSG_DONTWAIT
        );

    if (received > 0)
    {
        // The new bytes extend the valid part of rcv_buffer_.
        next_rcv_valid_index_ +=
            static_cast<std::size_t>(
                received
            );

        Nanos kernel_time = 0;

        // Walk through the optional control messages looking for the
        // software receive timestamp supplied by SO_TIMESTAMP.
        for (
            cmsghdr* cmsg =
                CMSG_FIRSTHDR(&message);

            cmsg != nullptr;

            cmsg =
                CMSG_NXTHDR(
                    &message,
                    cmsg
                )
        )
        {
            if (
                cmsg->cmsg_level
                    == SOL_SOCKET
                &&
                cmsg->cmsg_type
                    == SCM_TIMESTAMP
            )
            {
                timeval kernel_tv{};

                std::memcpy(
                    &kernel_tv,
                    CMSG_DATA(cmsg),
                    sizeof(kernel_tv)
                );

                // Convert seconds and microseconds into one nanosecond value.
                kernel_time =
                    static_cast<Nanos>(
                        kernel_tv.tv_sec
                    ) * NANOS_TO_SECS
                    +
                    static_cast<Nanos>(
                        kernel_tv.tv_usec
                    ) * NANOS_TO_MICROS;

                break;
            }
        }

        // The callback owns processing of all currently buffered bytes.
        recv_callback_(
            this,
            kernel_time
        );
    }
    else if (received == 0)
    {
        // For a non-empty TCP receive buffer, zero means the peer closed
        // its sending side in an orderly way.
        recv_disconnected_ = true;
    }
    else if (!wouldBlock())
    {
        // EAGAIN/EWOULDBLOCK means no data is currently available. Other
        // errors are treated as a broken receive side.
        recv_disconnected_ = true;
    }


    // Try to write every byte currently queued in send_buffer_.
    std::size_t total_sent = 0;

    while (
        total_sent
        < next_send_valid_index_)
    {
        const std::size_t remaining =
            next_send_valid_index_
            - total_sent;

        const ssize_t sent =
            ::send(
                fd_,
                send_buffer_
                    + total_sent,
                remaining,
                MSG_DONTWAIT
                | MSG_NOSIGNAL
            );

        if (sent < 0)
        {
            // A full kernel send buffer is temporary. Other errors mark the
            // sending side as disconnected.
            if (!wouldBlock()) {
                send_disconnected_ = true;
            }

            break;
        }

        if (sent == 0) {
            break;
        }

        total_sent +=
            static_cast<std::size_t>(
                sent
            );
    }


    // A non-blocking send may accept only part of the buffer. Move the
    // unsent bytes to index 0 so the next call can continue from there.
    if (total_sent > 0)
    {
        const std::size_t unsent =
            next_send_valid_index_
            - total_sent;

        if (unsent > 0)
        {
            std::memmove(
                send_buffer_,
                send_buffer_
                    + total_sent,
                unsent
            );
        }

        next_send_valid_index_ =
            unsent;
    }

    return received > 0;
}
