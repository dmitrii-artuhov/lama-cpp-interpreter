#include "parsing.h"
#include <cstring>
#include <iostream>
#include <string>

#ifndef _Noreturn
#define _Noreturn
#endif

extern "C" {
#include "runtime.h"
}

// Define custom data section boundaries for garbage collector
void *__start_custom_data;
void *__stop_custom_data;

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <bytecode_file.bc>" << std::endl;
    return 1;
  }

  try {
    BytecodeFile bc(argv[1]);

    std::cout << "Loaded bytecode file:" << std::endl;
    std::cout << "  String table size: " << bc.get_stringtab_size() << " bytes"
              << std::endl;
    std::cout << "  Global area size: " << bc.get_global_area_size() << " words"
              << std::endl;
    std::cout << "  Public symbols: " << bc.get_public_symbols_number()
              << std::endl;
    std::cout << "  Bytecode size: " << bc.get_bytecode_size() << " bytes"
              << std::endl;
    std::cout << "  Strings loaded: " << bc.get_strings().size() << std::endl;
    std::cout << "  Public symbols loaded: " << bc.get_public_symbols().size()
              << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
