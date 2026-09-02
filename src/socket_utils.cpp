#include "socket_utils.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

std::string getIfaceIP(const std::string& iface)
{
    char buffer[NI_MAXHOST] = {};

    ifaddrs* interfaces = nullptr;

    if (getifaddrs(&interfaces) == -1) {
        return {};
    }


    for (ifaddrs* current = interfaces;
         current != nullptr;
         current = current->ifa_next)
    {
        if (current->ifa_addr == nullptr) {
            continue;
        }

        if (current->ifa_addr->sa_family != AF_INET) {
            continue;
        }

        if (iface != current->ifa_name) {
            continue;
        }


        getnameinfo(
            current->ifa_addr,
            sizeof(sockaddr_in),
            buffer,
            sizeof(buffer),
            nullptr,
            0,
            NI_NUMERICHOST
        );

        break;
    }


    freeifaddrs(interfaces);

    return buffer;
}


bool setNonBlocking(int fd)
{
    const int flags =
        fcntl(fd, F_GETFL, 0);

    if (flags == -1) {
        return false;
    }

    if (flags & O_NONBLOCK) {
        return true;
    }

    return fcntl(
        fd,
        F_SETFL,
        flags | O_NONBLOCK
    ) != -1;
}

bool setNoDelay(int fd)
{
    int enabled = 1;

    return setsockopt(
        fd,
        IPPROTO_TCP,
        TCP_NODELAY,
        &enabled,
        sizeof(enabled)
    ) != -1;
}

bool wouldBlock()
{
    return
        errno == EWOULDBLOCK ||
        errno == EINPROGRESS;
}

bool setTTL(
    int fd,
    int ttl)
{
    return setsockopt(
        fd,
        IPPROTO_IP,
        IP_TTL,
        &ttl,
        sizeof(ttl)
    ) != -1;
}


bool setMcastTTL(
    int fd,
    int ttl)
{
    return setsockopt(
        fd,
        IPPROTO_IP,
        IP_MULTICAST_TTL,
        &ttl,
        sizeof(ttl)
    ) != -1;
}

bool setSOTimestamp(int fd)
{
    int enabled = 1;

    return setsockopt(
        fd,
        SOL_SOCKET,
        SO_TIMESTAMP,
        &enabled,
        sizeof(enabled)
    ) != -1;
}

int createSocket(
    Logger& logger,
    const std::string& target_ip,
    const std::string& iface,
    int port,
    bool is_udp,
    bool is_blocking,
    bool is_listening,
    int ttl,
    bool needs_so_timestamp)
{
    const std::string ip =
        target_ip.empty()
            ? getIfaceIP(iface)
            : target_ip;
    
    
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

    hints.ai_flags |= AI_NUMERICSERV;

    addrinfo* result = nullptr;

    const std::string port_string =
        std::to_string(port);

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

    for (addrinfo* candidate = result;
         candidate != nullptr;
         candidate = candidate->ai_next)
    {
        fd = ::socket(
            candidate->ai_family,
            candidate->ai_socktype,
            candidate->ai_protocol
        );

        if (fd == -1) {
            continue;
        }

        if (!is_blocking)
        {
            if (!setNonBlocking(fd))
            {
                ::close(fd);
                fd = -1;
                continue;
            }

            if (!is_udp &&
                !setNoDelay(fd))
            {
                ::close(fd);
                fd = -1;
                continue;
            }
        }

        if (!is_listening)
        {
            const int connect_result =
                ::connect(
                    fd,
                    candidate->ai_addr,
                    candidate->ai_addrlen
                );

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

        if (needs_so_timestamp &&
            !setSOTimestamp(fd))
        {
            ::close(fd);
            fd = -1;
            continue;
        }

        break;
    }

    freeaddrinfo(result);

    return fd;
}