#!/usr/bin/env bash

set -e

BUILD_DIR="build"

# Force colored diagnostics for gcc/clang
export CXXFLAGS="-fdiagnostics-color=always"

echo -e "\033[1;34m[INFO]\033[0m Configuring project..."

cmake -S . -B $BUILD_DIR -G Ninja

echo -e "\033[1;34m[INFO]\033[0m Building project using all CPU cores..."

cmake --build $BUILD_DIR -- -j$(nproc)

echo -e "\033[1;32m[INFO]\033[0m Running program..."

./$BUILD_DIR/main
