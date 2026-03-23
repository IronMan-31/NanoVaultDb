#pragma once
#include <cstdint>
#include <chrono>
#include <x86intrin.h>
#include "types.hpp"

FORCE_INLINE uint64_t rdtsc_start() noexcept {
    uint32_t lo, hi;
    __asm__ volatile (
        "cpuid\n\t"
        "rdtsc\n\t"
        "mov %%edx, %0\n\t"
        "mov %%eax, %1\n\t"
        : "=r"(hi), "=r"(lo)
        :: "%rax", "%rbx", "%rcx", "%rdx"
    );
    return (static_cast<uint64_t>(hi) << 32) | lo;
}


FORCE_INLINE uint64_t rdtsc_end() noexcept {
    uint32_t lo, hi;
    __asm__ volatile (
        "rdtscp\n\t"
        "mov %%edx, %0\n\t"
        "mov %%eax, %1\n\t"
        "cpuid\n\t"
        : "=r"(hi), "=r"(lo)
        :: "%rax", "%rbx", "%rcx", "%rdx"
    );
    return (static_cast<uint64_t>(hi) << 32) | lo;
}



FORCE_INLINE uint64_t rdtsc() noexcept {
    return __rdtsc();
}

inline double calibrate_cpu_ghz() noexcept {
    auto t0 = std::chrono::steady_clock::now();
    uint64_t c0 = __rdtsc();
    // Busy-spin for accuracy.
    volatile uint64_t sink = 0;
    for (int i = 0; i < 10'000'000; ++i) sink += static_cast<uint64_t>(i);
    uint64_t c1 = __rdtsc();
    auto t1 = std::chrono::steady_clock::now();
    double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    return static_cast<double>(c1 - c0) / ns;   // GHz
}

inline const double CPU_GHZ = calibrate_cpu_ghz();

FORCE_INLINE double cycles_to_ns(uint64_t cycles) noexcept {
    return static_cast<double>(cycles) / CPU_GHZ;
}
