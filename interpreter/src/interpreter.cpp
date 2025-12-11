#include "logger.h"
#include "parsing.h"
#include "utils.h"
#include <cstring>
#include <iostream>
#include <string>
#include <unistd.h>

extern "C" {
#ifndef _Noreturn
#define _Noreturn
#endif

#include "runtime.h"

aint Lread();
void Lwrite(aint value);
aint Ls__Infix_43(void *p, void *q);   // +
aint Ls__Infix_45(void *p, void *q);   // -
aint Ls__Infix_42(void *p, void *q);   // *
aint Ls__Infix_47(void *p, void *q);   // /
aint Ls__Infix_37(void *p, void *q);   // %
aint Ls__Infix_60(void *p, void *q);   // <
aint Ls__Infix_6061(void *p, void *q); // <=
aint Ls__Infix_62(void *p, void *q);   // >
aint Ls__Infix_6261(void *p, void *q); // >=
aint Ls__Infix_6161(void *p, void *q); // ==
aint Ls__Infix_3361(void *p, void *q); // !=
aint Ls__Infix_3838(void *p, void *q); // &&
aint Ls__Infix_3333(void *p, void *q); // !!
}

// Define custom data section boundaries for garbage collector
void *__start_custom_data;
void *__stop_custom_data;

// #define MAX_OPERANDS 1024

enum Opcode : uint8_t {
  BINOP_PLUS = 0x01,
  BINOP_MINUS = 0x02,
  BINOP_MULTIPLY = 0x03,
  BINOP_DIVIDE = 0x04,
  BINOP_MODULO = 0x05,
  BINOP_LT = 0x06,
  BINOP_LTE = 0x07,
  BINOP_GT = 0x08,
  BINOP_GTE = 0x09,
  BINOP_EQ = 0x0a,
  BINOP_NE = 0x0b,
  BINOP_AND = 0x0c,
  BINOP_OR = 0x0d,

  CONST = 0x10,
  STRING = 0x11,
  SEXP = 0x12,
  STI = 0x13,
  STA = 0x14,
  // TODO: labels?
  JMP = 0x15,
  END = 0x16,
  RET = 0x17,
  DROP = 0x18,
  DUP = 0x19,
  SWAP = 0x1a,
  ELEM = 0x1b,

  LD_G = 0x20,
  LD_L = 0x21,
  LD_A = 0x22,
  LD_C = 0x23,

  LDA_G = 0x30,
  LDA_L = 0x31,
  LDA_A = 0x32,
  LDA_C = 0x33,

  ST_G = 0x40,
  ST_L = 0x41,
  ST_A = 0x42,
  ST_C = 0x43,

  CJMP_Z = 0x50,
  CJMP_NZ = 0x51,
  CALL_READ = 0x70,
  CALL_WRITE = 0x71,
  CALL_LENGTH = 0x72,
  CALL_STRING = 0x73,
  CALL_ARRAY = 0x74,

  BEGIN_NO_CLOSURE = 0x52,
  BEGIN_WITH_CLOSURE = 0x53,

  CLOSURE = 0x54,
  CALLC = 0x55,
  CALL = 0x56,
  TAG = 0x57,
  ARRAY = 0x58,
  FAIL = 0x59,
  LINE = 0x5a,
  PATT = 0x60,
  // TODO: extern, public, import
};

const char *ops[] = {
    "+", "-", "*", "/", "%", "<", "<=", ">", ">=", "==", "!=", "&&", "!!"};

using binop_fun_ptr = aint (*)(void *, void *);

binop_fun_ptr binop_functions[] = {
    Ls__Infix_43,   // +
    Ls__Infix_45,   // -
    Ls__Infix_42,   // *
    Ls__Infix_47,   // /
    Ls__Infix_37,   // %
    Ls__Infix_60,   // <
    Ls__Infix_6061, // <=
    Ls__Infix_62,   // >
    Ls__Infix_6261, // >=
    Ls__Infix_6161, // ==
    Ls__Infix_3361, // !=
    Ls__Infix_3838, // &&
    Ls__Infix_3333  // !!
};

class Interpreter {
private:
  BytecodeFile &bc;
  std::vector<void *> globals;
  std::vector<void *> operands;
  // unsigned char operands[MAX_OPERANDS] = {0};
  // void **sp = nullptr;
  const uint8_t *ip = nullptr;

public:
  explicit Interpreter(BytecodeFile &bc) : bc(bc) {
    globals.resize(bc.get_global_area_size(), nullptr);
  }

