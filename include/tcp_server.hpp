#pragma once

#include "tcp_socket.hpp"

#include <functional>
#include <string>
#include <vector>

#include <sys/epoll.h>


constexpr int MAX_EPOLL_EVENTS = 1024;


class TCPServer final
{
private:

    // epoll instance file descriptor.
    int efd_ = -1;


    // Socket whose only job is to accept new connections.
    TCPSocket listener_socket_;


    // Filled by epoll_wait().
    epoll_event events_[MAX_EPOLL_EVENTS]{};


    // All accepted client connections.
    std::vector<TCPSocket*> sockets_;


    // Sockets that should be checked for incoming data.
    std::vector<TCPSocket*> receive_sockets_;


    // Sockets that epoll reports as writable.
    std::vector<TCPSocket*> send_sockets_;


    // Sockets scheduled for cleanup.
    std::vector<TCPSocket*> disconnected_sockets_;


    Logger& logger_;


    bool epollAdd(TCPSocket* socket);

    bool epollDel(TCPSocket* socket);


    void del(TCPSocket* socket);


public:

    // Called when data is received from a client.
    std::function<
        void(TCPSocket*, Nanos)
    > recv_callback_;


    // Called after a polling round in which
    // one or more receive callbacks ran.
    std::function<void()>
        recv_finished_callback_;


    explicit TCPServer(Logger& logger);

    ~TCPServer();


    TCPServer() = delete;

    TCPServer(const TCPServer&) = delete;
    TCPServer& operator=(const TCPServer&) = delete;

    TCPServer(TCPServer&&) = delete;
    TCPServer& operator=(TCPServer&&) = delete;


    void destroy() noexcept;


    void listen(
        const std::string& iface,
        int port
    );


    void poll() noexcept;


    void sendAndRecv() noexcept;
};