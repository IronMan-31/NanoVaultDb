#pragma once
/*
 * simd_utils.hpp  –  Runtime SIMD helpers.
 *
 * Compile-time feature detection via preprocessor macros set by -mavx512f etc.
 * Graceful scalar fallback when no SIMD is available.
 *
 * Exposed utilities:
 *   simd::find_max_price_idx   – locate best bid in a flat price array
 *   simd::find_min_price_idx   – locate best ask
 *   simd::fast_zero            – cache-bypassing memset (non-temporal stores)
 */

#include "types.hpp"
#include <cstring>

// ---------------------------------------------------------------------------
// Feature detection  (set by CMake via -mavx512f / -mavx2 / -msse4.2)
// ---------------------------------------------------------------------------
#if defined(__AVX512F__)
#  include <immintrin.h>
#  define HFT_AVX512 1
#elif defined(__AVX2__)
#  include <immintrin.h>
#  define HFT_AVX2 1
#elif defined(__AVX__)
#  include <immintrin.h>
#  define HFT_AVX 1
#elif defined(__SSE4_2__)
#  include <nmmintrin.h>
#  define HFT_SSE42 1
#endif

namespace Book::simd {

// ---------------------------------------------------------------------------
// find_max_price_idx  –  Returns index of the maximum int64_t in prices[0..n-1]
//   Used to locate the best bid (highest price) in a flat array scan.
// ---------------------------------------------------------------------------
FORCE_INLINE int find_max_price_idx(const int64_t* prices, int n) noexcept {
    if (UNLIKELY(n <= 0)) return -1;

#if defined(HFT_AVX512)
    // Process 8 x int64 per cycle with AVX-512
    if (n >= 8) {
        __m512i vmax     = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(prices));
        __m512i vidx_max = _mm512_set_epi64(7,6,5,4,3,2,1,0);
        __m512i vcur     = vidx_max;
        const __m512i vstep = _mm512_set1_epi64(8);

        for (int i = 8; i <= n - 8; i += 8) {
            __m512i v = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(prices + i));
            vcur = _mm512_add_epi64(vcur, vstep);
            __mmask8 mask = _mm512_cmpgt_epi64_mask(v, vmax);
            vmax     = _mm512_mask_blend_epi64(mask, vmax, v);
            vidx_max = _mm512_mask_blend_epi64(mask, vidx_max, vcur);
        }
        int64_t vals[8], idxs[8];
        _mm512_storeu_si512(reinterpret_cast<__m512i*>(vals), vmax);
        _mm512_storeu_si512(reinterpret_cast<__m512i*>(idxs), vidx_max);
        int best = 0;
        for (int k = 1; k < 8; ++k)
            if (vals[k] > vals[best]) best = k;
        int result = static_cast<int>(idxs[best]);
        // scalar tail
        int64_t cur_max = vals[best];
        for (int i = (n / 8) * 8; i < n; ++i)
            if (prices[i] > cur_max) { cur_max = prices[i]; result = i; }
        return result;
    }
#endif

    // Scalar fallback (also used for n < 8 even with AVX-512)
    int best = 0;
    for (int i = 1; i < n; ++i)
        if (prices[i] > prices[best]) best = i;
    return best;
}

// ---------------------------------------------------------------------------
// find_min_price_idx  –  Returns index of the minimum int64_t (best ask)
// ---------------------------------------------------------------------------
FORCE_INLINE int find_min_price_idx(const int64_t* prices, int n) noexcept {
    if (UNLIKELY(n <= 0)) return -1;
    int best = 0;
    for (int i = 1; i < n; ++i)
        if (prices[i] < prices[best]) best = i;
    return best;
}

// ---------------------------------------------------------------------------
// fast_zero  –  Zero `bytes` of memory at `dst`.
//   Uses AVX-512 non-temporal stores when available (bypasses cache —
//   ideal for resetting large pre-allocated slabs at startup).
// ---------------------------------------------------------------------------
FORCE_INLINE void fast_zero(void* dst, size_t bytes) noexcept {
#if defined(HFT_AVX512)
    auto* p = reinterpret_cast<__m512i*>(dst);
    __m512i zero = _mm512_setzero_si512();
    size_t n512  = bytes / 64;
    for (size_t i = 0; i < n512; ++i)
        _mm512_stream_si512(p + i, zero);
    _mm_sfence();
    size_t off = n512 * 64;
    std::memset(reinterpret_cast<char*>(dst) + off, 0, bytes - off);
#else
    std::memset(dst, 0, bytes);
#endif
}

} // namespace Book::simd
