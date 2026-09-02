#pragma once

#include "logger.hpp"

#include <string>


constexpr int MAX_TCP_SERVER_BACKLOG = 1024;


std::string getIfaceIP(
    const std::string& iface
);


bool setNonBlocking(int fd);

bool setNoDelay(int fd);

bool setSOTimestamp(int fd);

bool wouldBlock();

bool setTTL(
    int fd,
    int ttl
);

bool setMcastTTL(
    int fd,
    int ttl
);


int createSocket(
    Logger& logger,
    const std::string& target_ip,
    const std::string& iface,
    int port,
    bool is_udp,
    bool is_blocking,
    bool is_listening,
    int ttl,
    bool needs_so_timestamp
);