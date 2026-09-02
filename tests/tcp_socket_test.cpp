#include "tcp_socket.hpp"

#include <cstdio>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>


// Print an explanation when the test fails.
int fail(const char* message)
{
    std::cerr << "FAILED: " << message << '\n';
    return 1;
}


int main()
{
    const char* log_file = "tcp_socket_test.log";

    int peer_fds[2] = {-1, -1};

    // socketpair() creates two connected local stream sockets.
    // It lets us test both ends without internet or a TCP port.
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, peer_fds) == -1) {
        return fail("The test should create a connected socket pair.");
    }

    if (!setNonBlocking(peer_fds[0]) ||
        !setNonBlocking(peer_fds[1]))
    {
        ::close(peer_fds[0]);
        ::close(peer_fds[1]);
        return fail("Both test sockets should become non-blocking.");
    }

    {
        Logger logger(log_file);
        TCPSocket socket(logger);

        // Give one end of the connected pair to TCPSocket.
        socket.fd_ = peer_fds[0];

        std::string callback_data;

        socket.recv_callback_ =
            [&callback_data](TCPSocket* current, Nanos)
            {
                callback_data.append(
                    current->rcv_buffer_,
                    current->next_rcv_valid_index_
                );

                // The test consumed every received byte.
                current->next_rcv_valid_index_ = 0;
            };


        const std::string outgoing = "hello from TCPSocket";

        if (!socket.send(outgoing.data(), outgoing.size())) {
            return fail("Valid outgoing data should enter the send buffer.");
        }

        socket.sendAndRecv();

        char peer_buffer[128] = {};

        const ssize_t peer_received = ::recv(
            peer_fds[1],
            peer_buffer,
            sizeof(peer_buffer),
            0
        );

        if (
            peer_received != static_cast<ssize_t>(outgoing.size()) ||
            std::string(peer_buffer, outgoing.size()) != outgoing
        )
        {
            return fail("The peer should receive the queued outgoing data.");
        }


        const std::string incoming = "reply from peer";

        if (::send(
                peer_fds[1],
                incoming.data(),
                incoming.size(),
                MSG_NOSIGNAL
            ) != static_cast<ssize_t>(incoming.size()))
        {
            return fail("The peer should send the complete reply.");
        }

        // Retry briefly because both sockets are non-blocking.
        for (int attempt = 0;
             attempt < 100 && callback_data.empty();
             ++attempt)
        {
            socket.sendAndRecv();
            std::this_thread::yield();
        }

        if (callback_data != incoming) {
            return fail("The receive callback should get the peer's reply.");
        }


        // A message larger than the fixed buffer must be rejected safely.
        const std::vector<char> too_large(
            TCP_BUFFER_SIZE + 1,
            'x'
        );

        if (socket.send(too_large.data(), too_large.size())) {
            return fail("Data larger than the send buffer should be rejected.");
        }

        if (socket.next_send_valid_index_ != 0) {
            return fail("Rejected data must not change the send buffer.");
        }

        if (socket.send(nullptr, 1)) {
            return fail("A null data pointer should be rejected.");
        }


        // Simulate state left by an old connection, then destroy it.
        socket.next_send_valid_index_ = 10;
        socket.next_rcv_valid_index_ = 20;
        socket.send_disconnected_ = true;
        socket.recv_disconnected_ = true;

        socket.destroy();

        if (
            socket.fd_ != -1 ||
            socket.next_send_valid_index_ != 0 ||
            socket.next_rcv_valid_index_ != 0 ||
            socket.send_disconnected_ ||
            socket.recv_disconnected_
        )
        {
            return fail("destroy should clear the old connection state.");
        }

        if (socket.sendAndRecv()) {
            return fail("An invalid socket should not receive data.");
        }
    }

    // TCPSocket closed peer_fds[0] when destroy() ran.
    ::close(peer_fds[1]);
    std::remove(log_file);

    std::cout << "All TCP socket tests passed.\n";
    return 0;
}
