#include "logger.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>


// Print an explanation when the test fails.
int fail(const char* message)
{
    std::cerr << "FAILED: " << message << '\n';
    return 1;
}


int main()
{
    const char* file_name = "logger_test.log";

    // This message is larger than the logger queue.
    // It checks that a full queue waits instead of losing data.
    const std::string large_message(
        LOG_QUEUE_SIZE + 10'000,
        'x'
    );

    {
        Logger logger(file_name);

        const char* symbol = "AAPL";
        const std::string status = "accepted";

        logger.log(
            "id:% price:% symbol:% status:% percent:100%%\n",
            42,
            150.25,
            symbol,
            status
        );

        logger.log("%", large_message);

        // The destructor waits for all queued values to reach the file.
    }

    std::ifstream input(file_name);

    if (!input.is_open()) {
        return fail("The logger should create its output file.");
    }

    const std::string actual{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()
    };

    input.close();
    std::remove(file_name);

    const std::string expected =
        "id:42 price:150.25 symbol:AAPL "
        "status:accepted percent:100%\n" +
        large_message;

    if (actual != expected) {
        return fail("The file should contain the complete formatted message.");
    }

    std::cout << "All logger tests passed.\n";
    return 0;
}
