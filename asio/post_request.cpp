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
        asio::io_context ioc;
        ssl::context ctx(ssl::context::tlsv12_client);

        ctx.set_default_verify_paths();
        ctx.set_verify_mode(ssl::verify_peer);

        tcp::resolver resolver(ioc);
        beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx);

        const std::string host = "httpbin.org";   // test server
        const std::string port = "443";
        const std::string target = "/post";

        // 🔹 JSON payload
        std::string json_body = R"({
            "name": "Shivam",
            "role": "developer"
        })";

        // SNI
        if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
            throw beast::system_error(
                beast::error_code(static_cast<int>(::ERR_get_error()),
                asio::error::get_ssl_category())
            );
        }

        // Connect
        auto const results = resolver.resolve(host, port);
        beast::get_lowest_layer(stream).connect(results);
        stream.handshake(ssl::stream_base::client);

        // 🔹 Create POST request
        http::request<http::string_body> req{http::verb::post, target, 11};

        req.set(http::field::host, host);
        req.set(http::field::user_agent, "MyApp/1.0");

        // 🔹 JSON headers
        req.set(http::field::content_type, "application/json");

        // 🔹 API Key header (example)
        req.set("Authorization", "Bearer YOUR_API_KEY");
        // or:
        // req.set("x-api-key", "YOUR_API_KEY");

        // 🔹 Attach JSON body
        req.body() = json_body;

        // 🔹 MUST call this after setting body
        req.prepare_payload();

        // Send request
        http::write(stream, req);

        // Read response
        beast::flat_buffer buffer;
        http::response<http::dynamic_body> res;
        http::read(stream, buffer, res);

        // Output
        std::cout << "Status: " << res.result_int() << "\n";
        std::cout << beast::buffers_to_string(res.body().data()) << "\n";

        // Shutdown
        beast::error_code ec;
        stream.shutdown(ec);

        if (ec == asio::error::eof)
            ec = {};

        if (ec)
            throw beast::system_error{ec};
    }
    catch (std::exception const& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}