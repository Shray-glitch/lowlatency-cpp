#pragma once

#include <cassert>
#include <cstddef>
#include <memory>
#include <new>
#include <utility>
#include <vector>


// A fixed-size collection of reusable memory slots.
//
// The pool reserves all its memory when it is created. allocate() constructs
// an object inside a free slot, and deallocate() destroys the object so that
// the same slot can be used again.
//
// This class is not thread-safe.
template<typename T>
class MemPool
{
private:

    struct ObjectBlock
    {
        // Empty memory with the correct size and alignment for one T.
        // No T object exists here while is_free_ is true.
        alignas(T) std::byte storage_[sizeof(T)];

        bool is_free_ = true;
    };

    // Owns all the slots. This vector never grows after construction.
    std::vector<ObjectBlock> store_;

    // The next slot allocate() should try.
    std::size_t next_free_index_ = 0;

    // Lets allocate() detect a full pool without searching forever.
    std::size_t num_free_ = 0;


    // Starting after the current slot, find the next free slot.
    void updateNextFreeIndex()
    {
        const std::size_t start = next_free_index_;

        do
        {
            ++next_free_index_;

            // Continue searching from the beginning after reaching the end.
            if (next_free_index_ == store_.size()) {
                next_free_index_ = 0;
            }

            if (store_[next_free_index_].is_free_) {
                return;
            }

        } while (next_free_index_ != start);

        // allocate() calls this only when num_free_ says a slot exists.
        assert(false && "No free block found");
    }


public:

    explicit MemPool(std::size_t num_elems)
        : store_(num_elems),
          num_free_(num_elems)
    {
        assert(num_elems > 0);
    }


    ~MemPool()
    {
        // Clean up objects that the caller forgot to deallocate.
        for (ObjectBlock& block : store_)
        {
            if (!block.is_free_)
            {
                // is_free_ is false, so these bytes currently contain a T.
                T* object = std::launder(
                    reinterpret_cast<T*>(block.storage_)
                );

                std::destroy_at(object);
            }
        }
    }


    MemPool() = delete;

    MemPool(const MemPool&) = delete;
    MemPool& operator=(const MemPool&) = delete;


    template<typename... Args>
    T* allocate(Args&&... args)
    {
        // A fixed-size pool cannot grow when all slots are occupied.
        if (num_free_ == 0) {
            return nullptr;
        }

        ObjectBlock& block =
            store_[next_free_index_];

        assert(block.is_free_);

        // Placement new constructs T inside this slot's existing memory.
        T* object =
            new (block.storage_)
                T(std::forward<Args>(args)...);

        block.is_free_ = false;
        --num_free_;

        if (num_free_ > 0) {
            updateNextFreeIndex();
        }

        return object;
    }


    bool deallocate(T* elem) noexcept
    {
        if (elem == nullptr) {
            return false;
        }

        // Find the slot whose starting address matches elem.
        std::size_t elem_index = store_.size();

        for (std::size_t i = 0; i < store_.size(); ++i)
        {
            if (static_cast<void*>(store_[i].storage_) ==
                static_cast<void*>(elem))
            {
                elem_index = i;
                break;
            }
        }

        // Reject pointers from another pool and slots already freed.
        if (elem_index == store_.size() ||
            store_[elem_index].is_free_)
        {
            return false;
        }

        // Run T's destructor before marking its memory as reusable.
        std::destroy_at(elem);

        store_[elem_index].is_free_ = true;
        ++num_free_;

        // Prefer the slot that has just been released.
        next_free_index_ = elem_index;

        return true;
    }

};
