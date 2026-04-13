
// g++ binance.cpp -o binance   -lboost_system -lssl -lcrypto -lpthread

#include <boost/asio/connect.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <iostream>
#include <string>
#include <nlohmann/json.hpp>
#include "debug_macros.hpp"

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = net::ip::tcp;
using json = nlohmann::json;

int main() {
    try {
        net::io_context ioc;
        ssl::context ctx(ssl::context::tlsv12_client);
        websocket::stream<beast::ssl_stream<tcp::socket>> ws(ioc, ctx);

        auto const host = "stream.binance.com";
        auto const port = "9443";
        auto const target = "/stream?streams=btcusdt@trade/btcusdt@bookTicker";

        tcp::resolver resolver(ioc);
        auto const results = resolver.resolve(host, port);

        net::connect(ws.next_layer().next_layer(), results);
        ws.next_layer().handshake(ssl::stream_base::client);
        ws.handshake(host, target);

        double best_bid = 0.0, best_bid_qty = 0.0;
        double best_ask = 0.0, best_ask_qty = 0.0;

        while (true) {
            beast::flat_buffer buffer;
            ws.read(buffer);
            std::string msg = beast::buffers_to_string(buffer.data());
            auto j = json::parse(msg);
            std::string stream = j["stream"];
            auto data = j["data"];

            if (stream.find("@bookTicker") != std::string::npos) {
                best_bid = std::stod(data["b"].get<std::string>());
                best_bid_qty = std::stod(data["B"].get<std::string>());
                best_ask = std::stod(data["a"].get<std::string>());
                best_ask_qty = std::stod(data["A"].get<std::string>());
            } else if (stream.find("@trade") != std::string::npos) {
                long long timestamp = data["T"].get<long long>();
                double price = std::stod(data["p"].get<std::string>());
                double volume = std::stod(data["q"].get<std::string>());
                int side = data["m"].get<bool>() ? -1 : 1;
                double spread = best_ask - best_bid;

                DEBUG_LOG(timestamp << " | "
                          << price << " | "
                          << volume << " | "
                          << side << " | "
                          << best_bid << " | "
                          << best_ask << " | "
                          << spread);
            }
        }
    } catch (std::exception &e) {
        DEBUG_LOG("Error: " << e.what());
    }
}