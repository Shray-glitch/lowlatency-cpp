#include "tcp_socket.hpp"

#include <cassert>
#include <cstring>

#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>


// ============================================================
// Constructor
// ============================================================

TCPSocket::TCPSocket(Logger& logger)
    : logger_(logger)
{
    send_buffer_ =
        new char[TCP_BUFFER_SIZE];

    rcv_buffer_ =
        new char[TCP_BUFFER_SIZE];


    // Default callback.
    //
    // Applications can replace this with their own callback.
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
        };
}


// ============================================================
// Destructor
// ============================================================

TCPSocket::~TCPSocket()
{
    destroy();


    delete[] send_buffer_;
    send_buffer_ = nullptr;


    delete[] rcv_buffer_;
    rcv_buffer_ = nullptr;
}


// ============================================================
// Destroy socket
// ============================================================

void TCPSocket::destroy() noexcept
{
    if (fd_ >= 0)
    {
        ::close(fd_);

        fd_ = -1;
    }
}


// ============================================================
// Create/connect socket
// ============================================================

int TCPSocket::connect(
    const std::string& ip,
    const std::string& iface,
    int port,
    bool is_listening)
{
    // Close an existing socket first if this object
    // is being reused.
    destroy();


    fd_ = createSocket(
        logger_,
        ip,
        iface,
        port,

        false,          // TCP, not UDP
        false,          // non-blocking
        is_listening,

        0,              // TTL not needed for this TCP socket
        true            // enable SO_TIMESTAMP
    );


    in_addr_.sin_addr.s_addr =
        INADDR_ANY;

    in_addr_.sin_port =
        htons(port);

    in_addr_.sin_family =
        AF_INET;


    return fd_;
}


// ============================================================
// Queue outgoing data
// ============================================================

void TCPSocket::send(
    const void* data,
    std::size_t len) noexcept
{
    if (len == 0)
    {
        return;
    }


    // We use a fixed-size preallocated send buffer.
    assert(
        next_send_valid_index_ + len
        <= TCP_BUFFER_SIZE
    );


    std::memcpy(
        send_buffer_
            + next_send_valid_index_,

        data,
        len
    );


    next_send_valid_index_ += len;
}


// ============================================================
// Send and receive network data
// ============================================================

bool TCPSocket::sendAndRecv() noexcept
{
    // --------------------------------------------------------
    // RECEIVE SIDE
    // --------------------------------------------------------

    char control[
        CMSG_SPACE(sizeof(timeval))
    ]{};


    iovec io{};

    io.iov_base =
        rcv_buffer_
        + next_rcv_valid_index_;

    io.iov_len =
        TCP_BUFFER_SIZE
        - next_rcv_valid_index_;


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


    const ssize_t received =
        ::recvmsg(
            fd_,
            &message,
            MSG_DONTWAIT
        );


    if (received > 0)
    {
        next_rcv_valid_index_ +=
            static_cast<std::size_t>(
                received
            );


        // Extract the software receive timestamp
        // supplied by SO_TIMESTAMP.
        Nanos kernel_time = 0;


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


        // Notify whichever application owns this socket.
        recv_callback_(
            this,
            kernel_time
        );
    }

    else if (received == 0)
    {
        // TCP recv returning 0 means the peer
        // performed an orderly shutdown.
        recv_disconnected_ = true;
    }

    else if (!wouldBlock())
    {
        // Negative return that is not simply EWOULDBLOCK /
        // EINPROGRESS is treated as a connection failure.
        recv_disconnected_ = true;
    }


    // --------------------------------------------------------
    // SEND SIDE
    // --------------------------------------------------------

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
            if (!wouldBlock())
            {
                send_disconnected_ = true;
            }

            break;
        }


        if (sent == 0)
        {
            break;
        }


        total_sent +=
            static_cast<std::size_t>(
                sent
            );
    }


    // If only part of the outgoing buffer was accepted
    // by the kernel, move the remaining bytes to the front.
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