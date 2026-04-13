#include <iostream>
#include <cmath>
#include <cstring>
#include "benchmark.hpp"   // includes timing.hpp transitively

// ─────────────────────────────────────────────────────────────────────────────
//  Example 1 — measure a trivial memory read
// ─────────────────────────────────────────────────────────────────────────────

static char g_buf[64] = {};

void bench_cache_hit() {
    volatile char x = g_buf[0];
    (void)x;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Example 2 — measure std::sqrt
// ─────────────────────────────────────────────────────────────────────────────

void bench_sqrt() {
    volatile double r = std::sqrt(3.14159265358979);
    (void)r;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Example 3 — memcpy 64 bytes
// ─────────────────────────────────────────────────────────────────────────────

static char src[64], dst[64];

void bench_memcpy64() {
    std::memcpy(dst, src, 64);
}

// ─────────────────────────────────────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────────────────────────────────────

int main() {
    // Print calibrated CPU speed once
    std::cout << "CPU speed (calibrated): "
              << CPU_GHZ << " GHz\n\n";

    // ── standard benchmark ──────────────────────────────────────────────────
    {
        auto r = benchmark(bench_cache_hit, 100'000, 2'000);
        r.stats.print("Cache hit (L1 read)");
    }

    {
        auto r = benchmark(bench_sqrt, 100'000, 2'000);
        r.stats.print("std::sqrt(double)");
    }

    {
        auto r = benchmark(bench_memcpy64, 100'000, 2'000);
        r.stats.print("memcpy 64 bytes");
    }

    // ── parametric percentile query ─────────────────────────────────────────
    std::cout << "\n--- Parametric percentile demo (memcpy 64 B) ---\n";
    auto r = benchmark(bench_memcpy64, 100'000, 2'000);

    for (double pct : {1.0, 10.0, 25.0, 50.0, 75.0, 90.0, 95.0, 99.0, 99.9}) {
        std::cout << "  p" << pct << " = "
                  << r.percentile(pct) << " ns\n";
    }

    return 0;
}