#include "logger.hpp"
#include "thread_utils.hpp"

#include <chrono>
#include <stdexcept>
#include <thread>


Logger::Logger(
    const std::string& file_name
)
    : file_name_(file_name),
      queue_(LOG_QUEUE_SIZE)
{
    file_.open(file_name_);

    if (!file_.is_open())
    {
        throw std::runtime_error(
            "Could not open log file: " + file_name_
        );
    }


    logger_thread_ =
        createAndStartThread(
            -1,
            [this]()
            {
                flushQueue();
            }
        );
}


Logger::~Logger()
{
    // Wait until the logger thread consumes
    // everything that was queued.
    while (queue_.size() != 0)
    {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(1)
        );
    }


    // Tell the background thread to stop.
    running_ = false;


    // Wait until flushQueue() has actually returned.
    if (logger_thread_.joinable())
    {
        logger_thread_.join();
    }


    file_.close();
}


void Logger::flushQueue() noexcept
{
    while (running_)
    {
        const LogElement* next =
            queue_.getNextToRead();


        // Drain everything currently available.
        while (next != nullptr)
        {
            switch (next->type_)
            {
                case LogType::CHAR:
                    file_ << next->value_.c;
                    break;


                case LogType::INTEGER:
                    file_ << next->value_.i;
                    break;


                case LogType::LONG_INTEGER:
                    file_ << next->value_.l;
                    break;


                case LogType::LONG_LONG_INTEGER:
                    file_ << next->value_.ll;
                    break;


                case LogType::UNSIGNED_INTEGER:
                    file_ << next->value_.u;
                    break;


                case LogType::UNSIGNED_LONG_INTEGER:
                    file_ << next->value_.ul;
                    break;


                case LogType::UNSIGNED_LONG_LONG_INTEGER:
                    file_ << next->value_.ull;
                    break;


                case LogType::FLOAT:
                    file_ << next->value_.f;
                    break;


                case LogType::DOUBLE:
                    file_ << next->value_.d;
                    break;
            }


            queue_.updateReadIndex();

            next =
                queue_.getNextToRead();
        }


        // No work currently available.
        std::this_thread::sleep_for(
            std::chrono::milliseconds(1)
        );
    }
}


void Logger::pushValue(
    const LogElement& element
) noexcept
{
    LogElement* slot =
        queue_.getNextToWriteTo();

    *slot = element;

    queue_.updateWriteIndex();
}


void Logger::pushValue(
    char value
) noexcept
{
    LogElement element;

    element.type_ = LogType::CHAR;
    element.value_.c = value;

    pushValue(element);
}


void Logger::pushValue(
    int value
) noexcept
{
    LogElement element;

    element.type_ = LogType::INTEGER;
    element.value_.i = value;

    pushValue(element);
}


void Logger::pushValue(
    long value
) noexcept
{
    LogElement element;

    element.type_ = LogType::LONG_INTEGER;
    element.value_.l = value;

    pushValue(element);
}


void Logger::pushValue(
    long long value
) noexcept
{
    LogElement element;

    element.type_ =
        LogType::LONG_LONG_INTEGER;

    element.value_.ll = value;

    pushValue(element);
}


void Logger::pushValue(
    unsigned value
) noexcept
{
    LogElement element;

    element.type_ =
        LogType::UNSIGNED_INTEGER;

    element.value_.u = value;

    pushValue(element);
}


void Logger::pushValue(
    unsigned long value
) noexcept
{
    LogElement element;

    element.type_ =
        LogType::UNSIGNED_LONG_INTEGER;

    element.value_.ul = value;

    pushValue(element);
}


void Logger::pushValue(
    unsigned long long value
) noexcept
{
    LogElement element;

    element.type_ =
        LogType::UNSIGNED_LONG_LONG_INTEGER;

    element.value_.ull = value;

    pushValue(element);
}


void Logger::pushValue(
    float value
) noexcept
{
    LogElement element;

    element.type_ = LogType::FLOAT;
    element.value_.f = value;

    pushValue(element);
}


void Logger::pushValue(
    double value
) noexcept
{
    LogElement element;

    element.type_ = LogType::DOUBLE;
    element.value_.d = value;

    pushValue(element);
}


void Logger::pushValue(
    const char* value
) noexcept
{
    while (*value != '\0')
    {
        pushValue(*value);

        ++value;
    }
}


void Logger::pushValue(
    const std::string& value
) noexcept
{
    pushValue(value.c_str());
}