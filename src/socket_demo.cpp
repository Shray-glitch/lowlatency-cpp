#include "logger.hpp"
#include "tcp_server.hpp"
#include "tcp_socket.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>


int main()
{
    using namespace std::chrono_literals;


    // ========================================================
    // Logger
    // ========================================================

    Logger logger("socket_demo.log");


    // ========================================================
    // Network configuration
    // ========================================================

    const std::string iface = "lo";
    const std::string ip = "127.0.0.1";

    constexpr int PORT = 12345;
    constexpr std::size_t CLIENT_COUNT = 3;


    // ========================================================
    // Server receive callback
    // ========================================================

    auto server_recv_callback =
        [&logger](
            TCPSocket* socket,
            Nanos rx_time)
        {
            // Construct a string from the valid bytes
            // currently present in the receive buffer.
            const std::string message(
                socket->rcv_buffer_,
                socket->next_rcv_valid_index_
            );


            logger.log(
                "SERVER received socket:% rx:% msg:%\n",
                socket->fd_,
                rx_time,
                message
            );


            // We consumed all currently received bytes.
            socket->next_rcv_valid_index_ = 0;


            // Prepare a reply for this specific client.
            const std::string reply =
                "SERVER RECEIVED: " + message;


            // TCPSocket::send() only copies the data into
            // the socket's outgoing buffer.
            // Queue the reply only if it fits in the fixed send buffer.
            if (!socket->send(
                    reply.data(),
                    reply.size()
                ))
            {
                logger.log(
                    "SERVER send buffer full socket:%\n",
                    socket->fd_
                );
            }
        };


    // ========================================================
    // Server receive-finished callback
    // ========================================================

    auto server_recv_finished_callback =
        [&logger]()
        {
            logger.log(
                "SERVER receive round complete\n"
            );
        };


    // ========================================================
    // Client receive callback
    // ========================================================

    auto client_recv_callback =
        [&logger](
            TCPSocket* socket,
            Nanos rx_time)
        {
            const std::string message(
                socket->rcv_buffer_,
                socket->next_rcv_valid_index_
            );


            std::cout
                << "Client received: "
                << message
                << '\n';


            logger.log(
                "CLIENT received socket:% rx:% msg:%\n",
                socket->fd_,
                rx_time,
                message
            );


            // Application has consumed the bytes.
            socket->next_rcv_valid_index_ = 0;
        };


    // ========================================================
    // Create TCP server
    // ========================================================

    std::cout
        << "Starting server on "
        << ip
        << ':'
        << PORT
        << '\n';


    logger.log(
        "Creating TCPServer on iface:% port:%\n",
        iface,
        PORT
    );


    TCPServer server(logger);


    server.recv_callback_ =
        server_recv_callback;


    server.recv_finished_callback_ =
        server_recv_finished_callback;


    // listen() creates epoll, creates the listener socket, binds it to the
    // interface and port, and registers it for connection notifications.
    if (!server.listen(
            iface,
            PORT
        ))
    {
        std::cerr
            << "Failed to start TCP server on "
            << iface
            << ':'
            << PORT
            << '\n';

        return 1;
    }


    // ========================================================
    // Create TCP clients
    // ========================================================

    std::vector<
        std::unique_ptr<TCPSocket>
    > clients;


    clients.reserve(CLIENT_COUNT);


    for (
        std::size_t i = 0;
        i < CLIENT_COUNT;
        ++i)
    {
        auto client =
            std::make_unique<TCPSocket>(
                logger
            );


        client->recv_callback_ =
            client_recv_callback;


        logger.log(
            "Connecting client:% ip:% iface:% port:%\n",
            i,
            ip,
            iface,
            PORT
        );


        const int fd =
            client->connect(
                ip,
                iface,
                PORT,
                false
            );


        if (fd < 0)
        {
            std::cerr
                << "Failed to connect client "
                << i
                << '\n';

            return 1;
        }


        std::cout
            << "Connected client "
            << i
            << " with fd "
            << fd
            << '\n';


        clients.push_back(
            std::move(client)
        );


        // Give the server a few opportunities to observe
        // the new connection and accept it.
        for (
            int attempt = 0;
            attempt < 5;
            ++attempt)
        {
            server.poll();

            std::this_thread::sleep_for(
                10ms
            );
        }
    }


    // ========================================================
    // Send one message from each client
    // ========================================================

    for (
        std::size_t i = 0;
        i < clients.size();
        ++i)
    {
        const std::string message =
            "CLIENT-["
            + std::to_string(i)
            + "] says hello";


        std::cout
            << "Sending: "
            << message
            << '\n';


        logger.log(
            "Sending client:% msg:%\n",
            i,
            message
        );


        // Copies into TCPSocket's outgoing buffer.
        if (!clients[i]->send(
                message.data(),
                message.size()
            ))
        {
            std::cerr
                << "Client send buffer is full for client "
                << i
                << '\n';

            return 1;
        }


        // Actually attempts to write the buffered bytes
        // to the network.
        clients[i]->sendAndRecv();
    }


    // ========================================================
    // Drive the event loop
    // ========================================================

    for (
        int iteration = 0;
        iteration < 100;
        ++iteration)
    {
        // Ask epoll which server sockets have activity.
        server.poll();


        // Read client messages and flush server replies.
        server.sendAndRecv();


        // Allow every client to receive the server reply.
        for (auto& client : clients)
        {
            client->sendAndRecv();
        }


        std::this_thread::sleep_for(
            10ms
        );
    }


    std::cout
        << "Socket demo complete.\n"
        << "Check socket_demo.log for detailed logs.\n";


    return 0;
}
