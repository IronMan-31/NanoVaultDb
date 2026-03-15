#pragma once
#include <atomic>
#include <bits/stdc++.h>
#include <cstdint>
#include <emmintrin.h>
#include <functional>
#include <optional>
#include <pthread.h>
#include <sched.h>
#include <thread>
using namespace std;

inline uint32_t fast_rand() {
  static thread_local uint32_t x = 123456789;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return x;
}

template <typename T> class WorkStealingDeque {
  struct CircularArray {
    int64_t capacity;
    T *buffer;

    explicit CircularArray(int64_t cap) : capacity(cap), buffer(new T[cap]) {}

    ~CircularArray() { delete[] buffer; }

    T get(int64_t i) const noexcept { return buffer[i & (capacity - 1)]; }

    void put(int64_t i, T val) noexcept { buffer[i & (capacity - 1)] = val; }

    CircularArray *grow(int64_t b, int64_t t) {
      auto *na = new CircularArray(capacity * 2);
      for (int64_t i = t; i < b; ++i) {
        na->put(i, get(i));
      }
      return na;
    }
  };

  alignas(64) std::atomic<int64_t> top_{1};
  alignas(64) std::atomic<int64_t> bottom_{1};
  alignas(64) std::atomic<CircularArray *> array_;

public:
  explicit WorkStealingDeque(int64_t init_cap = 1024)
      : array_(new CircularArray(init_cap)) {}

  // owner
  void push(T x) noexcept {
    int64_t b = bottom_.load(std::memory_order_relaxed);
    int64_t t = top_.load(std::memory_order_acquire);

    CircularArray *a = array_.load(std::memory_order_relaxed);

    if (b - t > a->capacity - 1) [[unlikely]] {
      a = a->grow(b, t);
      array_.store(a, std::memory_order_release);
    }

    a->put(b, x);
    bottom_.store(b + 1, std::memory_order_release);
  }

  // owner only
  std::optional<T> pop() noexcept {
    int64_t b = bottom_.load(std::memory_order_relaxed) - 1;
    CircularArray *a = array_.load(std::memory_order_relaxed);
    bottom_.store(b, std::memory_order_relaxed);

    int64_t t = top_.load(std::memory_order_acquire);

    if (t <= b) {
      T x = a->get(b);

      if (t == b) { 
        if (!top_.compare_exchange_strong(t, t + 1, std::memory_order_acq_rel,
                                          std::memory_order_relaxed)) {
          bottom_.store(b + 1, std::memory_order_relaxed);
          return std::nullopt;
        }

        bottom_.store(b + 1, std::memory_order_relaxed);
      }

      return x;
    }

    bottom_.store(b + 1, std::memory_order_relaxed);
    return std::nullopt;
  }

  // any thread (thief)
  std::optional<T> steal() noexcept {
    int64_t t = top_.load(std::memory_order_acquire);
    int64_t b = bottom_.load(std::memory_order_acquire);

    if (t >= b)
      return std::nullopt;

    CircularArray *a = array_.load(std::memory_order_acquire);
    T x = a->get(t);

    if (!top_.compare_exchange_strong(t, t + 1, std::memory_order_acq_rel,
                                      std::memory_order_relaxed)) {
      return std::nullopt;
    }

    return x;
  }
  [[nodiscard]] bool empty() const noexcept {
    return bottom_.load(std::memory_order_relaxed) <=
           top_.load(std::memory_order_relaxed);
  }
  [[nodiscard]] int64_t size() const noexcept {
    return bottom_.load(std::memory_order_relaxed) -
           top_.load(std::memory_order_relaxed);
  }
};
