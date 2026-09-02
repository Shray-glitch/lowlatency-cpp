#pragma once

#include <chrono>
#include <cstdint>
#include <ctime>
#include <string>


// Store timestamps and measured durations as a signed number of nanoseconds.
using Nanos = std::int64_t;


// Unit conversion values.
// For example, one second contains NANOS_TO_SECS nanoseconds.
constexpr Nanos NANOS_TO_MICROS = 1000;
constexpr Nanos MICROS_TO_MILLIS = 1000;
constexpr Nanos MILLIS_TO_SECS = 1000;

constexpr Nanos NANOS_TO_MILLIS =
    NANOS_TO_MICROS * MICROS_TO_MILLIS;

constexpr Nanos NANOS_TO_SECS =
    NANOS_TO_MILLIS * MILLIS_TO_SECS;


// Return wall-clock time as nanoseconds since the Unix epoch.
//
// Use this for timestamps that must correspond to a date and time. The
// operating system can adjust system_clock, so do not use it to benchmark
// elapsed time or latency.
inline Nanos getCurrentNanos() noexcept
{
    return std::chrono::duration_cast<
        std::chrono::nanoseconds
    >(
        std::chrono::system_clock::now()
            .time_since_epoch()
    ).count();
}


// Return monotonic time in nanoseconds.
//
// steady_clock does not move backwards when the wall clock is corrected.
// Subtract two values from this function to measure elapsed time or latency.
inline Nanos getSteadyNanos() noexcept
{
    return std::chrono::duration_cast<
        std::chrono::nanoseconds
    >(
        std::chrono::steady_clock::now()
            .time_since_epoch()
    ).count();
}


// Write local wall-clock time as "YYYY-MM-DD HH:MM:SS".
//
// The caller supplies a string by reference so a null pointer is impossible.
// localtime_r() writes into our own std::tm object instead of using the shared
// internal buffer returned by std::ctime() or std::localtime().
inline std::string& getCurrentTimeStr(
    std::string& time_str)
{
    const auto now =
        std::chrono::system_clock::now();

    const std::time_t time =
        std::chrono::system_clock::to_time_t(now);

    std::tm local_time{};

    if (localtime_r(&time, &local_time) == nullptr)
    {
        time_str.clear();
        return time_str;
    }

    // Nineteen visible characters plus the terminating null character.
    char buffer[20] = {};

    const std::size_t characters_written =
        std::strftime(
            buffer,
            sizeof(buffer),
            "%Y-%m-%d %H:%M:%S",
            &local_time
        );

    if (characters_written == 0) {
        time_str.clear();
    }
    else {
        time_str.assign(buffer, characters_written);
    }

    return time_str;
}
