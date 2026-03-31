#include "UDPReceiver.hpp"
#include <iostream>
#include <thread>

int main() {
    std::cout << "Starting standalone test..." << std::endl;
    std::thread packet_receiver(NetFeed::run_receiver);
    std::thread packet_parser(NetFeed::run_packet_parser);

    packet_receiver.detach();
    packet_parser.detach();

    std::cout << "Threads started. Waiting for packets on 9090..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));
    std::cout << "Test ended." << std::endl;
    return 0;
}
