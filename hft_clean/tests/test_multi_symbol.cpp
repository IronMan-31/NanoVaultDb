#include "../binance.hpp"
#include <iostream>
#include <vector>
#include <memory>

using namespace BinanceHandler;
using namespace Book;

void run_symbol(const std::string& symbol_name) {
    // Each thread gets its own handler and sync instance
    MarketDataHandler handler;
    BinanceSyncConfig cfg;
    cfg.symbol = Symbol{symbol_name};
    cfg.max_live_frames = 10; // Test for 10 frames
    cfg.bbo_print_every = 5;

    std::cout << "[Test] Starting sync for " << symbol_name << " in background...\n";

    try {
        BinanceSync::run(handler, cfg);
        std::cout << "[Test] " << symbol_name << " sync finished.\n";
    } catch (const std::exception& e) {
        std::cerr << "[Test] " << symbol_name << " Error: " << e.what() << "\n";
    }
}

int main() {
    // A thread pool with 4 threads for our 5 symbols
    ::ThreadPool pool(4);

    std::vector<std::string> symbols = {"BTCUSDT", "ETHUSDT", "SOLUSDT", "BNBUSDT", "XRPUSDT"};

    std::cout << "[Main] Enqueueing symbols to the pool...\n";

    for (const auto& s : symbols) {
        pool.enqueue([s] {
            run_symbol(s);
        });
    }

    std::cout << "[Main] All symbols enqueued. Waiting for pool to finish...\n";
    // ThreadPool destructor will join all threads
    return 0;
}
