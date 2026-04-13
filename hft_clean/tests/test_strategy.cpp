#include "../../hft.hpp"
#include "../../fastindicator/sma.hpp"
#include "../../faststrategy/basic.hpp"
#include "../../faststrategy/again.hpp"
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

void test_basic_strategy() {
    std::printf("[ test_basic_strategy ]\n");
    
    int64_t symbol = 0;
    std::vector<int64_t> precisions = {2, 2, 2, 2};
    symbolAccessArray[symbol].init(precisions, 4, false, symbol);
    
    // BASIC strategy depends on indicator[0]
    SMA sma(symbolAccessArray, 1, symbolAccessArray[symbol], 0);
    std::vector<std::string> i_params = {"0"}; // SMA window 0? No, let's use 1
    std::vector<std::string> sma_params = {"1"};
    sma.set_parameter(sma_params);
    symbolAccessArray[symbol].indicators[0] = sma.create();
    
    BASIC basic(symbolAccessArray, 1, symbolAccessArray[symbol]);
    std::vector<std::string> s_params = {"500"}; // Condition: val > 500
    basic.set_parameter(s_params);
    
    // Simulate tick
    symbolAccessArray[symbol].pushHistory(0, 400); // SMA will be 400
    sma.on_tick(); 
    CHECK(basic.result() == false);
    
    symbolAccessArray[symbol].pushHistory(0, 600); // SMA will be (400+600)/2 = 500? No, if window=1, it's 600
    sma.on_tick();
    CHECK(basic.result() == true);
}

void test_again_strategy() {
    std::printf("[ test_again_strategy ]\n");
    
    int64_t symbol = 1;
    std::vector<int64_t> again_precisions = {2, 2};
    symbolAccessArray[symbol].init(again_precisions, 2, false, symbol);
    
    SMA sma(symbolAccessArray, 1, symbolAccessArray[symbol], 0);
    std::vector<std::string> again_sma_params = {"1"};
    sma.set_parameter(again_sma_params);
    symbolAccessArray[symbol].indicators[0] = sma.create();
    
    AGAIN again(symbolAccessArray, 1, symbolAccessArray[symbol]);
    std::vector<std::string> again_params = {"1000"};
    again.set_parameter(again_params);
    
    symbolAccessArray[symbol].pushHistory(0, 1100);
    sma.on_tick();
    CHECK(again.result() == true);
    
    symbolAccessArray[symbol].pushHistory(0, 900);
    sma.on_tick();
    CHECK(again.result() == false);
}

void bench_strategy() {
    std::printf("[ benchmark: strategy ]\n");
    int64_t symbol = 0;
    BASIC basic(symbolAccessArray, 1, symbolAccessArray[symbol]);
    basic.set_parameter({"500"});
    
    SMA sma(symbolAccessArray, 1, symbolAccessArray[symbol], 0);
    sma.set_parameter({"1"});
    symbolAccessArray[symbol].indicators[0] = sma.create();
    
    constexpr int N = 1'000'000;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) {
        symbolAccessArray[symbol].pushHistory(0, i);
        sma.on_tick();
        volatile bool res = basic.result();
        (void)res;
    }
    auto t1 = std::chrono::steady_clock::now();
    
    double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    std::printf("  BASIC + SMA (N=%d)  =>  %.1f ns/tick\n", N, ns / N);
}

int main() {
    std::printf("=== HFT Strategy  –  Tests & Benchmarks ===\n\n");
    
    test_basic_strategy();
    test_again_strategy();
    bench_strategy();
    
    std::printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return (g_fail > 0) ? 1 : 0;
}
