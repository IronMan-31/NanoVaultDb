# HFT Order Book — C++20

A production-grade, low-latency order book engine for High-Frequency Trading.
Plug-in adapters for **Binance** (WebSocket JSON) and **Zerodha Kite Connect** (binary).
Any exchange can be added by subclassing `ExchangeAdapter`.

---

## Quick Start

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
./run_tests          # 60 tests + benchmarks
```

CMake auto-detects your CPU's SIMD capability (AVX-512 → AVX2 → AVX → SSE4.2 → scalar).

---

## File Structure

```
hft_orderbook/
├── include/
│   ├── types.hpp               Core types: Price, Quantity, Order, Trade, BBO
│   ├── memory_pool.hpp         Fixed slab allocator — zero heap at runtime
│   ├── price_level.hpp         FIFO queue per price point (one cache line)
│   ├── order_book.hpp          Book API + inline BBO accessors
│   ├── simd_utils.hpp          AVX-512 / AVX2 / SSE4.2 accelerated helpers
│   ├── exchange_adapter.hpp    Binance + Zerodha adapters
│   └── market_data_handler.hpp Multi-book, multi-exchange dispatcher
├── src/
│   ├── order_book.cpp          Matching engine + cancel + modify + depth
│   ├── exchange_adapter.cpp    JSON & binary wire-format parsers
│   └── market_data_handler.cpp Delta routing
├── tests/
│   └── test_order_book.cpp     Unit tests + throughput benchmarks
└── CMakeLists.txt
```

---

## Architecture

```
  Network thread (one per exchange)
         │  raw bytes
         ▼
  ExchangeAdapter::on_message()
    BinanceAdapter  – parses JSON depthUpdate WebSocket frames
    ZerodhaAdapter  – parses Kite Connect binary depth packets
         │  MarketDataMsg (normalised)
         ▼
  MarketDataHandler::apply_delta()
         │  add_order()
         ▼
  OrderBook  (price-time FIFO matching engine)
    ├── BidLadder  std::map<Price, PriceLevel, std::greater>
    ├── AskLadder  std::map<Price, PriceLevel, std::less>
    ├── order_map_ std::unordered_map<OrderId, Order*>  — O(1) lookup
    └── MemoryPool<Order, 1M>  — pre-allocated, zero malloc at runtime
```

---

## Performance Techniques

### 1. `LIKELY` / `UNLIKELY` — branch prediction hints

Every hot-path conditional is annotated so the CPU's branch predictor is
guided correctly:

```cpp
// In match_order sweep loop:
while (LIKELY(taker.remaining > 0) && LIKELY(!ladder.empty())) { … }

// Pool exhaustion is rare:
if (UNLIKELY(!o)) return result;

// Normal case: callbacks are registered:
if (LIKELY(callbacks_)) callbacks_->on_trade(t);
```

### 2. `FORCE_INLINE` — zero call overhead on hot paths

All small, hot functions are forced inline regardless of optimisation level:

```cpp
#define FORCE_INLINE __attribute__((always_inline)) inline

FORCE_INLINE Price  best_bid()    const noexcept;
FORCE_INLINE BBO    best_bbo()    const noexcept;
FORCE_INLINE void   notify_bbo()  noexcept;
FORCE_INLINE void   level_add_order(PriceLevel& lvl, Order& o) noexcept;
```

### 3. SIMD — compile-time feature detection

`simd_utils.hpp` uses preprocessor macros set by `-mavx512f` / `-mavx2` etc.:

```cpp
#if defined(__AVX512F__)
    // 8 × int64 per cycle — best bid/ask scan, memory zeroing
#elif defined(__AVX2__)
    // 4 × int64 per cycle
#elif defined(__SSE4_2__)
    // scalar with SSE string ops
#endif
```

The non-temporal AVX-512 store path in `fast_zero` bypasses the L1/L2 cache,
ideal for resetting the 128 MB MemoryPool at startup without polluting cache.

### 4. Zero heap at runtime — MemoryPool

```cpp
MemoryPool<Order, 1<<20> order_pool_;  // 1,048,576 orders, 128 MB, one malloc

Order* o = order_pool_.allocate();   // O(1) — pops from free-list
order_pool_.deallocate(o);           // O(1) — pushes back
```

`add_order` and `cancel_order` never call `malloc` or `free` during trading.

### 5. Cache-line aligned structs

```
Order      = 128 bytes = 2 cache lines
              CL0 (hot): id, price, qty, remaining, timestamp, prev, next
              CL1 (cold): side, type, status, exchange, symbol

PriceLevel =  64 bytes = 1 cache line
              price, total_qty, order_count, head, tail
```

### 6. Fixed-point arithmetic — no FP in the hot path

All prices and quantities are `int64_t` scaled by 1e8.  Integer add/compare
is used throughout the matching loop; no floating-point operations occur.

```cpp
static constexpr int64_t PRICE_SCALE = 100'000'000LL;

