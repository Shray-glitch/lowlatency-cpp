#pragma once

#include <cassert>
#include <cstddef>
#include <memory>
#include <new>
#include <utility>
#include <vector>


template<typename T>
class MemPool
{
private:

    struct ObjectBlock
    {
        // Reserve correctly aligned memory without constructing T yet.
        alignas(T) std::byte storage_[sizeof(T)];
        bool is_free_ = true;
    };

    std::vector<ObjectBlock> store_;

    std::size_t next_free_index_ = 0;
    std::size_t num_free_ = 0;


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

        // allocate() calls this only when num_free_ is non-zero.
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
        // Destroy objects that were not explicitly deallocated.
        for (ObjectBlock& block : store_)
        {
            if (!block.is_free_)
            {
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
        if (num_free_ == 0) {
            return nullptr;
        }

        ObjectBlock& block =
            store_[next_free_index_];

        assert(block.is_free_);

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

        // Find the block whose storage begins at elem's address.
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

        if (elem_index == store_.size() ||
            store_[elem_index].is_free_)
        {
            return false;
        }

        std::destroy_at(elem);

        store_[elem_index].is_free_ = true;
        ++num_free_;

        // Reuse the newly freed block on the next allocation.
        next_free_index_ = elem_index;

        return true;
    }

};
