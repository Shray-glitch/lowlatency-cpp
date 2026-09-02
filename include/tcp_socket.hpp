#pragma once

#include "logger.hpp"
#include "socket_utils.hpp"
#include "time_utils.hpp"

#include <cstddef>
#include <functional>
#include <netinet/in.h>
#include <string>


constexpr std::size_t TCP_BUFFER_SIZE =
    64 * 1024;


struct TCPSocket
{
    int fd_ = -1;


    char* send_buffer_ = nullptr;

    std::size_t next_send_valid_index_ = 0;


    char* rcv_buffer_ = nullptr;

    std::size_t next_rcv_valid_index_ = 0;


    bool send_disconnected_ = false;
    bool recv_disconnected_ = false;


    sockaddr_in in_addr_{};


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


    void destroy() noexcept;


    int connect(
        const std::string& ip,
        const std::string& iface,
        int port,
        bool is_listening
    );


    void send(
        const void* data,
        std::size_t len
    ) noexcept;


    bool sendAndRecv() noexcept;
};