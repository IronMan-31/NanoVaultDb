#pragma once
/*
 * order_book.hpp  –  Price-time priority order book with integrated matching engine.
 *
 * Architecture:
 *   - std::map<Price, PriceLevel>  for the bid/ask ladders
 *       bids: std::greater<Price>  → begin() == best bid
 *       asks: std::less<Price>     → begin() == best ask
 *   - std::unordered_map<OrderId, Order*>  for O(1) cancel/modify
 *   - MemoryPool<Order, MAX_ORDERS>  – no heap after construction
 *   - Intrusive doubly-linked list (prev/next OrderId) inside each PriceLevel
 */

#include "types.hpp"
#include "memory_pool.hpp"
#include "price_level.hpp"
#include <map>
#include <unordered_map>
#include <functional>
#include <vector>

namespace Book {

static constexpr size_t MAX_ORDERS = 1u << 20;   // 1 048 576 orders pre-allocated

// ---------------------------------------------------------------------------
// Result types returned by the public API
// ---------------------------------------------------------------------------
struct AddResult {
    OrderId            id       {0};
    bool               accepted {false};
    std::vector<Trade> fills    {};
};

struct CancelResult {
    bool found     {false};
    bool cancelled {false};
};

struct ModifyResult {
    bool               success {false};
    OrderId            new_id  {0};
    std::vector<Trade> fills   {};
};

// ---------------------------------------------------------------------------
// Callback interface  –  implement to receive real-time events
// ---------------------------------------------------------------------------
struct OrderBookCallbacks {
    virtual ~OrderBookCallbacks() = default;
    virtual void on_trade           (const Trade&)  {}
    virtual void on_bbo_update      (const BBO&)    {}
    virtual void on_order_added     (const Order&)  {}
    virtual void on_order_cancelled (const Order&)  {}
    virtual void on_order_filled    (const Order&)  {}
};

// ---------------------------------------------------------------------------
// OrderBook
// ---------------------------------------------------------------------------
class OrderBook {
public:
    explicit OrderBook(Symbol sym, OrderBookCallbacks* cb = nullptr)
        : symbol_(sym), callbacks_(cb) {
        // Reserve buckets to avoid rehash during trading
        order_map_.reserve(MAX_ORDERS / 4);
    }

    // -- Public API -----------------------------------------------------------
    AddResult    add_order   (Side, OrderType, Price, Quantity, Exchange = Exchange::GENERIC);
    CancelResult cancel_order(OrderId id);
    ModifyResult modify_order(OrderId id, Price new_price, Quantity new_qty);
    void         clear();

    FORCE_INLINE BBO   best_bbo()  const noexcept;
    FORCE_INLINE Price best_bid()  const noexcept;
    FORCE_INLINE Price best_ask()  const noexcept;

    struct DepthLevel { Price price; Quantity qty; uint32_t count; };
    std::vector<DepthLevel> bid_depth(int levels = 10) const;
    std::vector<DepthLevel> ask_depth(int levels = 10) const;

    FORCE_INLINE size_t        order_count() const noexcept { return order_pool_.used(); }
    FORCE_INLINE const Symbol& symbol()      const noexcept { return symbol_; }

    struct Stats {
        uint64_t orders_added    {0};
        uint64_t orders_cancelled{0};
        uint64_t orders_filled   {0};
        uint64_t trades_executed {0};
        uint64_t total_volume    {0};
    };
    FORCE_INLINE const Stats& stats() const noexcept { return stats_; }

private:
    // -- Matching internals ---------------------------------------------------
    std::vector<Trade> match_order  (Order& taker);
    void               fill_order   (Order& maker, Order& taker,
                                     Quantity fill_qty, std::vector<Trade>& out);

    // -- Price-level linked-list helpers --------------------------------------
    FORCE_INLINE void level_add_order          (PriceLevel& lvl, Order& o) noexcept;
    FORCE_INLINE void level_remove_order_links (PriceLevel& lvl, Order& o) noexcept;

    // -- Sequence generators --------------------------------------------------
    FORCE_INLINE OrderId next_order_id() noexcept { return ++order_id_seq_; }
    FORCE_INLINE TradeId next_trade_id() noexcept { return ++trade_id_seq_; }

    // -- Notification ---------------------------------------------------------
    FORCE_INLINE void notify_bbo() noexcept;

    // -- Data -----------------------------------------------------------------
    Symbol              symbol_;
    OrderBookCallbacks* callbacks_ {nullptr};

    using BidLadder = std::map<Price, PriceLevel, std::greater<Price>>;
    using AskLadder = std::map<Price, PriceLevel, std::less<Price>>;
    BidLadder bid_ladder_;
    AskLadder ask_ladder_;

    std::unordered_map<OrderId, Order*> order_map_;
    MemoryPool<Order, MAX_ORDERS>       order_pool_;

    OrderId order_id_seq_ {0};
    TradeId trade_id_seq_ {0};
    Stats   stats_        {};
};

// ---------------------------------------------------------------------------
// Inline definitions (must be in the header so the compiler can inline them)
// ---------------------------------------------------------------------------

FORCE_INLINE Price OrderBook::best_bid() const noexcept {
    if (UNLIKELY(bid_ladder_.empty())) return 0;
    return bid_ladder_.begin()->first;
}

FORCE_INLINE Price OrderBook::best_ask() const noexcept {
    if (UNLIKELY(ask_ladder_.empty())) return 0;
    return ask_ladder_.begin()->first;
}

FORCE_INLINE BBO OrderBook::best_bbo() const noexcept {
    BBO bbo{};
    bbo.timestamp = now_ns();
    if (LIKELY(!bid_ladder_.empty())) {
        const PriceLevel& lvl = bid_ladder_.begin()->second;
        bbo.bid_price = lvl.price;
        bbo.bid_qty   = lvl.total_qty;
    }
    if (LIKELY(!ask_ladder_.empty())) {
        const PriceLevel& lvl = ask_ladder_.begin()->second;
        bbo.ask_price = lvl.price;
        bbo.ask_qty   = lvl.total_qty;
    }
    return bbo;
}

FORCE_INLINE void OrderBook::notify_bbo() noexcept {
    if (LIKELY(callbacks_ != nullptr))
        callbacks_->on_bbo_update(best_bbo());
}

} // namespace Book
