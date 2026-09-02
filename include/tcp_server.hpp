#pragma once

#include "tcp_socket.hpp"

#include <functional>
#include <string>
#include <vector>

#include <sys/epoll.h>


constexpr int MAX_EPOLL_EVENTS = 1024;


// A non-blocking TCP server built around Linux epoll.
//
// The server owns one listener socket and every client socket accepted from
// it. poll() asks epoll which descriptors changed state. sendAndRecv() then
// performs the actual network reads and writes on the selected connections.
class TCPServer final
{
private:

    // File descriptor for the epoll instance. epoll stores and reports the
    // listener and client descriptors that this server registers with it.
    int efd_ = -1;


    // This socket never carries application messages. Its only job is to
    // accept new TCP connections and create client descriptors.
    TCPSocket listener_socket_;


    // epoll_wait() writes ready-event descriptions into this fixed array.
    epoll_event events_[MAX_EPOLL_EVENTS]{};


    // The server owns these dynamically allocated client sockets and deletes
    // them after disconnection or when the server is destroyed.
    std::vector<TCPSocket*> sockets_;


    // Client sockets checked for incoming data during sendAndRecv(). This
    // learning implementation keeps accepted sockets here so they are polled
    // on every event-loop pass.
    std::vector<TCPSocket*> receive_sockets_;


    // Client sockets for which epoll reported a writable event.
    std::vector<TCPSocket*> send_sockets_;


    // Failed sockets are scheduled here first and deleted during the next
    // poll(). Delayed deletion avoids invalidating a list currently in use.
    std::vector<TCPSocket*> disconnected_sockets_;


    Logger& logger_;


    // Register or remove one socket descriptor from this server's epoll set.
    bool epollAdd(TCPSocket* socket);

    bool epollDel(TCPSocket* socket);


    // Remove the socket from every list and release its memory.
    void del(TCPSocket* socket);


public:

    // Called after a client socket appends bytes to its receive buffer.
    // The application must consume/reset those bytes before the buffer fills.
    std::function<
        void(TCPSocket*, Nanos)
    > recv_callback_;


    // Called once after a sendAndRecv() round where data was received.
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


    // Create the epoll instance and listener socket.
    // Return false after cleaning up if any setup operation fails.
    bool listen(
        const std::string& iface,
        int port
    );


    // Discover new connections, readable sockets, and disconnections.
    // This method does not read application bytes itself.
    void poll() noexcept;


    // Perform non-blocking reads and writes on the selected client sockets.
    void sendAndRecv() noexcept;
};
