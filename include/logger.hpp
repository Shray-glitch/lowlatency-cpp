#pragma once

#include "lf_queue.hpp"

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <thread>


// Number of values that can wait in the logger queue.
// The book uses 8 * 1024 * 1024; this demo uses less memory.
constexpr std::size_t LOG_QUEUE_SIZE = 64 * 1024;


// Identifies which value inside LogElement is currently valid.
enum class LogType : std::int8_t
{
    CHAR = 0,

    INTEGER,
    LONG_INTEGER,
    LONG_LONG_INTEGER,

    UNSIGNED_INTEGER,
    UNSIGNED_LONG_INTEGER,
    UNSIGNED_LONG_LONG_INTEGER,

    FLOAT,
    DOUBLE
};


// One value waiting to be written to the log file.
// Strings are divided into separate character elements.
struct LogElement
{
    LogType type_ = LogType::CHAR;

    // Only the member selected by type_ should be read.
    union
    {
        char c;

        int i;
        long l;
        long long ll;

        unsigned u;
        unsigned long ul;
        unsigned long long ull;

        float f;
        double d;

    } value_{};
};


// Sends log values to a background thread that writes them to a file.
//
// The queue is SPSC, so exactly one application thread should call log().
// The background logger thread is the single consumer.
class Logger
{
private:

    // File supplied when the logger is created.
    const std::string file_name_;

    // Used only by the background thread after construction.
    std::ofstream file_;

    // Passes values from the calling thread to the background thread.
    LFQueue<LogElement> queue_;

    // Becomes false when the logger is being destroyed.
    std::atomic<bool> running_{true};

    std::thread logger_thread_;


    // Read queued values and write them to the file.
    void flushQueue() noexcept;


    // Add one complete element to the queue.
    void pushValue(
        const LogElement& element
    ) noexcept;


    // Convert each supported number type into a LogElement.
    void pushValue(char value) noexcept;

    void pushValue(int value) noexcept;
    void pushValue(long value) noexcept;
    void pushValue(long long value) noexcept;

    void pushValue(unsigned value) noexcept;
    void pushValue(unsigned long value) noexcept;
    void pushValue(unsigned long long value) noexcept;

    void pushValue(float value) noexcept;
    void pushValue(double value) noexcept;


    // Add a string to the queue one character at a time.
    void pushValue(const char* value) noexcept;
    void pushValue(const std::string& value) noexcept;


public:

    explicit Logger(
        const std::string& file_name
    );

    ~Logger();


    Logger() = delete;

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;


    // Replace each single % with the next supplied value.
    // Use %% when a real percent character is needed.
    template<typename T, typename... Args>
    void log(
        const char* s,
        const T& value,
        Args... args
    ) noexcept
    {
        while (*s)
        {
            if (*s == '%')
            {
                // Two percent signs produce one percent character.
                if (*(s + 1) == '%')
                {
                    ++s;
                }
                else
                {
                    // This percent sign uses the current argument.
                    pushValue(value);

                    // Continue with the remaining arguments.
                    log(
                        s + 1,
                        args...
                    );

                    return;
                }
            }

            pushValue(*s);
            ++s;
        }

        // Reaching the end here means an argument was not used.
        assert(
            false &&
            "Extra arguments provided to Logger::log()"
        );
    }


    // Handle a message that has no remaining values to insert.
    void log(const char* s) noexcept
    {
        while (*s)
        {
            if (*s == '%')
            {
                if (*(s + 1) == '%')
                {
                    ++s;
                }
                else
                {
                    // A single percent sign still needs an argument.
                    assert(
                        false &&
                        "Missing argument for Logger::log()"
                    );
                }
            }

            pushValue(*s);
            ++s;
        }
    }

};
