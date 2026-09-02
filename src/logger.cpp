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
    // Open the file before starting the background thread.
    file_.open(file_name_);

    if (!file_.is_open())
    {
        throw std::runtime_error(
            "Could not open log file: " + file_name_
        );
    }


    // This thread consumes queued values and writes them to the file.
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
    // Wait until every queued value has been written.
    while (queue_.size() != 0)
    {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(1)
        );
    }


    // Tell the background thread to stop.
    running_ = false;


    // Wait until flushQueue() has returned.
    if (logger_thread_.joinable())
    {
        logger_thread_.join();
    }


    // Closing the stream also flushes its internal file buffer.
    file_.close();
}


void Logger::flushQueue() noexcept
{
    while (running_)
    {
        const LogElement* next =
            queue_.getNextToRead();


        // Write everything currently available in the queue.
        while (next != nullptr)
        {
            // type_ tells us which union member contains the value.
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


            // Release this queue slot after writing its value.
            queue_.updateReadIndex();

            next =
                queue_.getNextToRead();
        }


        // Avoid using an entire CPU core while no logs are waiting.
        std::this_thread::sleep_for(
            std::chrono::milliseconds(1)
        );
    }
}


void Logger::pushValue(
    const LogElement& element
) noexcept
{
    LogElement* slot = nullptr;

    // Wait if the producer has filled every queue slot.
    // This avoids losing the log value or dereferencing nullptr.
    while ((slot = queue_.getNextToWriteTo()) == nullptr)
    {
        std::this_thread::yield();
    }

    *slot = element;

    // Publish the completed element to the background thread.
    queue_.updateWriteIndex();
}


// Each overload stores the value and its matching LogType.
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
    // A string is stored as a sequence of character elements.
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