  void interpret() {
    const uint8_t *bytecode_start = bc.get_bytecode();
    ip = bytecode_start;
    do {
      log() << HEX_FMT(ip - bytecode_start, 8) << ": ";
      uint8_t opcode = *ip++;
      uint8_t l = opcode & 0x0F;

      switch (opcode) {
      case END: {
        log() << "END" << std::endl;
        return;
      }
      case BINOP_PLUS:
      case BINOP_MINUS:
      case BINOP_MULTIPLY:
      case BINOP_DIVIDE:
      case BINOP_MODULO:
      case BINOP_LT:
      case BINOP_LTE:
      case BINOP_GT:
      case BINOP_GTE:
      case BINOP_EQ:
      case BINOP_NE:
      case BINOP_AND:
      case BINOP_OR: {
        void *rhs = pop();
        void *lhs = pop();
        aint result = binop_functions[l - 1](rhs, lhs);
        push(result);
        log() << "BINOP " << UNBOX(reinterpret_cast<aint>(lhs)) << " "
              << ops[l - 1] << " " << UNBOX(reinterpret_cast<aint>(rhs))
              << " = " << UNBOX(result) << std::endl;
        break;
      }
      case CONST: {
        int32_t value = ip_int32();
        log() << "CONST " << value << std::endl;
        push(BOX(value));
        break;
      }
      case BEGIN_NO_CLOSURE:
        // case BEGIN_WITH_CLOSURE:
        {
          int32_t args = ip_int32();
          int32_t locals = ip_int32();
          log() << "BEGIN " << args << " " << locals << std::endl;
          break;
        }
      case CALL_READ: {
        aint value = read_value();
        log() << "CALL_READ -> " << UNBOX(value) << std::endl;
        push(value);
        break;
      }
      case CALL_WRITE: {
        aint value = pop_aint();
        log() << "CALL_WRITE -> " << UNBOX(value) << std::endl;
        write_value(value);
        break;
      }
      case ST_G:
        // case ST_L:
        // case ST_A:
        // case ST_C:
        {
          int32_t glob = ip_int32();
          check_global_index(glob);
          aint value = top_aint();
          globals[glob] = reinterpret_cast<void *>(value);
          log() << "ST G(" << glob << ") " << UNBOX(value) << std::endl;
          break;
        }
      case LD_G:
        // case LD_L:
        // case LD_A:
        // case LD_C:
        {
          int32_t glob = ip_int32();
          check_global_index(glob);
          aint value = reinterpret_cast<aint>(globals[glob]);
          push(value);
          log() << "LD G(" << glob << ") " << UNBOX(value) << std::endl;
          break;
        }
      case DROP: {
        log() << "DROP" << std::endl;
        pop();
        break;
      }
      case LINE: {
        log() << "LINE " << ip_int32() << std::endl;
        break;
      }
      default: {
        throw std::runtime_error("Invalid opcode: " +
                                 STR_HEX(static_cast<unsigned>(opcode), 2));
      }
      }
    } while (1);
  }

private:
  uint8_t ip_byte() { return *ip++; }

  int32_t ip_int32() {
    ip += sizeof(int32_t);
    return *(int32_t *)(ip - sizeof(int32_t));
  }

  const char *ip_string() { return bc.get_string(ip_int32())->c_str(); }

  void push(aint value) {
    check_stack_overflow();
    operands.push_back(reinterpret_cast<void *>(value));
  }

  void push(void *value) {
    check_stack_overflow();
    operands.push_back(value);
  }

  void *pop() {
    check_stack_underflow();
    void *value = operands.back();
    operands.pop_back();
    return value;
  }

  aint pop_aint() {
    void *value = pop();
    return reinterpret_cast<aint>(value);
  }

  void *top() {
    check_stack_underflow();
    return operands.back();
  }

  aint top_aint() { return reinterpret_cast<aint>(top()); }

  aint read_value() { return Lread(); }

  void write_value(aint value) { Lwrite(value); }

  void check_stack_overflow() { /* always passes */ }

  void check_stack_underflow() {
    if (operands.empty()) {
      throw StackUnderflowException(ip - bc.get_bytecode());
    }
  }

  void check_global_index(int32_t index) {
    if (index < 0 || static_cast<size_t>(index) >= globals.size()) {
      throw GlobalIndexOutOfBoundsException(index, globals.size(),
                                            ip - bc.get_bytecode());
    }
  }
};

int main(int argc, char *argv[]) {
  // TODO: add logger file name to arguments
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <bytecode_file.bc>" << std::endl;
    return 1;
  }
  init_logger("interpreter.log");

  try {
    BytecodeFile bc(argv[1]);
    Interpreter interpreter(bc);

    log() << "Loaded bytecode file:" << std::endl;
    log() << "  String table size: " << bc.get_stringtab_size() << " bytes"
          << std::endl;
    log() << "  Global area size: " << bc.get_global_area_size() << " words"
          << std::endl;
    log() << "  Strings loaded: " << bc.get_strings().size() << std::endl;
    log() << "  Public symbols: " << bc.get_public_symbols_number()
          << std::endl;
    for (const auto &[name, offset] : bc.get_public_symbols()) {
      log() << "    " << HEX_FMT(offset, 8) << ": " << name << std::endl;
    }
    log() << "  Bytecode size: " << bc.get_bytecode_size() << " bytes"
          << std::endl;

    log() << "Interpreting bytecode:" << std::endl;
    interpreter.interpret();
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
