#pragma once
#include "types.hpp"


struct IEventHandler {
    virtual ~IEventHandler() = default;

    virtual void on_order_added(OrderId id, Side side,
                                Price price, Quantity qty) noexcept = 0;

    virtual void on_order_cancelled(OrderId id, Quantity remaining) noexcept = 0;

    virtual void on_order_modified(OrderId id, Quantity old_qty,
                                   Quantity new_qty) noexcept = 0;

    virtual void on_trade(OrderId bid_id, OrderId ask_id,
                          Price price, Quantity qty) noexcept = 0;

    virtual void on_best_bid_changed(Price old_bid, Price new_bid) noexcept {}
    virtual void on_best_ask_changed(Price old_ask, Price new_ask) noexcept {}
};

struct NoOpEventHandler final : IEventHandler {
    void on_order_added(OrderId, Side, Price, Quantity) noexcept override {}
    void on_order_cancelled(OrderId, Quantity)          noexcept override {}
    void on_order_modified(OrderId, Quantity, Quantity) noexcept override {}
    void on_trade(OrderId, OrderId, Price, Quantity)    noexcept override {}
};
