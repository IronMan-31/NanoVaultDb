#ifndef HFT_CODE
#define HFT_CODE

#include <array>
#include <iostream>
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

  // ask order ask quantity bid order bid quantity
  int64_t topOrderBookPrecision = 10;
  alignas(64) int64_t topOrderBook[OrderBookSize];

  int64_t isTopOrderBook = false;

  int32_t columnCount = 0;
  int32_t symbol = -1;

  void init(int cols, bool isBook = false, int sym = -1) {
    std::cout<<"HFT symbol initialized  "<<" "<<cols<<" "<<isBook<<" "<<sym<<"\n";
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
  int64_t calc_mean_n(int col, int n){
      int64_t sum = 0;
      int idx = (history[col].head - 1) & MAXRINGMASK;  
      for(int i = 0; i < n; i++){
          sum += history[col].get(idx);
          idx = (idx - 1) & MAXRINGMASK;
      }
      return sum / n;
  }
  int64_t calc_max_n(int col, int n){
      int64_t max_val = INT64_MIN;
      int idx = (history[col].head - 1) & MAXRINGMASK;
      for(int i = 0; i < n; i++){
          int64_t v = history[col].get(idx);
          if(v > max_val) max_val = v;
          idx = (idx - 1) & MAXRINGMASK;
      }
      return max_val;
  }
  int64_t calc_min_n(int col, int n){
      int64_t min_val = INT64_MAX;
      int idx = (history[col].head - 1) & MAXRINGMASK;
      for(int i = 0; i < n; i++){
          int64_t v = history[col].get(idx);
          if(v < min_val) min_val = v;
          idx = (idx - 1) & MAXRINGMASK;
      }
      return min_val;
  }
  int64_t calc_count_n(int col, int n,int64_t threshold){
      int64_t count = 0;
      int idx = (history[col].head - 1) & MAXRINGMASK;
      for(int i = 0; i < n; i++){
          if(history[col].get(idx) >= threshold) count++;
          idx = (idx - 1) & MAXRINGMASK;
      }
      return count;
  }
  int64_t time_wise_mean(int col, int time){
      int64_t cutoff = *history[0].latest_ptr() - (int64_t)(time * 1e9);
      int idx = (history[0].head - 1) & MAXRINGMASK;
      int64_t sum = 0;
      int count = 0;
      while(history[0].get(idx) >= cutoff){
          sum += history[col].get(idx);
          count++;
          idx = (idx - 1) & MAXRINGMASK;
      }
      if (UNLIKELY(count==0)) return 0;
      return sum / count;
  }
  int64_t time_wise_max(int col, int time){
      int64_t cutoff = *history[0].latest_ptr() - (int64_t)(time * 1e9);
      int idx = (history[0].head - 1) & MAXRINGMASK;
      int64_t cur_max = INT64_MIN;  
      int count = 0;
      while(history[0].get(idx) >= cutoff){
          int64_t v = history[col].get(idx);
          if(v > cur_max) cur_max = v;  
          count++;
          idx = (idx - 1) & MAXRINGMASK;
      }
      if (UNLIKELY(count==0)) return 0;
      return cur_max;
  }
  int64_t time_wise_min(int col, int time){
      int64_t cutoff = *history[0].latest_ptr() - (int64_t)(time * 1e9);
      int idx = (history[0].head - 1) & MAXRINGMASK;
      int64_t cur_min = INT64_MAX;  
      int count = 0;
      while(history[0].get(idx) >= cutoff){
          int64_t v = history[col].get(idx);
          if(v < cur_min) cur_min = v;  
          count++;
          idx = (idx - 1) & MAXRINGMASK;
      }
      if (UNLIKELY(count == 0)) return 0;
      return cur_min;
  }
  int64_t time_wise_count(int col, int time,int64_t threshold){
      int64_t cutoff = *history[0].latest_ptr() - (int64_t)(time * 1e9);
      int idx = (history[0].head - 1) & MAXRINGMASK;
      int count = 0;
      while(history[0].get(idx) >= cutoff){
          if(history[col].get(idx) >= threshold) count++;
          idx = (idx - 1) & MAXRINGMASK;
      }
      return count;
  }
  int64_t time_wise_stddev(int col, int time){
      int64_t cutoff = *history[0].latest_ptr() - (int64_t)(time * 1e9);
      int idx = (history[0].head - 1) & MAXRINGMASK;
      int64_t sum = 0;
      int count = 0;
      int start_idx = idx;
      while(history[0].get(idx) >= cutoff){
          sum += history[col].get(idx);
          count++;
          idx = (idx - 1) & MAXRINGMASK;
      }
      if(UNLIKELY(count == 0)) return 0;
      int64_t mean = sum / count;
      idx = start_idx;
      int64_t var_sum = 0;
      for(int i = 0; i < count; i++){
          int64_t diff = history[col].get(idx) - mean;
          var_sum += diff * diff;
          idx = (idx - 1) & MAXRINGMASK;
      }
      return (int64_t)std::sqrt((double)(var_sum / count));
  }

};
alignas(64) std::array<TableColumn, MAXHFTSYMBOL> symbolAccessArray;

// inline void initTables(int columnsPerSymbol) {
//   for (int i = 0; i < MAXHFTSYMBOL; i++)
//     symbolAccessArray[i].init(columnsPerSymbol, false, i);
// }

} // namespace HFT

#endif