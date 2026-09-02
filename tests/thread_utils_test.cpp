#include "thread_utils.hpp"

#include <atomic>
#include <iostream>
#include <stdexcept>


// Print an explanation when the test fails.
int fail(const char* message)
{
    std::cerr << "FAILED: " << message << '\n';
    return 1;
}


int main()
{
    std::atomic<int> result{0};

    // -1 starts a normal thread without trying to pin it.
    auto thread = createAndStartThread(
        -1,
        [&result](int left, int right)
        {
            result.store(left + right);
        },
        20,
        22
    );

    thread.join();

    if (result.load() != 42) {
        return fail("The thread should run with its supplied arguments.");
    }

    // Invalid values must be rejected before CPU_SET is called.
    if (setThreadCore(-1)) {
        return fail("A negative CPU number should be rejected.");
    }

    if (setThreadCore(CPU_SETSIZE)) {
        return fail("A CPU number outside the set should be rejected.");
    }

    bool caught_expected_error = false;

    try
    {
        auto invalid_thread = createAndStartThread(
            CPU_SETSIZE,
            []() {}
        );

        invalid_thread.join();
    }
    catch (const std::runtime_error&)
    {
        caught_expected_error = true;
    }

    if (!caught_expected_error) {
        return fail("An invalid pinned thread should throw runtime_error.");
    }

    std::cout << "All thread utility tests passed.\n";
    return 0;
}
