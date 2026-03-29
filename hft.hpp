#pragma once

#include "utils/types.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <sys/types.h>
#include <unordered_map>
#include <utility>
#include <vector>
#include "utils/spsc.hpp"
#include "FastIndicators.hpp"
// #include "indicatorInclude.hpp"
namespace HFTStorage {
  // SPSCQueue<>;
  std::atomic<int64_t> dropped{0};
  static constexpr size_t PacketParserQueueSize = 1024;
  static constexpr size_t PacketSize = 1024;
   struct alignas(CACHELINE) Packet{
    char data[PacketSize];
    int64_t size;
  };
  
  SPSCQueue<Packet, PacketParserQueueSize> PacketParseQueue;

}


namespace HFT {

constexpr int64_t SCALINGFACTOR = 1e6;
constexpr int64_t MAXHFTSYMBOL = 100;
constexpr int64_t MAXCOLUMN = 16;
constexpr int64_t MAXRINGSIZE = 256;
constexpr int64_t MAXRINGMASK = MAXRINGSIZE - 1;
constexpr int64_t OrderBookSize = 4;
constexpr int64_t MAX_NO_OF_INDICATORS = 128;

namespace InitalStorage {
  std::unordered_map<std::string, std::pair<std::string, int64_t>> Indicators;
  bool initialIndicatorLoad() {
    // std::cout<<"INITIAL INDICATOR LOAD\n";
    namespace fs = std::filesystem;

    fs::path relativePath = "./fastindicator";

    if (!fs::exists(relativePath) || !fs::is_directory(relativePath)) {
        return false;
    }

    int64_t index =  0;
    for (const auto& entry : fs::directory_iterator(relativePath)) {
        if (entry.is_regular_file()) {
            fs::path filePath = entry.path();
            std::cout<<"file "<<filePath << " is loaded \n";
            std::string baseName = filePath.stem().string();

            std::string absolutePath = fs::absolute(filePath).string();
            std::cout<<baseName<<"\n";
            Indicators[baseName] = {absolutePath,index};
            index++;
        }
    }

    return true;
  }

  inline bool checkIndicatorExists(const std::string &s){
    return Indicators.find(s) != Indicators.end();
}

  // return the indcator path and index

inline std::pair<bool,std::pair<std::string,int64_t>> getIndicator(const std::string &s){
    if(UNLIKELY(!checkIndicatorExists(s))){
        return {false,{"",-1}};
    }

    return {true, Indicators[s]};
}
};

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
    int64_t indicatorIndex =0;
    std::array<FastIndicators::IndicatorEntry, MAX_NO_OF_INDICATORS> indicators{};
    ColumnRing history[MAXCOLUMN];

    // storage ticks
    int32_t storageTicks = -1;
    int32_t count = 0;
    std::unordered_map<std::string, int64_t> indicatorsIndexStorage;


    

    // ask order ask quantity bid order bid quantity




    ColumnRing& operator [](int64_t index){
      return this->history[index];
    }

    std::vector<std::int64_t> getWritingData(){
      std::vector<int64_t> data;  
      for(int i = 0;i<columnCount;i++){
        int64_t value = *history[i].latest_ptr();
        data.push_back(value);
      }
      if(isTopOrderBook){
        for(int j = 0;j<OrderBookSize;j++){
          data.push_back(topOrderBook[j]);
        }
      }
      return std::move(data);
    }
    void init(std::vector<int64_t>&precisions,int cols, bool isBook = false, int sym = -1) {
      std::cout << "HFT symbol initialized  " << " " << cols << " " << isBook
                << " " << sym << "\n";
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









