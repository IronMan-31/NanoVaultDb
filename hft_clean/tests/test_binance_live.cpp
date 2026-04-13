// binance_sync.hpp
#pragma once

#include "order_book.hpp"
#include "market_data_handler.hpp"
#include "exchange_adapter.hpp"
#include "../include/nlohmann/json.hpp"

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
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

namespace asio      = boost::asio;
namespace beast     = boost::beast;
namespace http      = beast::http;
namespace websocket = beast::websocket;
namespace ssl       = asio::ssl;
using tcp           = asio::ip::tcp;
using json          = nlohmann::json;
using namespace Book;

// ─────────────────────────────────────────────────────────────────────────────
// BinanceSyncConfig
// ─────────────────────────────────────────────────────────────────────────────
struct BinanceSyncConfig {
    Symbol      symbol{"BTCUSDT"};
    int         snapshot_depth  = 5000;
    int         bbo_print_every = 10;
    int         max_live_frames = -1;      // -1 = run forever
    bool        log_to_file     = false;
    bool        verify_ssl      = false;

    // Gap / resync policy
    int         max_resync_attempts = 5;          // hard stop after N resyncs
    int         resync_delay_ms     = 1500;       // wait before each resync
};

// ─────────────────────────────────────────────────────────────────────────────
// BinanceSyncCallbacks
// ─────────────────────────────────────────────────────────────────────────────
struct BinanceSyncCallbacks {
    std::function<void(uint64_t lastUpdateId)>                  on_snapshot_ready;
    std::function<void()>                                       on_live_start;
    std::function<void(uint64_t U, uint64_t u,
                       const std::string& payload)>             on_frame;
    std::function<void(uint64_t expected_U, uint64_t got_U)>    on_gap;      // NEW
    std::function<void(int attempt)>                            on_resync;   // NEW
    std::function<void(const std::string& what)>                on_error;
};

// ─────────────────────────────────────────────────────────────────────────────
// Internal
// ─────────────────────────────────────────────────────────────────────────────
namespace detail {

struct DepthUpdate {
    uint64_t    U;
    uint64_t    u;
    std::string payload;
    bool        sentinel = false;   // poison pill to unblock waits on error
};

class SyncBuffer {
public:
    void push(DepthUpdate upd) {
        { std::lock_guard<std::mutex> lk(mtx_); buf_.push_back(std::move(upd)); }
        cv_.notify_one();
    }
    void wait_for_first() {
        std::unique_lock<std::mutex> lk(mtx_);
        cv_.wait(lk, [this]{ return !buf_.empty(); });
    }
    std::deque<DepthUpdate> drain() {
        std::lock_guard<std::mutex> lk(mtx_);
        return std::exchange(buf_, {});
    }
    void clear() {
        std::lock_guard<std::mutex> lk(mtx_);
        buf_.clear();
    }
private:
    std::deque<DepthUpdate> buf_;
    std::mutex              mtx_;
    std::condition_variable cv_;
};

// Signals between the main sync-loop and the WS thread.
struct SyncState {
    std::atomic<bool>     snapshot_ready{false};
    std::atomic<uint64_t> snapshot_id{0};
    std::atomic<bool>     gap_detected{false};    // WS thread → main: gap found
    std::atomic<bool>     ws_ready{false};        // WS thread → main: connected
    std::atomic<bool>     stop{false};            // main → WS thread: shut down
    std::atomic<bool>     ws_error{false};
    std::string           ws_error_msg;
};

} // namespace detail

// ─────────────────────────────────────────────────────────────────────────────
// BinanceSync
// ─────────────────────────────────────────────────────────────────────────────
class BinanceSync {
public:
    static void run(MarketDataHandler&          handler,
                    const BinanceSyncConfig&    cfg       = {},
                    const BinanceSyncCallbacks& callbacks = {})
    {
        BinanceSync s(handler, cfg, callbacks);
        s.execute();
    }

    // Call from any thread / callback to stop gracefully
    static void stop() { s_stop_.store(true); }

private:
    BinanceSync(MarketDataHandler&          handler,
                const BinanceSyncConfig&    cfg,
                const BinanceSyncCallbacks& callbacks)
        : handler_(handler), cfg_(cfg), cb_(callbacks)
    {
        s_stop_.store(false);
    }

