#pragma once
#include <cstdint>
#include<cstring>
#include<atomic>
#include<optional>


#define FORCE_INLINE __attribute__((always_inline)) inline
#define HOT __attribute__((hot))
#define COLD __attribute__((cold))
#define LIKELY(x) __builtin_expect(!!(x) ,1)
#define UNLIKELY(x) __builtin_expect(!!(x),0)
#define CACHELINE 64
