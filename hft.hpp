#pragma once

#include "FastIndicators.hpp"
#include "FastStrategy.hpp"
#include "global.hpp"
#include "hft_clean/binance_ohlc.hpp"
#include "hft_clean/binance_trades.hpp"
#include "utils/spsc.hpp"
#include "utils/types.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <unordered_map>
#include <utility>
#include <vector>

#include "hft_clean/binance.hpp"
#include "hft_clean/include/exchange_adapter.hpp"
#include "hft_clean/include/market_data_handler.hpp"
#include "hft_clean/include/types.hpp"
#include "hft_storage.hpp"
#include "threadPool.hpp"
#include "tradePackets.hpp"

// #include "indicatorInclude.hpp"

namespace HFT {

constexpr int64_t SCALINGFACTOR = 1e6;
constexpr int64_t MAXHFTSYMBOL = 100;
constexpr int64_t MAXCOLUMN = 16;
constexpr int64_t MAXRINGSIZE = 256;
constexpr int64_t MAXRINGMASK = MAXRINGSIZE - 1;
constexpr int64_t OrderBookSize = 4;
constexpr int64_t MAX_NO_OF_INDICATORS = 128;
constexpr int64_t MAX_NO_OF_STRATEGY = 128;

namespace InitalStorage {
// name path index
std::unordered_map<std::string, std::pair<std::string, int64_t>> Indicators;
std::unordered_map<std::string, std::pair<std::string, int64_t>> Strategy;

bool initialIndicatorLoad() {
  // // std::cout<<"INITIAL INDICATOR LOAD\n";
  namespace fs = std::filesystem;

  fs::path relativePath = "./fastindicator";

  if (!fs::exists(relativePath) || !fs::is_directory(relativePath)) {
    return false;
  }

  int64_t index = 0;
  for (const auto &entry : fs::directory_iterator(relativePath)) {
    if (entry.is_regular_file()) {
      fs::path filePath = entry.path();
      // std::cout << "file " << filePath << " is loaded \n";
      std::string baseName = filePath.stem().string();

      std::string absolutePath = fs::absolute(filePath).string();
      // std::cout << baseName << "\n";
      Indicators[baseName] = {absolutePath, index};
      index++;
    }
  }

  return true;
}

bool initialStrategyLoad() {
  // // std::cout<<"INITIAL INDICATOR LOAD\n";
  namespace fs = std::filesystem;

  fs::path relativePath = "./faststrategy";

  if (!fs::exists(relativePath) || !fs::is_directory(relativePath)) {
    return false;
  }

  int64_t index = 0;
  for (const auto &entry : fs::directory_iterator(relativePath)) {
    if (entry.is_regular_file()) {
      fs::path filePath = entry.path();
      // std::cout << "strategy load ";
      // std::cout << "file " << filePath << " is loaded \n";
      std::string baseName = filePath.stem().string();

      std::string absolutePath = fs::absolute(filePath).string();
      // std::cout << baseName << "\n";
      Strategy[baseName] = {absolutePath, index};
      index++;
    }
  }

  return true;
}

inline bool checkIndicatorExists(const std::string &s) {
  return Indicators.find(s) != Indicators.end();
}

inline bool checkStrategyExits(const std::string &s) {
  return Strategy.find(s) != Strategy.end();
}

// return the indcator path and index

inline std::pair<bool, std::pair<std::string, int64_t>>
getIndicator(const std::string &s) {
  if (UNLIKELY(!checkIndicatorExists(s))) {
    return {false, {"", -1}};
  }

  return {true, Indicators[s]};
}

inline std::pair<bool, std::pair<std::string, int64_t>>
getStrategy(const std::string &s) {
  if (UNLIKELY(!checkStrategyExits(s))) {
    return {false, {"", -1}};
  }

  return {true, Strategy[s]};
}

}; // namespace InitalStorage

struct alignas(64) ColumnRing {

  int64_t head = 0;
  int64_t data[MAXRINGSIZE];

  inline void push(int64_t v) {
    data[head] = v;
    head = (head + 1) & MAXRINGMASK;
  }

  inline int64_t get(int32_t idx) const { return data[idx & MAXRINGMASK]; }

  inline int64_t *latest_ptr() { return &data[(head - 1) & MAXRINGMASK]; }
};

struct alignas(CACHELINE) TableColumn {

  int64_t precisions[MAXCOLUMN];
  int64_t topOrderBookPrecision = 10;
  int64_t topOrderBook[OrderBookSize];
  int64_t isTopOrderBook = false;
  int32_t columnCount = 0;
  int32_t symbol = -1;
  int64_t indicatorIndex = 0;
  int64_t strategyIndex = 0;
  std::array<FastIndicators::IndicatorEntry, MAX_NO_OF_INDICATORS> indicators{};
  std::array<FastStrategy::StrategyEntry, MAX_NO_OF_STRATEGY> strategys{};
  ColumnRing history[MAXCOLUMN];

  // storage ticks
  int32_t storageTicks = -1;
  int32_t count = 0;

  struct BinanceExchange {
    Book::Symbol symbol;
    Book::MarketDataHandler handler;
  };

  BinanceExchange binanceExchange;

  std::unordered_map<std::string, int64_t> indicatorsIndexStorage;
  std::unordered_map<std::string, int64_t> strategysIndexStorage;
  std::unordered_map<int64_t, std::string> indexToStrategy;
  ColumnRing &operator[](int64_t index) { return this->history[index]; }

