#pragma once
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

inline std::string STR_HEX(uint64_t val, int width) {
  std::ostringstream oss;
  oss << "0x" << std::hex << std::setw(width) << std::setfill('0') << (val);
  return oss.str();
}