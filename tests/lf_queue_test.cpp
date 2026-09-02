#include "lf_queue.hpp"

#include <atomic>
#include <iostream>
#include <thread>


struct TestOrder
{
    int id = 0;
};


// Print an explanation when a test fails.
int fail(const char* message)
{
    std::cerr << "FAILED: " << message << '\n';
    return 1;
}


// Check the queue's full, empty, ordering, and reuse behavior.
int testCapacityAndReuse()
{
    LFQueue<TestOrder> queue(2);

    TestOrder* first = queue.getNextToWriteTo();

    if (first == nullptr) {
        return fail("The first write slot should be available.");
    }

    first->id = 101;
    queue.updateWriteIndex();

    TestOrder* second = queue.getNextToWriteTo();

    if (second == nullptr) {
        return fail("The second write slot should be available.");
    }

    second->id = 202;
    queue.updateWriteIndex();

    // Both slots are occupied, so another write must be rejected.
    if (queue.getNextToWriteTo() != nullptr) {
        return fail("A full queue should reject another write.");
    }

    const TestOrder* next = queue.getNextToRead();

    if (next == nullptr || next->id != 101) {
        return fail("The first order read should be 101.");
    }

    queue.updateReadIndex();

    // Reading one item frees its slot for the producer.
    TestOrder* reused = queue.getNextToWriteTo();

    if (reused == nullptr || reused != first) {
        return fail("The first queue slot should be reused.");
    }

    reused->id = 303;
    queue.updateWriteIndex();

    next = queue.getNextToRead();

    if (next == nullptr || next->id != 202) {
        return fail("The second order read should be 202.");
    }

    queue.updateReadIndex();

    next = queue.getNextToRead();

    if (next == nullptr || next->id != 303) {
        return fail("The third order read should be 303.");
    }

    queue.updateReadIndex();

    if (queue.getNextToRead() != nullptr || queue.size() != 0) {
        return fail("The queue should be empty after every read.");
    }

    return 0;
}


// Check that one producer and one consumer can use the queue together.
int testProducerAndConsumer()
{
    constexpr int num_values = 10'000;

    LFQueue<int> queue(64);
    std::atomic<bool> correct_order{true};

    // The consumer expects the numbers in the same order they were written.
    std::thread consumer(
        [&queue, &correct_order]()
        {
            for (int expected = 0; expected < num_values; ++expected)
            {
                const int* value = nullptr;

                // Wait while the queue is empty.
                while ((value = queue.getNextToRead()) == nullptr) {
                    std::this_thread::yield();
                }

                if (*value != expected) {
                    correct_order.store(false);
                }

                queue.updateReadIndex();
            }
        }
    );

    // The main thread acts as the single producer.
    for (int value = 0; value < num_values; ++value)
    {
        int* slot = nullptr;

        // Wait while the queue is full.
        while ((slot = queue.getNextToWriteTo()) == nullptr) {
            std::this_thread::yield();
        }

        *slot = value;
        queue.updateWriteIndex();
    }

    consumer.join();

    if (!correct_order.load()) {
        return fail("The consumer should receive values in order.");
    }

    if (queue.size() != 0) {
        return fail("The queue should be empty when both threads finish.");
    }

    return 0;
}


int main()
{
    if (testCapacityAndReuse() != 0) {
        return 1;
    }

    if (testProducerAndConsumer() != 0) {
        return 1;
    }

    std::cout << "All lock-free queue tests passed.\n";
    return 0;
}
