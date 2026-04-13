// binance_sync.hpp
#pragma once

#include "../include/nlohmann/json.hpp"
#include "include/market_data_handler.hpp"
#include "include/order_book.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <vector>
#include <queue>
#include "../tradePackets.hpp"
#include "../debug_macros.hpp"

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace ssl = asio::ssl;
using tcp = asio::ip::tcp;
using json = nlohmann::json;
using namespace Book;

namespace BinanceHandler {

// ─────────────────────────────────────────────────────────────────────────────
// ThreadGuard: RAII wrapper to ensure a thread is joined on destruction
// ─────────────────────────────────────────────────────────────────────────────
class ThreadGuard {
    std::thread t_;
public:
    explicit ThreadGuard(std::thread t) : t_(std::move(t)) {}
    ~ThreadGuard() { if (t_.joinable()) t_.join(); }
    ThreadGuard(const ThreadGuard&) = delete;
    ThreadGuard& operator=(const ThreadGuard&) = delete;
    ThreadGuard(ThreadGuard&&) noexcept = default;
};

// ─────────────────────────────────────────────────────────────────────────────
// ThreadPool: Simple thread pool to handle 100+ tasks
// ─────────────────────────────────────────────────────────────────────────────
class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads) : stop_(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queue_mtx_);
                        condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                        if (stop_ && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
            });
        }
    }

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(queue_mtx_);
            stop_ = true;
        }
        condition_.notify_all();
        for (auto &worker : workers_) worker.join();
    }

    template<class F>
    void enqueue(F&& f) {
        {
            std::lock_guard<std::mutex> lock(queue_mtx_);
            tasks_.emplace(std::forward<F>(f));
        }
        condition_.notify_one();
    }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mtx_;
    std::condition_variable condition_;
    bool stop_;
};

// ─────────────────────────────────────────────────────────────────────────────
// BinanceSyncConfig
// ─────────────────────────────────────────────────────────────────────────────
struct BinanceSyncConfig {
  Symbol symbol{"BTCUSDT"};
  int snapshot_depth = 5000;
  int bbo_print_every = 10;
  int max_live_frames = -1; 
  bool verify_ssl = false;
  int max_resync_attempts = 5;
  int resync_delay_ms = 1500;
};

// ─────────────────────────────────────────────────────────────────────────────
// BinanceSyncCallbacks
// ─────────────────────────────────────────────────────────────────────────────
struct BinanceSyncCallbacks {
  std::function<void(uint64_t lastUpdateId)> on_snapshot_ready;
  std::function<void()> on_live_start;
  std::function<void(uint64_t U, uint64_t u, const std::string &payload)> on_frame;
  std::function<void(uint64_t expected_U, uint64_t got_U)> on_gap;
  std::function<void(int attempt)> on_resync;
  std::function<void(const std::string &what)> on_error;
};

namespace detail {
struct DepthUpdate {
  uint64_t U, u;
  std::string payload;
  bool sentinel = false;
};

class SyncBuffer {
public:
  void push(DepthUpdate upd) {
    { std::lock_guard<std::mutex> lk(mtx_); buf_.push_back(std::move(upd)); }
    cv_.notify_one();
  }
  void wait_for_first() {
    std::unique_lock<std::mutex> lk(mtx_);
    cv_.wait(lk, [this] { return !buf_.empty(); });
  }
  std::deque<DepthUpdate> drain() {
    std::lock_guard<std::mutex> lk(mtx_);
    return std::exchange(buf_, {});
  }
  void clear() { std::lock_guard<std::mutex> lk(mtx_); buf_.clear(); }
private:
  std::deque<DepthUpdate> buf_;
  std::mutex mtx_;
  std::condition_variable cv_;
};

struct SyncState {
  std::atomic<bool> snapshot_ready{false}, gap_detected{false}, ws_ready{false}, stop{false}, ws_error{false};
  std::atomic<uint64_t> snapshot_id{0};
  std::string ws_error_msg;
};
}

