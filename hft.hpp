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
constexpr int32_t MaxAggSize=10;

struct alignas(64) TimeWiseMeanAgg {
    int64_t sum = 0;
    int32_t count = 0;
    int32_t tail = 0;
    int64_t window_ns = 0;
    int32_t col = -1;
    bool active = false;
};

struct MonoDeque {
    int32_t data[MAXRINGSIZE];
    int32_t f = 0, b = 0, sz = 0;
    inline bool empty() const { return sz == 0; }
    inline int32_t front() const { return data[f]; }
    inline int32_t back() const { return data[(b - 1) & MAXRINGMASK]; }
    inline void push_back(int32_t v) { data[b] = v; b = (b + 1) & MAXRINGMASK; sz++; }
    inline void pop_back() { b = (b - 1) & MAXRINGMASK; sz--; }
    inline void pop_front() { f = (f + 1) & MAXRINGMASK; sz--; }
    inline void clear() { f = b = sz = 0; }
};

struct alignas(64) TimeWiseMaxAgg {
    MonoDeque dq;
    int32_t count = 0;
    int32_t tail = 0;
    int64_t window_ns = 0;
    int32_t col = -1;
    bool active = false;
};

struct alignas(64) TimeWiseMinAgg {
    MonoDeque dq;
    int32_t count = 0;
    int32_t tail = 0;
    int64_t window_ns = 0;
    int32_t col = -1;
    bool active = false;
};

struct alignas(64) TimeWiseCountAgg {
    int32_t match_count = 0;
    int32_t total = 0;
    int32_t tail = 0;
    int64_t window_ns = 0;
    int64_t threshold = 0;
    int32_t col = -1;
    bool active = false;
};

