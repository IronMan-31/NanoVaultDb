#pragma once
#include <cstdint>
#include <atomic>
#include <optional>
#include <string_view>

#define FORCE_INLINE  __attribute__((always_inline)) inline
#define HOT           __attribute__((hot))
#define COLD          __attribute__((cold))
#define LIKELY(x)     __builtin_expect(!!(x), 1)
#define UNLIKELY(x)   __builtin_expect(!!(x), 0)
#define CACHELINE     64

using Price    = int64_t;
using Quantity = uint64_t;
using OrderId  = uint64_t;
using Nanos    = uint64_t;   

constexpr Price TICK_SIZE   = 1;
constexpr Price PRICE_SCALE = 10'000;   // $0.0001 resolution

constexpr Price  double_to_price(double p) noexcept {
    return static_cast<Price>(p * PRICE_SCALE + 0.5);
}
constexpr double price_to_double(Price p) noexcept {
    return static_cast<double>(p) / PRICE_SCALE;
}

enum class Side : uint8_t { BID = 0, ASK = 1 };

enum class OrderType : uint8_t {
    LIMIT  = 0,
    MARKET = 1,
    IOC    = 2,   // Immediate-or-Cancel
    FOK    = 3,   // Fill-or-Kill
    STOP   = 4,
};

enum class OrderStatus : uint8_t {
    OPEN      = 0,
    PARTIAL   = 1,
    FILLED    = 2,
    CANCELLED = 3,
    REJECTED  = 4,
};

enum class EventType : uint8_t {
    NEW_ORDER  = 0,
    CANCEL     = 1,
    MODIFY     = 2,
    TRADE      = 3,
    SNAPSHOT   = 4,
    HEARTBEAT  = 5,
};

struct MarketEvent {
    EventType  type;
    Side       side;
    OrderType  order_type;
    uint8_t    _pad0[5];
    Price      price;
    Quantity   qty;
    OrderId    order_id;
    Nanos      timestamp;
    uint8_t    _pad1[6];
};
static_assert(sizeof(MarketEvent) == 48, "MarketEvent must be 48 bytes");
