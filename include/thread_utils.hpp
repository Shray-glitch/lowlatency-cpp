#pragma once

#include <functional>
#include <iostream>
#include <pthread.h>
#include <sched.h>
#include <thread>
#include <utility>
#include <atomic>


inline bool setThreadCore(int core_id) noexcept
{
    cpu_set_t cpu_set;

    CPU_ZERO(&cpu_set);
    CPU_SET(core_id, &cpu_set);

    return pthread_setaffinity_np(
        pthread_self(),
        sizeof(cpu_set),
        &cpu_set
    ) == 0;
}

template<typename Task, typename... Args>
std::thread createAndStartThread(
    int core_id,
    Task&& task,
    Args&&... args)
{
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
            if (core_id >= 0) {

                if (!setThreadCore(core_id)) {

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

            std::invoke(
                fn,
                std::forward<decltype(thread_args)>(
                    thread_args
                )...
            );
        },

        std::forward<Args>(args)...
    );


    while (!running.load() && !failed.load()) {
        std::this_thread::yield();
    }


    if (failed.load()) {

        thread.join();

        throw std::runtime_error(
            "Failed to start thread"
        );
    }


    return thread;
}