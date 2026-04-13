#pragma once
#include "types.hpp"

struct alignas(64) Order {
    OrderId    id;           //  8
    OrderId    client_id;    //  8

    Price      price;        //  8  
    Quantity   qty;          //  8  original quantity
    Quantity   remaining;    //  8  unfilled
    Nanos      timestamp_ns; //  8

    Order*     next;         //  8
    Order*     prev;         //  8

    uint8_t    side;         //  1  Side enum
    uint8_t    type;         //  1  OrderType enum
    uint8_t    status;       //  1  OrderStatus enum
    uint8_t    _pad[5];      //  5
    //                           ─────
    //                           64 bytes total
};
static_assert(sizeof(Order) == 128, "Order must be exactly 1 cache line");
static_assert(alignof(Order) == 64, "Order must be cache-line aligned");
