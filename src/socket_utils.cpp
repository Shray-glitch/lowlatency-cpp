#include "socket_utils.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>


std::string getIfaceIP(const std::string& iface)
{
    if (iface.empty()) {
        return {};
    }

    // getnameinfo() writes the numeric IPv4 address here.
    char buffer[NI_MAXHOST] = {};

    // getifaddrs() creates a linked list of network interfaces.
    ifaddrs* interfaces = nullptr;

    if (getifaddrs(&interfaces) == -1) {
        return {};
    }


    // Search the list for the requested IPv4 interface.
    for (ifaddrs* current = interfaces;
         current != nullptr;
         current = current->ifa_next)
    {
        // Some interface entries do not contain an address.
        if (current->ifa_addr == nullptr) {
            continue;
        }

        // This project creates IPv4 sockets, so ignore IPv6 entries.
        if (current->ifa_addr->sa_family != AF_INET) {
            continue;
        }

        if (iface != current->ifa_name) {
            continue;
        }


        const int result = getnameinfo(
            current->ifa_addr,
            sizeof(sockaddr_in),
            buffer,
            sizeof(buffer),
            nullptr,
            0,
            NI_NUMERICHOST
        );

        // Keep the return value empty when address conversion fails.
        if (result != 0) {
            buffer[0] = '\0';
        }

        break;
    }


    // Release the list allocated by getifaddrs().
    freeifaddrs(interfaces);

    return buffer;
}


bool setNonBlocking(int fd)
{
    // Read the descriptor's existing status flags first.
    const int flags =
        fcntl(fd, F_GETFL, 0);

    if (flags == -1) {
        return false;
    }

    // Do nothing if non-blocking mode is already enabled.
    if (flags & O_NONBLOCK) {
        return true;
    }

    // Preserve the existing flags and add O_NONBLOCK.
    return fcntl(
        fd,
        F_SETFL,
        flags | O_NONBLOCK
    ) != -1;
}


bool setNoDelay(int fd)
{
    int enabled = 1;

    // TCP_NODELAY disables Nagle's algorithm. This avoids waiting
    // to combine several small writes into a larger TCP packet.
    return setsockopt(
        fd,
        IPPROTO_TCP,
        TCP_NODELAY,
        &enabled,
        sizeof(enabled)
    ) != -1;
}


bool setSOTimestamp(int fd)
{
    int enabled = 1;

    // SO_TIMESTAMP asks the kernel to provide a receive timestamp
    // as control data when recvmsg() reads an incoming message.
    return setsockopt(
        fd,
        SOL_SOCKET,
        SO_TIMESTAMP,
        &enabled,
        sizeof(enabled)
    ) != -1;
}


bool wouldBlock()
{
    // These errors are temporary for non-blocking operations.
    // They do not mean that the socket is disconnected.
    return
        errno == EAGAIN ||
        errno == EWOULDBLOCK ||
        errno == EINPROGRESS;
}


int createSocket(
    Logger& logger,
    const std::string& target_ip,
    const std::string& iface,
    int port,
    bool is_udp,
    bool is_blocking,
    bool is_listening,
    bool needs_so_timestamp)
{
    // Use the supplied IP. If it is empty, find the interface's IP.
    const std::string ip =
        target_ip.empty()
            ? getIfaceIP(iface)
            : target_ip;

    if (ip.empty())
    {
        logger.log(
            "Could not find IPv4 address for interface:%\n",
            iface
        );

        return -1;
    }


    // Describe the kind of address and socket getaddrinfo() should return.
    addrinfo hints{};

    hints.ai_family = AF_INET;

    hints.ai_socktype =
        is_udp
            ? SOCK_DGRAM
            : SOCK_STREAM;

    hints.ai_protocol =
        is_udp
            ? IPPROTO_UDP
            : IPPROTO_TCP;

    hints.ai_flags =
        is_listening
            ? AI_PASSIVE
            : 0;

    // The port is already supplied as a numeric string.
    hints.ai_flags |= AI_NUMERICSERV;

    addrinfo* result = nullptr;

    const std::string port_string =
        std::to_string(port);

    // Convert the IP and port into the sockaddr structure used by Linux.
    const int rc =
        getaddrinfo(
            ip.c_str(),
            port_string.c_str(),
            &hints,
            &result
        );

    if (rc != 0)
    {
        logger.log(
            "getaddrinfo failed:%\n",
            gai_strerror(rc)
        );

        return -1;
    }

    int fd = -1;

    // Try each address returned by getaddrinfo() until one succeeds.
    for (addrinfo* candidate = result;
         candidate != nullptr;
         candidate = candidate->ai_next)
    {
        // socket() creates the operating-system socket descriptor.
        fd = ::socket(
            candidate->ai_family,
            candidate->ai_socktype,
            candidate->ai_protocol
        );

        if (fd == -1) {
            continue;
        }

        // Non-blocking calls return immediately when no work is possible.
        if (!is_blocking && !setNonBlocking(fd))
        {
            ::close(fd);
            fd = -1;
            continue;
        }

        // Nagle's algorithm is unrelated to blocking mode, so disable it
        // for every TCP socket used by this low-latency example.
        if (!is_udp && !setNoDelay(fd))
        {
            ::close(fd);
            fd = -1;
            continue;
        }

        if (!is_listening)
        {
            // A client uses connect() to begin connecting to the server.
            const int connect_result =
                ::connect(
                    fd,
                    candidate->ai_addr,
                    candidate->ai_addrlen
                );

            // EINPROGRESS is normal for a non-blocking connection.
            if (connect_result < 0 &&
                !wouldBlock())
            {
                ::close(fd);
                fd = -1;
                continue;
            }
        }

        if (is_listening)
        {
            int enabled = 1;

            // Allow the server address to be reused after a restart.
            if (setsockopt(
                    fd,
                    SOL_SOCKET,
                    SO_REUSEADDR,
                    &enabled,
                    sizeof(enabled)
                ) == -1)
            {
                ::close(fd);
                fd = -1;
                continue;
            }


            // Attach this socket to the requested local IP and port.
            if (::bind(
                    fd,
                    candidate->ai_addr,
                    candidate->ai_addrlen
                ) == -1)
            {
                ::close(fd);
                fd = -1;
                continue;
            }


            // A TCP server must listen before it can accept clients.
            if (!is_udp &&
                ::listen(
                    fd,
                    MAX_TCP_SERVER_BACKLOG
                ) == -1)
            {
                ::close(fd);
                fd = -1;
                continue;
            }
        }

        // Enable kernel receive timestamps when requested by the caller.
        if (needs_so_timestamp &&
            !setSOTimestamp(fd))
        {
            ::close(fd);
            fd = -1;
            continue;
        }

        // Every required step succeeded for this candidate.
        break;
    }

    // Release the address list allocated by getaddrinfo().
    freeaddrinfo(result);

    return fd;
}
