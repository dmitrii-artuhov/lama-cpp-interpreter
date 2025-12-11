#include "logger.h"
#include "parsing.h"
#include "utils.h"
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>

#ifndef _Noreturn
#define _Noreturn
#endif

extern "C" {
#include "runtime.h"

extern aint Lread();
extern void Lwrite(aint value);
}

// Define custom data section boundaries for garbage collector
void *__start_custom_data;
void *__stop_custom_data;

#define HEX_FMT(val, width)                                                    \
  "0x" << std::hex << std::setw(width) << std::setfill('0') << (val)

#define MAX_OPERANDS 1024

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

class Interpreter {
private:
  BytecodeFile &bc;
  unsigned char operands[MAX_OPERANDS] = {0};
  unsigned char *sp = nullptr;
  const uint8_t *ip = nullptr;

public:
  explicit Interpreter(BytecodeFile &bc) : bc(bc) {}

  void interpret() {
    const uint8_t *bytecode_start = bc.get_bytecode();
    ip = bytecode_start;
    sp = operands;
    do {
      log() << HEX_FMT(ip - bytecode_start, 8) << ": ";
      uint8_t opcode = *ip++;
      // uint8_t h = (opcode & 0xF0) >> 4;
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
        log() << "BINOP " << ops[l - 1] << std::endl;
        break;
      }
      case CONST: {
        int32_t value = ip_int32();
        log() << "CONST " << value << std::endl;
        push(value);
        break;
      }
      case BEGIN_NO_CLOSURE:
      case BEGIN_WITH_CLOSURE: {
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
      case ST_L:
      case ST_A:
      case ST_C: {
        int32_t glob = ip_int32();
        aint value = top_aint();
        log() << "ST " << glob << " " << UNBOX(value) << std::endl;
        // TODO: bc.set_global(glob, *reinterpret_cast<ssize_t *>(value));
        break;
      }
      case LD_G:
      case LD_L:
      case LD_A:
      case LD_C: {
        int32_t glob = ip_int32();
        log() << "LD " << glob << std::endl;
        // TODO: bc.get_global(glob); and store to stack
        push(-1);
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
        std::ostringstream oss;
        oss << "Invalid opcode: " << HEX_FMT(static_cast<unsigned>(opcode), 2);
        throw std::runtime_error(oss.str());
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
    check_stack_overflow(sizeof(aint));
    *(aint *)sp = value;
    sp += sizeof(aint);
  }

  // TODO:
  // void push(void *value) {
  //   check_stack_overflow(sp, sizeof(void *));
  //   *(void **)sp = value;
  //   sp += sizeof(void *);
  // }

  void *pop() {
    check_stack_underflow(sizeof(void *));
    sp -= sizeof(void *);
    return *(void **)sp;
  }

  aint pop_aint() {
    void *value = pop();
    return reinterpret_cast<aint>(value);
  }

  void *top() {
    check_stack_underflow(sizeof(void *));
    return *(void **)(sp - sizeof(void *));
  }

  aint top_aint() { return reinterpret_cast<aint>(top()); }

  aint read_value() { return Lread(); }

  void write_value(aint value) { Lwrite(value); }

  void check_stack_overflow(size_t bytes) {
    if (sp + bytes > operands + MAX_OPERANDS) {
      throw StackOverflowException("push");
    }
  }

  void check_stack_underflow(size_t bytes) {
    if (sp - bytes < operands) {
      // TODO: add macros for extracting current ip (printing the
      // instruction name)
      throw StackUnderflowException("pop");
    }
  }
};

int main(int argc, char *argv[]) {
  // TODO: add logger file name
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
