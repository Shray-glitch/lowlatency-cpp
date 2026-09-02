#pragma once

#include <atomic>
#include <functional>
#include <iostream>
#include <pthread.h>
#include <sched.h>
#include <stdexcept>
#include <thread>
#include <utility>


// Pin the calling thread to one Linux CPU core.
// Return false when the core number is invalid or pinning fails.
inline bool setThreadCore(int core_id) noexcept
{
    // CPU_SET cannot safely store a number outside this range.
    if (core_id < 0 || core_id >= CPU_SETSIZE) {
        return false;
    }

    cpu_set_t cpu_set;

    CPU_ZERO(&cpu_set);
    CPU_SET(core_id, &cpu_set);

    return pthread_setaffinity_np(
        pthread_self(),
        sizeof(cpu_set),
        &cpu_set
    ) == 0;
}


// Start a thread and optionally pin it to a CPU core.
// Pass -1 as core_id when no CPU pinning is required.
template<typename Task, typename... Args>
std::thread createAndStartThread(
    int core_id,
    Task&& task,
    Args&&... args)
{
    // The caller waits until the new thread finishes its setup.
    std::atomic<bool> running{false};
    std::atomic<bool> failed{false};

    std::thread thread(
        [
            &running,
            &failed,
            core_id,
            fn = std::forward<Task>(task)
        ]
        (auto&&... thread_args) mutable
        {
            // Negative core IDs mean that pinning is disabled.
            if (core_id >= 0)
            {
                if (!setThreadCore(core_id))
                {
                    std::cerr
                        << "Failed to set thread affinity to CPU "
                        << core_id
                        << '\n';

                    failed.store(true);
                    return;
                }
            }

            // Thread setup completed successfully.
            running.store(true);

            // Run the task with the supplied arguments.
            std::invoke(
                fn,
                std::forward<decltype(thread_args)>(
                    thread_args
                )...
            );
        },

        std::forward<Args>(args)...
    );

    // Do not return until setup has either succeeded or failed.
    while (!running.load() && !failed.load()) {
        std::this_thread::yield();
    }

    if (failed.load())
    {
        // The failed thread has already returned, but it must be joined.
        thread.join();

        throw std::runtime_error(
            "Failed to start thread"
        );
    }

    return thread;
}
