#pragma once
#include <atomic>
#include <cstdint>
#include "utils/spsc.hpp"
#include "threadPool.hpp"

namespace HFTStorage {
  static constexpr size_t PacketParserQueueSize = 1024;
  static constexpr size_t PacketSize = 1024;
  
  struct Packet {
    char data[PacketSize];
    int64_t size;
  };
  
  struct strategyPacket {
    bool valid = false;
    int64_t tick;
    int64_t strategyIndex;
  };

  // Using inline to allow header-only inclusion if needed, or matched with a .cpp later
  inline std::atomic<int64_t> dropped{0};
  inline ThreadPool pool(8);
  inline SPSCQueue<strategyPacket, PacketParserQueueSize> webSocketsPacketParser;
  inline SPSCQueue<Packet, PacketParserQueueSize> PacketParseQueue;
}
