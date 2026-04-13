#pragma once

#include <algorithm>
#include <atomic>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <cctype>
#include <iostream>
#include <string>
#include <thread>
#include <utility>
#include "../include/nlohmann/json.hpp"
#include "../tradePackets.hpp"
#include "../debug_macros.hpp"

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace ssl = asio::ssl;
using tcp = asio::ip::tcp;

namespace BinanceHandler {

class BinanceOHLCSync {
public:
  static void run( tradePackets::webSocketDataPacket &packet,  std::string& interval) {

    std::string symbol = packet.symbol;
    try {
      asio::io_context ioc;
      ssl::context ctx{ssl::context::tlsv12_client};
      ctx.set_verify_mode(ssl::verify_none);

      tcp::resolver resolver(ioc);
      websocket::stream<ssl::stream<tcp::socket>> ws(ioc, ctx);

      DEBUG_LOG("[INFO] Connecting to stream.binance.com:9443 for OHLC ("
                << symbol << " @ " << interval << ")...");
      auto const results = resolver.resolve("stream.binance.com", "9443");
      asio::connect(ws.next_layer().next_layer(), results.begin(),
                    results.end());

      SSL_set_tlsext_host_name(ws.next_layer().native_handle(),
                               "stream.binance.com");
      ws.next_layer().handshake(ssl::stream_base::client);

      std::string lower_symbol = symbol;
      std::transform(lower_symbol.begin(), lower_symbol.end(),
                     lower_symbol.begin(),
                     [](unsigned char c) { return std::tolower(c); });

      // path: /ws/<symbol>@kline_<interval>
      std::string path = "/ws/" + lower_symbol + "@kline_" + interval;
      ws.handshake("stream.binance.com", path);
      DEBUG_LOG("[INFO] Connected to OHLC WebSocket. Streaming response...");

      while (true) {
        beast::flat_buffer buffer;
        ws.read(buffer);
        
        std::string buffer_str = beast::buffers_to_string(buffer.data());
        try {
            auto j = nlohmann::json::parse(buffer_str);
            if (j.contains("k") && j["k"]["x"].get<bool>()) {
                DEBUG_LOG("[INFO] Closed Candle: " << buffer_str);
                
                tradePackets::webSocketDataPacket send_packet = {
                  packet.exchange,
                  packet.symbol,
                  packet.sym,
                  buffer_str,
                  tradePackets::DataType::OHLC,
                  packet.tableSymbol
                };

                while (!tradePackets::tradePacketsQueue.push(send_packet)) {
                  std::this_thread::yield();
                }
            }
        } catch (const std::exception& e) {
            DEBUG_LOG("[ERROR] JSON parse error: " << e.what());
        }
      }
    } catch (const std::exception &e) {
      DEBUG_LOG("[ERROR] BinanceOHLCSync: " << e.what());
    }
  }
};

} // namespace BinanceHandler
