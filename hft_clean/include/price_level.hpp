#pragma once
/*
 * price_level.hpp  –  One rung on the price ladder.
 *
 * Orders at the same price are kept in a doubly-linked FIFO queue stored as
 * OrderId handles (indices into the MemoryPool), not raw pointers.  This
 * keeps the struct relocatable and avoids pointer invalidation.
 *
 * Exactly one cache line (64 bytes).
 */

#include "types.hpp"

namespace Book {

struct alignas(64) PriceLevel {
    Price    price      {0};
    Quantity total_qty  {0};   // sum of all resting Order::remaining at this price
    uint32_t order_count{0};
    OrderId  head       {0};   // oldest order  (filled first — FIFO)
    OrderId  tail       {0};   // newest order
    uint8_t  _pad[12]  {};

    FORCE_INLINE bool empty() const noexcept { return order_count == 0; }
};
static_assert(sizeof(PriceLevel) == 64, "PriceLevel must be exactly one cache line");

} // namespace Book
