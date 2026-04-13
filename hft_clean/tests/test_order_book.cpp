/*
 * test_order_book.cpp  –  Unit tests + throughput benchmarks.
 *
 * All OrderBook objects are heap-allocated (std::make_unique) to avoid
 * stack overflow — the MemoryPool is ~128 MB per book.
 *
 * Run:  ./run_tests
 * Expected: "Results: 46 passed, 0 failed"
 */

#include "order_book.hpp"
#include "simd_utils.hpp"
#include <cassert>
#include <cstdio>
#include <chrono>
#include <memory>
#include <vector>

using namespace Book;

// ---------------------------------------------------------------------------
static int g_pass = 0, g_fail = 0;
#define CHECK(cond) do { \
    if (cond) { ++g_pass; } \
    else { ++g_fail; \
        std::fprintf(stderr, "FAIL: %s  (line %d)\n", #cond, __LINE__); } \
} while(0)
#define CHECK_EQ(a, b) CHECK((a) == (b))

static std::unique_ptr<OrderBook> make_book(const char* sym = "BTCUSDT") {
    return std::make_unique<OrderBook>(Symbol{sym});
}

// ---------------------------------------------------------------------------
void test_basic_limit_match() {
    std::printf("[ test_basic_limit_match ]\n");
    auto book = make_book();

    auto sell = book->add_order(Side::SELL, OrderType::LIMIT, to_price(50000.0), to_qty(1.0));
    CHECK(sell.accepted);
    CHECK(sell.fills.empty());

    auto buy = book->add_order(Side::BUY, OrderType::LIMIT, to_price(50000.0), to_qty(1.0));
    CHECK(buy.accepted);
    CHECK_EQ(buy.fills.size(), 1u);
    CHECK_EQ(buy.fills[0].price, to_price(50000.0));
    CHECK_EQ(buy.fills[0].qty,   to_qty(1.0));
    CHECK_EQ(book->best_bid(), 0);
    CHECK_EQ(book->best_ask(), 0);
    CHECK_EQ(book->order_count(), 0u);
    CHECK_EQ(book->stats().trades_executed, 1u);
}

void test_partial_fill() {
    std::printf("[ test_partial_fill ]\n");
    auto book = make_book();

    book->add_order(Side::SELL, OrderType::LIMIT, to_price(100.0), to_qty(5.0));
    auto buy = book->add_order(Side::BUY, OrderType::LIMIT, to_price(100.0), to_qty(2.0));

    CHECK_EQ(buy.fills.size(), 1u);
    CHECK_EQ(buy.fills[0].qty, to_qty(2.0));
    auto bbo = book->best_bbo();
    CHECK_EQ(bbo.ask_price, to_price(100.0));
    CHECK_EQ(bbo.ask_qty,   to_qty(3.0));
}

void test_fifo_priority() {
    std::printf("[ test_fifo_priority ]\n");
    auto book = make_book();

    auto s1 = book->add_order(Side::SELL, OrderType::LIMIT, to_price(100.0), to_qty(1.0));
    auto s2 = book->add_order(Side::SELL, OrderType::LIMIT, to_price(100.0), to_qty(2.0));
    auto s3 = book->add_order(Side::SELL, OrderType::LIMIT, to_price(100.0), to_qty(1.0));

    auto buy = book->add_order(Side::BUY, OrderType::LIMIT, to_price(100.0), to_qty(3.0));
    CHECK(buy.accepted);
    CHECK_EQ(buy.fills.size(), 2u);
    CHECK_EQ(buy.fills[0].maker_id, s1.id);
    CHECK_EQ(buy.fills[0].qty,      to_qty(1.0));
    CHECK_EQ(buy.fills[1].maker_id, s2.id);
    CHECK_EQ(buy.fills[1].qty,      to_qty(2.0));
    CHECK_EQ(book->best_ask(), to_price(100.0));  // s3 still resting
    (void)s3;
}

void test_cancel() {
    std::printf("[ test_cancel ]\n");
    auto book = make_book();

    auto order = book->add_order(Side::BUY, OrderType::LIMIT, to_price(50.0), to_qty(1.0));
    CHECK_EQ(book->best_bid(), to_price(50.0));

    auto res = book->cancel_order(order.id);
    CHECK(res.found);
    CHECK(res.cancelled);
    CHECK_EQ(book->best_bid(), 0);
    CHECK_EQ(book->order_count(), 0u);
}

void test_cancel_nonexistent() {
    std::printf("[ test_cancel_nonexistent ]\n");
    auto book = make_book();
    auto res = book->cancel_order(999999ULL);
    CHECK(!res.found);
    CHECK(!res.cancelled);
}

void test_ioc() {
    std::printf("[ test_ioc ]\n");
    auto book = make_book();

    book->add_order(Side::SELL, OrderType::LIMIT, to_price(100.0), to_qty(2.0));
    auto buy = book->add_order(Side::BUY, OrderType::IOC, to_price(100.0), to_qty(5.0));
    CHECK(buy.accepted);
    CHECK_EQ(buy.fills.size(), 1u);
    CHECK_EQ(buy.fills[0].qty, to_qty(2.0));
    // Remainder cancelled — no resting bid
    CHECK_EQ(book->best_bid(), 0);
}

void test_ioc_no_match() {
    std::printf("[ test_ioc_no_match ]\n");
    auto book = make_book();
    // No asks — IOC should still be accepted but with no fills and no resting bid
    auto buy = book->add_order(Side::BUY, OrderType::IOC, to_price(100.0), to_qty(1.0));
    CHECK(buy.accepted);
    CHECK(buy.fills.empty());
    CHECK_EQ(book->best_bid(), 0);
}

void test_fok_success() {
    std::printf("[ test_fok_success ]\n");
    auto book = make_book();

    book->add_order(Side::SELL, OrderType::LIMIT, to_price(100.0), to_qty(5.0));
    auto buy = book->add_order(Side::BUY, OrderType::FOK, to_price(100.0), to_qty(5.0));
    CHECK(buy.accepted);
    CHECK_EQ(buy.fills.size(), 1u);
    CHECK_EQ(buy.fills[0].qty, to_qty(5.0));
}

void test_fok_fail() {
    std::printf("[ test_fok_fail ]\n");
    auto book = make_book();

    // Only 2 lots available; FOK for 5 must be rejected without touching the book
    book->add_order(Side::SELL, OrderType::LIMIT, to_price(100.0), to_qty(2.0));
    auto buy = book->add_order(Side::BUY, OrderType::FOK, to_price(100.0), to_qty(5.0));
    CHECK(!buy.accepted);
    CHECK(buy.fills.empty());
    // Original sell still intact
    CHECK_EQ(book->best_ask(), to_price(100.0));
}

void test_market_order() {
    std::printf("[ test_market_order ]\n");
    auto book = make_book();

    book->add_order(Side::SELL, OrderType::LIMIT, to_price(100.0), to_qty(1.0));
    book->add_order(Side::SELL, OrderType::LIMIT, to_price(101.0), to_qty(2.0));
    auto buy = book->add_order(Side::BUY, OrderType::MARKET, 0, to_qty(3.0));
    CHECK(buy.accepted);
    CHECK_EQ(buy.fills.size(), 2u);
    CHECK_EQ(book->best_ask(), 0);
}

void test_multi_level_sweep() {
    std::printf("[ test_multi_level_sweep ]\n");
    auto book = make_book();

    for (int i = 0; i < 5; ++i)
        book->add_order(Side::SELL, OrderType::LIMIT, to_price(100.0 + i), to_qty(1.0));

    // Buy 5 — sweeps all 5 ask levels
    auto buy = book->add_order(Side::BUY, OrderType::LIMIT, to_price(105.0), to_qty(5.0));
    CHECK(buy.accepted);
    CHECK_EQ(buy.fills.size(), 5u);
    CHECK_EQ(book->best_ask(), 0);
}

void test_modify_order() {
    std::printf("[ test_modify_order ]\n");
    auto book = make_book();

    auto orig = book->add_order(Side::BUY, OrderType::LIMIT, to_price(90.0), to_qty(1.0));
    CHECK_EQ(book->best_bid(), to_price(90.0));

    auto mod = book->modify_order(orig.id, to_price(95.0), to_qty(2.0));
    CHECK(mod.success);
    CHECK_EQ(book->best_bid(), to_price(95.0));
}

void test_depth_snapshot() {
    std::printf("[ test_depth_snapshot ]\n");
    auto book = make_book();

    for (int i = 1; i <= 5; ++i)
        book->add_order(Side::BUY,  OrderType::LIMIT, to_price(100.0 - i), to_qty(i));
    for (int i = 1; i <= 5; ++i)
        book->add_order(Side::SELL, OrderType::LIMIT, to_price(101.0 + i), to_qty(i));

    auto bids = book->bid_depth(3);
    auto asks = book->ask_depth(3);
    CHECK_EQ(bids.size(), 3u);
    CHECK_EQ(asks.size(), 3u);
    CHECK_EQ(bids[0].price, to_price(99.0));    // best bid = 100-1
    CHECK_EQ(asks[0].price, to_price(102.0));   // best ask = 101+1
}

void test_simd_find_max() {
    std::printf("[ test_simd_find_max ]\n");
    int64_t prices[] = {100, 500, 200, 800, 300, 750, 50, 900};
    CHECK_EQ(simd::find_max_price_idx(prices, 8), 7);   // 900 at index 7
    CHECK_EQ(simd::find_min_price_idx(prices, 8), 6);   // 50  at index 6
    CHECK_EQ(simd::find_max_price_idx(prices, 1), 0);   // single element
}

void test_bbo_spread() {
    std::printf("[ test_bbo_spread ]\n");
    auto book = make_book();

    book->add_order(Side::BUY,  OrderType::LIMIT, to_price(99.5),  to_qty(1.0));
    book->add_order(Side::SELL, OrderType::LIMIT, to_price(100.0), to_qty(1.0));

    auto bbo = book->best_bbo();
    CHECK(bbo.valid());
    CHECK_EQ(bbo.spread(), to_price(100.0) - to_price(99.5));
}

// ---------------------------------------------------------------------------
// Benchmarks
// ---------------------------------------------------------------------------
void bench_add_resting() {
    std::printf("[ benchmark: resting add_order ]\n");
    auto book = make_book();
    constexpr int N = 200'000;

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i)
        book->add_order(Side::BUY, OrderType::LIMIT, to_price(100.0 - i * 0.01), to_qty(1.0));
    auto t1 = std::chrono::steady_clock::now();

    double us  = std::chrono::duration<double, std::micro>(t1 - t0).count();
    double ops = N / (us / 1e6);
    std::printf("  %d resting orders  =>  %.0f orders/sec  (%.1f ns/order)\n",
                N, ops, 1e9 / ops);
}

