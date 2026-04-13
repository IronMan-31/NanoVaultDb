#pragma once
#include "order.hpp"


struct alignas(64) PriceLevel {
    // ══ Cache line 1 (hot) ═══════════════════════════════════════════════════
    std::atomic<uint64_t> total_qty{0};     //  8  sum of all resting quantities
    std::atomic<uint32_t> order_count{0};   //  4  number of resting orders
    uint32_t              price_raw{0};     //  4  index into levels array
    uint8_t               flags{0};         //  1  active / dirty bits
    uint8_t               _pad1[3];         //  3
    Order*                head{nullptr};    //  8  FIFO queue head
    Order*                tail{nullptr};    //  8  FIFO queue tail
    uint64_t              seq_num{0};       //  
    uint8_t               _pad2[20];        // 20  → 64 bytes
    //                                          ────
    //                                          64

    uint64_t total_executed{0};
    uint64_t num_trades{0};
    uint64_t last_update_ns{0};
    uint64_t add_count{0};
    uint64_t cancel_count{0};
    uint64_t modify_count{0};
    uint8_t  _pad3[16];
    //                                          ────
    //                                          64 → total 128
};
static_assert(alignof(PriceLevel) == 64, "PriceLevel must be cache-line aligned");
