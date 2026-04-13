#!/bin/bash

# Configuration
NUM_PACKETS=1000000
PORT=9090
COMPILER=g++-13

echo "Compiling benchmark suite..."
$COMPILER -O3 -march=native -std=c++20 benchmark_sender.cpp -o benchmark_sender -pthread
$COMPILER -O3 -march=native -std=c++20 benchmark_receiver.cpp \
    hft_clean/src/order_book.cpp \
    hft_clean/src/exchange_adapter.cpp \
    hft_clean/src/market_data_handler.cpp \
    -o benchmark_receiver -pthread -luring -lcrypto -lssl -I. -I./hft_clean/include

if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi

echo "Starting receiver in background..."
./benchmark_receiver &
RECEIVER_PID=$!

# Give it a second to start
sleep 2

echo "Starting sender..."
./benchmark_sender 127.0.0.1 $PORT $NUM_PACKETS

# Wait for receiver to process all packets (rough estimate)
echo "Waiting for receiver to finish processing..."
sleep 5

# Kill the receiver
kill $RECEIVER_PID

echo -e "\nBenchmark complete."
