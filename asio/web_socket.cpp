#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio.hpp>
#include <iostream>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
namespace ssl = asio::ssl;
using tcp = asio::ip::tcp;

int main() {
    try {
        asio::io_context ioc;

        // SSL context
        ssl::context ctx(ssl::context::tlsv12_client);
        ctx.set_default_verify_paths();

        // Resolver + WebSocket stream
        tcp::resolver resolver(ioc);
        websocket::stream<beast::ssl_stream<tcp::socket>> ws(ioc, ctx);

        const std::string host = "stream.binance.com";
        const std::string port = "9443";
        const std::string target = "/ws/btcusdt@depth";

        // Resolve
        auto const results = resolver.resolve(host, port);

        // Connect TCP
        auto ep = asio::connect(ws.next_layer().next_layer(), results);

        // SNI (important!)
        if (!SSL_set_tlsext_host_name(ws.next_layer().native_handle(), host.c_str())) {
            throw beast::system_error(
                beast::error_code(static_cast<int>(::ERR_get_error()),
                asio::error::get_ssl_category())
            );
        }

        // SSL handshake
        ws.next_layer().handshake(ssl::stream_base::client);

        // WebSocket handshake
        ws.handshake(host, target);

        std::cout << "Connected to Binance WebSocket!\n";

        // Buffer for incoming messages
        beast::flat_buffer buffer;

        while (true) {
            // Read message
            ws.read(buffer);

            // Print message
            std::cout << beast::buffers_to_string(buffer.data()) << "\n";

            // Clear buffer
            buffer.consume(buffer.size());
        }
    }
    catch (std::exception const& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}