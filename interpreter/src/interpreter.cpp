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

// Pattern matching funcions
aint Barray_patt(void *x, aint n);   // array with length n
aint Bstring_patt(void *x, void *y); // =str
aint Bstring_tag_patt(void *x);      // #string
aint Barray_tag_patt(void *x);       // #array
aint Bsexp_tag_patt(void *x);        // #sexp
aint Bboxed_patt(void *x);           // #ref
aint Bunboxed_patt(void *x);         // #val
aint Bclosure_tag_patt(void *x);     // #fun

void __init();
void __shutdown();

extern size_t __gc_stack_top, __gc_stack_bottom;
}
// GC bounds for globals
size_t __start_custom_data, __stop_custom_data;

// binop functions
aint binop_plus(void *p, void *q) { // +
  check_unboxed(p, "lhs");
  check_unboxed(q, "rhs");
  return BOX(UNBOX(p) + UNBOX(q));
}
aint binop_minus(void *p, void *q) { // -
  if (UNBOXED(p)) {
    check_unboxed(q, "rhs");
    return BOX(UNBOX(p) - UNBOX(q));
  }
  check_boxed(q, "rhs");
  return BOX(reinterpret_cast<char *>(p) - reinterpret_cast<char *>(q));
}
aint binop_multiply(void *p, void *q) { // *
  check_unboxed(p, "lhs");
  check_unboxed(q, "rhs");
  return BOX(UNBOX(p) * UNBOX(q));
}
aint binop_divide(void *p, void *q) { // /
  check_unboxed(p, "lhs");
  check_unboxed(q, "rhs");
  if (UNBOX(q) == 0) {
    throw std::runtime_error("rhs must be non-zero");
  }
  return BOX(UNBOX(p) / UNBOX(q));
}
aint binop_modulo(void *p, void *q) { // %
  check_unboxed(p, "lhs");
  check_unboxed(q, "rhs");
  if (UNBOX(q) == 0) {
    throw std::runtime_error("rhs must be non-zero");
  }
  return BOX(UNBOX(p) % UNBOX(q));
}
aint binop_lt(void *p, void *q) { // <
  check_unboxed(p, "lhs");
  check_unboxed(q, "rhs");
  return BOX(UNBOX(p) < UNBOX(q));
}
aint binop_lte(void *p, void *q) { // <=
  check_unboxed(p, "lhs");
  check_unboxed(q, "rhs");
  return BOX(UNBOX(p) <= UNBOX(q));
}
aint binop_gt(void *p, void *q) { // >
  check_unboxed(p, "lhs");
  check_unboxed(q, "rhs");
  return BOX(UNBOX(p) > UNBOX(q));
}
aint binop_gte(void *p, void *q) { // >=
  check_unboxed(p, "lhs");
  check_unboxed(q, "rhs");
  return BOX(UNBOX(p) >= UNBOX(q));
}
aint binop_eq(void *p, void *q) { // ==
  return BOX(p == q);
}
aint binop_ne(void *p, void *q) { // !=
  check_unboxed(p, "lhs");
  check_unboxed(q, "rhs");
  return BOX(UNBOX(p) != UNBOX(q));
}
aint binop_and(void *p, void *q) { // &&
  check_unboxed(p, "lhs");
  check_unboxed(q, "rhs");
  return BOX(UNBOX(p) && UNBOX(q));
}
aint binop_or(void *p, void *q) { // !!
  check_unboxed(p, "lhs");
  check_unboxed(q, "rhs");
  return BOX(UNBOX(p) || UNBOX(q));
}

#define MAX_OPERANDS 32768
#define MAX_FRAME_STACK_SIZE 32768
// max call stack depth in Lama
#define MAX_FRAMES 16384

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
  BEGINC = 0x53,

  CLOSURE = 0x54,
  CALLC = 0x55,
  CALL = 0x56,
  TAG = 0x57,
  ARRAY = 0x58,
  FAIL = 0x59,
  LINE = 0x5a,

  PATT_STRCMP = 0x60,
  PATT_STRING = 0x61,
  PATT_ARRAY = 0x62,
  PATT_SEXP = 0x63,
  PATT_REF = 0x64,
  PATT_VAL = 0x65,
  PATT_FUN = 0x66,
  // TODO: extern, public, import
};

