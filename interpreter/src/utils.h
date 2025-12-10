#pragma once
#include <cstdint>
#include <fstream>

// Helper to read int32 from buffer (little-endian)
static uint32_t read_int32(const uint8_t *buf) {
  return static_cast<uint32_t>(buf[0]) | (static_cast<uint32_t>(buf[1]) << 8) |
         (static_cast<uint32_t>(buf[2]) << 16) |
         (static_cast<uint32_t>(buf[3]) << 24);
}

// Helper to read int32 from file
static uint32_t read_int32(std::ifstream &file) {
  uint8_t buf[4];
  file.read(reinterpret_cast<char *>(buf), 4);
  return read_int32(buf);
}