// ─────────────────────────────────────────────────────────────────────────────
// BinanceSync: Synchronous session that can run on its own thread
// ─────────────────────────────────────────────────────────────────────────────
class BinanceSync {
public:
  BinanceSync(MarketDataHandler &handler, const BinanceSyncConfig &cfg = {},
              const BinanceSyncCallbacks &callbacks = {}, const tradePackets::WebSocketIdentiFierPacket & packet = {} )
      : handler_(handler), cfg_(cfg), cb_(callbacks), stop_(false),webSocketInentifier(packet) {}

  static void run(MarketDataHandler &handler, const BinanceSyncConfig &cfg = {},
                  const BinanceSyncCallbacks &callbacks = {},const tradePackets::WebSocketIdentiFierPacket &packet = {}) {
    BinanceSync s(handler, cfg, callbacks,packet);
    s.execute();
  }

  void stop() { stop_.store(true); }

  void execute() {
    Symbol sym{cfg_.symbol};
    DEBUG_LOG("[DEBUG] BinanceSync::execute() started for " << sym.view());
    int resync_count = 0;
    while (!stop_.load()) {
      if (resync_count > 0) {
        if (resync_count > cfg_.max_resync_attempts) {
          if (cb_.on_error) cb_.on_error("Max resync attempts exceeded.");
          return;
        }
        if (cb_.on_resync) cb_.on_resync(resync_count);
        std::this_thread::sleep_for(std::chrono::milliseconds(cfg_.resync_delay_ms));
        clear_book(sym);
      }
      bool gap = run_one_sync_cycle(sym);
      if (stop_.load() || !gap) break;
      ++resync_count;
    }
  }

private:
  bool run_one_sync_cycle(const Symbol &sym) {
    detail::SyncBuffer sync_buf;
    detail::SyncState state;
    DEBUG_LOG("[DEBUG] Starting Sync Cycle for " << sym.view());
    std::thread ws_thread([&] {
      try { 
        DEBUG_LOG("[DEBUG] ws_thread started");
        ws_loop(sym, sync_buf, state); 
      }
      catch (const std::exception &e) { 
        DEBUG_LOG("[ERROR] ws_loop exception: " << e.what());
        state.ws_error.store(true); 
      }
      catch (...) { 
        DEBUG_LOG("[ERROR] ws_loop unknown exception");
        state.ws_error.store(true); 
      }
      state.stop.store(true);
      sync_buf.push({{}, {}, {}, true});
    });

    DEBUG_LOG("[DEBUG] Waiting for first WS update...");
    sync_buf.wait_for_first();
    if (state.ws_error.load()) { 
      DEBUG_LOG("[ERROR] WS thread error before snapshot");
      ws_thread.join(); 
      return false; 
    }
    DEBUG_LOG("[DEBUG] First WS update received. Fetching snapshot...");

    uint64_t S = 0;
    try {
        S = fetch_snapshot(sym);
    } catch (const std::exception &e) {
        DEBUG_LOG("[ERROR] fetch_snapshot failed: " << e.what());
        state.stop.store(true);
        ws_thread.join();
        return false;
    }
    DEBUG_LOG("[DEBUG] Snapshot fetched. lastUpdateId=" << S);
    state.snapshot_id.store(S);
    if (cb_.on_snapshot_ready) cb_.on_snapshot_ready(S);

    auto buffered = sync_buf.drain();
    for (const auto &upd : buffered) {
      if (upd.sentinel || upd.u <= S) continue;
      handler_.feed(Exchange::BINANCE, sym, upd.payload.c_str(), upd.payload.size());
    }
    state.snapshot_ready.store(true);
    if (cb_.on_live_start) cb_.on_live_start();

    ws_thread.join();
    return state.gap_detected.load();
  }

