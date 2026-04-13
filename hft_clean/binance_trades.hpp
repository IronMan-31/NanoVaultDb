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
#include "../tradePackets.hpp"
#include "../debug_macros.hpp"

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace ssl = asio::ssl;
using tcp = asio::ip::tcp;

namespace BinanceHandler {

class BinanceTradeSync {
public:
  static void run(const tradePackets::webSocketDataPacket &packet) {

    std::string symbol = packet.symbol;
    try {
      asio::io_context ioc;
      ssl::context ctx{ssl::context::tlsv12_client};
      // For production, you should verify the SSL certificate
      ctx.set_verify_mode(ssl::verify_none);

      tcp::resolver resolver(ioc);
      websocket::stream<ssl::stream<tcp::socket>> ws(ioc, ctx);

      DEBUG_LOG("[INFO] Connecting to stream.binance.com:9443 for trades ("
                << symbol << ")...");
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

      std::string path = "/ws/" + lower_symbol + "@trade";
      ws.handshake("stream.binance.com", path);
      DEBUG_LOG("[INFO] Connected to Trades WebSocket. Streaming response...");

      while (true) {
        beast::flat_buffer buffer;
        ws.read(buffer);
        DEBUG_LOG(beast::make_printable(buffer.data()));
        
        std::string buffer_str(static_cast<const char*>(buffer.data().data()), buffer.size());
        
        tradePackets::webSocketDataPacket send_packet = {
          packet.exchange,
          packet.symbol,
          packet.sym,
          buffer_str,
          packet.dataType,
          packet.tableSymbol
        };

        while (!tradePackets::tradePacketsQueue.push(send_packet)) {
          
        }

      }
    } catch (const std::exception &e) {
      DEBUG_LOG("[ERROR] BinanceTradeSync: " << e.what());
    }
  }
};

} // namespace BinanceHandler

// int main(int argc, char *argv[]) {
//   std::string symbol = "btcusdt";
//   if (argc > 1) {
//     symbol = argv[1];
//   }
//   BinanceHandler::BinanceTradeSync::run(symbol);
//   return 0;
// }
