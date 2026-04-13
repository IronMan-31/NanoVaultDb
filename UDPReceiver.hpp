#ifndef __UDP_RECEIVER
#define __UDP_RECEIVER

#include "batchWriter.hpp"
#include "global.hpp"
#include "hft.hpp"
#include "utility.hpp"
#include "utils/cpu_affinity.hpp"
#include "utils/types.hpp"
#include <arpa/inet.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <endian.h>
#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace NetFeed {

constexpr int PORT = 9090;
// SPSCQueue<BatchWriter::batchWriterPacket, BatchWriter::batchWriterQueueSize>&
// __restrict batchWriter = BatchWriter::batchWriterPacketQueue;

FORCE_INLINE int64_t read_be64(const char *p) {
  int64_t val;
  memcpy(&val, p, sizeof(val));
  return be64toh(val);
}
COLD void packet_error() { std::cerr << "Invalid packet\n"; }
int64_t count = 0;

HOT void process_packet(const HFTStorage::Packet &packet, ssize_t n) {
  const char *__restrict__ buffer = packet.data;
  count++;

  // tick is the incoming symbol
  const int64_t tick = read_be64(buffer);
  if (UNLIKELY(tick < 0 || tick >= HFT::MAXHFTSYMBOL)) {
    DEBUG_LOG("Invalid tick index: " << tick);
    return;
  }

  DEBUG_LOG("tick = " << tick);

  auto *__restrict entry = &HFT::symbolAccessArray[tick];

  DEBUG_LOG("symbol = " << entry->symbol);

  if (UNLIKELY(entry->symbol == -1)) {
    DEBUG_LOG("ERROR: invalid symbol");
    return;
  }

  const int64_t columnCount = entry->columnCount;
  const int64_t isTop = entry->isTopOrderBook;

  DEBUG_LOG("columnCount = " << columnCount);
  DEBUG_LOG("isTop = " << isTop);

  const int64_t expected = (columnCount + 1 + (isTop << 2)) << 3;

  DEBUG_LOG("packet size n = " << n);

  if (UNLIKELY(n != expected)) {
    DEBUG_LOG("ERROR: packet size mismatch");
    return;
  }

  const char *__restrict ptr = buffer + 8;

  DEBUG_LOG("starting history loop");

  for (int64_t i = 0; i < columnCount; ++i) {
    int64_t value = read_be64(ptr);
    DEBUG_LOG("history[" << i << "] = " << value);

    entry->pushHistory(i, value);
    ptr += 8;
  }

  if (LIKELY(isTop)) {
    DEBUG_LOG("reading topOrderBook");

    entry->topOrderBook[0] = read_be64(ptr);
    entry->topOrderBook[1] = read_be64(ptr + 8);
    entry->topOrderBook[2] = read_be64(ptr + 16);
    entry->topOrderBook[3] = read_be64(ptr + 24);

    DEBUG_LOG(std::format("topOrderBook: {} {} {} {}", entry->topOrderBook[0],
                          entry->topOrderBook[1], entry->topOrderBook[2],
                          entry->topOrderBook[3]));
  }

  for (int64_t i = 0; i < entry->indicatorIndex; i++) {
    auto *__restrict indicator = &entry->indicators[i];
    indicator->fn(indicator->ptr);
  }

  for (int64_t i = 0; i < entry->strategyIndex; i++) {
    auto *__restrict strategy = &entry->strategys[i];
    bool val = strategy->result_fn(strategy->ptr);

    while (!HFTStorage::webSocketsPacketParser.push({val, tick, i})) {
      HFT_DEBUG_FILE("strategy.txt",
                               std::format("pushed data to strategy.txt"));
    }
  }

  BatchWriter::writeHFTDataToIndexFile(tick);

  DEBUG_LOG(std::format("symbol {} columns {} count {}/{}", entry->symbol, 
                        entry->columnCount, entry->count, entry->storageTicks));
}

void run_receiver(std::stop_token st, int cpu_id) {
  pin_thread_to_cpu(cpu_id);

  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    perror("socket");
    return;
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(sock, (sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    close(sock);
    return;
  }

  alignas(CACHELINE) char buffer[HFTStorage::PacketSize * 4];

  while (!st.stop_requested()) {

    ssize_t n = recvfrom(sock, buffer, sizeof(buffer), 0, nullptr, nullptr);

    if (UNLIKELY(n & 7)) {
      packet_error();
      DEBUG_LOG("error");
      continue;
    }

    HFTStorage::Packet pkt;
    pkt.size = n;
    memcpy(pkt.data, buffer, n);

    if (UNLIKELY(!HFTStorage::PacketParseQueue.push(pkt))) {
      HFTStorage::dropped.fetch_add(1, std::memory_order_relaxed);
    }

    DEBUG_LOG("count " << count);
  }
}

void run_packet_parser(std::stop_token st, int cpu_id) {
  pin_thread_to_cpu(cpu_id);
  HFTStorage::Packet pkt;
  while (!st.stop_requested()) {

    if (HFTStorage::PacketParseQueue.pop(pkt)) {
      process_packet(pkt, pkt.size);
    }
  }
}

void run_strategy_parser(std::stop_token st, int cpu_id) {
  pin_thread_to_cpu(cpu_id);
  HFTStorage::strategyPacket pkt;
  while (!st.stop_requested()) {

    if (HFTStorage::webSocketsPacketParser.pop(pkt)) {
      DEBUG_LOG("Web socket");
      if ((pkt.valid)) {
        HFT_DEBUG_FILE(
            "strategy.txt",
            std::format("the strategy run with symbol {} and strategy id {} ",
                        pkt.tick, pkt.strategyIndex));
        HFT_DEBUG_FILE("strategy.txt",
                                std::format("Websocket packet pushed "));
        web_socket_queue.push(
            web_socket_Packet{pkt.valid, pkt.tick, pkt.strategyIndex});
      }
    }
  }
}

} // namespace NetFeed
#endif