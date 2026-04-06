#include <boost/asio/connect.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>

#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

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
    auto const target = "/ws/btcusdt@trade";

    tcp::resolver resolver(ioc);
    auto const results = resolver.resolve(host, port);

    // Connect
    net::connect(ws.next_layer().next_layer(), results);

    // SSL handshake
    ws.next_layer().handshake(ssl::stream_base::client);

    // WebSocket handshake
    ws.handshake(host, target);

    std::cout << "Connected ✅\n";

    while (true) {
      beast::flat_buffer buffer;
      ws.read(buffer);

      std::string msg = beast::buffers_to_string(buffer.data());
      auto data = json::parse(msg);

      long long timestamp = data["T"].get<long long>();
      double price = std::stod(data["p"].get<std::string>());
      double volume = std::stod(data["q"].get<std::string>());
      int side = data["m"].get<bool>() ? -1 : 1;

      std::cout << timestamp << " | " << price << " | " << volume << " | "
                << side << std::endl;
    }

  } catch (std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }
}