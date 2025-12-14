#include "parsing.h"
#include "utils.h"

BytecodeFile::BytecodeFile(const std::string &file_path)
    : stringtab_size(0), global_area_size(0), public_symbols_number(0),
      bytecode(nullptr), bytecode_size(0) {

  std::ifstream file(file_path, std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open bytecode file: " + file_path);
  }

  // Read header: 3 int32s
  stringtab_size = read_int32(file);
  global_area_size = read_int32(file);
  public_symbols_number = read_int32(file);

  // Read public symbols table: N entries, each 2 int32s (name_index,
  // code_offset)
  for (uint32_t i = 0; i < public_symbols_number; ++i) {
    uint32_t name_index = read_int32(file);
    uint32_t code_offset = read_int32(file);
    public_symbols[name_index] = code_offset;
  }

  // Read string table: stringtab_size bytes
  string_table_buffer.resize(stringtab_size);
  file.read(string_table_buffer.data(), stringtab_size);

  if (file.gcount() != static_cast<std::streamsize>(stringtab_size)) {
    throw std::runtime_error("Failed to read complete string table");
  }

  // Read bytecode: remaining bytes
  file.seekg(0, std::ios::end);
  size_t file_size = file.tellg();
  size_t header_size = 12 + (public_symbols_number * 8) + stringtab_size;
  bytecode_size = file_size - header_size;

  file.seekg(header_size, std::ios::beg);
  bytecode = new uint8_t[bytecode_size];
  file.read(reinterpret_cast<char *>(bytecode), bytecode_size);

  if (file.gcount() != static_cast<std::streamsize>(bytecode_size)) {
    delete[] bytecode;
    bytecode = nullptr;
    throw std::runtime_error("Failed to read complete bytecode");
  }

  file.close();
}

BytecodeFile::~BytecodeFile() { delete[] bytecode; }

BytecodeFile::BytecodeFile(BytecodeFile &&other) noexcept
    : stringtab_size(other.stringtab_size),
      global_area_size(other.global_area_size),
      public_symbols_number(other.public_symbols_number),
      string_table_buffer(std::move(other.string_table_buffer)),
      public_symbols(std::move(other.public_symbols)), bytecode(other.bytecode),
      bytecode_size(other.bytecode_size) {
  other.bytecode = nullptr;
  other.bytecode_size = 0;
}

BytecodeFile &BytecodeFile::operator=(BytecodeFile &&other) noexcept {
  if (this != &other) {
    delete[] bytecode;
    stringtab_size = other.stringtab_size;
    global_area_size = other.global_area_size;
    public_symbols_number = other.public_symbols_number;
    public_symbols = std::move(other.public_symbols);
    string_table_buffer = std::move(other.string_table_buffer);
    bytecode = other.bytecode;
    bytecode_size = other.bytecode_size;
    other.bytecode = nullptr;
    other.bytecode_size = 0;
  }
  return *this;
}

const std::vector<char> &BytecodeFile::get_strings() const {
  return string_table_buffer;
}
const std::map<uint32_t, uint32_t> &BytecodeFile::get_public_symbols() const {
  return public_symbols;
}

uint8_t *BytecodeFile::get_bytecode() const { return bytecode; }
size_t BytecodeFile::get_bytecode_size() const { return bytecode_size; }

// Get string by index
const std::string_view BytecodeFile::get_string(uint32_t index) const {
  if (index >= string_table_buffer.size()) {
    throw std::runtime_error("Invalid string index: " + STR_HEX(index, 8));
  }
  return std::string_view(string_table_buffer.data() + index);
}

// Get code offset for public symbol
uint32_t
BytecodeFile::get_public_symbol_offset(uint32_t public_symbol_index) const {
  if (public_symbol_index >= public_symbols.size()) {
    throw std::runtime_error("Invalid public symbol index: " +
                             STR_HEX(public_symbol_index, 8));
  }
  return public_symbols.at(public_symbol_index);
}