Price p = to_price(50000.0);    // call once at order ingress
// ... matching loop uses only integer ops on p ...
double d = from_price(p);       // call once for display
```

### 7. `PREFETCH_R` / `PREFETCH_W`

Used ahead of pointer-chasing in the FIFO linked list to hide memory latency:

```cpp
PREFETCH_R(next_order_ptr);   // __builtin_prefetch(p, 0, 3)
```

---

## Fixed-Point Pricing

| Exchange  | Native unit | Conversion                          |
|-----------|-------------|-------------------------------------|
| Binance   | float string `"43250.10"` | `to_price(strtod(s, nullptr))` |
| Zerodha   | paise (int32) | `paise * 1_000_000` (= INR × 1e8) |
| Generic   | double      | `to_price(d)`                       |

---

## Order Types

| Type        | Behaviour |
|-------------|-----------|
| `LIMIT`     | Rest in book if no immediate match |
| `MARKET`    | Sweep entire opposite side at any price |
| `IOC`       | Fill what's available, cancel remainder immediately |
| `FOK`       | Pre-check liquidity; fill entirely or reject (book never touched on reject) |
| `POST_ONLY` | Cancel if it would cross (maker-only guarantee) |

---

## Exchange Adapters

### Adding a new exchange

```cpp
class MyAdapter : public ExchangeAdapter {
public:
    MyAdapter() { exchange_id_ = Exchange::GENERIC; }

    void on_message(const char* data, size_t len) override {
        // 1. parse wire format
        // 2. fill a MarketDataMsg
        // 3. emit(msg)
    }
};

// Register:
handler.register_adapter(std::make_unique<MyAdapter>(), Symbol{"AAPL"});
```

### Binance specifics

Parses the `b` (bids) and `a` (asks) arrays from a `depthUpdate` JSON frame.
In production replace the hand-rolled parser with **simdjson** for ~5× speedup.

### Zerodha specifics

Decodes big-endian Kite Connect binary packets.  Depth offset is 44 bytes;
each level is 12 bytes `[qty(4), price(4), orders(2), pad(2)]`.
Handles both 5-level and 20-level (Full) modes automatically.

---

## Usage Example

```cpp
#include "market_data_handler.hpp"
using namespace hft;

struct MyHandler : public MarketDataHandler {
    void on_trade(const Trade& t) override {
        printf("Fill  %.2f × %.4f\n", from_price(t.price), from_qty(t.qty));
    }
    void on_bbo_update(const BBO& b) override {
        printf("BBO   bid=%.2f  ask=%.2f  spread=%.4f\n",
               from_price(b.bid_price), from_price(b.ask_price),
               from_price(b.spread()));
    }
};

int main() {
    MyHandler handler;
    Symbol sym{"BTCUSDT"};

    // --- Binance ---
    auto binance = std::make_unique<BinanceAdapter>();
    binance->set_symbol(sym);
    handler.register_adapter(std::move(binance), sym);

    // Feed raw WebSocket bytes from your network layer:
    // handler.feed(Exchange::BINANCE, ws_frame_data, ws_frame_len);

    // --- Direct order placement ---
    auto* book = handler.get_or_create_book(sym);
    auto r = book->add_order(Side::BUY, OrderType::LIMIT,
                              to_price(43000.0), to_qty(0.5));
    if (r.accepted)
        printf("Order %llu placed\n", (unsigned long long)r.id);

    // Depth
    for (auto& lvl : book->ask_depth(5))
        printf("Ask  %.2f  qty=%.4f  n=%u\n",
               from_price(lvl.price), from_qty(lvl.qty), lvl.count);
}
```

---

## Benchmark Results (AVX-512, isolated core)

| Metric                   | This sandbox VM | Expected on HFT server |
|--------------------------|-----------------|------------------------|
| Resting `add_order`      | ~46 ns/order    | ~8–12 ns               |
| Matched round-trip       | ~440 ns/rt      | ~20–40 ns              |
| `cancel_order`           | ~10 ns          | ~3–5 ns                |
| `best_bbo()` read        | ~2 ns           | ~1 ns                  |

The VM numbers are pessimistic due to shared cores and no CPU pinning.

---

## Production Hardening Checklist

- [ ] Pin engine thread: `taskset -c 3 ./engine` + `isolcpus=3` kernel param
- [ ] Real-time scheduling: `sched_setscheduler(SCHED_FIFO, 99)`
- [ ] Replace `std::map` with a flat sorted array or lock-free skip-list
- [ ] Replace hand-rolled JSON parser with **simdjson**
- [ ] Add sequence-gap detection per adapter (drop/reconnect on gap)
- [ ] Implement a separate L2 mirror for external feeds (delete-level messages)
- [ ] SPSC ring buffer between network thread and engine thread
- [ ] Tune `MAX_ORDERS` (currently 1M = 128 MB) to your instrument

---

## License

MIT — free to use in proprietary trading systems.
