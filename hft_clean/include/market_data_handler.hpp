#pragma once
/*
 * market_data_handler.hpp  –  Multi-book, multi-exchange dispatcher.
 *
 * Ties ExchangeAdapters → OrderBooks.
 * Single-threaded by design; pin the calling thread to an isolated core.
 */

#include "exchange_adapter.hpp"
#include "order_book.hpp"
#include <memory>
#include <unordered_map>

namespace Book {

// single threaded run on a cpu core
// [Raw Exchange Data] → [Adapter] → [MarketDataHandler] → [OrderBook] →
// [Callbacks/Strategy]
class MarketDataHandler : public OrderBookCallbacks {
public:
  // Create or return an existing book for a symbol
  OrderBook *get_or_create_book(Exchange exch, const Symbol &sym);

  // Register an adapter; creates the book automatically
  void register_adapter(std::unique_ptr<ExchangeAdapter> adapter,
                        const Symbol &sym);

  // Route raw bytes from network → the correct adapter
  void feed(Exchange exch, const Symbol &sym, const char *data, size_t len);

  // Apply a normalised delta to the book
  void apply_delta(const MarketDataMsg &msg);

  FORCE_INLINE size_t book_count() const noexcept { return books_.size(); }

  // OrderBookCallbacks – override in subclass to act on events
  void on_trade(const Trade &) override;
  void on_bbo_update(const BBO &) override;
  void on_order_added(const Order &) override;
  void on_order_cancelled(const Order &) override;
  void on_order_filled(const Order &) override;

private:
  struct SymbolHash {
    size_t operator()(const Symbol &s) const noexcept {
      // FNV-1a over the fixed 24-byte buffer
      uint64_t h = 14695981039346656037ULL;
      for (size_t i = 0; i < Symbol::MAX_LEN; ++i) {
        h ^= static_cast<uint8_t>(s.data[i]);
        h *= 1099511628211ULL;
      }
      return static_cast<size_t>(h);
    }
  };
  struct ExchangeSymbolKey {
    Exchange exchange;
    Symbol symbol;
  };

  struct KeyEq {
    bool operator()(const ExchangeSymbolKey &a, const ExchangeSymbolKey &b) const noexcept {
      return a.exchange == b.exchange && a.symbol == b.symbol;
    }
  };

  struct KeyHash {
    size_t operator()(const ExchangeSymbolKey &k) const noexcept {
      // combine exchange and symbol hash
      size_t h1 = static_cast<size_t>(k.exchange);
      size_t h2 = SymbolHash{}(k.symbol);
      return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
  };

  using BookMap = std::unordered_map<ExchangeSymbolKey, std::unique_ptr<OrderBook>,
                                     KeyHash, KeyEq>;
  using AdapterMap = std::unordered_map<ExchangeSymbolKey, std::unique_ptr<ExchangeAdapter>, KeyHash, KeyEq>;

  BookMap books_;
  AdapterMap adapters_;
};

} // namespace Book
