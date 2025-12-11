#pragma once
#include <cstdint>
#include <cstring>
#include <exception>
#include <fstream>
#include <stdexcept>

// Helper to read int32 from buffer (little-endian)
// Uses reinterpret_cast to read directly from byte buffer
inline uint32_t read_int32(const uint8_t *buf) {
  // Align the buffer pointer and use reinterpret_cast to read as uint32_t
  // Bytecode format is little-endian (native endian on x86/x64)
  return *reinterpret_cast<const uint32_t *>(buf);
}

// Helper to read int32 from file (little-endian)
inline uint32_t read_int32(std::ifstream &file) {
  uint32_t value;
  file.read(reinterpret_cast<char *>(&value), sizeof(uint32_t));
  return value;
}

struct StackUnderflowException : public std::runtime_error {
  StackUnderflowException(const std::string &msg)
      : std::runtime_error("Stack underflow: " + msg) {}
};

struct StackOverflowException : public std::runtime_error {
  StackOverflowException(const std::string &msg)
      : std::runtime_error("Stack overflow: " + msg) {}
};
