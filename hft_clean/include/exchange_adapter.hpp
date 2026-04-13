#pragma once
/*
 * exchange_adapter.hpp  –  Pluggable exchange wire-format adapters.
 *
 * Each adapter translates exchange-specific bytes into normalised
 * MarketDataMsg values, then calls emit() which routes to the caller's
 * callback.  Add a new exchange by subclassing ExchangeAdapter.
 *
 * Provided adapters:
 *   BinanceAdapter  – WebSocket JSON depthUpdate frames
 *   ZerodhaAdapter  – Kite Connect binary depth packets (big-endian)
 */

#include "types.hpp"
#include <functional>
#include <memory>

namespace Book {

// ---------------------------------------------------------------------------
// Normalised market-data message (exchange-agnostic)
// ---------------------------------------------------------------------------
struct MarketDataMsg {
  enum class Type : uint8_t { SNAPSHOT = 0, DELTA = 1, TRADE = 2 };

  Type type{Type::DELTA};
  Side side{Side::BUY};
  Price price{0};
  Quantity qty{0}; // qty == 0  means "delete this price level"
  Nanos exch_ts{0};
  uint64_t seq{0};
  Exchange exchange{Exchange::GENERIC};
  Symbol symbol{};
};

// ---------------------------------------------------------------------------
// Base adapter
// ---------------------------------------------------------------------------
class ExchangeAdapter {
public:
  virtual ~ExchangeAdapter() = default;

  // Feed raw bytes from the network layer
  virtual void on_message(const char *data, size_t len) = 0;

  // Register the callback that receives normalised messages
  using MsgCallback = std::function<void(const MarketDataMsg &)>;
  void set_callback(MsgCallback cb) { cb_ = std::move(cb); }

  FORCE_INLINE Exchange exchange_id() const noexcept { return exchange_id_; }

protected:
  FORCE_INLINE void emit(MarketDataMsg &msg) {
    if (LIKELY(cb_))
      cb_(msg);
  }

  Exchange exchange_id_{Exchange::GENERIC};
  MsgCallback cb_;
};

// ---------------------------------------------------------------------------
// Binance adapter
// ---------------------------------------------------------------------------
class BinanceAdapter : public ExchangeAdapter {
public:
  BinanceAdapter() { exchange_id_ = Exchange::BINANCE; }
  void set_symbol(Symbol sym) noexcept { symbol_ = sym; }
  void on_message(const char *data, size_t len) override;

private:
  void parse_side(const char *data, const char *end, const char *key, Side side,
                  uint64_t seq);
  void emit_level(const char *price_str, const char *qty_str, Side side,
                  uint64_t seq);
  Symbol symbol_;
  uint64_t seq_{0};
};

// ---------------------------------------------------------------------------
// Zerodha Kite Connect adapter
// ---------------------------------------------------------------------------
class ZerodhaAdapter : public ExchangeAdapter {
public:
  ZerodhaAdapter() { exchange_id_ = Exchange::ZERODHA; }
  void set_symbol(Symbol sym) noexcept { symbol_ = sym; }
  void on_message(const char *data, size_t len) override;

private:
  // Zerodha prices are in paise (1 INR = 100 paise).
  // Our PRICE_SCALE = 1e8, so: fixed = paise * 1_000_000
  FORCE_INLINE Price paise_to_price(int32_t paise) noexcept {
    return static_cast<Price>(paise) * 1'000'000LL;
  }
  Symbol symbol_;
  uint64_t seq_{0};
};

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------
inline std::unique_ptr<ExchangeAdapter> make_adapter(Exchange e) {
  switch (e) {
  case Exchange::BINANCE:
    return std::make_unique<BinanceAdapter>();
  case Exchange::ZERODHA:
    return std::make_unique<ZerodhaAdapter>();
  default:
    return nullptr;
  }
}

} // namespace Book
