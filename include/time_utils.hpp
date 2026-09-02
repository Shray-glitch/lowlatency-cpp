#pragma once

#include <chrono>
#include <cstdint>
#include <ctime>
#include <string>


using Nanos = std::int64_t;


constexpr Nanos NANOS_TO_MICROS = 1000;
constexpr Nanos MICROS_TO_MILLIS = 1000;
constexpr Nanos MILLIS_TO_SECS = 1000;

constexpr Nanos NANOS_TO_MILLIS =
    NANOS_TO_MICROS * MICROS_TO_MILLIS;

constexpr Nanos NANOS_TO_SECS =
    NANOS_TO_MILLIS * MILLIS_TO_SECS;


inline Nanos getCurrentNanos() noexcept
{
    return std::chrono::duration_cast<
        std::chrono::nanoseconds
    >(
        std::chrono::system_clock::now()
            .time_since_epoch()
    ).count();
}


inline std::string& getCurrentTimeStr(
    std::string* time_str)
{
    const auto now =
        std::chrono::system_clock::now();

    const std::time_t time =
        std::chrono::system_clock::to_time_t(now);

    time_str->assign(std::ctime(&time));

    if (!time_str->empty()) {
        time_str->pop_back();
    }

    return *time_str;
}