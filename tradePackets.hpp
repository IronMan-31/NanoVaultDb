#pragma once
#include "hft_clean/include/order_book.hpp"
#include "include/nlohmann/json.hpp"
#include "utils/cpu_affinity.hpp"
#include "utils/spsc.hpp"
#include "utils/types.hpp"
#include <cstdint>
#include <iostream>
#include <stop_token>
#include <string>
#include <endian.h>
#include <chrono>
#include "hft_storage.hpp"

namespace tradePackets {
#pragma pack(push, 1)
struct BinanceOHLCData {
  int64_t tick;
  int64_t timestamp; 
  int64_t open;
  int64_t high;
  int64_t low;
  int64_t close;
  int64_t volume;
  int64_t quoteVolume;
  int64_t takerBuyBaseVolume;
  int64_t takerBuyQuoteVolume;
};

struct BinanceLiveTradeData {
  int64_t tick;
  int64_t eventTime;
  int64_t tradeId;
  int64_t price;
  int64_t quantity;
  int64_t tradeTime;
  int64_t isBuyerMaker;
};
#pragma pack(pop)

const int tradePacketQUEUESize = 1024;
enum class DataType {
  OrderBook_UPDATES,
  OrderUpdates, // binance
  OHLC,
};

struct webSocketDataPacket {
  Book::Exchange exchange;
  std::string symbol;
  Book::Symbol sym;
  std::string raw_data;
  DataType dataType;
  int64_t tableSymbol;
};

struct WebSocketIdentiFierPacket {
  int64_t tableSymbol;
  Book::Exchange exchange;
  DataType dataType;
};

inline SPSCQueue<webSocketDataPacket, tradePacketQUEUESize> tradePacketsQueue;
} // namespace tradePackets

namespace tradeHandler {
inline void handle_trade_packet(tradePackets::webSocketDataPacket &packet) {
  if (packet.exchange == Book::Exchange::BINANCE) {
    if (packet.dataType == tradePackets::DataType::OHLC) {
      try {
        auto j = nlohmann::json::parse(packet.raw_data);
        auto &k = j["k"];

        tradePackets::BinanceOHLCData ohlc;
        // Convert string floats to fixed-point int64_t (scaled by 1e8)
        ohlc.tick = packet.tableSymbol;
        ohlc.timestamp = k["t"].get<int64_t>();
        ohlc.open =
            static_cast<int64_t>(std::stod(k["o"].get<std::string>()) * 1e8);
        ohlc.high =
            static_cast<int64_t>(std::stod(k["h"].get<std::string>()) * 1e8);
        ohlc.low =
            static_cast<int64_t>(std::stod(k["l"].get<std::string>()) * 1e8);
        ohlc.close =
            static_cast<int64_t>(std::stod(k["c"].get<std::string>()) * 1e8);
        ohlc.volume =
            static_cast<int64_t>(std::stod(k["v"].get<std::string>()) * 1e8);
        ohlc.quoteVolume =
            static_cast<int64_t>(std::stod(k["q"].get<std::string>()) * 1e8);
        ohlc.takerBuyBaseVolume =
            static_cast<int64_t>(std::stod(k["V"].get<std::string>()) * 1e8);
        ohlc.takerBuyQuoteVolume =
            static_cast<int64_t>(std::stod(k["Q"].get<std::string>()) * 1e8);

        // Debug print to verify parsing
        DEBUG_LOG("[TRADE_HANDLER] OHLC " << packet.symbol
                  << " | CLOSE=" << ohlc.close);

        // Directly push to HFTStorage queue for processing
        HFTStorage::Packet hftPkt{};
        hftPkt.size = sizeof(tradePackets::BinanceOHLCData);
        
        tradePackets::BinanceOHLCData be_ohlc;
        be_ohlc.tick = htobe64(ohlc.tick);
        be_ohlc.timestamp = htobe64(ohlc.timestamp);
        be_ohlc.open = htobe64(ohlc.open);
        be_ohlc.high = htobe64(ohlc.high);
        be_ohlc.low = htobe64(ohlc.low);
        be_ohlc.close = htobe64(ohlc.close);
        be_ohlc.volume = htobe64(ohlc.volume);
        be_ohlc.quoteVolume = htobe64(ohlc.quoteVolume);
        be_ohlc.takerBuyBaseVolume = htobe64(ohlc.takerBuyBaseVolume);
        be_ohlc.takerBuyQuoteVolume = htobe64(ohlc.takerBuyQuoteVolume);

        memcpy(hftPkt.data, &be_ohlc, sizeof(be_ohlc));

        if (!HFTStorage::PacketParseQueue.push(hftPkt)) {
          HFTStorage::dropped.fetch_add(1, std::memory_order_relaxed);
        }

      } catch (const std::exception &e) {
        std::cerr << "[ERROR] handle_trade_packet OHLC parse error: "
                  << e.what() << std::endl;
      }
    } else if (packet.dataType == tradePackets::DataType::OrderUpdates) {
      try {
        auto j = nlohmann::json::parse(packet.raw_data);
        
        tradePackets::BinanceLiveTradeData trade;
        trade.tick = packet.tableSymbol;
        trade.eventTime = j["E"].get<int64_t>();
        trade.tradeId = j["t"].get<int64_t>();
        trade.price = static_cast<int64_t>(std::stod(j["p"].get<std::string>()) * 1e8);
        trade.quantity = static_cast<int64_t>(std::stod(j["q"].get<std::string>()) * 1e8);
        trade.tradeTime = j["T"].get<int64_t>();
        trade.isBuyerMaker = j["m"].get<bool>() ? 1 : 0;

        DEBUG_LOG("[TRADE_HANDLER] TRADE " << packet.symbol 
                  << " | PRICE=" << trade.price << " QTY=" << trade.quantity);
        HFTStorage::Packet hftPkt{};
        hftPkt.size = sizeof(tradePackets::BinanceLiveTradeData);
        
        tradePackets::BinanceLiveTradeData be_trade;
        be_trade.tick = htobe64(trade.tick);
        be_trade.eventTime = htobe64(trade.eventTime);
        be_trade.tradeId = htobe64(trade.tradeId);
        be_trade.price = htobe64(trade.price);
        be_trade.quantity = htobe64(trade.quantity);
        be_trade.tradeTime = htobe64(trade.tradeTime);
        be_trade.isBuyerMaker = htobe64(trade.isBuyerMaker);

        memcpy(hftPkt.data, &be_trade, sizeof(be_trade));

        if (!HFTStorage::PacketParseQueue.push(hftPkt)) {
          HFTStorage::dropped.fetch_add(1, std::memory_order_relaxed);
        }
      } catch (const std::exception &e) {
        std::cerr << "[ERROR] handle_trade_packet Trade parse error: "
                  << e.what() << std::endl;
      }
    }
  }
}

inline void run_trade_handler(std::stop_token st, int cpu_id) {
  pin_thread_to_cpu(cpu_id);
  while (!st.stop_requested()) {
    tradePackets::webSocketDataPacket packet;
    if (tradePackets::tradePacketsQueue.pop(packet)) {
      handle_trade_packet(packet);
    }
  }
}
} // namespace tradeHandler
