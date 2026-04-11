#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <iostream>
int main() {
    boost::asio::io_context ioc;
    std::cout << "Boost ASIO found.\n";
    return 0;
}
