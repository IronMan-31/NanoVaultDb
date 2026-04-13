#include "../../hft.hpp"
#include "../../fastindicator/sma.hpp"
#include "../../fastindicator/obi.hpp"
#include <cassert>
#include <cstdio>
#include <chrono>
#include <vector>

using namespace HFT;

static int g_pass = 0, g_fail = 0;
#define CHECK(cond) do { \
    if (cond) { ++g_pass; } \
    else { ++g_fail; \
        std::fprintf(stderr, "FAIL: %s  (line %d)\n", #cond, __LINE__); } \
} while(0)

void test_sma() {
    std::printf("[ test_sma ]\n");
    
    // Setup
    int64_t symbol = 0;
    std::vector<int64_t> precisions = {2, 2, 2, 2};
    symbolAccessArray[symbol].init(precisions, 4, false, symbol);
    
    SMA sma(symbolAccessArray, 1, symbolAccessArray[symbol], 0);
    std::vector<std::string> params = {"5"}; // Window = 5
    sma.set_parameter(params);
    
    // Simulate ticks
    for (int i = 1; i <= 10; ++i) {
        symbolAccessArray[symbol].pushHistory(0, i * 100); // 1.00, 2.00, ...
        sma.on_tick();
        
        int64_t result = sma.result();
        // For window 5:
        // i=1: sum=100, count=1, result=100
        // i=2: sum=300, count=2, result=150
        // i=3: sum=600, count=3, result=200
        // i=4: sum=1000, count=4, result=250
        // i=5: sum=1500, count=5, result=300
        // i=6: sum=2000, count=5, result=400 (600+500+400+300+200)
        
        if (i == 1) CHECK(result == 100);
        if (i == 5) CHECK(result == 300);
        if (i == 6) CHECK(result == 400);
    }
}

void test_obi() {
    std::printf("[ test_obi ]\n");
    
    int64_t symbol = 1;
    std::vector<int64_t> precisions = {2, 2, 2, 2};
    symbolAccessArray[symbol].init(precisions, 4, true, symbol); // isTop = true
    
    OBI obi(symbolAccessArray, 1, symbolAccessArray[symbol]);
    
    // book[1] = ask_qty, book[3] = bid_qty
    // bid=100, ask=100 -> imbalance = 0
    symbolAccessArray[symbol].topOrderBook[1] = 100;
    symbolAccessArray[symbol].topOrderBook[3] = 100;
    obi.on_tick();
    CHECK(obi.result() == 0);
    
    // bid=200, ask=100 -> imbalance = (200-100)/300 = 1/3
    symbolAccessArray[symbol].topOrderBook[1] = 100;
    symbolAccessArray[symbol].topOrderBook[3] = 200;
    obi.on_tick();
    // 1/3 * 1e6 = 333333
    CHECK(obi.result() == 333333);
}

void bench_indicators() {
    std::printf("[ benchmark: indicators ]\n");
    int64_t symbol = 0;
    SMA sma(symbolAccessArray, 1, symbolAccessArray[symbol], 0);
    std::vector<std::string> sma_params = {"100"};
    sma.set_parameter(sma_params);
    
    constexpr int N = 1'000'000;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) {
        symbolAccessArray[symbol].pushHistory(0, i);
        sma.on_tick();
        volatile int64_t res = sma.result();
        (void)res;
    }
    auto t1 = std::chrono::steady_clock::now();
    
    double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    std::printf("  SMA (N=%d)  =>  %.1f ns/tick\n", N, ns / N);
}

int main() {
    std::printf("=== HFT Indicators  –  Tests & Benchmarks ===\n\n");
    
    test_sma();
    test_obi();
    bench_indicators();
    
    std::printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return (g_fail > 0) ? 1 : 0;
}
