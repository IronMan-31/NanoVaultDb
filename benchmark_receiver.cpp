#include <stop_token>
#include <condition_variable>
#include <shared_mutex>
#include <format>
#include "UDPReceiver.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <numeric>
#include <iomanip>

// Mock implementations for externs if needed
namespace MyUtility {
    // Already defined in utility.hpp as inline, but we might need to provide 
    // a definition if some TU calls it without including utility.hpp.
    // However, benchmark_receiver.cpp includes UDPReceiver.hpp -> batchWriter.hpp -> global.hpp -> utility.hpp
}

// Global variables defined in global.cpp (normally)
// We define them here to avoid linking global.cpp which might pull in more dependencies.
std::unordered_map<std::string, std::string> API;
std::priority_queue<ExpiryNode, std::vector<ExpiryNode>, std::greater<>> expiryHeap;
std::mutex expiryMutex;
std::condition_variable expiryCV;
std::atomic<bool> memorySchedulerRunning{false};
std::unordered_map<std::string, std::shared_ptr<PythonLikeJSONParser>> globalJsonCache;
std::unordered_map<std::string, MemoryEntry> memoryStore;
std::shared_mutex memoryMutex;
std::unordered_map<std::string, std::mutex> tableLocks;
std::unordered_map<int64_t, std::unique_ptr<IoUringQueue>> batchWriterFileMap;
std::mutex dbMutex;
std::atomic<bool> shuttingDown{false};
SPSCQueue<web_socket_Packet, 1024> web_socket_queue;
std::unordered_map<std::string, std::unordered_map<std::string, std::pair<std::string, std::vector<std::shared_ptr<TableGlobalColumnNode>>>>> globalTableCache;
std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, std::pair<TreeVariant, int64_t>>>> dbBtrees;

static int64_t now_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

int main() {
    std::cout << "Initializing Benchmark Receiver..." << std::endl;

    // Initialize HFT structures
    int64_t benchmark_tick = 1;
    auto& entry = HFT::symbolAccessArray[benchmark_tick];
    std::vector<int64_t> precisions = {10, 10, 10, 10}; // 4 columns
    entry.init(precisions, 4, true, benchmark_tick);
    entry.symbol = benchmark_tick; // Ensure symbol != -1

    std::cout << "Starting threads..." << std::endl;
    std::stop_source ss;
    std::thread receiver_thread(NetFeed::run_receiver, ss.get_token(), 0);
    std::thread parser_thread(NetFeed::run_packet_parser, ss.get_token(), 1);

    std::cout << "Receiver listening on port " << NetFeed::PORT << "..." << std::endl;
    std::cout << "Packet size expected: 72 bytes" << std::endl;

    const int interval_sec = 2;
    int64_t last_count = 0;

    while (true) {
        auto start_loop = std::chrono::steady_clock::now();
        std::this_thread::sleep_for(std::chrono::seconds(interval_sec));
        
        int64_t current_count = NetFeed::count;
        int64_t processed = current_count - last_count;
        double throughput = (double)processed / interval_sec;

        // Calculate latency if we have data
        int64_t last_ts = *entry.history[0].latest_ptr();
        int64_t latency = -1;
        if (last_ts > 0) {
            latency = now_us() - last_ts;
        }

        std::cout << "\r" << std::fixed << std::setprecision(2)
                  << "Count: " << current_count 
                  << " | Throughput: " << throughput << " pkts/s"
                  << " | Latency: " << (latency >= 0 ? std::to_string(latency) + " us" : "N/A")
                  << std::flush;

        last_count = current_count;

        if (current_count > 1000000) break; // Auto stop after 1M packets
    }

    ss.request_stop();
    if(receiver_thread.joinable()) receiver_thread.join();
    if(parser_thread.joinable()) parser_thread.join();

    std::cout << "\nBenchmark Receiver finished." << std::endl;
    return 0;
}
