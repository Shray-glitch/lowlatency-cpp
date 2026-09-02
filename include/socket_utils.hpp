#pragma once

#include "logger.hpp"

#include <string>


// Maximum number of TCP connections that can wait for accept().
constexpr int MAX_TCP_SERVER_BACKLOG = 1024;


// Convert a Linux interface name, such as "lo" or "eth0",
// into its IPv4 address. Return an empty string when not found.
std::string getIfaceIP(
    const std::string& iface
);


// Make socket operations return immediately instead of waiting.
bool setNonBlocking(int fd);

// Disable Nagle's algorithm so small TCP messages are sent promptly.
bool setNoDelay(int fd);

// Ask Linux to attach a software receive timestamp to incoming data.
bool setSOTimestamp(int fd);

// Check whether the last socket error means "try again later."
// Call this immediately after a failed non-blocking socket operation.
bool wouldBlock();


// Create either a TCP or UDP IPv4 socket.
//
// For a listening socket, the socket is bound and TCP starts listening.
// For a client socket, a connection attempt is started.
// Return the file descriptor on success or -1 on failure.
int createSocket(
    Logger& logger,
    const std::string& target_ip,
    const std::string& iface,
    int port,
    bool is_udp,
    bool is_blocking,
    bool is_listening,
    bool needs_so_timestamp
);
