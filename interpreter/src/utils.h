#pragma once
#include <cstdint>
#include <cstring>
#include <exception>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

extern "C" {
#ifndef _Noreturn
#define _Noreturn
#endif
#include "runtime.h"

void *Lstring(aint *args);
}

#define HEX_FMT(val, width)                                                    \
  "0x" << std::hex << std::setw(width) << std::setfill('0') << (val)

#define STR_HEX(val, width)                                                    \
  ([&] {                                                                       \
    std::ostringstream oss;                                                    \
    oss << HEX_FMT((val), width);                                              \
    return std::string(oss.str());                                             \
  })()

#define LAMA_TO_STR(value) lama_value_to_string(reinterpret_cast<void *>(value))

// Helper to read int32 from buffer (little-endian)
inline uint32_t read_int32(const uint8_t *buf) {
  return *reinterpret_cast<const uint32_t *>(buf);
}

// Helper to read int32 from file (little-endian)
inline uint32_t read_int32(std::ifstream &file) {
  uint32_t value;
  file.read(reinterpret_cast<char *>(&value), sizeof(uint32_t));
  return value;
}

// Convert lama data pointer to string
inline std::string lama_value_to_string(void *value) {
  aint args[] = {reinterpret_cast<aint>(value)};
  void *lama_string = Lstring(args);

  // Extract C string from Lama string
  data *d = TO_DATA(lama_string);
  const char *c_str = reinterpret_cast<const char *>(d->contents);

  return std::string(c_str);
}

// Base exception that stores an instruction number/context
struct InstructionException : public std::runtime_error {
  size_t instruction_number;

  InstructionException(const std::string &msg, size_t instruction_number)
      : std::runtime_error(msg + " at bytecode position " +
                           STR_HEX(instruction_number, 8)),
        instruction_number(instruction_number) {}

  size_t get_instruction_number() const { return instruction_number; }
};

struct GlobalsIndexOutOfBoundsException : public InstructionException {
  int32_t global_index;
  size_t globals_total;

  GlobalsIndexOutOfBoundsException(int32_t global_index, size_t globals_total,
                                   size_t instruction_number)
      : InstructionException(
            "Global index out of bounds: " + std::to_string(global_index) +
                " / " + std::to_string(globals_total),
            instruction_number),
        global_index(global_index), globals_total(globals_total) {}
};

struct StackUnderflowException : public InstructionException {
  StackUnderflowException(size_t instruction_number)
      : InstructionException("Stack underflow", instruction_number) {}
  using InstructionException::InstructionException;
};

struct StackOverflowException : public InstructionException {
  StackOverflowException(size_t instruction_number)
      : InstructionException("Stack overflow", instruction_number) {}
  using InstructionException::InstructionException;
};

struct FramesUnderflowException : public InstructionException {
  FramesUnderflowException(size_t instruction_number)
      : InstructionException("Frames underflow", instruction_number) {}
  using InstructionException::InstructionException;
};

struct FramesOverflowException : public InstructionException {
  FramesOverflowException(size_t instruction_number)
      : InstructionException("Frames overflow", instruction_number) {}
  using InstructionException::InstructionException;
};