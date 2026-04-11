#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
namespace ssl = asio::ssl;
using tcp = asio::ip::tcp;

// ---------------- CONFIG ----------------
struct StreamConfig {
  std::string symbol;
  std::string stream;
  std::string target;

  std::string to_string() const {
    return "[symbol=" + symbol + " | stream=" + stream + " | target=" + target +
           "]";
  }
};

// ---------------- SESSION ----------------
class WsSession : public std::enable_shared_from_this<WsSession> {
  websocket::stream<beast::ssl_stream<tcp::socket>> ws_;
  tcp::resolver resolver_;
  beast::flat_buffer buffer_;
  asio::steady_timer timer_;

  std::string host_, port_;
  StreamConfig config_;

  int reconnect_delay_ = 1;

public:
  WsSession(asio::io_context &ioc, ssl::context &ctx, std::string host,
            std::string port, StreamConfig cfg)
      : ws_(ioc, ctx), resolver_(ioc), timer_(ioc), host_(std::move(host)),
        port_(std::move(port)), config_(std::move(cfg)) {}

  void start() {
    resolver_.async_resolve(
        host_, port_,
        beast::bind_front_handler(&WsSession::on_resolve, shared_from_this()));
  }

private:
  void on_resolve(beast::error_code ec, tcp::resolver::results_type results) {
    if (ec)
      return retry(ec, "resolve");

    asio::async_connect(
        ws_.next_layer().next_layer(), results,
        beast::bind_front_handler(&WsSession::on_connect, shared_from_this()));
  }

  void on_connect(beast::error_code ec, tcp::endpoint) {
    if (ec)
      return retry(ec, "connect");

    if (!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(),
                                  host_.c_str())) {
      return retry(beast::error_code(static_cast<int>(::ERR_get_error()),
                                     asio::error::get_ssl_category()),
                   "SNI");
    }

    ws_.next_layer().async_handshake(
        ssl::stream_base::client,
        beast::bind_front_handler(&WsSession::on_ssl_handshake,
                                  shared_from_this()));
  }

  void on_ssl_handshake(beast::error_code ec) {
    if (ec)
      return retry(ec, "ssl_handshake");

    ws_.async_handshake(host_, config_.target,
                        beast::bind_front_handler(&WsSession::on_ws_handshake,
                                                  shared_from_this()));
  }

  void on_ws_handshake(beast::error_code ec) {
    if (ec)
      return retry(ec, "ws_handshake");

    std::cout << "Connected: " << config_.to_string() << "\n";
    reconnect_delay_ = 1;

    do_read();
  }

  void do_read() {
    ws_.async_read(buffer_, beast::bind_front_handler(&WsSession::on_read,
                                                      shared_from_this()));
  }

  void on_read(beast::error_code ec, std::size_t) {
    if (ec)
      return retry(ec, "read");

    std::cout << config_.to_string() << " "
              << beast::buffers_to_string(buffer_.data()) << "\n";

    buffer_.consume(buffer_.size());
    do_read();
  }

  // ---------------- RECONNECT LOGIC ----------------
  void retry(beast::error_code ec, const char *where) {
    std::cerr << config_.to_string() << " " << where << ": " << ec.message()
              << "\n";

    beast::error_code ignored;
    ws_.next_layer().next_layer().close(ignored);

    timer_.expires_after(std::chrono::seconds(reconnect_delay_));
    timer_.async_wait([self = shared_from_this()](beast::error_code ec) {
      if (!ec) {
        self->reconnect_delay_ = std::min(self->reconnect_delay_ * 2, 30);
        self->start();
      }
    });
  }
};

// ---------------- MAIN ----------------
int main() {
  asio::io_context ioc;

  ssl::context ctx(ssl::context::tlsv12_client);
  ctx.set_default_verify_paths();

  const std::string host = "stream.binance.com";
  const std::string port = "9443";

  std::vector<std::shared_ptr<WsSession>> sessions;

  // Create 10 connections (change to 100+)
  for (int i = 0; i < 10; ++i) {
    StreamConfig cfg{"btcusdt", "btcusdt@depth", "/ws/btcusdt@depth"};

    auto session = std::make_shared<WsSession>(ioc, ctx, host, port, cfg);
    session->start();
    sessions.push_back(session);
  }

  // Thread pool
  int threads = std::thread::hardware_concurrency();
  std::vector<std::thread> v;

  for (int i = 0; i < threads; ++i) {
    v.emplace_back([&ioc]() { ioc.run(); });
  }

  for (auto &t : v)
    t.join();

  return 0;
}