struct alignas(64) TimeWiseStddevAgg {
    int64_t sum = 0;
    __int128 sum_sq = 0;
    int32_t count = 0;
    int32_t tail = 0;
    int64_t window_ns = 0;
    int32_t col = -1;
    bool active = false;
};

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

  TimeWiseMeanAgg time_wise_means[MaxAggSize];
  int32_t numMeans = 0;
  TimeWiseMaxAgg time_wise_maxes[MaxAggSize];
  int32_t numMaxes = 0;
  TimeWiseMinAgg time_wise_mins[MaxAggSize];
  int32_t numMins = 0;
  TimeWiseCountAgg time_wise_counts[MaxAggSize];
  int32_t numCounts = 0;
  TimeWiseStddevAgg time_wise_stddevs[MaxAggSize];
  int32_t numStddevs = 0;
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
  int64_t calc_mean_n(int col, int n){
      int64_t sum = 0;
      int idx = (history[col].head - 1) & MAXRINGMASK;  
      int last=idx;
      int count = 0;
      for(int i = 0; i < n; i++){
          sum += history[col].get(idx);
          count++;
          idx = (idx - 1) & MAXRINGMASK;
          if (UNLIKELY(idx==last)) break;
      }
      if (UNLIKELY(count == 0)) return 0;
      return sum / count;
  }
  int64_t calc_max_n(int col, int n){
      int64_t max_val = INT64_MIN;
      int idx = (history[col].head - 1) & MAXRINGMASK;
      int last=idx;
      for(int i = 0; i < n; i++){
          int64_t v = history[col].get(idx);
          if(v > max_val) max_val = v;
          idx = (idx - 1) & MAXRINGMASK;
          if (UNLIKELY(idx==last)) break;
      }
      return max_val;
  }
  int64_t calc_min_n(int col, int n){
      int64_t min_val = INT64_MAX;
      int idx = (history[col].head - 1) & MAXRINGMASK;
      int last=idx;
      for(int i = 0; i < n; i++){
          int64_t v = history[col].get(idx);
          if(v < min_val) min_val = v;
          idx = (idx - 1) & MAXRINGMASK;
          if (UNLIKELY(idx==last)) break;
      }
      return min_val;
  }
  int64_t calc_count_n(int col, int n,int64_t threshold){
      int64_t count = 0;
      int idx = (history[col].head - 1) & MAXRINGMASK;
      int last=idx;
      for(int i = 0; i < n; i++){
          if(history[col].get(idx) >= threshold) count++;
          idx = (idx - 1) & MAXRINGMASK;
          if (UNLIKELY(idx==last)) break;
      }
      return count;
  }
  void registerTimeWiseMean(int col, int64_t window_seconds) {
      if (numMeans >= MaxAggSize) return;
      auto& s = time_wise_means[numMeans++];
      s.col = col;
      s.window_ns = (int64_t)(window_seconds * 1e9);
      s.active = true;
      s.sum = 0; s.count = 0; s.tail = 0;
  }
  void registerTimeWiseMax(int col, int64_t window_seconds) {
      if (numMaxes >= MaxAggSize) return;
      auto& s = time_wise_maxes[numMaxes++];
      s.col = col;
      s.window_ns = (int64_t)(window_seconds * 1e9);
      s.active = true;
      s.count = 0; s.tail = 0; s.dq.clear();
  }
  void registerTimeWiseMin(int col, int64_t window_seconds) {
      if (numMins >= MaxAggSize) return;
      auto& s = time_wise_mins[numMins++];
      s.col = col;
      s.window_ns = (int64_t)(window_seconds * 1e9);
      s.active = true;
      s.count = 0; s.tail = 0; s.dq.clear();
  }
  void registerTimeWiseCount(int col, int64_t window_seconds, int64_t threshold) {
      if (numCounts >= MaxAggSize) return;
      auto& s = time_wise_counts[numCounts++];
      s.col = col;
      s.window_ns = (int64_t)(window_seconds * 1e9);
      s.threshold = threshold;
      s.active = true;
      s.match_count = 0; s.total = 0; s.tail = 0;
  }
  void registerTimeWiseStddev(int col, int64_t window_seconds) {
      if (numStddevs >= MaxAggSize) return;
      auto& s = time_wise_stddevs[numStddevs++];
      s.col = col;
      s.window_ns = (int64_t)(window_seconds * 1e9);
      s.active = true;
      s.sum = 0; s.sum_sq = 0; s.count = 0; s.tail = 0;
  }
  HOT void updateAllAggregates() {
      int64_t latest_ts = *history[0].latest_ptr();
      for (int32_t i = 0; i < numMeans; i++) {
          auto& s = time_wise_means[i];
          int newest_idx = (history[s.col].head - 1) & MAXRINGMASK;
          s.sum += history[s.col].get(newest_idx);
          s.count++;
          int64_t cutoff = latest_ts - s.window_ns;
          while (s.count > 0 && history[0].get(s.tail) < cutoff) {
              s.sum -= history[s.col].get(s.tail);
              s.count--;
              s.tail = (s.tail + 1) & MAXRINGMASK;
          }
          while (s.count > MAXRINGSIZE) {
              s.sum -= history[s.col].get(s.tail);
              s.count--;
              s.tail = (s.tail + 1) & MAXRINGMASK;
          }
      }
      for (int32_t i = 0; i < numMaxes; i++) {
          auto& s = time_wise_maxes[i];
          int newest_idx = (history[s.col].head - 1) & MAXRINGMASK;
          int64_t new_val = history[s.col].get(newest_idx);
          while (!s.dq.empty() && history[s.col].get(s.dq.back()) <= new_val)
              s.dq.pop_back();
          s.dq.push_back(newest_idx);
          s.count++;
          int64_t cutoff = latest_ts - s.window_ns;
          while (s.count > 0 && history[0].get(s.tail) < cutoff) {
              if (!s.dq.empty() && s.dq.front() == s.tail) s.dq.pop_front();
              s.count--;
              s.tail = (s.tail + 1) & MAXRINGMASK;
          }
          while (s.count > MAXRINGSIZE) {
              if (!s.dq.empty() && s.dq.front() == s.tail) s.dq.pop_front();
              s.count--;
              s.tail = (s.tail + 1) & MAXRINGMASK;
          }
      }
      for (int32_t i = 0; i < numMins; i++) {
          auto& s = time_wise_mins[i];
          int newest_idx = (history[s.col].head - 1) & MAXRINGMASK;
          int64_t new_val = history[s.col].get(newest_idx);
          while (!s.dq.empty() && history[s.col].get(s.dq.back()) >= new_val)
              s.dq.pop_back();
          s.dq.push_back(newest_idx);
          s.count++;
          int64_t cutoff = latest_ts - s.window_ns;
          while (s.count > 0 && history[0].get(s.tail) < cutoff) {
              if (!s.dq.empty() && s.dq.front() == s.tail) s.dq.pop_front();
              s.count--;
              s.tail = (s.tail + 1) & MAXRINGMASK;
          }
          while (s.count > MAXRINGSIZE) {
              if (!s.dq.empty() && s.dq.front() == s.tail) s.dq.pop_front();
              s.count--;
              s.tail = (s.tail + 1) & MAXRINGMASK;
          }
      }
      for (int32_t i = 0; i < numCounts; i++) {
          auto& s = time_wise_counts[i];
          int newest_idx = (history[s.col].head - 1) & MAXRINGMASK;
          if (history[s.col].get(newest_idx) >= s.threshold) s.match_count++;
          s.total++;
          int64_t cutoff = latest_ts - s.window_ns;
          while (s.total > 0 && history[0].get(s.tail) < cutoff) {
              if (history[s.col].get(s.tail) >= s.threshold) s.match_count--;
              s.total--;
              s.tail = (s.tail + 1) & MAXRINGMASK;
          }
          while (s.total > MAXRINGSIZE) {
              if (history[s.col].get(s.tail) >= s.threshold) s.match_count--;
              s.total--;
              s.tail = (s.tail + 1) & MAXRINGMASK;
          }
      }
      for (int32_t i = 0; i < numStddevs; i++) {
          auto& s = time_wise_stddevs[i];
          int newest_idx = (history[s.col].head - 1) & MAXRINGMASK;
          int64_t v = history[s.col].get(newest_idx);
          s.sum += v;
          s.sum_sq += (__int128)v * v;
          s.count++;
          int64_t cutoff = latest_ts - s.window_ns;
          while (s.count > 0 && history[0].get(s.tail) < cutoff) {
              int64_t old = history[s.col].get(s.tail);
              s.sum -= old;
              s.sum_sq -= (__int128)old * old;
              s.count--;
              s.tail = (s.tail + 1) & MAXRINGMASK;
          }
          while (s.count > MAXRINGSIZE) {
              int64_t old = history[s.col].get(s.tail);
              s.sum -= old;
              s.sum_sq -= (__int128)old * old;
              s.count--;
              s.tail = (s.tail + 1) & MAXRINGMASK;
          }
      }
  }
  int64_t time_wise_mean(int col, int time){
      int64_t window_ns = (int64_t)(time * 1e9);
      for (int32_t i = 0; i < numMeans; i++) {
          if (time_wise_means[i].active &&
              time_wise_means[i].col == col &&
              time_wise_means[i].window_ns == window_ns) {
              if (UNLIKELY(time_wise_means[i].count == 0)) return 0;
              return time_wise_means[i].sum / time_wise_means[i].count;
          }
      }
      int64_t cutoff = *history[0].latest_ptr() - window_ns;
      int idx = (history[0].head - 1) & MAXRINGMASK;
      int64_t sum = 0;
      int count = 0;
      int last=idx;
      while(history[0].get(idx) >= cutoff){
          sum += history[col].get(idx);
          count++;
          idx = (idx - 1) & MAXRINGMASK;
          if (UNLIKELY(idx==last)) break;
      }
      if (UNLIKELY(count==0)) return 0;
      return sum / count;
  }
  int64_t time_wise_max(int col, int time){
      int64_t window_ns = (int64_t)(time * 1e9);
      for (int32_t i = 0; i < numMaxes; i++) {
          if (time_wise_maxes[i].active &&
              time_wise_maxes[i].col == col &&
              time_wise_maxes[i].window_ns == window_ns) {
              if (UNLIKELY(time_wise_maxes[i].count == 0)) return 0;
              return history[col].get(time_wise_maxes[i].dq.front());
          }
      }
      int64_t cutoff = *history[0].latest_ptr() - window_ns;
      int idx = (history[0].head - 1) & MAXRINGMASK;
      int64_t cur_max = INT64_MIN;  
      int count = 0;
      int last=idx;
      while(history[0].get(idx) >= cutoff){
          int64_t v = history[col].get(idx);
          if(v > cur_max) cur_max = v;  
          count++;
          idx = (idx - 1) & MAXRINGMASK;
          if (UNLIKELY(idx==last)) break;
      }
      if (UNLIKELY(count==0)) return 0;
      return cur_max;
  }
  int64_t time_wise_min(int col, int time){
      int64_t window_ns = (int64_t)(time * 1e9);
      for (int32_t i = 0; i < numMins; i++) {
          if (time_wise_mins[i].active &&
              time_wise_mins[i].col == col &&
              time_wise_mins[i].window_ns == window_ns) {
              if (UNLIKELY(time_wise_mins[i].count == 0)) return 0;
              return history[col].get(time_wise_mins[i].dq.front());
          }
      }
      int64_t cutoff = *history[0].latest_ptr() - window_ns;
      int idx = (history[0].head - 1) & MAXRINGMASK;
      int64_t cur_min = INT64_MAX;  
      int count = 0;
      int last=idx;
      while(history[0].get(idx) >= cutoff){
          int64_t v = history[col].get(idx);
          if(v < cur_min) cur_min = v;  
          count++;
          idx = (idx - 1) & MAXRINGMASK;
          if (UNLIKELY(idx==last)) break;
      }
      if (UNLIKELY(count == 0)) return 0;
      return cur_min;
  }
  int64_t time_wise_count(int col, int time, int64_t threshold){
      int64_t window_ns = (int64_t)(time * 1e9);
      for (int32_t i = 0; i < numCounts; i++) {
          if (time_wise_counts[i].active &&
              time_wise_counts[i].col == col &&
              time_wise_counts[i].window_ns == window_ns &&
              time_wise_counts[i].threshold == threshold) {
              return time_wise_counts[i].match_count;
          }
      }
      int64_t cutoff = *history[0].latest_ptr() - window_ns;
      int idx = (history[0].head - 1) & MAXRINGMASK;
      int count = 0;
      int last=idx;
      while(history[0].get(idx) >= cutoff){
          if(history[col].get(idx) >= threshold) count++;
          idx = (idx - 1) & MAXRINGMASK;
          if (UNLIKELY(idx==last)) break;
      }
      return count;
  }
  int64_t time_wise_stddev(int col, int time){
      int64_t window_ns = (int64_t)(time * 1e9);
      for (int32_t i = 0; i < numStddevs; i++) {
          if (time_wise_stddevs[i].active &&
              time_wise_stddevs[i].col == col &&
              time_wise_stddevs[i].window_ns == window_ns) {
              auto& s = time_wise_stddevs[i];
              if (UNLIKELY(s.count <= 1)) return 0;
              __int128 count_128 = s.count;
              __int128 var_num = count_128 * s.sum_sq - (__int128)s.sum * s.sum;
              if (var_num < 0) var_num = 0;
              double variance = (double)var_num / ((double)count_128 * count_128);
              return (int64_t)std::sqrt(variance);
          }
      }
      int64_t cutoff = *history[0].latest_ptr() - window_ns;
      int idx = (history[0].head - 1) & MAXRINGMASK;
      int64_t sum = 0;
      int count = 0;
      int start_idx = idx;
      int last=idx;
      while(history[0].get(idx) >= cutoff){
          sum += history[col].get(idx);
          count++;
          idx = (idx - 1) & MAXRINGMASK;
          if (UNLIKELY(idx==last)) break;
      }
      if(UNLIKELY(count == 0)) return 0;
      int64_t mean = sum / count;
      idx = start_idx;
      int64_t var_sum = 0;
      for(int i = 0; i < count; i++){
          int64_t diff = history[col].get(idx) - mean;
          var_sum += diff * diff;
          idx = (idx - 1) & MAXRINGMASK;
          if (UNLIKELY(idx==last)) break;
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