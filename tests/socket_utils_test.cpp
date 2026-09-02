#include "socket_utils.hpp"

#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>


// Print an explanation when a test fails.
int fail(const char* message)
{
    std::cerr << "FAILED: " << message << '\n';
    return 1;
}


int main()
{
    // Most Linux systems provide a loopback interface named "lo".
    // Some restricted test containers do not allow interface inspection.
    const std::string loopback_ip = getIfaceIP("lo");

    if (!getIfaceIP("interface-that-does-not-exist").empty()) {
        return fail("A missing interface should return an empty string.");
    }


    // Check the errors that mean a non-blocking operation should retry.
    errno = EAGAIN;
    if (!wouldBlock()) {
        return fail("EAGAIN should mean try again later.");
    }

    errno = EWOULDBLOCK;
    if (!wouldBlock()) {
        return fail("EWOULDBLOCK should mean try again later.");
    }

    errno = EINPROGRESS;
    if (!wouldBlock()) {
        return fail("EINPROGRESS should mean connection in progress.");
    }

    errno = EBADF;
    if (wouldBlock()) {
        return fail("EBADF should be treated as a real error.");
    }


    // A pipe is enough to verify the O_NONBLOCK helper without networking.
    int pipe_fds[2] = {-1, -1};

    if (::pipe(pipe_fds) == -1) {
        return fail("The test should be able to create a pipe.");
    }

    if (!setNonBlocking(pipe_fds[0])) {
        ::close(pipe_fds[0]);
        ::close(pipe_fds[1]);
        return fail("setNonBlocking should work on a valid descriptor.");
    }

    const int pipe_flags = fcntl(pipe_fds[0], F_GETFL, 0);

    ::close(pipe_fds[0]);
    ::close(pipe_fds[1]);

    if (pipe_flags == -1 || !(pipe_flags & O_NONBLOCK)) {
        return fail("O_NONBLOCK should be present after the helper runs.");
    }


    if (loopback_ip.empty())
    {
        std::cout
            << "Loopback socket checks skipped: "
            << "interface access is unavailable.\n";

        std::cout << "All socket utility tests passed.\n";
        return 0;
    }


    const char* log_file = "socket_utils_test.log";

    {
        Logger logger(log_file);

        // Port 0 asks Linux to choose an available local port.
        const int listener_fd = createSocket(
            logger,
            loopback_ip,
            "",
            0,
            false,  // TCP
            false,  // non-blocking
            true,   // listening socket
            true    // kernel receive timestamps
        );

        if (listener_fd < 0) {
            return fail("A loopback listening socket should be created.");
        }


        // Confirm that O_NONBLOCK was added to the descriptor flags.
        const int flags = fcntl(listener_fd, F_GETFL, 0);

        if (flags == -1 || !(flags & O_NONBLOCK)) {
            ::close(listener_fd);
            return fail("The listening socket should be non-blocking.");
        }


        // Confirm that TCP_NODELAY was enabled.
        int no_delay = 0;
        socklen_t option_size = sizeof(no_delay);

        if (getsockopt(
                listener_fd,
                IPPROTO_TCP,
                TCP_NODELAY,
                &no_delay,
                &option_size
            ) == -1 || no_delay != 1)
        {
            ::close(listener_fd);
            return fail("TCP_NODELAY should be enabled.");
        }


        // Confirm that SO_TIMESTAMP was enabled.
        int timestamp_enabled = 0;
        option_size = sizeof(timestamp_enabled);

        if (getsockopt(
                listener_fd,
                SOL_SOCKET,
                SO_TIMESTAMP,
                &timestamp_enabled,
                &option_size
            ) == -1 || timestamp_enabled != 1)
        {
            ::close(listener_fd);
            return fail("SO_TIMESTAMP should be enabled.");
        }


        // getsockname() reveals the port selected by Linux.
        sockaddr_in address{};
        socklen_t address_size = sizeof(address);

        if (getsockname(
                listener_fd,
                reinterpret_cast<sockaddr*>(&address),
                &address_size
            ) == -1 || ntohs(address.sin_port) == 0)
        {
            ::close(listener_fd);
            return fail("Linux should assign an available port.");
        }

        ::close(listener_fd);
    }

    std::remove(log_file);

    std::cout << "All socket utility tests passed.\n";
    return 0;
}
