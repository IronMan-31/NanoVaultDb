
#pragma once 

#include <cstdint>
// symbolAccessArray , tick, TableColumn of that symbol

namespace FastIndicators {
using TickFn = void (*)(void *);

struct alignas(64) IndicatorEntry {
  int64_t checked = -1;        // 8 bytes
  void *ptr;                   // 8 bytes
  TickFn fn;                   // 8 bytes
  int64_t indicatorIndex = -1; // 8 bytes
  char padding[64 - 32];       // 32 bytes
};



}; // namespace FastIndicators
