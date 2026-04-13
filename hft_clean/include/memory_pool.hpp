#pragma once
/*
 * memory_pool.hpp  –  Fixed-capacity slab allocator.

 */

#include "types.hpp"
#include <array>
#include <cassert>

namespace Book {

template <typename T, size_t CAPACITY> class MemoryPool {
  static_assert(CAPACITY > 0 && (CAPACITY & (CAPACITY - 1)) == 0,
                "CAPACITY must be a power of 2");

private:
  alignas(CACHE_LINE) std::array<T, CAPACITY> storage_{};
  alignas(CACHE_LINE) std::array<uint32_t, CAPACITY> free_list_{};
  size_t free_top_{0};

public:
  MemoryPool() noexcept {
    for (uint32_t i = 0; i < CAPACITY; ++i)
      free_list_[i] = i;
    free_top_ = CAPACITY;
  }

  FORCE_INLINE T *allocate() noexcept {
    if (UNLIKELY(free_top_ == 0))
      return nullptr;
    uint32_t idx = free_list_[--free_top_];
    T *ptr = &storage_[idx];
    // use memory of ptr does not allocate memory
    new (ptr) T{}; // placement-new: resets the object
    return ptr;
  }

  FORCE_INLINE void deallocate(T *ptr) noexcept {
    assert(owns(ptr));
    ptr->~T();
    uint32_t idx = static_cast<uint32_t>(ptr - storage_.data());
    free_list_[free_top_++] = idx;
  }

  FORCE_INLINE bool owns(const T *ptr) const noexcept {
    return ptr >= storage_.data() && ptr < storage_.data() + CAPACITY;
  }

  FORCE_INLINE T *at(size_t idx) noexcept {
    assert(idx < CAPACITY);
    return &storage_[idx];
  }
  FORCE_INLINE const T *at(size_t idx) const noexcept {
    assert(idx < CAPACITY);
    return &storage_[idx];
  }

  FORCE_INLINE void reset() noexcept {
    for (uint32_t i = 0; i < CAPACITY; ++i)
      free_list_[i] = i;
    free_top_ = CAPACITY;
  }

  FORCE_INLINE size_t capacity() const noexcept { return CAPACITY; }
  FORCE_INLINE size_t available() const noexcept { return free_top_; }
  FORCE_INLINE size_t used() const noexcept { return CAPACITY - free_top_; }
};

} // namespace Book
