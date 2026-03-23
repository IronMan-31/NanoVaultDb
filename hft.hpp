#ifndef HFT_CODE
#define HFT_CODE

#include "utils/types.hpp"
#include <array>
#include <cstdint>
#include <cmath>
#include <climits>
#include "utils/types.hpp"

namespace HFT {

constexpr int32_t MAXHFTSYMBOL = 100;
constexpr int32_t MAXCOLUMN = 16;
constexpr int32_t MAXRINGSIZE = 256;
constexpr int32_t MAXRINGMASK = MAXRINGSIZE - 1;
constexpr int32_t OrderBookSize = 4;

struct alignas(64) ColumnRing {

  alignas(64) int64_t data[MAXRINGSIZE];
  int32_t head = 0;

  inline void push(int64_t v) {
    data[head] = v;
    head = (head + 1) & MAXRINGMASK;
  }

  inline int64_t get(int32_t idx) const { return data[idx & MAXRINGMASK]; }

  inline int64_t *latest_ptr() { return &data[(head - 1) & MAXRINGMASK]; }
};

struct alignas(64) TableColumn {

  int64_t precisions[MAXCOLUMN];

  ColumnRing history[MAXCOLUMN];
  int64_t topOrderBookPrecision = 10;
  alignas(64) int64_t topOrderBook[OrderBookSize];

  int64_t isTopOrderBook = false;

  int32_t columnCount = 0;
  int32_t symbol = -1;

  void init(int cols, bool isBook = false, int sym = -1) {
    std::cout << "HFT symbol initialized  " << " " << cols << " " << isBook
              << " " << sym << "\n";
    columnCount = cols;
    this->symbol = sym;
    isTopOrderBook = isBook;

    for (int i = 0; i < cols; i++) {
      // values[i] = -1;
      precisions[i] = -1;
    }

    for (int i = 0; i < OrderBookSize; i++)
      topOrderBook[i] = -1;
  }

  inline void pushHistory(int col, int64_t v) { history[col].push(v); }

};
alignas(64) std::array<TableColumn, MAXHFTSYMBOL> symbolAccessArray;

// inline void initTables(int columnsPerSymbol) {
//   for (int i = 0; i < MAXHFTSYMBOL; i++)
//     symbolAccessArray[i].init(columnsPerSymbol, false, i);
// }

} // namespace HFT









#endif