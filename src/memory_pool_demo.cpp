#include "mem_pool.hpp"

#include <iostream>

struct Order
{
    int id = 0;
    double price = 0.0;

    Order() = default;

    Order(int id, double price)
        : id(id), price(price)
    {
    }
};

int main()
{
    MemPool<Order> pool(5);

    Order* a = pool.allocate(1, 100.0);
    Order* b = pool.allocate(2, 200.0);
    Order* c = pool.allocate(3, 300.0);

    std::cout << "a = " << a << '\n';
    std::cout << "b = " << b << '\n';
    std::cout << "c = " << c << '\n';

    pool.deallocate(b);

    std::cout << "\nDeallocated b: " << b << "\n\n";

    Order* d = pool.allocate(4, 400.0);
    Order* e = pool.allocate(5, 500.0);

    // At this point next_free_index_ should have wrapped
    // around and found b's old slot.

    // Free another slot first so the pool won't become full.
    pool.deallocate(a);

    Order* f = pool.allocate(6, 600.0);

    std::cout << "d = " << d << '\n';
    std::cout << "e = " << e << '\n';
    std::cout << "f = " << f << '\n';

    std::cout << "\nb old address = " << b << '\n';
    std::cout << "f new address = " << f << '\n';
}