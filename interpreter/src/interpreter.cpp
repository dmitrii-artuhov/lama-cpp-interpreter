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

void *Bstring(aint *args);

extern void __init();
extern void __shutdown();

extern size_t __gc_stack_top, __gc_stack_bottom;
}
// GC bounds for globals
// size_t __start_custom_data, __stop_custom_data;

#define MAX_OPERANDS 32768
#define MAX_FRAME_STACK_SIZE 1024

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

  BEGIN = 0x52,
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
  uint8_t *ip = nullptr;
  alignas(16) void *operands[MAX_OPERANDS] = {0};
  void **sp = nullptr;
  std::vector<std::pair<int, int>> frames; // (args, locals) for current calls
  void *frame_stack[MAX_FRAME_STACK_SIZE] = {0};
  void **fp = nullptr;

public:
  explicit Interpreter(BytecodeFile &bc) : bc(bc) {

    globals.resize(bc.get_global_area_size(), nullptr);
    // __start_custom_data = __stop_custom_data =
    //     reinterpret_cast<size_t>(globals.data());
    // __stop_custom_data =
    //     reinterpret_cast<size_t>(globals.data() + globals.size());

    __gc_stack_top = reinterpret_cast<size_t>(&operands);
    __gc_stack_bottom = reinterpret_cast<size_t>(&operands + MAX_OPERANDS);
    __init();
  }

  void interpret() {
    uint8_t *bytecode_start = bc.get_bytecode();
    ip = bytecode_start;
    sp = operands;
    fp = frame_stack;
    frames.clear();

    // `main` always has 2 arguments, so its `BEGIN` expects 2 values on
    // operands stack
    push(BOX(0));
    push(BOX(0));
    // as a return address for `main` just set bytecode_start, there will not be
    // a inifinite loop, because when `frames` becomes empty, then interpreter
    // will stop
    push_frame(reinterpret_cast<void *>(bytecode_start));

    do {
      log() << STR_HEX(ip - bytecode_start, 8) << ": ";
      uint8_t opcode = *ip++;
      uint8_t l = opcode & 0x0F;

      switch (opcode) {
      case END: {
        pop_frame();
        log() << "END" << std::endl;
        if (frames.empty()) {
          __shutdown();
          return; // main finished
        }
        break;
      }
      case BEGIN: {
        int32_t args = ip_int32();
        int32_t locals = ip_int32();
        log() << "BEGIN " << args << " " << locals << std::endl;

        frames.push_back({args, locals});
        // Note: return address is stored by the `CALL` opcode
        // push args (take them from operands)
        for (int i = 0; i < args; ++i) {
          void *arg = pop();
          push_frame(arg);
        }
        // push locals (set to zeros)
        for (int i = 0; i < locals; ++i) {
          push_frame(reinterpret_cast<void *>(BOX(0)));
        }
        break;
      }
      case CALL: {
        int32_t callee_offset = ip_int32();
        // Note: args will be handled by the `BEGIN` opcode anyway, so we ignore
        // them here
        int32_t args = ip_int32();
        log() << "CALL " << callee_offset << " " << args << std::endl;
        // push return address
        push_frame(reinterpret_cast<void *>(ip)); // the next instruction
        // go to callee
        set_ip(bytecode_start + callee_offset);
        break;
      }
      case CALL_READ: {
        aint value = read_value();
        log() << "CALL_READ -> " << UNBOX(value) << std::endl;
        push(value);
        break;
      }
      case CALL_WRITE: {
        aint value = top_aint(); // TODO: why it does not `pop` though?
        log() << "CALL_WRITE -> " << UNBOX(value) << std::endl;
        write_value(value);
        break;
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
        aint result = binop_functions[l - 1](lhs, rhs);
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
      case STRING: {
        int32_t id = ip_int32();
        const std::string *string = bc.get_string(id);
        if (string == nullptr) {
          throw std::runtime_error("Invalid string id: " + std::to_string(id));
        }
        log() << "STRING " << *string << std::endl;
        char *cstr = const_cast<char *>(string->c_str());
        aint args = reinterpret_cast<aint>(cstr);
        void *bstr = Bstring(&args);
        push(bstr);
        break;
      }
      case JMP: {
        int32_t offset = ip_int32();
        log() << "JMP " << offset << std::endl;
        set_ip(bytecode_start + offset);
        break;
      }
      case CJMP_Z:
      case CJMP_NZ: {
        int32_t offset = ip_int32();
        aint value = UNBOX(pop_aint());
        log() << (opcode == CJMP_Z ? "CJMP_Z " : "CJMP_NZ ") << offset << " "
              << value << std::endl;
        if ((opcode == CJMP_Z && value == 0) ||
            (opcode == CJMP_NZ && value != 0)) {
          set_ip(bytecode_start + offset);
        }
        break;
      }
      case ST_G: {
        int32_t glob = ip_int32();
        check_global_index(glob);
        aint value = top_aint();
        globals[glob] = reinterpret_cast<void *>(value);
        log() << "ST G(" << glob << ") " << UNBOX(value) << std::endl;
        break;
      }
      case ST_L: {
        int32_t local = ip_int32();
        check_local_index(local);
        aint value = top_aint();
        write_local(local, reinterpret_cast<void *>(value));
        log() << "ST L(" << local << ") " << UNBOX(value) << std::endl;
        break;
      }
      case ST_A: {
        int32_t arg = ip_int32();
        check_argument_index(arg);
        aint value = top_aint();
        write_arg(arg, reinterpret_cast<void *>(value));
        log() << "ST A(" << arg << ") " << UNBOX(value) << std::endl;
        break;
      }
      case LD_G: {
        int32_t glob = ip_int32();
        check_global_index(glob);
        aint value = reinterpret_cast<aint>(globals[glob]);
        push(value);
        log() << "LD G(" << glob << ") " << UNBOX(value) << std::endl;
        break;
      }
      case LD_L: {
        int32_t local = ip_int32();
        check_local_index(local);
        aint value = reinterpret_cast<aint>(read_local(local));
        push(value);
        log() << "LD L(" << local << ") " << UNBOX(value) << std::endl;
        break;
      }
      case LD_A: {
        int32_t arg = ip_int32();
        check_argument_index(arg);
        aint value = reinterpret_cast<aint>(read_arg(arg));
        push(value);
        log() << "LD A(" << arg << ") " << UNBOX(value) << std::endl;
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
  // instruction pointer operations
  uint8_t ip_byte() { return *ip++; }

  int32_t ip_int32() {
    int32_t value;
    std::memcpy(&value, ip, sizeof(int32_t));
    ip += sizeof(int32_t);
    return value;
  }

  const char *ip_string() { return bc.get_string(ip_int32())->c_str(); }

  void set_ip(uint8_t *new_ip) {
    check_ip_valid(new_ip);
    ip = new_ip;
  }

  // operands stack operations
  void push(aint value) {
    check_stack_overflow();
    *sp++ = reinterpret_cast<void *>(value);
  }

  void push(void *value) {
    check_stack_overflow();
    *sp++ = value;
  }

  void *pop() {
    check_stack_underflow();
    void *value = *(--sp);
    return value;
  }

  aint pop_aint() {
    void *value = pop();
    return reinterpret_cast<aint>(value);
  }

  void *top() {
    check_stack_underflow();
    return *(sp - 1);
  }

  aint top_aint() { return reinterpret_cast<aint>(top()); }

  // frame stack operations
  void *read_arg(int index) {
    check_frames_not_empty();
    check_argument_index(index);
    auto [args, locals] = frames.back();
    return *(fp - locals - args +
             index); // [ret] [args] [locals] fp
                     //         ^---- index points somewhere here
  }

  void *read_local(int index) {
    check_frames_not_empty();
    check_local_index(index);
    auto [args, locals] = frames.back();
    return *(fp - locals +
             index); // [ret] [args] [locals] fp
                     //                 ^---- index points somewhere here
  }

  void write_arg(int index, void *value) {
    check_frames_not_empty();
    check_argument_index(index);
    auto [args, locals] = frames.back();
    *(fp - locals - args + index) = value;
  }

  void write_local(int index, void *value) {
    check_frames_not_empty();
    check_local_index(index);
    auto [args, locals] = frames.back();
    *(fp - locals + index) = value;
  }

  void push_frame(void *value) {
    check_frames_overflow();
    *fp++ = value;
  }

  void pop_frame() {
    check_frames_not_empty();
    auto [args, locals] = frames.back();
    frames.pop_back();

    check_frames_underflow(args + locals + 1);
    fp -= (args + locals + 1); // +1 for return address
    set_ip(reinterpret_cast<uint8_t *>(
        *fp)); // `fp` now dereferences to return address
  }

  // IO
  aint read_value() {
    std::cout << " ";
    return Lread();
  }

  void write_value(aint value) { Lwrite(value); }

  // checks
  void check_ip_valid(uint8_t *ip) {
    uint8_t *bytecode_start = bc.get_bytecode();
    if (ip < bytecode_start || ip >= bytecode_start + bc.get_bytecode_size()) {
      throw InstructionException("Invalid IP", ip - bytecode_start);
    }
  }

  void check_stack_overflow() {
    if (sp >= operands + MAX_OPERANDS) {
      throw StackOverflowException(ip - bc.get_bytecode());
    }
  }

  void check_stack_underflow() {
    if (sp == nullptr || sp <= operands) {
      throw StackUnderflowException(ip - bc.get_bytecode());
    }
  }

  void check_frames_overflow() {
    if (fp >= frame_stack + MAX_FRAME_STACK_SIZE) {
      throw FramesOverflowException(ip - bc.get_bytecode());
    }
  }

  void check_frames_underflow(int substract) {
    if (fp == nullptr || fp - frame_stack < substract) {
      throw FramesUnderflowException(ip - bc.get_bytecode());
    }
  }

  void check_frames_not_empty() {
    if (frames.empty()) {
      throw InstructionException("Empty 'frames' stack",
                                 ip - bc.get_bytecode());
    }
  }

  void check_argument_index(int index) {
    if (index < 0 || index >= frames.back().first) {
      throw InstructionException("Invalid argument index",
                                 ip - bc.get_bytecode());
    }
  }

  void check_local_index(int index) {
    if (index < 0 || index >= frames.back().second) {
      throw InstructionException("Invalid local index", ip - bc.get_bytecode());
    }
  }

  void check_global_index(int32_t index) {
    if (index < 0 || static_cast<size_t>(index) >= globals.size()) {
      throw GlobalsIndexOutOfBoundsException(index, globals.size(),
                                             ip - bc.get_bytecode());
    }
  }
};

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <bytecode_file.bc> [log_file]"
              << std::endl;
    return 1;
  }

  // Use provided log file path or default to "interpreter.log"
  std::string log_file = (argc >= 3) ? argv[2] : "interpreter.log";
  init_logger(log_file);

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
    for (const auto &symbol : bc.get_public_symbols()) {
      auto &name = symbol.first;
      auto offset = symbol.second;
      log() << "    " << STR_HEX(offset, 8) << ": " << name << std::endl;
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
