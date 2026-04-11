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

#include <iostream>
#include <iomanip>
#include <string>
#include <cstdlib>
#include <memory>
#include <thread>
#include <chrono>
#include <fstream>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace ssl = asio::ssl;
using tcp = asio::ip::tcp;
using json = nlohmann::json;

using namespace Book;

void load_snapshot(MarketDataHandler& handler, const Symbol& sym, asio::ssl::context& ctx, asio::io_context& ioc) {
    std::cout << "[REST] Fetching depth snapshot for " << sym.view() << "...\n";

    ssl::stream<tcp::socket> stream(ioc, ctx);

    tcp::resolver resolver(ioc);
    auto const results = resolver.resolve("api.binance.com", "443");
    asio::connect(stream.next_layer(), results.begin(), results.end());

    // Setup SSL SNI
    if (!SSL_set_tlsext_host_name(stream.native_handle(), "api.binance.com")) {
        throw boost::system::system_error(::ERR_get_error(), asio::error::get_ssl_category());
    }

    stream.handshake(ssl::stream_base::client);

    std::string target = "/api/v3/depth?symbol=";
    target += sym.view();
    target += "&limit=2000";

    http::request<http::string_body> req{http::verb::get, target, 11};
    req.set(http::field::host, "api.binance.com");
    req.set(http::field::user_agent, "hft-client");

    http::write(stream, req);

    beast::flat_buffer buffer;
    http::response<http::string_body> res;
    http::read(stream, buffer, res);

    // Close SSL gracefully
    boost::system::error_code ec;
    stream.shutdown(ec);
    if (ec == asio::error::eof) {
        ec = {};
    }

    // Parse JSON
    auto j = json::parse(res.body());
    
    // Write snapshot to file
    std::ofstream snap_file("binance_snapshot.json");
    if (snap_file.is_open()) {
        snap_file << j.dump(4);
        snap_file.close();
        std::cout << "[REST] Snapshot written to binance_snapshot.json\n";
    }

    // We get lastUpdateId, bids, asks
    uint64_t last_update_id = j["lastUpdateId"].get<uint64_t>();
    std::cout << "[REST] Snapshot received. lastUpdateId: " << last_update_id << "\n";

    OrderBook* book = handler.get_or_create_book(Exchange::BINANCE, sym);

    auto add_levels = [&](const json& levels, Side side) {
        for (const auto& item : levels) {
            double price_val = std::stod(item[0].get<std::string>());
            double qty_val   = std::stod(item[1].get<std::string>());
            if (qty_val > 0) {
                book->add_order(side, OrderType::LIMIT, to_price(price_val), to_qty(qty_val), Exchange::BINANCE);
            }
        }
    };

    add_levels(j["bids"], Side::BUY);
    add_levels(j["asks"], Side::SELL);

    std::cout << "[REST] Snapshot loaded. Resting orders: " << book->order_count() << "\n";
}

void run_websocket(MarketDataHandler& handler, const Symbol& sym, asio::ssl::context& ctx, asio::io_context& ioc) {
    std::cout << "[WS] Connecting to stream.binance.com:9443...\n";

    tcp::resolver resolver(ioc);
    auto const results = resolver.resolve("stream.binance.com", "9443");

    websocket::stream<ssl::stream<tcp::socket>> ws(ioc, ctx);

    asio::connect(ws.next_layer().next_layer(), results.begin(), results.end());

    // Setup SNI
    if (!SSL_set_tlsext_host_name(ws.next_layer().native_handle(), "stream.binance.com")) {
        throw boost::system::system_error(::ERR_get_error(), asio::error::get_ssl_category());
    }

    ws.next_layer().handshake(ssl::stream_base::client);

    // Configure websocket
    ws.set_option(websocket::stream_base::decorator(
        [](websocket::request_type& req) {
            req.set(http::field::user_agent, "hft-client-ws");
        }));

    std::string stream_name{sym.view()};
    for (auto& c : stream_name) c = std::tolower(c);
    stream_name += "@depth";

    std::string path = "/ws/" + stream_name;
    ws.handshake("stream.binance.com", path);

    std::cout << "[WS] Connected to " << path << ". Reading frames...\n";

    std::ofstream ws_file("binance_stream.json");
    if (!ws_file.is_open()) {
        std::cerr << "Failed to open binance_stream.json for writing.\n";
    }

    int frame_count = 0;
    while (frame_count < 50) {
        beast::flat_buffer buffer;
        ws.read(buffer);

        std::string payload = beast::buffers_to_string(buffer.data());
        
        if (ws_file.is_open()) {
            ws_file << payload << "\n";
        }
        
        handler.feed(Exchange::BINANCE, sym, payload.c_str(), payload.size());

        frame_count++;
        if (frame_count % 10 == 0) {
            OrderBook* book = handler.get_or_create_book(Exchange::BINANCE, sym);
            auto bbo = book->best_bbo();
            std::cout << "[WS] Frames processed: " << frame_count << " | "
                      << "BBO Ask: " << std::fixed << std::setprecision(2) << (double)bbo.ask_price / 1e8 << " | "
                      << "BBO Bid: " << std::fixed << std::setprecision(2) << (double)bbo.bid_price / 1e8 << "\n";
        }
    }

    std::cout << "[WS] Finished reading 50 frames. Closing...\n";
    if (ws_file.is_open()) {
        ws_file.close();
    }
    ws.close(websocket::close_code::normal);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    asio::io_context ioc;
    ssl::context ctx{ssl::context::tlsv12_client};

    // To simplify verification, we don't strictly verify certificates, since this is a test.
    ctx.set_verify_mode(ssl::verify_none);

    Symbol sym{"BTCUSDT"};
    MarketDataHandler handler;
    auto adapter = make_adapter(Exchange::BINANCE);
    if (auto* binance_adapter = dynamic_cast<BinanceAdapter*>(adapter.get())) {
        binance_adapter->set_symbol(sym);
    }
    handler.register_adapter(std::move(adapter), sym);

    try {
        load_snapshot(handler, sym, ctx, ioc);
        run_websocket(handler, sym, ctx, ioc);
    } catch (boost::system::system_error const& se) {
        if(se.code() != boost::asio::ssl::error::stream_truncated) {
            std::cerr << "Error: " << se.what() << "\n";
            return 1;
        }
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
