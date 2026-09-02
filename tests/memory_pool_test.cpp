#include "mem_pool.hpp"

#include <iostream>


struct TestOrder
{
    static int objects_alive;

    int id;

    explicit TestOrder(int order_id)
        : id(order_id)
    {
        ++objects_alive;
    }

    ~TestOrder()
    {
        --objects_alive;
    }
};


int TestOrder::objects_alive = 0;


// Print an explanation when a test fails.
int fail(const char* message)
{
    std::cerr << "FAILED: " << message << '\n';
    return 1;
}


int main()
{
    {
        // This pool can hold two orders.
        MemPool<TestOrder> pool(2);

        TestOrder* first = pool.allocate(101);
        TestOrder* second = pool.allocate(202);

        if (first == nullptr || second == nullptr) {
            return fail("The first two allocations should succeed.");
        }

        if (first->id != 101 || second->id != 202) {
            return fail("The order IDs should be stored correctly.");
        }

        if (TestOrder::objects_alive != 2) {
            return fail("Two orders should be alive.");
        }

        // The pool is full, so another allocation should fail.
        if (pool.allocate(303) != nullptr) {
            return fail("Allocation should fail when the pool is full.");
        }

        if (!pool.deallocate(first)) {
            return fail("The first deallocation should succeed.");
        }

        // The same object must not be deallocated twice.
        if (pool.deallocate(first)) {
            return fail("A second deallocation should fail.");
        }

        TestOrder* reused = pool.allocate(303);

        // The pool should reuse the slot released by first.
        if (reused != first) {
            return fail("The released slot should be reused.");
        }

        if (reused->id != 303) {
            return fail("The reused slot should contain the new order.");
        }

        pool.deallocate(reused);

        // We intentionally do not deallocate second.
        // The MemPool destructor should destroy it.
    }

    if (TestOrder::objects_alive != 0) {
        return fail("All orders should have been destroyed.");
    }

    std::cout << "All memory pool tests passed.\n";
    return 0;
}
