#pragma once
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

class BytecodeFile {
private:
  // File format data
  uint32_t stringtab_size;
  uint32_t global_area_size;
  uint32_t public_symbols_number;

  // Raw string table buffer for direct byte-offset access
  std::vector<char> string_table_buffer;

  // Public symbols: maps symbol name index in string_table_buffer to code
  // offset
  std::map<uint32_t, uint32_t> public_symbols;

  // Bytecode data (raw pointer, deleted in destructor)
  const uint8_t *bytecode;
  size_t bytecode_size;

public:
  explicit BytecodeFile(const std::string &file_path);
  ~BytecodeFile();
  BytecodeFile(const BytecodeFile &) = delete;
  BytecodeFile &operator=(const BytecodeFile &) = delete;
  BytecodeFile(BytecodeFile &&other) noexcept;
  BytecodeFile &operator=(BytecodeFile &&other) noexcept;

  // Getters
  // TODO: remove these as they seem to be redundant because of the containers
  uint32_t get_stringtab_size() const { return stringtab_size; }
  uint32_t get_global_area_size() const { return global_area_size; }
  uint32_t get_public_symbols_number() const { return public_symbols_number; }

  const std::vector<char> &get_strings() const;
  const std::map<uint32_t, uint32_t> &get_public_symbols() const;

  const uint8_t *get_bytecode() const;
  size_t get_bytecode_size() const;

  const std::string_view get_string(uint32_t index) const;
  uint32_t get_public_symbol_offset(uint32_t public_symbol_index) const;
};