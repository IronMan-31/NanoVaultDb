cmake_minimum_required(VERSION 3.18)
project(hft_orderbook CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# SIMD auto-detection (Delegated to -march=native)
# ---------------------------------------------------------------------------
set(SIMD_FLAGS "")
message(STATUS "[SIMD] Relying on -march=native for SIMD feature detection")

# ---------------------------------------------------------------------------
# Compile options applied to all targets
# ---------------------------------------------------------------------------
set(HFT_FLAGS
    -O3
    -march=native
    -mtune=native
    -funroll-loops
    -fomit-frame-pointer
    -Wall
    -Wextra
    ${SIMD_FLAGS}
)

# ---------------------------------------------------------------------------
# Core static library
# ---------------------------------------------------------------------------
add_library(hft_core STATIC
    src/order_book.cpp
    src/exchange_adapter.cpp
    src/market_data_handler.cpp
    ../global.cpp
)
target_include_directories(hft_core PUBLIC include)
target_compile_options(hft_core PRIVATE ${HFT_FLAGS})

# ---------------------------------------------------------------------------
# Test + benchmark executable
# ---------------------------------------------------------------------------
add_executable(run_tests tests/test_order_book.cpp)
target_link_libraries(run_tests PRIVATE hft_core uring)
target_compile_options(run_tests PRIVATE ${HFT_FLAGS})

add_executable(test_binance_live tests/test_binance_live.cpp)
target_link_libraries(test_binance_live PRIVATE hft_core ssl crypto pthread uring)
target_compile_options(test_binance_live PRIVATE ${HFT_FLAGS})

add_executable(test_multi_symbol tests/test_multi_symbol.cpp)
target_link_libraries(test_multi_symbol PRIVATE hft_core ssl crypto pthread uring)
target_compile_options(test_multi_symbol PRIVATE ${HFT_FLAGS})

add_executable(test_indicators tests/test_indicators.cpp)
target_link_libraries(test_indicators PRIVATE hft_core ssl crypto pthread uring)
target_compile_options(test_indicators PRIVATE ${HFT_FLAGS})

add_executable(test_strategy tests/test_strategy.cpp)
target_link_libraries(test_strategy PRIVATE hft_core ssl crypto pthread uring)
target_compile_options(test_strategy PRIVATE ${HFT_FLAGS})

add_executable(test_teach tests/test_teach.cpp)
target_link_libraries(test_teach PRIVATE hft_core ssl crypto pthread uring)
target_compile_options(test_teach PRIVATE ${HFT_FLAGS})

enable_testing()
add_test(NAME order_book_tests COMMAND run_tests)
add_test(NAME indicator_tests COMMAND test_indicators)
add_test(NAME strategy_tests COMMAND test_strategy)
add_test(NAME teach_tests COMMAND test_teach)

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
install(TARGETS hft_core       ARCHIVE DESTINATION lib)
install(DIRECTORY include/      DESTINATION include/hft)