    // ─────────────────────────────────────────────────────────────────────
    // execute()
    //   Outer retry loop. Each iteration opens a fresh WS connection, does
    //   the full snapshot+drain handshake, then runs the live stream until
    //   either a gap is detected or we're told to stop.
    //   On gap: clear the book, increment attempt counter, sleep, repeat.
    // ─────────────────────────────────────────────────────────────────────
    void execute() {
        Symbol sym{cfg_.symbol};
        int    resync_count = 0;

        while (!s_stop_.load()) {

            if (resync_count > 0) {
                if (resync_count > cfg_.max_resync_attempts) {
                    std::string msg = "Max resync attempts (" +
                                      std::to_string(cfg_.max_resync_attempts) +
                                      ") exceeded. Giving up.";
                    std::cerr << "[BinanceSync] " << msg << "\n";
                    if (cb_.on_error) cb_.on_error(msg);
                    return;
                }
                std::cout << "[BinanceSync] Resync attempt " << resync_count
                          << "/" << cfg_.max_resync_attempts
                          << " — waiting " << cfg_.resync_delay_ms << "ms...\n";

                if (cb_.on_resync) cb_.on_resync(resync_count);

                std::this_thread::sleep_for(
                    std::chrono::milliseconds(cfg_.resync_delay_ms));

                // Wipe the book clean before re-populating
                clear_book(sym);
            }

            bool gap = run_one_sync_cycle(sym);

            if (s_stop_.load()) break;

            if (gap) {
                // WS detected a sequence gap — loop back and resync
                ++resync_count;
                continue;
            }

            // Clean exit (max_live_frames reached or stop() called)
            break;
        }
    }

    // ─────────────────────────────────────────────────────────────────────
    // run_one_sync_cycle()
    //   Returns true  if a gap was detected (caller should resync).
    //   Returns false on clean stop.
    // ─────────────────────────────────────────────────────────────────────
    bool run_one_sync_cycle(const Symbol& sym) {
        detail::SyncBuffer sync_buf;
        detail::SyncState  state;

        // ── Step 1: spawn WS thread — buffers immediately ─────────────
        std::thread ws_thread([&] {
            try {
                ws_loop(sym, sync_buf, state);
            } catch (boost::system::system_error const& se) {
                if (se.code() != boost::asio::ssl::error::stream_truncated) {
                    state.ws_error_msg = se.what();
                    state.ws_error.store(true);
                }
            } catch (std::exception& e) {
                state.ws_error_msg = e.what();
                state.ws_error.store(true);
            }
            state.stop.store(true);
            sync_buf.push({{}, {}, {}, true}); // unblock wait_for_first
        });

        // ── Step 2: wait for first buffered frame ─────────────────────
        std::cout << "[BinanceSync] Waiting for first WS frame...\n";
        sync_buf.wait_for_first();

        if (state.ws_error.load()) {
            ws_thread.join();
            std::string msg = "WS failed before snapshot: " + state.ws_error_msg;
            if (cb_.on_error) cb_.on_error(msg);
            throw std::runtime_error(msg);
        }

        // ── Step 3: fetch REST snapshot ───────────────────────────────
        std::cout << "[BinanceSync] WS live. Fetching snapshot...\n";
        uint64_t S = fetch_snapshot(sym);
        state.snapshot_id.store(S);

        if (cb_.on_snapshot_ready) cb_.on_snapshot_ready(S);

        // ── Step 4: drain buffer — discard stale, apply valid ─────────
        auto buffered = sync_buf.drain();
        std::cout << "[BinanceSync] Draining " << buffered.size()
                  << " buffered frames (S=" << S << ")...\n";

        int applied = 0, discarded = 0;
        for (const auto& upd : buffered) {
            if (upd.sentinel || upd.u == 0) continue;
            if (upd.u <= S) { ++discarded; continue; }
            handler_.feed(Exchange::BINANCE, sym,
                          upd.payload.c_str(), upd.payload.size());
            ++applied;
        }
        std::cout << "[BinanceSync] Applied=" << applied
                  << " Discarded=" << discarded << "\n";

        // ── Step 5: open gate — WS thread switches to live apply ──────
        state.snapshot_ready.store(true);
        std::cout << "[BinanceSync] Book synced. Live stream active.\n";

        if (cb_.on_live_start) cb_.on_live_start();

        // ── Step 6: wait — WS thread runs until gap or stop ──────────
        ws_thread.join();

        if (state.ws_error.load()) {
            std::string msg = "WS error: " + state.ws_error_msg;
            if (cb_.on_error) cb_.on_error(msg);
            throw std::runtime_error(msg);
        }

        // Did the WS thread detect a gap?
        return state.gap_detected.load();
    }

