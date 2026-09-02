#pragma once

#include "logger.hpp"
#include "socket_utils.hpp"
#include "time_utils.hpp"

#include <cstddef>
#include <functional>
#include <netinet/in.h>
#include <string>


// Maximum number of outgoing or incoming bytes stored by one TCPSocket.
constexpr std::size_t TCP_BUFFER_SIZE =
    64 * 1024;


// Owns one TCP socket and its fixed-size send and receive buffers.
//
// send() only copies data into send_buffer_. sendAndRecv() performs the
// actual non-blocking operating-system send and receive calls.
struct TCPSocket
{
    // Linux file descriptor for this socket. -1 means no socket is open.
    int fd_ = -1;


    // Bytes waiting to be sent begin at index 0 and end before
    // next_send_valid_index_.
    char* send_buffer_ = nullptr;

    std::size_t next_send_valid_index_ = 0;


    // Newly received bytes are appended at next_rcv_valid_index_.
    // The receive callback must reset or reduce this index after consuming
    // data so that later network data has somewhere to be stored.
    char* rcv_buffer_ = nullptr;

    std::size_t next_rcv_valid_index_ = 0;


    // TCPServer checks these flags and removes failed connections.
    bool send_disconnected_ = false;
    bool recv_disconnected_ = false;


    // Address storage used by the recvmsg() call.
    sockaddr_in in_addr_{};


    // Called after new bytes are appended to rcv_buffer_.
    // The callback must not throw because sendAndRecv() is noexcept.
    std::function<
        void(TCPSocket*, Nanos)
    > recv_callback_;


    Logger& logger_;


    explicit TCPSocket(Logger& logger);

    ~TCPSocket();


    TCPSocket() = delete;

    TCPSocket(const TCPSocket&) = delete;
    TCPSocket& operator=(const TCPSocket&) = delete;

    TCPSocket(TCPSocket&&) = delete;
    TCPSocket& operator=(TCPSocket&&) = delete;


    // Close the descriptor and clear state left by the old connection.
    void destroy() noexcept;


    // Create a non-blocking TCP client or listening socket.
    // Return the file descriptor, or -1 when creation fails.
    int connect(
        const std::string& ip,
        const std::string& iface,
        int port,
        bool is_listening
    );


    // Copy outgoing data into the fixed send buffer.
    // Return false if the pointer is invalid or the data will not fit.
    bool send(
        const void* data,
        std::size_t len
    ) noexcept;


    // Try one non-blocking receive, then flush queued outgoing bytes.
    // Return true only when new bytes were received in this call.
    bool sendAndRecv() noexcept;
};
