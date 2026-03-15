cmake_minimum_required(VERSION 3.10)
project(main)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-O3 -march=native")

add_executable(main main.cpp)