#include "logger.h"
#include "parsing.h"
#include "utils.h"
#include <cstring>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

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

void *Bstring(aint *args); // TODO: should this be Lstring?
void *Barray(aint *args, aint bn);
void *Bsexp(aint *args, aint bn);
void *Bclosure(aint *args, aint bn);
void *Belem(void *p, aint i);
void *Bsta(void *x, aint i, void *v);
aint Btag(void *d, aint t, aint n);
void *Lstring(aint *args);
aint Llength(void *p);
aint LtagHash(char *s);

void __init();
void __shutdown();

extern size_t __gc_stack_top, __gc_stack_bottom;
}
// GC bounds for globals
size_t __start_custom_data, __stop_custom_data;

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
  struct Frame {
    int closure_values;
    int args;
    int locals;
  };
  std::vector<Frame> frames;
  void *frame_stack[MAX_FRAME_STACK_SIZE] = {0};
  void **fp = nullptr;

public:
  explicit Interpreter(BytecodeFile &bc) : bc(bc) {
    // Set globals for GC once
    globals.resize(bc.get_global_area_size(), nullptr);
    __start_custom_data = reinterpret_cast<size_t>(globals.data());
    __stop_custom_data =
        reinterpret_cast<size_t>(globals.data() + globals.size());
  }

  void interpret() {
    // Reset GC
    __gc_stack_top = reinterpret_cast<size_t>(&operands);
    __gc_stack_bottom = reinterpret_cast<size_t>(&operands + MAX_OPERANDS);
    __init();

    // Reset pointers
    uint8_t *bytecode_start = bc.get_bytecode();
    ip = bytecode_start;
    sp = operands;
    fp = frame_stack;
    frames.clear();

    // Partially setup the `main` frame: it has 2 arguments, the locals will be
    // set by BEGIN opcode
    frames.push_back({0, 2, 0});

    // as a return address for `main` just set bytecode_start, there will not be
    // a inifinite loop, because when `frames` becomes empty, then interpreter
    // will stop
    push_frame(reinterpret_cast<void *>(bytecode_start));

    // `main` always has 2 arguments, so we push them on frame stack
    push_frame(reinterpret_cast<void *>(BOX(0)));
    push_frame(reinterpret_cast<void *>(BOX(0)));

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
      case BEGIN:
      case BEGIN_WITH_CLOSURE: {
        int32_t args = ip_int32();
        int32_t locals = ip_int32();
        log() << "BEGIN " << args << " " << locals << std::endl;

        check_frames_not_empty();
        // Fully initialize the frame
        frames.back().locals = locals;

        // Note: return address, closure values, and args are stored by the
        // CALL/CALLC opcodes

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
        // create partially initialized frame
        frames.push_back({0, args, 0});
        // push return address
        push_frame(reinterpret_cast<void *>(ip)); // the next instruction
        // push args
        for (int i = 0; i < args; ++i) {
          void *arg = pop();
          push_frame(arg);
        }
        // go to callee
        set_ip(bytecode_start + callee_offset);
        break;
      }
      case CALLC: {
        // Format: n (int32) - number of arguments
        int32_t args_count = ip_int32();

        // Pop n arguments from stack
        std::vector<void *> args(args_count);
        for (int32_t i = args_count - 1; i >= 0; --i) {
          args[i] = pop();
        }

        // Pop closure from stack
        void *closure_ptr = pop();

        // Extract function offset and closure data from closure object
        // Closure structure: [function_offset, captured_value1, ...]
        data *closure_data = TO_DATA(closure_ptr);
        check(TAG(closure_data->data_header) == CLOSURE_TAG,
              "CALLC: expected closure tag (" + STR_HEX(CLOSURE_TAG, 8) +
                  "), got " + STR_HEX(TAG(closure_data->data_header), 8));

        void **closure_contents = (void **)closure_ptr;
        int64_t function_offset =
            reinterpret_cast<int64_t>(closure_contents[0]);

        // Get captured values (closure_contents[1..n])
        int32_t captured_count = LEN(closure_data->data_header) - 1;
        std::vector<void *> captured(captured_count);
        for (int32_t i = 0; i < captured_count; ++i) {
          captured[i] = closure_contents[i + 1];
        }

        log() << "CALLC " << STR_HEX(function_offset, 8)
              << " args=" << args_count << " captured=" << captured_count
              << std::endl;

        // Partially initialize the frame
        frames.push_back({captured_count, args_count, 0});

        // Push return address
        push_frame(reinterpret_cast<void *>(ip));

        // Push the captured values to frame stack
        for (int32_t i = 0; i < captured_count; ++i) {
          push_frame(captured[i]);
        }

        // Push arguments to frame stack (they will be read by BEGIN)
        for (int32_t i = 0; i < args_count; ++i) {
          push_frame(args[i]);
        }

        // Go to callee
        set_ip(bytecode_start + function_offset);
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
      case CALL_LENGTH: {
        void *arr = pop();
        aint length = Llength(arr);
        log() << "CALL_LENGTH -> " << UNBOX(length) << std::endl;
        push(length);
        break;
      }
      case CALL_STRING: {
        void *value = pop();
        aint args = reinterpret_cast<aint>(value);
        void *lstr = Lstring(&args);
        push(lstr);
        log() << "CALL_STRING -> " << TO_DATA(lstr)->contents << std::endl;
        break;
      }
      case CALL_ARRAY: {
        int32_t n = ip_int32();
        log() << "CALL_ARRAY " << n << std::endl;

        // Pop n values from the operands stack (in reverse order to maintain
        // correct order)
        std::vector<aint> args(n);
        for (int32_t i = n - 1; i >= 0; --i) {
          args[i] = pop_aint();
        }

        void *arr = Barray(args.data(), BOX(n));
        push(arr);
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
        char *cstr = const_cast<char *>(string->c_str());
        aint args = reinterpret_cast<aint>(cstr);
        void *bstr = Bstring(&args);
        push(bstr);
        log() << "STRING " << std::string(TO_DATA(bstr)->contents) << std::endl;
        break;
      }
      case SEXP: {
        int32_t tag_string_id = ip_int32();
        int32_t n = ip_int32();

        const std::string *tag_str = bc.get_string(tag_string_id);
        if (tag_str == nullptr) {
          throw std::runtime_error("Invalid tag string id: " +
                                   std::to_string(tag_string_id));
        }
        log() << "SEXP tag=" << *tag_str << " n=" << n << std::endl;

        // Pop n field values from the operands stack (in reverse order to
        // maintain correct order)
        std::vector<aint> args(n + 1); // +1 for the tag hash at the end
        for (int32_t i = n - 1; i >= 0; --i) {
          args[i] = pop_aint();
        }

        // Convert tag string to hash and add it as the last argument
        args[n] = LtagHash(const_cast<char *>(tag_str->c_str()));

        // Call Bsexp with the arguments (fields + tag hash)
        void *sexp = Bsexp(args.data(), BOX(n + 1));
        push(sexp);
        break;
      }
      case STA: {
        void *value = pop();
        aint index = pop_aint();
        void *arr = pop();
        Bsta(arr, index, value);
        push(value);
        log() << "STA " << UNBOX(index) << std::endl;
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
        void *value = top();
        globals[glob] = reinterpret_cast<void *>(value);
        log() << "ST G(" << glob << ")" << std::endl;
        break;
      }
      case ST_L: {
        int32_t local = ip_int32();
        check_local_index(local);
        void *value = top();
        write_local(local, value);
        log() << "ST L(" << local << ")" << std::endl;
        break;
      }
      case ST_A: {
        int32_t arg = ip_int32();
        check_argument_index(arg);
        void *value = top();
        write_arg(arg, value);
        log() << "ST A(" << arg << ")" << std::endl;
        break;
      }
      case LD_G: {
        int32_t glob = ip_int32();
        check_global_index(glob);
        void *value = globals[glob];
        push(value);
        log() << "LD G(" << glob << ")" << std::endl;
        break;
      }
      case LD_L: {
        int32_t local = ip_int32();
        check_local_index(local);
        void *value = read_local(local);
        push(value);
        log() << "LD L(" << local << ")" << std::endl;
        break;
      }
      case LD_A: {
        int32_t arg = ip_int32();
        check_argument_index(arg);
        void *value = read_arg(arg);
        push(value);
        log() << "LD A(" << arg << ")" << std::endl;
        break;
      }
      case LD_C: {
        int32_t closure_value_idx = ip_int32();
        void *value = read_closure_value(closure_value_idx);
        push(value);
        log() << "LD C(" << closure_value_idx << ")" << std::endl;
        break;
      }
      case DROP: {
        log() << "DROP" << std::endl;
        pop();
        break;
      }
      case DUP: {
        log() << "DUP" << std::endl;
        void *value = top();
        push(value);
        break;
      }
      case ELEM: {
        aint index = pop_aint();
        void *arr = pop();
        void *result = Belem(arr, index);
        push(result);
        log() << "ELEM " << UNBOX(index) << std::endl;
        break;
      }
      case TAG: {
        int32_t tag_string_id = ip_int32();
        int32_t n = ip_int32();

        const std::string *tag_str = bc.get_string(tag_string_id);
        if (tag_str == nullptr) {
          throw std::runtime_error("Invalid tag string id: " +
                                   std::to_string(tag_string_id));
        }

        void *value = pop();
        aint tag_hash = LtagHash(const_cast<char *>(tag_str->c_str()));
        aint result = Btag(value, tag_hash, BOX(n));
        push(result);

        log() << "TAG " << *tag_str << " " << n << " -> " << UNBOX(result)
              << std::endl;
        break;
      }
      case CLOSURE: {
        // Format: function_offset (int32), n (int32), then n designations
        int32_t function_offset = ip_int32();
        int32_t n = ip_int32();

        log() << "CLOSURE " << STR_HEX(function_offset, 8) << " " << n << " ";

        // Prepare arguments for Bclosure: [function_offset, captured_value1,
        // ...]
        std::vector<aint> bclosure_args(n + 1);
        bclosure_args[0] = function_offset;

        // Read n designations and load values onto stack
        for (int32_t i = n; i >= 1; --i) {
          uint8_t designation_type = ip_byte();
          int32_t index = ip_int32();
          void *value = nullptr;

          check_frames_not_empty();
          switch (designation_type) {
          case 0: // Global
            check_global_index(index);
            value = globals[index];
            log() << "G(" << index << ")";
            break;
          case 1: // Local
            check_local_index(index);
            value = read_local(index);
            log() << "L(" << index << ")";
            break;
          case 2: // Argument
            check_argument_index(index);
            value = read_arg(index);
            log() << "A(" << index << ")";
            break;
          case 3: { // Closure (from current closure)
            check_closure_value_index(index);
            value = read_closure_value(index);
            log() << "C(" << index << ")";
            break;
          }
          default:
            fail_with("Invalid designation type of CLOSURE: " +
                      std::to_string(designation_type));
          }
          bclosure_args[i] = reinterpret_cast<aint>(value);
        }

        // Call Bclosure to create the closure
        void *closure = Bclosure(bclosure_args.data(), BOX(n /* +1? */));
        push(closure);

        log() << std::endl;
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
  void *read_closure_value(int index) {
    check_frames_not_empty();
    check_closure_value_index(index);
    const Frame &frame = frames.back();
    return *(fp - frame.closure_values - frame.args - frame.locals +
             index); // [ret] [closure vals] [args] [locals] fp
    //                             ^---- index points somewhere here
  }

  void *read_arg(int index) {
    check_frames_not_empty();
    check_argument_index(index);
    const Frame &frame = frames.back();
    return *(
        fp - frame.args - frame.locals +
        index); // [ret] [closure vals] [args] [locals] fp
                //                         ^---- index points somewhere here
  }

  void *read_local(int index) {
    check_frames_not_empty();
    check_local_index(index);
    const Frame &frame = frames.back();
    return *(fp - frame.locals +
             index); // [ret] [closure vals] [args] [locals] fp
                     //                                ^---- index points
                     //                                somewhere here
  }

  void write_closure_value(int index, void *value) {
    check_frames_not_empty();
    check_closure_value_index(index);
    const Frame &frame = frames.back();
    *(fp - frame.closure_values - frame.args - frame.locals + index) = value;
  }

  void write_arg(int index, void *value) {
    check_frames_not_empty();
    check_argument_index(index);
    const Frame &frame = frames.back();
    *(fp - frame.args - frame.locals + index) = value;
  }

  void write_local(int index, void *value) {
    check_frames_not_empty();
    check_local_index(index);
    const Frame &frame = frames.back();
    *(fp - frame.locals + index) = value;
  }

  void push_frame(void *value) {
    check_frames_overflow();
    *fp++ = value;
  }

  void pop_frame() {
    check_frames_not_empty();
    auto [closure_values, args, locals] = frames.back();
    frames.pop_back();

    check_frames_underflow(closure_values + args + locals + 1);
    fp -= (closure_values + args + locals + 1); // +1 for return address
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
  void fail_with(const std::string &msg) { check(false, msg); }

  void check(bool value, const std::string &message) {
    if (!value) {
      throw InstructionException(message, ip - bc.get_bytecode());
    }
  }

  void check_ip_valid(uint8_t *ip) {
    uint8_t *bytecode_start = bc.get_bytecode();
    check(ip >= bytecode_start && ip < bytecode_start + bc.get_bytecode_size(),
          "Invalid IP");
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
    check(!frames.empty(), "Empty 'frames' stack");
  }

  void check_closure_value_index(int index) {
    check(index >= 0 && index < frames.back().closure_values,
          "Invalid closure value index: " + std::to_string(index));
  }

  void check_argument_index(int index) {
    check(index >= 0 && index < frames.back().args,
          "Invalid argument index: " + std::to_string(index));
  }

  void check_local_index(int index) {
    check(index >= 0 && index < frames.back().locals,
          "Invalid local index: " + std::to_string(index));
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
    for (const auto &entry : bc.get_strings()) {
      auto &index = entry.first;
      auto &string = entry.second;
      log() << "    " << STR_HEX(index, 8) << ": " << string << std::endl;
    }
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
