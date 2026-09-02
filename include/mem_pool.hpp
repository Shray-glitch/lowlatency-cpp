#pragma once

#include <cassert>
#include <cstddef>
#include <new>
#include <utility>
#include <vector>


template<typename T>
class MemPool
{
private:

    struct ObjectBlock
    {
        T object_;
        bool is_free_ = true;
    };

    std::vector<ObjectBlock> store_;

    std::size_t next_free_index_ = 0;


    void updateNextFreeIndex()
    {
        const std::size_t start = next_free_index_;

        do
        {
            ++next_free_index_;

            if (next_free_index_ == store_.size()) {
                next_free_index_ = 0;
            }

            if (store_[next_free_index_].is_free_) {
                return;
            }

        } while (next_free_index_ != start);

        assert(false && "Memory pool is full");
    }


public:

    explicit MemPool(std::size_t num_elems)
        : store_(num_elems)
    {
        assert(num_elems > 0);

        assert(
            reinterpret_cast<const ObjectBlock*>(
                &store_[0].object_
            ) == &store_[0]
        );
    }


    MemPool() = delete;

    MemPool(const MemPool&) = delete;
    MemPool& operator=(const MemPool&) = delete;


    template<typename... Args>
    T* allocate(Args&&... args)
    {
        ObjectBlock& block =
            store_[next_free_index_];

        assert(block.is_free_);

        T* object =
            new (&block.object_)
                T(std::forward<Args>(args)...);

        block.is_free_ = false;

        updateNextFreeIndex();

        return object;
    }

    void deallocate(const T* elem) noexcept
    {
        const auto elem_index =
            reinterpret_cast<const ObjectBlock*>(elem)
            - &store_[0];

        assert(
            elem_index >= 0 &&
            static_cast<std::size_t>(elem_index) < store_.size()
        );

        assert(
            !store_[elem_index].is_free_
        );

        store_[elem_index].is_free_ = true;
    }


};