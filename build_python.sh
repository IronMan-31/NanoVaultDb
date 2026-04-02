#!/usr/bin/env bash
g++-13 -O3 -Wall -shared -std=c++20 -fPIC py_bindings.cpp -I/usr/include/python3.10 -o nanodb$(python3-config --extension-suffix) -luring -lcrypto -lssl
