#include "lf_queue.hpp"
#include "thread_utils.hpp"

#include <chrono>
#include <iostream>
#include <thread>


struct Order
{
    int id = 0;
    double price = 0.0;
};

void consumeOrders(LFQueue<Order>* queue)
{
    for (int i = 0; i < 5; ++i)
    {
        const Order* order = nullptr;

        // Wait until an element becomes available.
        while ((order = queue->getNextToRead()) == nullptr)
        {
            std::this_thread::yield();
        }

        std::cout
            << "Consumed: id="
            << order->id
            << " price="
            << order->price
            << '\n';

        queue->updateReadIndex();
    }
}

int main()
{
    LFQueue<Order> queue(10);

    auto consumer =
        createAndStartThread(
            -1,
            consumeOrders,
            &queue
        );


    for (int i = 0; i < 5; ++i)
    {
        Order* order =
            queue.getNextToWriteTo();

        order->id = i;
        order->price = 100.0 + i;

        // Only now make it visible logically.
        queue.updateWriteIndex();

        std::cout
            << "Produced: id="
            << order->id
            << " price="
            << order->price
            << '\n';

        std::this_thread::sleep_for(
            std::chrono::milliseconds(100)
        );
    }


    consumer.join();

    return 0;
}