    // ─────────────────────────────────────────────────────────────────────
    // ws_loop()
    //   Phase 1 (snapshot_ready=false): push every frame into sync_buf.
    //   Phase 2 (snapshot_ready=true):  apply frames, check sequence.
    //
    //   Sequence check (the new part):
    //     expected_U starts at S+1 after the first valid live frame.
    //     Every subsequent frame must have U == last_u + 1.
    //     If not: set gap_detected, stop.
    // ─────────────────────────────────────────────────────────────────────
    void ws_loop(const Symbol&         sym,
                 detail::SyncBuffer&   sync_buf,
                 detail::SyncState&    state)
    {
        asio::io_context ioc;
        ssl::context     ctx{ssl::context::tlsv12_client};
        setup_ssl(ctx);

        tcp::resolver resolver(ioc);
        websocket::stream<ssl::stream<tcp::socket>> ws(ioc, ctx);

        asio::connect(ws.next_layer().next_layer(),
                      resolver.resolve("stream.binance.com", "9443"));

        if (!SSL_set_tlsext_host_name(ws.next_layer().native_handle(),
                                      "stream.binance.com"))
            throw boost::system::system_error(::ERR_get_error(),
                                              asio::error::get_ssl_category());

        ws.next_layer().handshake(ssl::stream_base::client);
        ws.set_option(websocket::stream_base::decorator(
            [](websocket::request_type& req) {
                req.set(http::field::user_agent, "hft-client-ws");
            }));

        std::string stream_name{sym.view()};
        for (auto& c : stream_name) c = static_cast<char>(std::tolower(c));
        ws.handshake("stream.binance.com", "/ws/" + stream_name + "@depth");
        std::cout << "[BinanceSync] WS connected.\n";

        std::ofstream ws_file;
        if (cfg_.log_to_file) ws_file.open("binance_stream.json");

        OrderBook* book       = handler_.get_or_create_book(Exchange::BINANCE, sym);
        int        live_count = 0;
        uint64_t   last_u     = 0;   // last accepted frame's u field

        while (!s_stop_.load() && !state.stop.load()) {
            beast::flat_buffer buf;
            boost::system::error_code ec;
            ws.read(buf, ec);

            if (ec) {
                if (!s_stop_.load() && !state.stop.load())
                    throw boost::system::system_error(ec);
                break;
            }

            std::string payload = beast::buffers_to_string(buf.data());
            if (ws_file.is_open()) ws_file << payload << "\n";

            auto j = json::parse(payload, nullptr, false);
            if (!j.is_object()) continue;

            uint64_t U = j.value("U", uint64_t{0});
            uint64_t u = j.value("u", uint64_t{0});

            // ── Phase 1: buffer (snapshot not ready yet) ──────────────
            if (!state.snapshot_ready.load()) {
                sync_buf.push({U, u, payload, false});
                continue;
            }

            // ── Phase 2: live apply ───────────────────────────────────
            uint64_t S = state.snapshot_id.load();

            if (u <= S) continue; // stale, skip

            // ── Sequence gap check ────────────────────────────────────
            //   First live frame after snapshot:
            //     Binance spec: U must be <= S+1  (frame covers the boundary)
            //   Every subsequent frame:
            //     U must equal last_u + 1
            if (last_u == 0) {
                // First live frame — validate it covers the snapshot boundary
                if (U > S + 1) {
                    // Gap between snapshot and first live frame
                    uint64_t expected = S + 1;
                    std::cerr << "[BinanceSync] GAP on first frame: "
                              << "expected U<=" << expected
                              << " got U=" << U << "\n";
                    if (cb_.on_gap) cb_.on_gap(expected, U);
                    state.gap_detected.store(true);
                    break;
                }
            } else {
                // Subsequent frames — must be contiguous
                if (U != last_u + 1) {
                    std::cerr << "[BinanceSync] GAP detected: "
                              << "expected U=" << (last_u + 1)
                              << " got U=" << U
                              << " (missing " << (U - last_u - 1) << " updates)\n";
                    if (cb_.on_gap) cb_.on_gap(last_u + 1, U);
                    state.gap_detected.store(true);
                    break;
                }
            }

            // ── No gap — apply ────────────────────────────────────────
            last_u = u;
            handler_.feed(Exchange::BINANCE, sym,
                          payload.c_str(), payload.size());
            ++live_count;

            if (cb_.on_frame) cb_.on_frame(U, u, payload);

            if (cfg_.bbo_print_every > 0 &&
                live_count % cfg_.bbo_print_every == 0)
            {
                auto bbo = book->best_bbo();
                std::cout << "[BinanceSync] frames=" << live_count
                          << " | Ask=" << std::fixed << std::setprecision(2)
                          << static_cast<double>(bbo.ask_price) / 1e8
                          << " | Bid="
                          << static_cast<double>(bbo.bid_price) / 1e8 << "\n";
            }

            if (cfg_.max_live_frames > 0 &&
                live_count >= cfg_.max_live_frames)
            {
                std::cout << "[BinanceSync] max_live_frames reached.\n";
                s_stop_.store(true);
            }
        }

        boost::system::error_code close_ec;
        ws.close(websocket::close_code::normal, close_ec);
    }

