#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <vector>


template<typename T>
class LFQueue
{
private:

    std::vector<T> store_;

    std::atomic<std::size_t> next_write_index_{0};
    std::atomic<std::size_t> next_read_index_{0};
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


    // ========================================================
    // Producer side
    // ========================================================

    T* getNextToWriteTo() noexcept
    {
        return &store_[next_write_index_];
    }


    void updateWriteIndex() noexcept
    {
        next_write_index_ =
            (next_write_index_ + 1) % store_.size();

        ++num_elements_;
    }


    // ========================================================
    // Consumer side
    // ========================================================

    const T* getNextToRead() const noexcept
    {
        if (next_read_index_ == next_write_index_) {
            return nullptr;
        }

        return &store_[next_read_index_];
    }


    void updateReadIndex() noexcept
    {
        assert(num_elements_ != 0);

        next_read_index_ =
            (next_read_index_ + 1) % store_.size();

        --num_elements_;
    }


    std::size_t size() const noexcept
    {
        return num_elements_.load();
    }
};