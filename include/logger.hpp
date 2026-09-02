#pragma once

#include "lf_queue.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <thread>


// Book uses: 8 * 1024 * 1024.
// Smaller size for our learning/demo implementation.
constexpr std::size_t LOG_QUEUE_SIZE = 64 * 1024;


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


struct LogElement
{
    LogType type_ = LogType::CHAR;

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


class Logger
{
private:

    const std::string file_name_;

    std::ofstream file_;

    LFQueue<LogElement> queue_;

    std::atomic<bool> running_{true};

    std::thread logger_thread_;


    // Background logger thread runs this.
    void flushQueue() noexcept;


    // Fundamental queue insertion function.
    void pushValue(
        const LogElement& element
    ) noexcept;


    // Primitive overloads.
    void pushValue(char value) noexcept;

    void pushValue(int value) noexcept;
    void pushValue(long value) noexcept;
    void pushValue(long long value) noexcept;

    void pushValue(unsigned value) noexcept;
    void pushValue(unsigned long value) noexcept;
    void pushValue(unsigned long long value) noexcept;

    void pushValue(float value) noexcept;
    void pushValue(double value) noexcept;


    // String overloads.
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
                // %% means: print a literal %
                if (*(s + 1) == '%')
                {
                    ++s;
                }
                else
                {
                    pushValue(value);

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

        assert(
            false &&
            "Extra arguments provided to Logger::log()"
        );
    }

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