const char *ops[] = {
    "+", "-", "*", "/", "%", "<", "<=", ">", ">=", "==", "!=", "&&", "!!"};

using binop_fun_ptr = aint (*)(void *, void *);

binop_fun_ptr binop_functions[] = {
    binop_plus,     // +
    binop_minus,    // -
    binop_multiply, // *
    binop_divide,   // /
    binop_modulo,   // %
    binop_lt,       // <
    binop_lte,      // <=
    binop_gt,       // >
    binop_gte,      // >=
    binop_eq,       // ==
    binop_ne,       // !=
    binop_and,      // &&
    binop_or,       // !!
};

const char *patts[] = {"=str", "#string", "#array", "#sexp",
                       "#ref", "#val",    "#fun"};

using single_arg_patt_fun_ptr = aint (*)(void *);

single_arg_patt_fun_ptr patt_functions[] = {
    nullptr,          // =str (Bstring_patt), should be called manually
    Bstring_tag_patt, // #string
    Barray_tag_patt,  // #array
    Bsexp_tag_patt,   // #sexp
    Bboxed_patt,      // #ref
    Bunboxed_patt,    // #val
    Bclosure_tag_patt // #fun
};

class Interpreter {
private:
  BytecodeFile &bc;
  std::vector<void *> globals;
  const uint8_t *ip = nullptr;
  alignas(16) void *memory[MAX_OPERANDS + MAX_FRAME_STACK_SIZE] = {0};
  void **operands = memory;
  void **sp = nullptr;
  struct Frame {
    bool closure; // 0 - regular call, 1 - closure call the pointer to closure
                  // is bottom most element on frame stack for current frame
    uint32_t args;
    uint32_t locals;
  };
  std::vector<Frame> frames;
  void **frame_stack = memory + MAX_OPERANDS;
  void **fp = nullptr;

public:
  explicit Interpreter(BytecodeFile &bc) : bc(bc) {
    // Set globals for GC once
    globals.resize(bc.get_global_area_size(), nullptr);
    __start_custom_data = reinterpret_cast<size_t>(globals.data());
    __stop_custom_data =
        reinterpret_cast<size_t>(globals.data() + globals.size());

    frames.reserve(MAX_FRAMES);
  }

