#pragma once
#include "../FastIndicators.hpp"
#include "../hft.hpp"
#include "../utils//types.hpp"
#include <charconv>
#include <cstdint>
#include "../utility.hpp"
#include "format"

#include "../global.hpp"
#include <iostream>

class alignas(CACHELINE) SMA {

private:
  int64_t running_sum = 0;
  int64_t tick = 0;
  int64_t count = 0;
  int64_t window = 1;
  int64_t filled = 0;  
  int64_t column_to_use = -1;

  HFT::ColumnRing *columnRing = nullptr;
  HFT::TableColumn *tableColumn = nullptr;
  std::array<HFT::TableColumn, HFT::MAXHFTSYMBOL> *symbolAccessArr;
  const int64_t *precision = nullptr;

public:
  SMA(std::array<HFT::TableColumn, HFT::MAXHFTSYMBOL> &symbolAccessArr,
      int64_t tick, HFT::TableColumn &tableColumn, int64_t column_to_use = -1) {

    this->symbolAccessArr = &symbolAccessArr;
    this->tick = tick;
    this->count = 0;
    this->column_to_use = column_to_use;
    this->tableColumn = &tableColumn;

    if (LIKELY(column_to_use >= 0 && column_to_use < HFT::MAXCOLUMN))
      this->columnRing = &tableColumn.history[column_to_use];
    else
      this->columnRing = nullptr;

    this->precision = tableColumn.precisions;
  }

  void set_parameter(const std::vector<std::string> &win) {
    std::string param = win[0];
    int64_t window = 1;

    auto [ptr, ec] = std::from_chars(param.data(), param.data() + param.size(), window);

    if (ec != std::errc() || window <= 0) {
      throw std::runtime_error("Invalid SMA parameter");
    }

    this->window = window;
    this->running_sum = 0;
    this->filled = 0;

    DEBUG_LOG("[SMA] Window set to: " << this->window);
  }

  inline int64_t result() const {
    int64_t denom = (filled < window) ? filled : window;
    if (denom == 0) return 0;
    return running_sum / denom;
  }

  void on_tick() {
    DEBUG_LOG("on tick called");
    if (!columnRing)
      return;
    DEBUG_LOG("on tick called 1");
    count++;
    if (count != tick)
      return;
    DEBUG_LOG("on tick called 2");
    // MyUtility::appendToFile("hello.txt", "called");
    count = 0;

    const int64_t latest = *columnRing->latest_ptr();
    int32_t head = columnRing->head;

    // Old value index (value that falls out of window)
    int32_t old_idx = (head - window - 1) & HFT::MAXRINGMASK;

    int64_t old_val = 0;

    // Add new value
    running_sum += latest;

    if (filled < window) {
      filled++;  // warming phase
    } else {
      // subtract only when window full
      old_val = columnRing->get(old_idx);
      running_sum -= old_val;
    }

    HFT_DEBUG_FILE(
    "hello.txt",
    std::format("latest is {} old val is {} sum is {} column is {} \n", latest, old_val, running_sum,column_to_use)
);
    DEBUG_LOG("[SMA DEBUG]");
    DEBUG_LOG("Latest: " << latest);
    DEBUG_LOG("Old Val: " << old_val);
    DEBUG_LOG("Running Sum: " << running_sum);
    DEBUG_LOG("Filled: " << filled);
    DEBUG_LOG("SMA: " << result());
    DEBUG_LOG("----------------------");
  }


  static void run(void *p) { static_cast<SMA *>(p)->on_tick(); }

  static int64_t get_result(void *p) {
    return static_cast<SMA *>(p)->result();
  }

  FastIndicators::IndicatorEntry create() {
    FastIndicators::IndicatorEntry e;
    e.checked = 1;
    e.ptr = this;
    e.fn = &SMA::run;
    e.result_fn = &SMA::get_result;
    e.indicatorIndex = -1;
    return e;
  }
};