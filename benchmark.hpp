#pragma once
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <functional>
#include <iostream>
#include "utils/rdtsc.hpp"   // your rdtsc_start / rdtsc_end / cycles_to_ns header

// ─────────────────────────────────────────────────────────────────────────────
//  Stats
// ─────────────────────────────────────────────────────────────────────────────

struct BenchStats {
    double min_ns   = 0;
    double max_ns   = 0;
    double mean_ns  = 0;
    double p50_ns   = 0;
    double p90_ns   = 0;
    double p99_ns   = 0;
    double p999_ns  = 0;
    double stddev_ns= 0;
    uint64_t samples= 0;

    // Returns the value at an arbitrary percentile in [0,100].
    double percentile_ns(double pct) const;

    std::string to_string(const std::string& label = "") const;
    void print(const std::string& label = "") const;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Percentile helper — works on a *sorted* copy of raw nanosecond samples.
//  We keep the sorted vector inside BenchResult for reuse.
// ─────────────────────────────────────────────────────────────────────────────

struct BenchResult {
    BenchStats           stats;
    std::vector<double>  sorted_ns;   // sorted nanosecond samples

    // Query any percentile without re-sorting.
    double percentile(double pct) const {
        if (sorted_ns.empty()) return 0.0;
        pct = std::clamp(pct, 0.0, 100.0);
        double idx = (pct / 100.0) * static_cast<double>(sorted_ns.size() - 1);
        size_t lo  = static_cast<size_t>(idx);
        size_t hi  = std::min(lo + 1, sorted_ns.size() - 1);
        double frac= idx - static_cast<double>(lo);
        return sorted_ns[lo] * (1.0 - frac) + sorted_ns[hi] * frac;
    }
};

inline BenchResult benchmark(
    const std::function<void()>& fn,
    uint64_t iterations = 10'000,
    uint64_t warmup     = 1'000)
{
    // Warmup
    for (uint64_t i = 0; i < warmup; ++i) fn();

    std::vector<double> samples;
    samples.reserve(iterations);

    for (uint64_t i = 0; i < iterations; ++i) {
        uint64_t t0 = rdtsc_start();
        fn();
        uint64_t t1 = rdtsc_end();
        samples.push_back(cycles_to_ns(t1 - t0));
    }

    // Sort for percentile calculations
    std::vector<double> sorted = samples;
    std::sort(sorted.begin(), sorted.end());

    const size_t n = sorted.size();

    auto pct = [&](double p) -> double {
        if (n == 0) return 0.0;
        double idx = (p / 100.0) * static_cast<double>(n - 1);
        size_t lo  = static_cast<size_t>(idx);
        size_t hi  = std::min(lo + 1, n - 1);
        double f   = idx - static_cast<double>(lo);
        return sorted[lo] * (1.0 - f) + sorted[hi] * f;
    };

    // Mean
    double sum = 0.0;
    for (double v : samples) sum += v;
    double mean = sum / static_cast<double>(n);

    // Stddev
    double var = 0.0;
    for (double v : samples) var += (v - mean) * (v - mean);
    var /= static_cast<double>(n);

    BenchStats s;
    s.samples   = n;
    s.min_ns    = sorted.front();
    s.max_ns    = sorted.back();
    s.mean_ns   = mean;
    s.p50_ns    = pct(50.0);
    s.p90_ns    = pct(90.0);
    s.p99_ns    = pct(99.0);
    s.p999_ns   = pct(99.9);
    s.stddev_ns = std::sqrt(var);

    return BenchResult{ s, std::move(sorted) };
}


inline std::string BenchStats::to_string(const std::string& label) const {
    std::ostringstream oss;
    auto w = [](double v) {
        std::ostringstream o;
        o << std::fixed << std::setprecision(2) << v;
        return o.str();
    };

    if (!label.empty())
        oss << "=== " << label << " ===\n";

    oss << "  samples : " << samples          << "\n"
        << "  min     : " << w(min_ns)   << " ns\n"
        << "  mean    : " << w(mean_ns)  << " ns\n"
        << "  stddev  : " << w(stddev_ns)<< " ns\n"
        << "  p50     : " << w(p50_ns)   << " ns\n"
        << "  p90     : " << w(p90_ns)   << " ns\n"
        << "  p99     : " << w(p99_ns)   << " ns\n"
        << "  p99.9   : " << w(p999_ns)  << " ns\n"
        << "  max     : " << w(max_ns)   << " ns\n";
    return oss.str();
}

inline void BenchStats::print(const std::string& label) const {
    std::cout << to_string(label);
}