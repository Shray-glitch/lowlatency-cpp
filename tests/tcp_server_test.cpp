#include "tcp_server.hpp"

#include <arpa/inet.h>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>


// Print an explanation when the test fails.
int fail(const char* message)
{
    std::cerr << "FAILED: " << message << '\n';
    return 1;
}


// Ask Linux for an unused loopback port, then release it so TCPServer can
// bind to it. This avoids choosing a fixed port that another program may use.
int findAvailablePort()
{
    const int fd = ::socket(
        AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP
    );

    if (fd < 0) {
        return -1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(0);

    if (::bind(
            fd,
            reinterpret_cast<sockaddr*>(&address),
            sizeof(address)
        ) == -1)
    {
        ::close(fd);
        return -1;
    }

    socklen_t address_size = sizeof(address);

    if (::getsockname(
            fd,
            reinterpret_cast<sockaddr*>(&address),
            &address_size
        ) == -1)
    {
        ::close(fd);
        return -1;
    }

    const int port = ntohs(address.sin_port);

    ::close(fd);
    return port;
}


int main()
{
    using namespace std::chrono_literals;

    const char* log_file = "tcp_server_test.log";

    {
        Logger logger(log_file);
        TCPServer server(logger);

        // These calls must be harmless before listen() succeeds.
        server.poll();
        server.sendAndRecv();

        // A missing interface should fail cleanly instead of relying on an
        // assertion that disappears in Release builds.
        if (server.listen("interface-that-does-not-exist", 12345)) {
            return fail("Listening on a missing interface should fail.");
        }


        const std::string loopback_ip = getIfaceIP("lo");

        // Some restricted containers forbid interface or network access.
        // WSL and normal Linux environments run the full integration test.
        if (loopback_ip.empty())
        {
            std::cout
                << "Loopback server checks skipped: "
                << "interface access is unavailable.\n";

            std::cout << "All TCP server tests passed.\n";
            return 0;
        }

        const int port = findAvailablePort();

        if (port <= 0) {
            return fail("The test should find an available loopback port.");
        }


        const std::string request = "PING";
        const std::string reply = "PONG";

        std::string server_received;
        std::string client_received;

        bool reply_queued = false;
        bool reply_queue_failed = false;
        int completed_receive_rounds = 0;

        // The server callback consumes received bytes and queues one reply.
        server.recv_callback_ =
            [&](TCPSocket* socket, Nanos)
            {
                server_received.append(
                    socket->rcv_buffer_,
                    socket->next_rcv_valid_index_
                );

                socket->next_rcv_valid_index_ = 0;

                if (
                    server_received == request &&
                    !reply_queued
                )
                {
                    reply_queued = socket->send(
                        reply.data(),
                        reply.size()
                    );

                    reply_queue_failed = !reply_queued;
                }
            };

        server.recv_finished_callback_ =
            [&completed_receive_rounds]()
            {
                ++completed_receive_rounds;
            };


        if (!server.listen("lo", port)) {
            return fail("The server should listen on loopback.");
        }

        TCPSocket client(logger);

        client.recv_callback_ =
            [&client_received](TCPSocket* socket, Nanos)
            {
                client_received.append(
                    socket->rcv_buffer_,
                    socket->next_rcv_valid_index_
                );

                socket->next_rcv_valid_index_ = 0;
            };

        if (client.connect(
                loopback_ip,
                "lo",
                port,
                false
            ) < 0)
        {
            return fail("The client should begin connecting to the server.");
        }

        // Give epoll time to report and accept the new local connection.
        for (int attempt = 0; attempt < 100; ++attempt)
        {
            server.poll();
            std::this_thread::sleep_for(1ms);
        }

        if (!client.send(request.data(), request.size())) {
            return fail("The client request should fit in its send buffer.");
        }

        // Drive both sides of the non-blocking event loop until the reply
        // arrives or the bounded retry count is exhausted.
        for (int attempt = 0;
             attempt < 2000 && client_received != reply;
             ++attempt)
        {
            server.poll();
            server.sendAndRecv();
            client.sendAndRecv();

            std::this_thread::sleep_for(1ms);
        }

        if (server_received != request) {
            return fail("The server callback should receive PING.");
        }

        if (reply_queue_failed || !reply_queued) {
            return fail("The server should queue its PONG reply.");
        }

        if (client_received != reply) {
            return fail("The client callback should receive PONG.");
        }

        if (completed_receive_rounds == 0) {
            return fail("The receive-finished callback should run.");
        }
    }

    std::remove(log_file);

    std::cout << "All TCP server tests passed.\n";
    return 0;
}