  std::vector<std::int64_t> getWritingData() {
    std::vector<int64_t> data;
    for (int i = 0; i < columnCount; i++) {
      int64_t value = *history[i].latest_ptr();
      data.push_back(value);
    }
    if (isTopOrderBook) {
      for (int j = 0; j < OrderBookSize; j++) {
        data.push_back(topOrderBook[j]);
      }
    }
    return std::move(data);
  }
  void init(std::vector<int64_t> &precisions, int cols, bool isBook = false,
            int sym = -1) {
    // std::cout << "HFT symbol initialized  " << " " << cols << " " << isBook
    //           << " " << sym << "\n";
    columnCount = cols;
    this->symbol = sym;
    isTopOrderBook = isBook;

    for (int i = 0; i < cols; i++) {
      // values[i] = -1;
      this->precisions[i] = precisions[i];
    }

    for (int i = 0; i < OrderBookSize; i++)
      topOrderBook[i] = -1;
  }

  inline void pushHistory(int col, int64_t v) { history[col].push(v); }
};

alignas(64) std::array<TableColumn, MAXHFTSYMBOL> symbolAccessArray{};

// inline void initTables(int columnsPerSymbol) {
//   for (int i = 0; i < MAXHFTSYMBOL; i++)
//     symbolAccessArray[i].init(columnsPerSymbol, false, i);
// }

// Indicator creation

} // namespace HFT

namespace WebSocketHandler {

std::unordered_map<std::string, std::string> webSocketUrlMap;
void handleWebsocket(std::unique_ptr<WebSocketCommand> &&statement) {
  std::string name = statement->strategyName;
  std::string url = statement->url;
  webSocketUrlMap[name] = url;
}

}; // namespace WebSocketHandler

namespace BinanceExchangeHandler {

void handle_binance_live_orders(
    std::unique_ptr<Binance_LIVE_Orders_Statement> &statement) {

  int64_t table_symbol = statement->tableSymbol;
  std::string binance_symbol = statement->tradeSymbol;

  if (statement->tableSymbol < 0) {
    throw std::runtime_error("the symbol cannot be less than 0");
  }

  if (HFT::symbolAccessArray[statement->tableSymbol].symbol == -1) {
    throw std::runtime_error("no table with symbol " +
                             std::to_string(table_symbol) + " exist");
  }

  Symbol sym(binance_symbol);

  tradePackets::webSocketDataPacket packet = {
      Book::Exchange::BINANCE,
      binance_symbol,
      sym,
      "",
      tradePackets::DataType::OrderUpdates,
      table_symbol};

  HFTStorage::pool.enqueue([packet = std::move(packet)]() mutable {
    BinanceHandler::BinanceTradeSync::run(packet);
  });
}

void handle_binance_ohlc(
    std::unique_ptr<Binance_LIVE_OHLC_STATEMENT> &statement) {
  int64_t table_symbol = statement->tableSymbol;
  std::string binance_symbol = statement->trade_symbol;
  std::string time = statement->time;

  if (statement->tableSymbol < 0) {
    throw std::runtime_error("the symbol cannot be less than 0");
  }
  if (HFT::symbolAccessArray[statement->tableSymbol].symbol == -1) {
    throw std::runtime_error("no table with symbol " +
                             std::to_string(table_symbol) + " exist");
  }
  Symbol sym(binance_symbol);

  tradePackets::webSocketDataPacket packet = {
      Book::Exchange::BINANCE,      binance_symbol, sym, "",
      tradePackets::DataType::OHLC, table_symbol};

  HFTStorage::pool.enqueue([packet = std::move(packet), time]() mutable {
    BinanceHandler::BinanceOHLCSync::run(packet, time);
  });
}

void handle_binance_order_book_parsing(
    std::unique_ptr<BinanceOrderBookStatement> &statement) {
  int64_t table_symbol = statement->tableSymbol;
  std::string binance_symbol = statement->binance_symbol;

  if (statement->tableSymbol < 0) {
    throw std::runtime_error("the symbol cannot be less than 0");
  }

  if (HFT::symbolAccessArray[table_symbol].symbol == -1) {
    throw std::runtime_error("no table with symbol " +
                             std::to_string(table_symbol) + " exist");
  }

  Book::Symbol sym(binance_symbol);
  auto adapter = Book::make_adapter(Book::Exchange::BINANCE);
  if (auto *binance_adapter =
          dynamic_cast<Book::BinanceAdapter *>(adapter.get())) {
    binance_adapter->set_symbol(sym);
  } else {
    throw std::runtime_error(
        "some error occured during binance registration\n");
  }

  HFT::symbolAccessArray[table_symbol].binanceExchange.symbol = sym;
  HFT::symbolAccessArray[table_symbol].binanceExchange.handler.register_adapter(
      std::move(adapter), sym);

  BinanceHandler::BinanceSyncConfig cfg;
  cfg.symbol = sym;
  cfg.max_live_frames = -1;

  tradePackets::WebSocketIdentiFierPacket packet = {
      table_symbol, Book::Exchange::BINANCE,
      tradePackets::DataType::OrderBook_UPDATES};

  HFTStorage::pool.enqueue([table_symbol, cfg, packet] {
    try {
      BinanceHandler::BinanceSync::run(
          HFT::symbolAccessArray[table_symbol].binanceExchange.handler, cfg, {},
          packet);
    } catch (std::exception &e) {
      throw std::runtime_error("some error occured " + std::string(e.what()));
    }
  });
}
}; // namespace BinanceExchangeHandler
