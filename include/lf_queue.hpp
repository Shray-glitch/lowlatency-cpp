#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <vector>


// A fixed-size queue for exactly one producer and one consumer.
//
// The producer asks for a free slot, fills it, and then publishes it.
// The consumer asks for the next item, reads it, and then releases it.
// The slots are reused in a circle instead of allocating new memory.
template<typename T>
class LFQueue
{
private:

    // Stores every reusable queue slot.
    std::vector<T> store_;

    // Position where the producer will write the next item.
    std::atomic<std::size_t> next_write_index_{0};

    // Position where the consumer will read the next item.
    std::atomic<std::size_t> next_read_index_{0};

    // Number of items that have been published but not yet consumed.
    std::atomic<std::size_t> num_elements_{0};


public:

    explicit LFQueue(std::size_t num_elems)
        : store_(num_elems)
    {
        assert(num_elems > 0);
    }


    LFQueue() = delete;

    LFQueue(const LFQueue&) = delete;
    LFQueue& operator=(const LFQueue&) = delete;


    // Return the next free slot to the producer.
    // Return nullptr when every slot is still in use.
    T* getNextToWriteTo() noexcept
    {
        if (num_elements_.load() == store_.size()) {
            return nullptr;
        }

        return &store_[next_write_index_];
    }


    // Call this after the producer has finished writing the item.
    // This publishes the item so the consumer can read it.
    void updateWriteIndex() noexcept
    {
        next_write_index_ =
            (next_write_index_ + 1) % store_.size();

        ++num_elements_;
    }


    // Return the next available item to the consumer.
    // Return nullptr when the queue is empty.
    const T* getNextToRead() const noexcept
    {
        if (num_elements_.load() == 0) {
            return nullptr;
        }

        return &store_[next_read_index_];
    }


    // Call this after the consumer has finished reading the item.
    // This releases the slot so the producer can reuse it.
    void updateReadIndex() noexcept
    {
        assert(num_elements_ != 0);

        next_read_index_ =
            (next_read_index_ + 1) % store_.size();

        --num_elements_;
    }


    // Return the number of items currently waiting to be consumed.
    std::size_t size() const noexcept
    {
        return num_elements_.load();
    }
};
