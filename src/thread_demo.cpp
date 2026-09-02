#include "thread_utils.hpp"

#include <iostream>
#include <sched.h>
#include <stdexcept>
#include <string>


// Print the CPU that is running this thread.
void printMessage(
    int id,
    std::string message)
{
    std::cout
        << "CPU " << sched_getcpu()
        << " | id=" << id
        << " | message=" << message
        << '\n';
}


int main()
{
    // This CPU is already available to the current process.
    const int current_cpu = sched_getcpu();

    if (current_cpu < 0) {
        std::cerr << "Could not detect the current CPU.\n";
        return 1;
    }

    // Start one thread pinned to the current CPU.
    auto pinned_thread = createAndStartThread(
        current_cpu,
        printMessage,
        10,
        std::string("pinned thread")
    );

    // Passing -1 starts a normal thread without CPU pinning.
    auto normal_thread = createAndStartThread(
        -1,
        printMessage,
        20,
        std::string("normal thread")
    );

    // Join successful threads before testing the failure path.
    pinned_thread.join();
    normal_thread.join();

    try
    {
        // CPU_SETSIZE is just outside the valid CPU-set range.
        auto invalid_thread = createAndStartThread(
            CPU_SETSIZE,
            printMessage,
            30,
            std::string("this should not run")
        );

        invalid_thread.join();
    }
    catch (const std::runtime_error& error)
    {
        // Failure is expected and is handled without terminating.
        std::cout
            << "Expected error: "
            << error.what()
            << '\n';
    }

    return 0;
}
