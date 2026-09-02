#include "lf_queue.hpp"
#include "thread_utils.hpp"

#include <chrono>
#include <iostream>
#include <thread>


// A simple item passed from the producer to the consumer.
struct Order
{
    int id = 0;
    double price = 0.0;
};


// Read and print five orders from the queue.
void consumeOrders(LFQueue<Order>* queue)
{
    for (int i = 0; i < 5; ++i)
    {
        const Order* order = nullptr;

        // Wait while the queue is empty.
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

        // Tell the queue that this order has been fully read.
        queue->updateReadIndex();
    }
}


int main()
{
    // The queue reserves space for ten orders.
    LFQueue<Order> queue(10);

    // Start the single consumer on another thread.
    auto consumer =
        createAndStartThread(
            -1,
            consumeOrders,
            &queue
        );


    // The main thread acts as the single producer.
    for (int i = 0; i < 5; ++i)
    {
        Order* order = nullptr;

        // Wait while every queue slot is in use.
        while ((order = queue.getNextToWriteTo()) == nullptr)
        {
            std::this_thread::yield();
        }

        // Fill the slot before publishing it.
        order->id = i;
        order->price = 100.0 + i;

        // Tell the consumer that this order is ready.
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


    // Wait until the consumer has read all five orders.
    consumer.join();

    return 0;
}
