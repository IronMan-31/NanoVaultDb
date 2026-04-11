#include "market_data_handler.hpp"

namespace Book {

OrderBook *MarketDataHandler::get_or_create_book(Exchange exch,
                                                 const Symbol &sym) {
  ExchangeSymbolKey key{exch, sym};
  auto it = books_.find(key);
  if (LIKELY(it != books_.end()))
    return it->second.get();
  auto [ins, ok] = books_.emplace(key, std::make_unique<OrderBook>(sym, this));
  (void)ok;
  return ins->second.get();
}

void MarketDataHandler::register_adapter(
    std::unique_ptr<ExchangeAdapter> adapter, const Symbol &sym) {
  Exchange exch = adapter->exchange_id();
  get_or_create_book(exch, sym);
  adapter->set_callback([this](const MarketDataMsg &msg) { apply_delta(msg); });
  adapters_[ExchangeSymbolKey{exch, sym}] = std::move(adapter);
}

void MarketDataHandler::feed(Exchange exch, const Symbol &sym, const char *data,
                             size_t len) {
  auto it = adapters_.find(ExchangeSymbolKey{exch, sym});
  if (UNLIKELY(it == adapters_.end()))
    return;
  it->second->on_message(data, len);
}

void MarketDataHandler::apply_delta(const MarketDataMsg &msg) {
  auto it = books_.find(ExchangeSymbolKey{msg.exchange, msg.symbol});
  if (UNLIKELY(it == books_.end()))
    return;
  if (msg.qty == 0)
    return; // qty==0 means delete-level; L2 mirror handles this
  it->second->add_order(msg.side, OrderType::LIMIT, msg.price, msg.qty,
                        msg.exchange);
}

// Callbacks — override in a subclass to hook into events
void MarketDataHandler::on_trade(const Trade &t) { (void)t; }
void MarketDataHandler::on_bbo_update(const BBO &b) { (void)b; }
void MarketDataHandler::on_order_added(const Order &o) { (void)o; }
void MarketDataHandler::on_order_cancelled(const Order &o) { (void)o; }
void MarketDataHandler::on_order_filled(const Order &o) { (void)o; }

} // namespace Book