void bench_match() {
    std::printf("[ benchmark: match round-trip ]\n");
    auto book = make_book();
    constexpr int N = 100'000;

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) {
        book->add_order(Side::SELL, OrderType::LIMIT, to_price(100.0), to_qty(1.0));
        book->add_order(Side::BUY,  OrderType::LIMIT, to_price(100.0), to_qty(1.0));
    }
    auto t1 = std::chrono::steady_clock::now();

    double us  = std::chrono::duration<double, std::micro>(t1 - t0).count();
    double rts = N / (us / 1e6);
    std::printf("  %d round-trips  =>  %.0f pairs/sec  (%.1f ns/rt)\n",
                N, rts, 1e9 / rts);
    CHECK_EQ(book->stats().trades_executed, static_cast<uint64_t>(N));
}

// ---------------------------------------------------------------------------
int main()
{
    std::printf("=== HFT Order Book  –  Tests & Benchmarks ===\n\n");

    test_basic_limit_match();
    test_partial_fill();
    test_fifo_priority();
    test_cancel();
    test_cancel_nonexistent();
    test_ioc();
    test_ioc_no_match();
    test_fok_success();
    test_fok_fail();
    test_market_order();
    test_multi_level_sweep();
    test_modify_order();
    test_depth_snapshot();
    test_simd_find_max();
    test_bbo_spread();

    std::printf("\n");
    bench_add_resting();
    bench_match();

    std::printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return (g_fail > 0) ? 1 : 0;
}
