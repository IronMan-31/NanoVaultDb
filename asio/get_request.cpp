#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio.hpp>
#include <iostream>

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
namespace ssl = asio::ssl;
using tcp = asio::ip::tcp;

int main() {
    try {
        // I/O context
        asio::io_context ioc;

        // SSL context (TLS client)
        ssl::context ctx(ssl::context::tlsv12_client);

        // Load system root certificates
        ctx.set_default_verify_paths();
        ctx.set_verify_mode(ssl::verify_peer);

        // Resolver and SSL stream
        tcp::resolver resolver(ioc);
        beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx);

        const std::string host = "httpbin.org";
        const std::string target = "/get";
        const std::string port = "443";
        int version = 11; 

        // Set SNI hostname
        if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
            beast::error_code ec{
                static_cast<int>(::ERR_get_error()),
                asio::error::get_ssl_category()
            };
            throw beast::system_error{ec};
        }

        // Resolve domain
        auto const results = resolver.resolve(host, port);

        // Connect TCP
        beast::get_lowest_layer(stream).connect(results);

        // SSL handshake
        stream.handshake(ssl::stream_base::client);

        // Create HTTP GET request
        http::request<http::string_body> req{http::verb::get, target, version};
        req.set(http::field::host, host);
        req.set(http::field::user_agent, "MyApp/1.0");

        // Send request
        http::write(stream, req);

        // Receive response
        beast::flat_buffer buffer;
        http::response<http::dynamic_body> res;
        http::read(stream, buffer, res);

        // Output response
        std::cout << "Status: " << res.result_int() << "\n";
        std::cout << "Body:\n";
        std::cout << beast::buffers_to_string(res.body().data()) << "\n";

        // Graceful shutdown
        beast::error_code ec;
        stream.shutdown(ec);

        if (ec == asio::error::eof) {
            ec = {}; // EOF is expected
        }

        if (ec) {
            throw beast::system_error{ec};
        }
    }
    catch (std::exception const& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}