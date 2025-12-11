#pragma once
#include <cstdint>
#include <cstring>
#include <exception>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

#define HEX_FMT(val, width)                                                    \
  "0x" << std::hex << std::setw(width) << std::setfill('0') << (val)

#define STR_HEX(val, width)                                                    \
  ([&] {                                                                       \
    std::ostringstream oss;                                                    \
    oss << HEX_FMT((val), width);                                              \
    return std::string(oss.str());                                             \
  })()

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

// Base exception that stores an instruction number/context
struct InstructionException : public std::runtime_error {
  size_t instruction_number;

  InstructionException(const std::string &msg, size_t instruction_number)
      : std::runtime_error(msg + " at bytecode position " +
                           STR_HEX(instruction_number, 8)),
        instruction_number(instruction_number) {}

  size_t get_instruction_number() const { return instruction_number; }
};

// Exception for globals index out of bounds, inherits from InstructionException
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

// Stack underflow exception, inherits from InstructionException
struct StackUnderflowException : public InstructionException {
  StackUnderflowException(size_t instruction_number)
      : InstructionException("Stack underflow", instruction_number) {}
  using InstructionException::InstructionException;
};

// Stack overflow exception, inherits from InstructionException
struct StackOverflowException : public InstructionException {
  StackOverflowException(size_t instruction_number)
      : InstructionException("Stack overflow", instruction_number) {}
  using InstructionException::InstructionException;
};