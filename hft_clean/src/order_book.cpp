
#include "order_book.hpp"
#include <algorithm>
#include <limits>

namespace Book {

static bool
can_fill_fok(Side side, Price price, Quantity qty,
             const std::map<Price, PriceLevel, std::greater<Price>> &bids,
             const std::map<Price, PriceLevel, std::less<Price>> &asks) {
  Quantity avail = 0;
  if (side == Side::BUY) {
    for (const auto &[p, lvl] : asks) {
      if (p > price)
        break; // ask above our limit — stop
      avail += lvl.total_qty;
      if (avail >= qty)
        return true;
    }
  } else {
    for (const auto &[p, lvl] : bids) {
      if (p < price)
        break; // bid below our limit — stop
      avail += lvl.total_qty;
      if (avail >= qty)
        return true;
    }
  }
  return avail >= qty;
}

AddResult OrderBook::add_order(Side side, OrderType type, Price price,
                               Quantity qty, Exchange exch) {
  AddResult result{};

  // --- Validate ---
  if (UNLIKELY(qty <= 0))
    return result;
  if (UNLIKELY(type == OrderType::LIMIT && price <= 0))
    return result;

  // --- Allocate from pool (O(1), no malloc) ---
  Order *o = order_pool_.allocate();
  if (UNLIKELY(!o))
    return result; // pool exhausted

  o->id = next_order_id();
  o->qty = qty;
  o->remaining = qty;
  o->price = price;
  o->timestamp = now_ns();
  o->side = side;
  o->type = type;
  o->status = OrderStatus::NEW;
  o->exchange = exch;
  o->symbol = symbol_;
  o->prev = 0;
  o->next = 0;

  // Market orders get an extreme price so they sweep the entire book
  if (type == OrderType::MARKET) {
    o->price = (side == Side::BUY) ? std::numeric_limits<Price>::max()
                                   : std::numeric_limits<Price>::min();
  }

  // --- FOK pre-check (no book mutation if it fails) ---
  if (type == OrderType::FOK) {
    if (!can_fill_fok(side, o->price, o->qty, bid_ladder_, ask_ladder_)) {
      order_pool_.deallocate(o);
      return result; // result.accepted == false
    }
  }

  // --- Register for O(1) lookup ---
  order_map_.emplace(o->id, o);
  result.id = o->id;
  result.accepted = true;
  ++stats_.orders_added;

  // --- Match against opposite side ---
  result.fills = match_order(*o);

  // --- POST_ONLY: cancel if it crossed ---
  if (UNLIKELY(type == OrderType::POST_ONLY && !result.fills.empty())) {
    if (o->is_active()) {
      order_map_.erase(o->id);
      order_pool_.deallocate(o);
    }
    result.accepted = false;
    result.fills.clear();
    return result;
  }

  // --- Taker fully consumed during matching ---
  if (o->remaining == 0) {
    o->status = OrderStatus::FILLED;
    order_map_.erase(o->id);
    ++stats_.orders_filled;
    if (LIKELY(callbacks_))
      callbacks_->on_order_filled(*o);
    order_pool_.deallocate(o);
    notify_bbo();
    return result;
  }

  // --- IOC/FOK: cancel any unfilled remainder ---
  if (type == OrderType::IOC || type == OrderType::FOK) {
    order_map_.erase(o->id);
    order_pool_.deallocate(o);
    notify_bbo();
    return result;
  }

  // --- LIMIT: rest in book ---
  if (LIKELY(type == OrderType::LIMIT)) {
    if (side == Side::BUY) {
      PriceLevel &lvl = bid_ladder_[o->price];
      lvl.price = o->price;
      level_add_order(lvl, *o);
    } else {
      PriceLevel &lvl = ask_ladder_[o->price];
      lvl.price = o->price;
      level_add_order(lvl, *o);
    }
    if (LIKELY(callbacks_))
      callbacks_->on_order_added(*o);
    notify_bbo();
  }

  return result;
}

std::vector<Trade> OrderBook::match_order(Order &taker) {
  std::vector<Trade> trades;
  trades.reserve(8);

  auto sweep = [&](auto &ladder) {
    while (LIKELY(taker.remaining > 0) && LIKELY(!ladder.empty())) {
      auto it = ladder.begin();

      // Price check
      if (taker.side == Side::BUY) {
        if (UNLIKELY(it->first > taker.price))
          break; // ask > bid limit
      } else {
        if (UNLIKELY(it->first < taker.price))
          break; // bid < ask limit
      }

      // Walk the FIFO queue at this level
      OrderId cur_id = it->second.head;
      while (LIKELY(cur_id != 0) && LIKELY(taker.remaining > 0)) {
        auto mit = order_map_.find(cur_id);
        if (UNLIKELY(mit == order_map_.end()))
          break;
        Order &maker = *mit->second;
        OrderId next_id = maker.next; // snapshot before fill mutates it

        Quantity fill = std::min(taker.remaining, maker.remaining);
        fill_order(maker, taker, fill, trades);
        cur_id = next_id;
      }

      // Clean up exhausted level
      auto check = ladder.find(it->first);
      if (check != ladder.end() && check->second.empty())
        ladder.erase(check);
    }
  };

  if (taker.side == Side::BUY)
    sweep(ask_ladder_);
  else
    sweep(bid_ladder_);

  return trades;
}

void OrderBook::fill_order(Order &maker, Order &taker, Quantity fill_qty,
                           std::vector<Trade> &out) {
  Trade t{};
  t.id = next_trade_id();
  t.maker_id = maker.id;
  t.taker_id = taker.id;
  t.price = maker.price; // execution at maker price
  t.qty = fill_qty;
  t.timestamp = now_ns();
  t.aggressor = taker.side;

  maker.remaining -= fill_qty;
  taker.remaining -= fill_qty;
  stats_.total_volume += static_cast<uint64_t>(fill_qty);
  ++stats_.trades_executed;

  // Update maker's price level
  auto update_ladder = [&](auto &ladder) {
    auto it = ladder.find(maker.price);
    if (LIKELY(it != ladder.end())) {
      it->second.total_qty -= fill_qty;
      if (maker.remaining == 0) {
        level_remove_order_links(it->second, maker);
        if (it->second.empty())
          ladder.erase(it);
      }
    }
  };

  if (taker.side == Side::BUY)
    update_ladder(ask_ladder_);
  else
    update_ladder(bid_ladder_);

  // Retire fully-filled maker
  if (maker.remaining == 0) {
    maker.status = OrderStatus::FILLED;
    order_map_.erase(maker.id);
    ++stats_.orders_filled;
    if (LIKELY(callbacks_))
      callbacks_->on_order_filled(maker);
    order_pool_.deallocate(&maker);
  } else {
    maker.status = OrderStatus::PARTIAL;
  }

  taker.status =
      (taker.remaining == 0) ? OrderStatus::FILLED : OrderStatus::PARTIAL;

  if (LIKELY(callbacks_))
    callbacks_->on_trade(t);
  out.push_back(t);
}

CancelResult OrderBook::cancel_order(OrderId id) {
  CancelResult result{};
  auto it = order_map_.find(id);
  if (UNLIKELY(it == order_map_.end()))
    return result;
  result.found = true;

  Order *o = it->second;
  if (UNLIKELY(!o->is_active()))
    return result;

  auto remove_from = [&](auto &ladder) {
    auto lit = ladder.find(o->price);
    if (LIKELY(lit != ladder.end())) {
      lit->second.total_qty -= o->remaining;
      level_remove_order_links(lit->second, *o);
      if (lit->second.empty())
        ladder.erase(lit);
    }
  };

  if (o->side == Side::BUY)
    remove_from(bid_ladder_);
  else
    remove_from(ask_ladder_);

  o->status = OrderStatus::CANCELLED;
  order_map_.erase(it);
  ++stats_.orders_cancelled;
  result.cancelled = true;

  if (LIKELY(callbacks_))
    callbacks_->on_order_cancelled(*o);
  order_pool_.deallocate(o);
  notify_bbo();
  return result;
}

ModifyResult OrderBook::modify_order(OrderId id, Price new_price,
                                     Quantity new_qty) {
  ModifyResult result{};
  auto it = order_map_.find(id);
  if (UNLIKELY(it == order_map_.end()))
    return result;

  Side side = it->second->side;
  OrderType tp = it->second->type;
  Exchange exch = it->second->exchange;

  cancel_order(id);

  auto add = add_order(side, tp, new_price, new_qty, exch);
  result.success = add.accepted;
  result.new_id = add.id;
  result.fills = std::move(add.fills);
  return result;
}

FORCE_INLINE void OrderBook::level_add_order(PriceLevel &lvl,
                                             Order &o) noexcept {
  o.prev = lvl.tail;
  o.next = 0;
  if (LIKELY(lvl.tail != 0)) {
    auto pit = order_map_.find(lvl.tail);
    if (LIKELY(pit != order_map_.end()))
      pit->second->next = o.id;
  } else {
    lvl.head = o.id;
  }
  lvl.tail = o.id;
  lvl.total_qty += o.remaining;
  ++lvl.order_count;
}

FORCE_INLINE void OrderBook::level_remove_order_links(PriceLevel &lvl,
                                                      Order &o) noexcept {
  if (o.prev != 0) {
    auto pit = order_map_.find(o.prev);
    if (LIKELY(pit != order_map_.end()))
      pit->second->next = o.next;
  } else {
    lvl.head = o.next;
  }
  if (o.next != 0) {
    auto nit = order_map_.find(o.next);
    if (LIKELY(nit != order_map_.end()))
      nit->second->prev = o.prev;
  } else {
    lvl.tail = o.prev;
  }
  if (LIKELY(lvl.order_count > 0))
    --lvl.order_count;
}

std::vector<OrderBook::DepthLevel> OrderBook::bid_depth(int levels) const {
  std::vector<DepthLevel> out;
  out.reserve(static_cast<size_t>(levels));
  for (const auto &[price, lvl] : bid_ladder_) {
    if (static_cast<int>(out.size()) >= levels)
      break;
    out.push_back({lvl.price, lvl.total_qty, lvl.order_count});
  }
  return out;
}

std::vector<OrderBook::DepthLevel> OrderBook::ask_depth(int levels) const {
  std::vector<DepthLevel> out;
  out.reserve(static_cast<size_t>(levels));
  for (const auto &[price, lvl] : ask_ladder_) {
    if (static_cast<int>(out.size()) >= levels)
      break;
    out.push_back({lvl.price, lvl.total_qty, lvl.order_count});
  }
  return out;
}

void OrderBook::clear() {
  bid_ladder_.clear();
  ask_ladder_.clear();
  order_map_.clear();
  order_pool_.reset();
  order_id_seq_ = 0;
  trade_id_seq_ = 0;
  stats_ = {};
  notify_bbo();
}

} // namespace Book
