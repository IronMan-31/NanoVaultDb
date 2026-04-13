cmake_minimum_required(VERSION 3.10)

set(CMAKE_C_COMPILER gcc-13)
set(CMAKE_CXX_COMPILER g++-13)

project(main)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(main 
    main.cpp
    global.cpp
    hft_clean/src/order_book.cpp
    hft_clean/src/exchange_adapter.cpp
    hft_clean/src/market_data_handler.cpp
)

target_include_directories(main PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/hft_clean/include
    ${CMAKE_CURRENT_SOURCE_DIR}/hft_clean/src
)

target_compile_options(main PRIVATE
    -Wall
    -Wextra
    -fdiagnostics-color=always
)

# Debug (ASan)
target_compile_options(main PRIVATE
    $<$<CONFIG:Debug>:-O0 -g -fsanitize=address -fno-omit-frame-pointer>
)

target_link_options(main PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address>
)

target_compile_options(main PRIVATE
    $<$<CONFIG:Release>:-O3 -march=native>
)

target_link_libraries(main PRIVATE uring crypto ssl pthread)