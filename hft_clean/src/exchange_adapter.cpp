
#include "exchange_adapter.hpp"
#include <cstdlib>
#include <cstring>

namespace Book {

// BinanceAdapter

/*

    {
  "e":"depthUpdate",
  "E":123456789,
  "s":"BTCUSDT",
  "U":100,
  "u":105,
  "b":[["50000.0","0.5"], ["49950.0","1.2"]],
  "a":[["50100.0","0.8"], ["50200.0","0.1"]]
}

*/

namespace {

FORCE_INLINE const char *skip_to(const char *p, const char *end,
                                 char ch) noexcept {
  while (p < end && *p != ch)
    ++p;
  return p;
}

FORCE_INLINE const char *skip_ws(const char *p, const char *end) noexcept {
  while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
    ++p;
  return p;
}

} // namespace

void BinanceAdapter::on_message(const char *data, size_t len) {
  const char *end = data + len;

  // Extract final-update-id ("u" field) as sequence number
  uint64_t seq = ++seq_;
  for (const char *p = data; p < end - 4; ++p) {
    if (p[0] == '"' && p[1] == 'u' && p[2] == '"' && p[3] == ':') {
      seq = static_cast<uint64_t>(std::strtoull(p + 4, nullptr, 10));
      break;
    }
  }

  parse_side(data, end, "\"b\":[", Side::BUY, seq);
  parse_side(data, end, "\"a\":[", Side::SELL, seq);
}

void BinanceAdapter::parse_side(const char *data, const char *end,
                                const char *key, Side side, uint64_t seq) {
  size_t klen = std::strlen(key);
  for (const char *p = data; p < end - (ptrdiff_t)klen; ++p) {
    if (std::memcmp(p, key, klen) != 0)
      continue;
    p += klen;
    while (p < end && *p != '[')
      ++p;
    ++p; // skip '['

    while (p < end) {
      p = skip_ws(p, end);
      if (p >= end || *p == ']')
        break;
      if (*p != '[') {
        ++p;
        continue;
      }
      ++p; // skip '['

      // price string
      p = skip_to(p, end, '"');
      if (p >= end)
        return;
      ++p;
      const char *ps = p;
      p = skip_to(p, end, '"');
      if (p >= end)
        return;
      char pbuf[32]{};
      std::memcpy(pbuf, ps, std::min((size_t)(p - ps), (size_t)31));
      ++p;

      // qty string
      p = skip_to(p, end, '"');
      if (p >= end)
        return;
      ++p;
      const char *qs = p;
      p = skip_to(p, end, '"');
      if (p >= end)
        return;
      char qbuf[32]{};
      std::memcpy(qbuf, qs, std::min((size_t)(p - qs), (size_t)31));
      ++p;

      emit_level(pbuf, qbuf, side, seq);
      p = skip_to(p, end, ']');
      ++p; // skip to end of this entry
    }
    break;
  }
}

void BinanceAdapter::emit_level(const char *price_str, const char *qty_str,
                                Side side, uint64_t seq) {
  MarketDataMsg msg{};
  msg.type = MarketDataMsg::Type::DELTA;
  msg.side = side;
  msg.price = to_price(std::strtod(price_str, nullptr));
  msg.qty = to_qty(std::strtod(qty_str, nullptr));
  msg.exch_ts = now_ns();
  msg.seq = seq;
  msg.exchange = Exchange::BINANCE;
  msg.symbol = symbol_;
  emit(msg);
}

// ZerodhaAdapter

// Kite Connect binary depth packet layout (big-endian, Mode Full = 184 B):
//
//   Offset  Bytes  Field
//    0       4      instrument_token
//    4       4      last_price        (paise)
//    8       4      last_qty
//   12       4      avg_price         (paise)
//   16       4      volume
//   20       4      buy_qty (total)
//   24       4      sell_qty (total)
//   28       4      open              (paise)
//   32       4      high              (paise)
//   36       4      low               (paise)
//   40       4      close             (paise)
//   44     n*12     depth bids        [qty(4), price(4), orders(2), pad(2)]
//   44+n*12  n*12   depth asks
//
// Standard 5-level mode: n=5, total depth = 2 * 5 * 12 = 120 B, offset 44..163
// Full 20-level mode:    n=20, offset 44..524

namespace {
FORCE_INLINE int32_t be32(const char *p) noexcept {
  const auto *u = reinterpret_cast<const uint8_t *>(p);
  return static_cast<int32_t>((uint32_t(u[0]) << 24) | (uint32_t(u[1]) << 16) |
                              (uint32_t(u[2]) << 8) | uint32_t(u[3]));
}
} // namespace

void ZerodhaAdapter::on_message(const char *data, size_t len) {
  constexpr size_t DEPTH_OFFSET = 44;
  constexpr size_t BYTES_PER_SIDE = 12; // qty(4)+price(4)+orders(2)+pad(2)

  if (UNLIKELY(len < DEPTH_OFFSET + 2 * BYTES_PER_SIDE))
    return;

  size_t avail_bytes = len - DEPTH_OFFSET;
  size_t levels = (avail_bytes / (2 * BYTES_PER_SIDE));
  if (levels > 20)
    levels = 20;
  if (levels == 0)
    return;

  uint64_t seq = ++seq_;

  const char *bid_base = data + DEPTH_OFFSET;
  const char *ask_base =
      bid_base + static_cast<ptrdiff_t>(levels * BYTES_PER_SIDE);

  auto emit_lvl = [&](const char *base, int idx, Side side) {
    const char *p = base + idx * BYTES_PER_SIDE;
    int32_t qty = be32(p);
    int32_t paise = be32(p + 4);
    if (qty == 0 && paise == 0)
      return;

    MarketDataMsg msg{};
    msg.type = MarketDataMsg::Type::DELTA;
    msg.side = side;
    msg.price = paise_to_price(paise);
    msg.qty = to_qty(static_cast<double>(qty));
    msg.exch_ts = now_ns();
    msg.seq = seq;
    msg.exchange = Exchange::ZERODHA;
    msg.symbol = symbol_;
    emit(msg);
  };

  for (int i = 0; i < static_cast<int>(levels); ++i)
    emit_lvl(bid_base, i, Side::BUY);
  if (ask_base + levels * BYTES_PER_SIDE <= data + len)
    for (int i = 0; i < static_cast<int>(levels); ++i)
      emit_lvl(ask_base, i, Side::SELL);
}

} // namespace Book