  void interpret() {
    // Reset GC
    __gc_stack_top = reinterpret_cast<size_t>(memory);
    __gc_stack_bottom =
        reinterpret_cast<size_t>(memory + MAX_OPERANDS + MAX_FRAME_STACK_SIZE);
    __init();

    // Reset pointers
    const uint8_t *bytecode_start = bc.get_bytecode();
    ip = bytecode_start;
    sp = operands;
    fp = frame_stack;
    frames.clear();

    // Partially setup the `main` frame: it has 2 arguments, the locals will be
    // set by BEGIN opcode
    check_frames_max_size();
    frames.push_back({false, 2, 0});

    // as a return address for `main` just set bytecode_start, there will not be
    // a inifinite loop, because when `frames` becomes empty, then interpreter
    // will stop
    push_frame(reinterpret_cast<void *>(const_cast<uint8_t *>(bytecode_start)));

    // `main` always has 2 arguments, so we push them on frame stack
    push_frame(reinterpret_cast<void *>(BOX(0)));
    push_frame(reinterpret_cast<void *>(BOX(0)));

    do {
      LOG(log() << STR_HEX(ip - bytecode_start, 8) << ": ");
      uint8_t opcode = *ip++;
      uint8_t l = opcode & 0x0F;

      switch (opcode) {
      case END: {
        pop_frame();
        LOG(log() << "END" << std::endl);
        if (frames.empty()) {
          __shutdown();
          return; // main finished
        }
        break;
      }
      case BEGIN:
      case BEGINC: {
        int32_t args = ip_int32();
        check(args >= 0, "args must be non-negative");
        int32_t locals = ip_int32();
        check(locals >= 0, "locals must be non-negative");
        LOG(log() << (opcode == BEGIN ? "BEGIN " : "BEGINC ") << args << " "
                  << locals << std::endl);

        check_frames_not_empty();
        // Fully initialize the frame
        frames.back().locals = static_cast<uint32_t>(locals);

        // Note: return address, closure_ptr, and args are stored by the
        // CALL/CALLC opcodes

        // push locals (set to zeros)
        for (int i = 0; i < locals; ++i) {
          push_frame(reinterpret_cast<void *>(BOX(0)));
        }

        // Print the arguments currently on the frame stack after pushing locals
        // (in BEGIN/BEGINC)
        PRINT_STACKS({
          uint32_t arg_count = frames.back().args;
          if (arg_count > 0) {
            std::ostringstream ss;
            ss << "\tFRAME_ARGS: size=" << arg_count << ": [";
            void **frame_args_ptr = fp - frames.back().locals - arg_count;
            for (int i = 0; i < arg_count; ++i) {
              if (i != 0)
                ss << ", ";
              ss << UNBOX(frame_args_ptr[i]);
            }
            ss << "]";
            log() << ss.str() << std::endl;
          }
        });

        break;
      }
      case CALL: {
        int32_t callee_offset = ip_int32();
        check(callee_offset >= 0, "callee_offset must be non-negative");
        int32_t args_count = ip_int32();
        check(args_count >= 0, "args_count must be non-negative");
        LOG(log() << "CALL " << STR_HEX(callee_offset, 8) << " " << args_count
                  << std::endl);
        // create partially initialized frame
        check_frames_max_size();
        frames.push_back({false, static_cast<uint32_t>(args_count), 0});
        // push return address
        push_frame(reinterpret_cast<void *>(
            const_cast<uint8_t *>(ip))); // the next instruction
        // push args
        std::vector<void *> args(args_count);
        for (int i = args_count - 1; i >= 0; --i) {
          args[i] = pop();
        }
        for (void *arg : args) {
          push_frame(arg);
        }
        // go to callee
        set_ip(bytecode_start + callee_offset);
        break;
      }
      case CALLC: {
        // Format: n (int32) - number of arguments
        int32_t args_count = ip_int32();
        check(args_count >= 0, "args_count must be non-negative");

        // Pop n arguments from stack
        std::vector<void *> args(args_count);
        for (int32_t i = args_count - 1; i >= 0; --i) {
          args[i] = pop();
        }

        // Pop closure from stack
        void *closure_ptr = pop();

        PRINT_STACKS(
            log() << "CALLC: args=" << args_count << " [";
            for (void *arg : args) { log() << UNBOX(arg) << ", "; } log()
            << "] closure=" << UNBOX(closure_ptr) << std::endl;);
        // Extract function offset and closure data from closure object
        // Closure structure: [function_offset, captured_value1, ...]
        data *closure_data = TO_DATA(closure_ptr);
        check(TAG(closure_data->data_header) == CLOSURE_TAG,
              "CALLC: expected closure tag (" + STR_HEX(CLOSURE_TAG, 8) +
                  "), got " + STR_HEX(TAG(closure_data->data_header), 8));

        void **closure_contents = (void **)closure_ptr;
        int64_t function_offset =
            reinterpret_cast<int64_t>(closure_contents[0]);

        // Get number of captured values (closure_contents[1..n])
        int32_t captured_count = LEN(closure_data->data_header) - 1;
        check(captured_count >= 0, "captured_count must be non-negative");

        LOG(log() << "CALLC " << STR_HEX(function_offset, 8) << " args="
                  << args_count << " captured=" << captured_count << std::endl);

        // Partially initialize the frame
        // Note: closure values are NOT copied to frame stack - they're accessed
        // directly from the closure object via closure_ptr
        // The captured_count is available from the closure object when needed
        check_frames_max_size();
        frames.push_back({true, static_cast<uint32_t>(args_count), 0});
        // Push return address
        push_frame(reinterpret_cast<void *>(const_cast<uint8_t *>(ip)));

        // Push closure pointer
        push_frame(closure_ptr);

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
        LOG(log() << "CALL_READ -> " << UNBOX(value) << std::endl);
        push(value);
        break;
      }
      case CALL_WRITE: {
        aint value = top_aint(); // TODO: why it does not `pop` though?
        LOG(log() << "CALL_WRITE -> " << UNBOX(value) << std::endl);
        write_value(value);
        break;
      }
      case CALL_LENGTH: {
        void *arr = pop();
        aint length = Llength(arr);
        LOG(log() << "CALL_LENGTH -> " << UNBOX(length) << std::endl);
        push(length);
        break;
      }
      case CALL_STRING: {
        void *value = pop();
        aint args = reinterpret_cast<aint>(value);
        void *lstr = Lstring(&args);
        push(lstr);
        LOG(log() << "CALL_STRING -> " << TO_DATA(lstr)->contents << std::endl);
        break;
      }
      case CALL_ARRAY: {
        int32_t n = ip_int32();
        LOG(log() << "CALL_ARRAY " << n << std::endl);

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
        try {
          void *rhs = pop();
          void *lhs = pop();
          aint result = binop_functions[l - 1](lhs, rhs);
          push(result);
          LOG(log() << "BINOP " << UNBOX(reinterpret_cast<aint>(lhs)) << " "
                    << ops[l - 1] << " " << UNBOX(reinterpret_cast<aint>(rhs))
                    << " = " << UNBOX(result) << std::endl);
        } catch (const std::runtime_error &e) {
          LOG(log() << "BINOP " << e.what() << std::endl);
          fail_with("Error in operation '" + std::string(ops[l - 1]) +
                    "': " + e.what());
        }
        break;
      }
      case CONST: {
        int32_t value = ip_int32();
        LOG(log() << "CONST " << value << std::endl);
        push(BOX(value));
        break;
      }
      case STRING: {
        int32_t id = ip_int32();
        char *cstr = const_cast<char *>(bc.get_string(id).data());
        aint args = reinterpret_cast<aint>(cstr);
        void *bstr = Bstring(&args);
        push(bstr);
        LOG(log() << "STRING " << std::string(TO_DATA(bstr)->contents)
                  << std::endl);
        break;
      }
      case SEXP: {
        int32_t tag_string_id = ip_int32();
        int32_t n = ip_int32();

        const std::string_view tag_str = bc.get_string(tag_string_id);
        LOG(log() << "SEXP tag=" << tag_str << " n=" << n << std::endl);

        // Pop n field values from the operands stack (in reverse order to
        // maintain correct order)
        std::vector<aint> args(n + 1); // +1 for the tag hash at the end
        for (int32_t i = n - 1; i >= 0; --i) {
          args[i] = pop_aint();
        }

        // Convert tag string to hash and add it as the last argument
        args[n] = LtagHash(const_cast<char *>(tag_str.data()));

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
        LOG(log() << "STA " << UNBOX(index) << std::endl);
        break;
      }
      case JMP: {
        int32_t offset = ip_int32();
        LOG(log() << "JMP " << STR_HEX(offset, 8) << std::endl);
        set_ip(bytecode_start + offset);
        break;
      }
      case CJMP_Z:
      case CJMP_NZ: {
        int32_t offset = ip_int32();
        aint value = UNBOX(pop_aint());
        LOG(log() << (opcode == CJMP_Z ? "CJMP_Z " : "CJMP_NZ ")
                  << STR_HEX(offset, 8) << " " << value << std::endl);
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
        LOG(log() << "ST G(" << glob << ")" << std::endl);
        break;
      }
      case ST_L: {
        int32_t local = ip_int32();
        void *value = top();
        write_local(local, value);
        LOG(log() << "ST L(" << local << ")" << std::endl);
        break;
      }
      case ST_A: {
        int32_t arg = ip_int32();
        void *value = top();
        write_arg(arg, value);
        LOG(log() << "ST A(" << arg << ")" << std::endl);
        break;
      }
      case ST_C: {
        int32_t closure_value_idx = ip_int32();
        void *value = top();
        write_closure_value(closure_value_idx, value);
        LOG(log() << "ST C(" << closure_value_idx << ")" << std::endl);
        break;
      }
      case LD_G: {
        int32_t glob = ip_int32();
        check_global_index(glob);
        void *value = globals[glob];
        push(value);
        LOG(log() << "LD G(" << glob << ")" << std::endl);
        break;
      }
      case LD_L: {
        int32_t local = ip_int32();
        check_local_index(local);
        void *value = read_local(local);
        push(value);
        LOG(log() << "LD L(" << local << ")" << std::endl);
        break;
      }
      case LD_A: {
        int32_t arg = ip_int32();
        check_argument_index(arg);
        void *value = read_arg(arg);
        push(value);
        LOG(log() << "LD A(" << arg << ")" << std::endl);
        break;
      }
      case LD_C: {
        int32_t closure_value_idx = ip_int32();
        void *value = read_closure_value(closure_value_idx);
        push(value);
        LOG(log() << "LD C(" << closure_value_idx << ")" << std::endl);
        break;
      }
      case DROP: {
        LOG(log() << "DROP" << std::endl);
        pop();
        break;
      }
      case DUP: {
        LOG(log() << "DUP" << std::endl);
        void *value = top();
        push(value);
        break;
      }
      case ELEM: {
        aint index = pop_aint();
        void *arr = pop();
        void *result = Belem(arr, index);
        push(result);
        LOG(log() << "ELEM " << UNBOX(index) << std::endl);
        break;
      }
      case TAG: {
        // The TAG bytecode instruction is used for pattern matching.
        // It checks if the value on top of the stack has a particular tag and
        // arity.
        int32_t tag_string_id = ip_int32();
        int32_t n = ip_int32();

        const std::string_view tag_str = bc.get_string(tag_string_id);

        void *value = pop();
        aint tag_hash = LtagHash(const_cast<char *>(tag_str.data()));
        aint result = Btag(value, tag_hash, BOX(n));
        push(result);

        LOG(log() << "TAG " << tag_str << " " << n << " -> " << UNBOX(result)
                  << std::endl);
        break;
      }
      case ARRAY: {
        int32_t n = ip_int32();

        void *value = pop();
        aint result = Barray_patt(value, BOX(n));
        push(result);

        LOG(log() << "ARRAY " << n << " -> " << UNBOX(result) << std::endl);
        break;
      }
      case CLOSURE: {
        // Format: function_offset (int32), n (int32), then n designations
        int32_t function_offset = ip_int32();
        int32_t n = ip_int32();

        LOG(log() << "CLOSURE " << STR_HEX(function_offset, 8) << " " << n);

        // Prepare arguments for Bclosure: [function_offset, captured_value1,
        // ...]
        std::vector<aint> bclosure_args(n + 1);
        bclosure_args[0] = function_offset;

        // Read n designations in order and store them
        for (int32_t i = 1; i <= n; ++i) {
          uint8_t designation_type = ip_byte();
          int32_t index = ip_int32();
          void *value = nullptr;

          check_frames_not_empty();
          switch (designation_type) {
          case 0: // Global
            check_global_index(index);
            value = globals[index];
            LOG(log() << "G(" << index << ")");
            break;
          case 1: // Local
            check_local_index(index);
            value = read_local(index);
            LOG(log() << "L(" << index << ")");
            break;
          case 2: // Argument
            check_argument_index(index);
            value = read_arg(index);
            LOG(log() << " A(" << index << ")");
            break;
          case 3: { // Closure (from current closure)
            check_closure_value_index(index);
            value = read_closure_value(index);
            LOG(log() << " C(" << index << ")");
            break;
          }
          default:
            fail_with("Invalid designation type of CLOSURE: " +
                      std::to_string(designation_type));
          }
          bclosure_args[i] = reinterpret_cast<aint>(value);
        }

        // Call Bclosure to create the closure
        // Bclosure expects: args[0] = function_offset, args[1..n] = captured
        // values. The second argument is n (number of captured values), not n+1
        // Bclosure allocates n+1 elements internally
        void *closure = Bclosure(bclosure_args.data(), BOX(n));
        push(closure);

        LOG(log() << std::endl);
        break;
      }
      case PATT_STRCMP:
      case PATT_STRING:
      case PATT_ARRAY:
      case PATT_SEXP:
      case PATT_REF:
      case PATT_VAL:
      case PATT_FUN: {
        // PATT opcode is (0x60 + pattern enum value), the second part is stored
        // in `l` variable
        aint result = BOX(0);
        void *value = pop();

        if (l == 0) {
          void *y = pop();
          result = Bstring_patt(value, y);
        } else if (l >= 1 && l <= 6) {
          result = patt_functions[l](value);
        } else {
          fail_with("Invalid PATT pattern: " + std::to_string(l));
        }
        push(result);

        LOG(log() << "PATT " << patts[l] << " -> " << UNBOX(result)
                  << std::endl);
        break;
      }
      case LINE: {
        int32_t line = ip_int32();
        LOG(log() << "LINE " << line << std::endl);
        break;
      }
      default: {
        fail_with("Invalid opcode: " +
                  STR_HEX(static_cast<unsigned>(opcode), 2));
      }
      }
      PRINT_STACKS(
          size_t operand_stack_size = static_cast<size_t>(sp - operands);
          log() << "\tOPERANDS: size=" << operand_stack_size << ": [";
          for (size_t i = 0; i < operand_stack_size; ++i) {
            log() << UNBOX(operands[i]);
            if (i < operand_stack_size - 1) {
              log() << ", ";
            }
          } log()
          << "]" << std::endl;

          // Print the current frame stack contents for the current frame (ret
          // address, args, locals)
          if (!frames.empty()) {
            const auto &current_frame = frames.back();
            uint32_t num_args = current_frame.args;
            uint32_t num_locals = current_frame.locals;
            // Frame structure: [ret, captured (if closure), args..., locals...]
            void **frame_start = fp - (num_args + num_locals + 1);
            log() << "\tFRAME_STACK: ";
            log() << "ret=[";
            if ((num_args + num_locals + 1) > 0) {
              log() << UNBOX(frame_start[0]);
            }
            log() << "]";

            void *closure_ptr =
                (frames.back().closure ? get_closure_ptr() : nullptr);
            if (closure_ptr != nullptr) {
              // If it is a closure, the closure pointer is in
              // current_frame.closure
              log() << " closure=[";
              log() << UNBOX(closure_ptr);
              log() << "]";
            }

            log() << " args=[";
            for (int i = 0; i < num_args; ++i) {
              if (i != 0)
                log() << ", ";
              log() << UNBOX(frame_start[1 + i]);
            }
            log() << "]";

            log() << " locals=[";
            for (int i = 0; i < num_locals; ++i) {
              if (i != 0)
                log() << ", ";
              log() << UNBOX(frame_start[1 + num_args + i]);
            }
            log() << "]" << std::endl;
          });
    } while (1);
  }

private:
  // instruction pointer operations
  uint8_t ip_byte() {
    check_ip_valid(ip);
    return *ip++;
  }