    // ─────────────────────────────────────────────────────────────────────
    // clear_book()
    //   Wipes all resting orders before a resync so we don't double-apply.
    // ─────────────────────────────────────────────────────────────────────
    void clear_book(const Symbol& sym) {
        OrderBook* book = handler_.get_or_create_book(Exchange::BINANCE, sym);
        book->clear();   // assumes OrderBook::clear() exists in your impl
        std::cout << "[BinanceSync] Book cleared for resync.\n";
    }

    // ─────────────────────────────────────────────────────────────────────
    // fetch_snapshot() — unchanged from before
    // ─────────────────────────────────────────────────────────────────────
    uint64_t fetch_snapshot(const Symbol& sym) {
        asio::io_context ioc;
        ssl::context     ctx{ssl::context::tlsv12_client};
        setup_ssl(ctx);

        ssl::stream<tcp::socket> stream(ioc, ctx);
        tcp::resolver resolver(ioc);
        asio::connect(stream.next_layer(),
                      resolver.resolve("api.binance.com", "443"));

        if (!SSL_set_tlsext_host_name(stream.native_handle(), "api.binance.com"))
            throw boost::system::system_error(::ERR_get_error(),
                                              asio::error::get_ssl_category());
        stream.handshake(ssl::stream_base::client);

        std::string target = "/api/v3/depth?symbol=" + std::string(sym.view())
                           + "&limit=" + std::to_string(cfg_.snapshot_depth);

        http::request<http::string_body> req{http::verb::get, target, 11};
        req.set(http::field::host,       "api.binance.com");
        req.set(http::field::user_agent, "hft-client");
        http::write(stream, req);

        beast::flat_buffer                buf;
        http::response<http::string_body> res;
        http::read(stream, buf, res);

        boost::system::error_code ec;
        stream.shutdown(ec);
        if (ec == asio::error::eof) ec = {};

        auto j = json::parse(res.body());

        if (cfg_.log_to_file) {
            std::ofstream f("binance_snapshot.json");
            if (f) f << j.dump(4);
        }

        uint64_t S = j["lastUpdateId"].get<uint64_t>();
        std::cout << "[BinanceSync] Snapshot loaded. lastUpdateId=" << S << "\n";

        OrderBook* book = handler_.get_or_create_book(Exchange::BINANCE, sym);
        auto add = [&](const json& levels, Side side) {
            for (const auto& item : levels) {
                double price = std::stod(item[0].get<std::string>());
                double qty   = std::stod(item[1].get<std::string>());
                if (qty > 0.0)
                    book->add_order(side, OrderType::LIMIT,
                                    to_price(price), to_qty(qty),
                                    Exchange::BINANCE);
            }
        };
        add(j["bids"], Side::BUY);
        add(j["asks"], Side::SELL);
        std::cout << "[BinanceSync] " << book->order_count()
                  << " resting orders in book.\n";
        return S;
    }

    void setup_ssl(ssl::context& ctx) {
        if (cfg_.verify_ssl) ctx.set_default_verify_paths();
        else                  ctx.set_verify_mode(ssl::verify_none);
    }

    MarketDataHandler&          handler_;
    const BinanceSyncConfig&    cfg_;
    const BinanceSyncCallbacks& cb_;

    inline static std::atomic<bool> s_stop_{false};
};
int main(int argc, char *argv[]) {
    std::string symbol = (argc > 1) ? argv[1] : "BTCUSDT";
    
    std::cout << "[TestLive] Starting live test for " << symbol << "..." << std::endl;
    
    MarketDataHandler handler;
    BinanceSyncConfig cfg;
    cfg.symbol = Symbol{symbol};
    cfg.bbo_print_every = 5;
    cfg.max_live_frames = 50; // test for 50 updates
    
    BinanceSyncCallbacks cbs;
    cbs.on_snapshot_ready = [](uint64_t id) {
        std::cout << "[TestLive] Snapshot ready: " << id << std::endl;
    };
    cbs.on_live_start = []() {
        std::cout << "[TestLive] Live stream started." << std::endl;
    };
    cbs.on_gap = [](uint64_t exp, uint64_t got) {
        std::cerr << "[TestLive] GAP DETECTED! Expected " << exp << ", got " << got << std::endl;
    };
    cbs.on_error = [](const std::string& err) {
        std::cerr << "[TestLive] ERROR: " << err << std::endl;
    };

    try {
        BinanceSync::run(handler, cfg, cbs);
    } catch (const std::exception& e) {
        std::cerr << "[TestLive] EXCEPTION: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "[TestLive] Test completed." << std::endl;
    return 0;
}
