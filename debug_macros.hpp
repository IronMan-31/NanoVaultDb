#pragma once
#include <iostream>
#include <string>
#include <string_view>

// Forward declaration of the utility function used by the macro
namespace MyUtility {
void appendToFile(const std::string_view &filename,
                  const std::string_view &msg);
}

// Global Debug Toggle
#ifndef ENABLE_DEBUG
#define ENABLE_DEBUG 0
#endif

#if ENABLE_DEBUG
#define DEBUG_LOG(x) std::cout << x << std::endl
#define HFT_DEBUG_FILE(file, msg) MyUtility::appendToFile(file, msg)
#else
#define DEBUG_LOG(x) (void)0
#define HFT_DEBUG_FILE(file, msg) (void)0
#endif
