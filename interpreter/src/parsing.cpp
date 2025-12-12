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
  std::vector<std::pair<uint32_t, uint32_t>> public_symbol_entries;
  for (uint32_t i = 0; i < public_symbols_number; ++i) {
    uint32_t name_index = read_int32(file);
    uint32_t code_offset = read_int32(file);
    public_symbol_entries.push_back({name_index, code_offset});
  }

  // Read string table: stringtab_size bytes
  string_table_buffer.resize(stringtab_size);
  file.read(string_table_buffer.data(), stringtab_size);

  if (file.gcount() != static_cast<std::streamsize>(stringtab_size)) {
    throw std::runtime_error("Failed to read complete string table");
  }

  // Parse strings from string table (null-terminated strings)
  // Store ALL string start positions, including empty strings, to handle
  // any byte offset that might be referenced in the bytecode
  // This matches how byterun.c's get_string works: it returns &string_ptr[pos]
  uint32_t string_index = 0;
  const char *str_ptr = string_table_buffer.data();
  const char *str_end = string_table_buffer.data() + stringtab_size;

  while (str_ptr < str_end && string_index < stringtab_size) {
    if (*str_ptr == '\0') {
      strings[string_index] = std::string("");
      str_ptr++;
      string_index++;
      continue;
    }

    size_t len = std::strlen(str_ptr);
    if (str_ptr + len >= str_end) {
      break;
    }

    strings[string_index] = std::string(str_ptr, len);
    str_ptr += len + 1; // Skip null terminator
    string_index += len + 1;
  }

  // Build public symbols map using parsed strings
  for (const auto &entry : public_symbol_entries) {
    uint32_t name_index = entry.first;
    uint32_t code_offset = entry.second;

    auto it = strings.find(name_index);
    if (it != strings.end()) {
      public_symbols[it->second] = code_offset;
    }
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
      strings(std::move(other.strings)),
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
    strings = std::move(other.strings);
    public_symbols = std::move(other.public_symbols);
    string_table_buffer = std::move(other.string_table_buffer);
    bytecode = other.bytecode;
    bytecode_size = other.bytecode_size;
    other.bytecode = nullptr;
    other.bytecode_size = 0;
  }
  return *this;
}

const std::map<uint32_t, std::string> &BytecodeFile::get_strings() const {
  return strings;
}
const std::map<std::string, uint32_t> &
BytecodeFile::get_public_symbols() const {
  return public_symbols;
}

uint8_t *BytecodeFile::get_bytecode() const { return bytecode; }
size_t BytecodeFile::get_bytecode_size() const { return bytecode_size; }

// Get string by index
const std::string *BytecodeFile::get_string(uint32_t index) const {
  auto it = strings.find(index);
  return (it != strings.end()) ? &it->second : nullptr;
  // if (it != strings.end()) {
  //   return &it->second;
  // }

  // // If not found, try direct byte-offset access (like byterun.c does)
  // // This handles cases where the bytecode references a string offset we
  // didn't
  // // parse (e.g., due to empty strings or padding in the string table)
  // if (index < string_table_buffer.size()) {
  //   const char *str_ptr = string_table_buffer.data() + index;
  //   const char *str_end =
  //       string_table_buffer.data() + string_table_buffer.size();

  //   // Check if there's a valid null-terminated string at this offset
  //   if (str_ptr < str_end && *str_ptr != '\0') {
  //     size_t len = std::strlen(str_ptr);
  //     if (str_ptr + len < str_end) {
  //       // Found a valid string - cache it in the map for future lookups
  //       // (we need to modify the map, so we need a mutable reference or use
  //       a
  //       // different approach) For now, we'll create a temporary string and
  //       // return it, but this won't work with const Let's use a mutable
  //       static
  //       // cache or thread_local, or better: make strings mutable Actually,
  //       the
  //       // simplest fix: ensure we parse ALL strings, including empty ones
  //       But
  //       // for now, let's just return nullptr and fix the parsing
  //       return nullptr;
  //     }
  //   }
  // }

  // return nullptr;
}

// Get code offset for public symbol
uint32_t *BytecodeFile::get_public_symbol_offset(const std::string &name) {
  auto it = public_symbols.find(name);
  return (it != public_symbols.end()) ? &it->second : nullptr;
}

const uint32_t *
BytecodeFile::get_public_symbol_offset(const std::string &name) const {
  auto it = public_symbols.find(name);
  return (it != public_symbols.end()) ? &it->second : nullptr;
}