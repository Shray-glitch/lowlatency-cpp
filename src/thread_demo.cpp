#include "thread_utils.hpp"

#include <iostream>
#include <sched.h>
#include <string>


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
    auto t = createAndStartThread(
        2,
        printMessage,
        10,
        std::string("hello")
    );

    auto t1 = createAndStartThread(
        -1,
        printMessage,
        10,
        std::string("hello1")
    );

    auto t2 = createAndStartThread(
        50000000,
        printMessage,
        10,
        std::string("hello2")
    );

    t.join();
    t1.join();
    t2.join();
}