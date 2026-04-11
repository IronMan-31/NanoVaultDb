    #pragma once
    #include <cstddef>
    #include <cstring>

    #define likely(x)   __builtin_expect(!!(x), 1)
    #define unlikely(x) __builtin_expect(!!(x), 0)

    template<typename T, std::size_t PoolSize = 1'048'576>
    class ObjectPool {
        alignas(64) T   slab_[PoolSize];
        alignas(64) T*  free_list_[PoolSize];
        std::size_t     free_head_{0};

    public:
        ObjectPool() {
            std::memset(slab_, 0, sizeof(slab_));

            for (std::size_t i = 0; i < PoolSize; ++i)
                free_list_[i] = &slab_[i];

            free_head_ = PoolSize;
        }

        ObjectPool(const ObjectPool&) = delete;
        ObjectPool& operator=(const ObjectPool&) = delete;

        [[nodiscard]]
        __attribute__((always_inline))
        T* acquire() noexcept {

            if (unlikely(free_head_ == 0))
                return nullptr;

            return free_list_[--free_head_];
        }

        __attribute__((always_inline))
        void release(T* obj) noexcept {

            obj->~T();

            if (unlikely(free_head_ == PoolSize))
                return;

            free_list_[free_head_++] = obj;
        }

        [[nodiscard]]
        std::size_t available() const noexcept {
            return free_head_;
        }

        [[nodiscard]]
        std::size_t capacity() const noexcept {
            return PoolSize;
        }
    };