  int32_t ip_int32() {
    check_ip_valid(ip);
    check_ip_valid(ip + sizeof(int32_t) -
                   1); // `ip + sizeof(int32_t)` could be the end of the
                       // bytecode, so we need -1
    int32_t value;
    std::memcpy(&value, ip, sizeof(int32_t));
    ip += sizeof(int32_t);
    return value;
  }

  void set_ip(const uint8_t *new_ip) {
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
    *sp = nullptr; // Zero out after popping
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
  void *get_closure_ptr() {
    check_frames_not_empty();
    auto [has_closure, args, locals] = frames.back();
    check(has_closure, "get_closure_ptr: not a closure frame");
    return *(fp - args - locals -
             has_closure); // [ret] [closure_ptr] [args] [locals] fp
                           //             ^---- closure ptr must be here
  }

  void *read_closure_value(int index) {
    check_frames_not_empty();
    void *closure_ptr = get_closure_ptr();
    void **closure_contents = (void **)closure_ptr;
    check_closure_value_index(index);
    return closure_contents[index + 1]; // +1 because index 0 is function_offset
  }

  void *read_arg(int index) {
    check_frames_not_empty();
    check_argument_index(index);
    const Frame &frame = frames.back();
    return *(fp - frame.args - frame.locals +
             index); // [ret] [closure_ptr]? [args] [locals] fp
                     //                        ^---- index points somewhere here
  }

  void *read_local(int index) {
    check_frames_not_empty();
    check_local_index(index);
    const Frame &frame = frames.back();
    return *(fp - frame.locals +
             index); // [ret] [closure_ptr]? [args] [locals] fp
                     //                                ^---- index points
                     //                                somewhere here
  }

  void write_closure_value(int index, void *value) {
    check_frames_not_empty();
    void *closure_ptr = get_closure_ptr();
    check_closure_value_index(index);
    void **closure_contents = (void **)closure_ptr;
    closure_contents[index + 1] =
        value; // +1 because index 0 is function_offset
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
    auto [has_closure, args, locals] = frames.back();
    frames.pop_back();

    // Frame stack layout: [ret] [closure_ptr]? [args] [locals] fp
    // Closure values are NOT on the frame stack - they're in the closure object
    check_frames_underflow(args + locals + has_closure + 1);
    fp -= (args + locals + has_closure + 1); // +1 for return address

    // Read return address before zeroing
    void *ret_addr = *fp;

    // Zero out the frame stack entries after popping to help detect stale
    // pointers
    void **frame_start = fp;
    void **frame_end = fp + (args + locals + 1);
    for (void **p = frame_start; p < frame_end; ++p) {
      *p = nullptr;
    }

    set_ip(reinterpret_cast<uint8_t *>(ret_addr));
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

  void check_ip_valid(const uint8_t *ip) {
    const uint8_t *bytecode_start = bc.get_bytecode();
    check(ip >= bytecode_start && ip < bytecode_start + bc.get_bytecode_size(),
          "Invalid IP");
  }

  void check_stack_overflow() {
    if (sp >= operands + MAX_OPERANDS) {
      throw StackOverflowException(ip - bc.get_bytecode());
    }
  }

  void check_stack_underflow() {
    if (sp <= operands) {
      throw StackUnderflowException(ip - bc.get_bytecode());
    }
  }

  void check_frames_max_size() {
    if (frames.size() >= MAX_FRAMES) {
      throw FramesMaxSizeException(ip - bc.get_bytecode());
    }
  }

  void check_frames_overflow() {
    if (fp >= frame_stack + MAX_FRAME_STACK_SIZE) {
      throw FramesOverflowException(ip - bc.get_bytecode());
    }
  }

  void check_frames_underflow(int substract) {
    if (fp - frame_stack < substract) {
      throw FramesUnderflowException(ip - bc.get_bytecode());
    }
  }

  void check_frames_not_empty() {
    check(!frames.empty(), "Empty 'frames' stack");
  }

  void check_closure_value_index(int index) {
    check_frames_not_empty();
    void *closure_ptr = get_closure_ptr();
    data *closure_data = TO_DATA(closure_ptr);
    int32_t captured_count = LEN(closure_data->data_header) - 1;
    check(index >= 0 && index < captured_count,
          "Invalid closure value index: " + std::to_string(index) + " / " +
              std::to_string(captured_count));
  }

  void check_argument_index(int index) {
    check(index >= 0 && static_cast<uint32_t>(index) < frames.back().args,
          "Invalid argument index: " + std::to_string(index));
  }

  void check_local_index(int index) {
    check(index >= 0 && static_cast<uint32_t>(index) < frames.back().locals,
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

    LOG(log() << "Loaded bytecode file:" << std::endl);
    LOG(log() << "  String table size: " << bc.get_stringtab_size() << " bytes"
              << std::endl);
    LOG(log() << "  Global area size: " << bc.get_global_area_size() << " words"
              << std::endl);
    LOG(log() << "  Strings loaded: " << bc.get_strings().size() << std::endl);
    for (uint32_t i = 0; i < bc.get_strings().size();) {
      const std::string_view string = bc.get_string(i);
      LOG(log() << "    " << STR_HEX(i, 8) << ": "
                << (string.empty() ? "<empty>" : string) << std::endl);
      i += string.size() + 1;
    }
    LOG(log() << "  Public symbols: " << bc.get_public_symbols_number()
              << std::endl);
    for (const auto &symbol : bc.get_public_symbols()) {
      uint32_t name_index = symbol.first;
      uint32_t offset = symbol.second;
      LOG(log() << "    " << STR_HEX(offset, 8) << ": "
                << bc.get_string(name_index) << std::endl);
    }
    LOG(log() << "  Bytecode size: " << bc.get_bytecode_size() << " bytes"
              << std::endl);

    LOG(log() << "Interpreting bytecode:" << std::endl);
    interpreter.interpret();
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