  void ws_loop(const Symbol &sym, detail::SyncBuffer &sync_buf, detail::SyncState &state) {
    asio::io_context ioc;
    ssl::context ctx{ssl::context::tlsv12_client};
    if (!cfg_.verify_ssl) ctx.set_verify_mode(ssl::verify_none);
    tcp::resolver resolver(ioc);
    websocket::stream<ssl::stream<tcp::socket>> ws(ioc, ctx);

    DEBUG_LOG("[DEBUG] Connecting to stream.binance.com:9443...");
    asio::connect(ws.next_layer().next_layer(), resolver.resolve("stream.binance.com", "9443"));
    DEBUG_LOG("[DEBUG] SSL Handshake...");
    SSL_set_tlsext_host_name(ws.next_layer().native_handle(), "stream.binance.com");
    ws.next_layer().handshake(ssl::stream_base::client);
    
    std::string stream{sym.view()};
    for (auto &c : stream) c = static_cast<char>(std::tolower(c));
    DEBUG_LOG("[DEBUG] WS Handshake for " << stream << "@depth...");
    ws.handshake("stream.binance.com", "/ws/" + stream + "@depth");
    DEBUG_LOG("[DEBUG] WS Connected.");

    OrderBook *book = handler_.get_or_create_book(Exchange::BINANCE, sym);
    uint64_t last_u = 0;
    int live_count = 0;

    while (!stop_.load() && !state.stop.load()) {
      beast::flat_buffer buf;
      ws.read(buf);
      std::string payload = beast::buffers_to_string(buf.data());
      auto j = json::parse(payload);
      uint64_t U = j["U"], u = j["u"];

      if (!state.snapshot_ready.load()) {
        sync_buf.push({U, u, payload, false});
        continue;
      }

      uint64_t S = state.snapshot_id.load();
      if (u <= S) continue;

      if (last_u == 0) {
        if (U > S + 1) { state.gap_detected.store(true); break; }
      } else if (U != last_u + 1) { state.gap_detected.store(true); break; }

      last_u = u;
      handler_.feed(Exchange::BINANCE, sym, payload.c_str(), payload.size());
      if (cb_.on_frame) cb_.on_frame(U, u, payload);
      
      if (cfg_.bbo_print_every > 0 && ++live_count % cfg_.bbo_print_every == 0) {
        auto bbo = book->best_bbo();
        DEBUG_LOG("[" << sym.view() << "] Ask=" << std::fixed << std::setprecision(2) << static_cast<double>(bbo.ask_price)/1e8 << " Bid=" << static_cast<double>(bbo.bid_price)/1e8);
      }
      if (cfg_.max_live_frames > 0 && live_count >= cfg_.max_live_frames) { stop_.store(true); }
    }
  }

  uint64_t fetch_snapshot(const Symbol &sym) {
    asio::io_context ioc;
    ssl::context ctx{ssl::context::tlsv12_client};
    ssl::stream<tcp::socket> stream(ioc, ctx);
    tcp::resolver res(ioc);
    
    DEBUG_LOG("[DEBUG] REST Connecting to api.binance.com:443...");
    asio::connect(stream.next_layer(), res.resolve("api.binance.com", "443"));
    DEBUG_LOG("[DEBUG] REST SSL Handshake...");
    SSL_set_tlsext_host_name(stream.native_handle(), "api.binance.com");
    stream.handshake(ssl::stream_base::client);

    DEBUG_LOG("[DEBUG] Sending REST request for snapshot...");

    http::request<http::string_body> req{http::verb::get, "/api/v3/depth?symbol=" + std::string(sym.view()) + "&limit=" + std::to_string(cfg_.snapshot_depth), 11};
    req.set(http::field::host, "api.binance.com");
    req.set(http::field::user_agent, "hft-client");
    http::write(stream, req);

    beast::flat_buffer buf;
    http::response<http::string_body> res_http;
    http::read(stream, buf, res_http);
    
    auto j = json::parse(res_http.body());
    uint64_t S = j["lastUpdateId"];
    OrderBook *book = handler_.get_or_create_book(Exchange::BINANCE, sym);
    auto add = [&](const json &levels, Side side) {
      for (const auto &item : levels) {
        double p = std::stod(item[0].get<std::string>()), q = std::stod(item[1].get<std::string>());
        if (q > 0.0) book->add_order(side, OrderType::LIMIT, to_price(p), to_qty(q), Exchange::BINANCE);
      }
    };
    add(j["bids"], Side::BUY); add(j["asks"], Side::SELL);
    return S;
  }

  void clear_book(const Symbol &sym) {
    handler_.get_or_create_book(Exchange::BINANCE, sym)->clear();
  }

  MarketDataHandler &handler_;
  const BinanceSyncConfig cfg_;
  const BinanceSyncCallbacks cb_;
  std::atomic<bool> stop_;
  struct tradePackets::WebSocketIdentiFierPacket webSocketInentifier;
};

} // namespace BinanceHandler
