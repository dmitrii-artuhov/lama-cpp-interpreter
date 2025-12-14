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

inline std::string STR_HEX(uint64_t val, int width) {
  std::ostringstream oss;
  oss << "0x" << std::hex << std::setw(width) << std::setfill('0') << (val);
  return oss.str();
}

// Note: causes problems with GC, use with caustion disable when facing problems
#define LAMA_TO_STR(value) lama_value_to_string(reinterpret_cast<void *>(value))

// Helper to read uint32 from file (little-endian)
inline uint32_t read_uint32(std::ifstream &file) {
  uint32_t value;
  file.read(reinterpret_cast<char *>(&value), sizeof(uint32_t));
  if (file.eof() && file.gcount() != sizeof(uint32_t)) {
    throw std::runtime_error("Failed to read 4 bytes from file: unexpected "
                             "end of file: read only " +
                             std::to_string(file.gcount()));
  }
  if (file.fail()) {
    throw std::runtime_error(
        "Failed to read 4 bytes from file: input failure (failbit set): " +
        std::to_string(file.gcount()));
  }
  if (file.bad()) {
    throw std::runtime_error("Failed to read 4 bytes from file: "
                             "irrecoverable stream error (badbit set): " +
                             std::string(std::strerror(errno)));
  }
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

inline void check_unboxed(void *value, const std::string &message) {
  if (!UNBOXED(value)) {
    throw std::runtime_error(message + " must be unboxed");
  }
}

inline void check_boxed(void *value, const std::string &message) {
  if (UNBOXED(value)) {
    throw std::runtime_error(message + " must be boxed");
  }
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

struct FramesMaxSizeException : public InstructionException {
  FramesMaxSizeException(size_t instruction_number)
      : InstructionException("Frames max size exceeded", instruction_number) {}
  using InstructionException::InstructionException;
};

struct ReadArgsMaxSizeException : public InstructionException {
  ReadArgsMaxSizeException(int size, int max_size, size_t instruction_number)
      : InstructionException(
            "Read args max size exceeded: " + std::to_string(size) + " / " +
                std::to_string(max_size),
            instruction_number) {}
};