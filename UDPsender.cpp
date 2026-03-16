#include <endian.h>
#include <iostream>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <chrono>
#include <thread>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

static int64_t to_fixed(double val, int precision) {
    return (int64_t)std::round(val * std::pow(10.0, precision));
}

#pragma pack(push, 1)
struct TickPacket {
    int64_t tick;
    int64_t timestamp;   
    int64_t price;       
    int64_t volume;      
    int64_t side;        
};
#pragma pack(pop)
static_assert(sizeof(TickPacket) == 40, "packet must be 40 bytes");

static int64_t now_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

int main(int argc, char* argv[]) {
    const char* host = (argc > 1) ? argv[1] : "127.0.0.1";
    int         port = (argc > 2) ? std::atoi(argv[2]) : 9090;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    sockaddr_in dest{};
    dest.sin_family      = AF_INET;
    dest.sin_port        = htons(port);
    inet_pton(AF_INET, host, &dest.sin_addr);

    std::cout << "Sending ticks to " << host
              << ":" << port << "\n";


    double price  = 65000.00;
    double volume = 1.50;
    int    count  = 0;

    while (true) {
        TickPacket pkt;

        pkt.tick = htobe64((uint64_t)to_fixed(1, 0));
        pkt.timestamp = htobe64((uint64_t)now_us());
        pkt.price     = htobe64((uint64_t)to_fixed(price,  10));
        pkt.volume    = htobe64((uint64_t)to_fixed(volume,  2));
        pkt.side      = htobe64((uint64_t)(count % 3));  // BUY/SELL/UNK

        ssize_t sent = sendto(sock,
                              &pkt, sizeof(pkt), 0,
                              (sockaddr*)&dest, sizeof(dest));
        if (sent != sizeof(pkt)) {
            perror("sendto"); break;
        }

        std::cout << "sent tick #" << count
                  << "  price=" << price
                  << "  vol="   << volume
                  << "  side="  << (count % 3) << "\n";

        price += (count % 2 == 0) ? 0.01 : -0.01;
        count++;

        std::this_thread::sleep_for(
            std::chrono::milliseconds(1));
    }

    close(sock);
    return 0;
}