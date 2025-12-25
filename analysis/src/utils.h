#pragma once

#include <cstdint>
#include <iostream>

#include "common.h"
#include "opcodes.h"
#include "parsing.h"

// returns instruction length in bytes
inline uint32_t instruction_length(const uint8_t *start,
                                   const BytecodeFile &bc) {
  const uint8_t *bytecode_start = bc.get_bytecode();
  uint8_t opcode = *start++;

  switch (opcode) {
  case CALL_READ:
  case CALL_WRITE:
  case CALL_LENGTH:
  case CALL_STRING:
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
  case BINOP_OR:
  case STI:
  case STA:
  case DROP:
  case DUP:
  case SWAP:
  case ELEM:
  case PATT_STRCMP:
  case PATT_STRING:
  case PATT_ARRAY:
  case PATT_SEXP:
  case PATT_REF:
  case PATT_VAL:
  case PATT_FUN:
  case END:
  case RET:
  case EOF_OPCODE:
    return 1;

  case BEGIN:
  case BEGINC:
    return 1 + 4 + 4; // opcode, args, locals
  case CALL:
    return 1 + 4 + 4; // opcode, callee_offset, args_count
  case CALLC:
    return 1 + 4; // opcode, args_count
  case CALL_ARRAY:
    return 1 + 4; // opcode, n
  case CONST:
    return 1 + 4; // opcode, value
  case STRING:
    return 1 + 4; // opcode, string_id
  case SEXP:
    return 1 + 4 + 4; // opcode, tag_string_id, n
  case JMP:
  case CJMP_Z:
  case CJMP_NZ:
    return 1 + 4; // opcode, offset
  case LD_G:
  case LD_L:
  case LD_A:
  case LD_C:
  case LDA_G:
  case LDA_L:
  case LDA_A:
  case LDA_C:
  case ST_G:
  case ST_L:
  case ST_A:
  case ST_C:
    return 1 + 4; // opcode, index
  case TAG:
    return 1 + 4 + 4; // opcode + tag_string_id + n
  case ARRAY:
    return 1 + 4; // opcode + n
  case CLOSURE: {
    start += 4; // function_offset
    uint32_t n = read_uint32(start);
    // opcode, function_offset, n, n designations (type + index)
    return 1 + 4 + 4 + n * (1 + 4);
  }
  case LINE:
    return 1 + 4; // opcode + line
  case FAIL:
    return 1 + 4 + 4; // opcode + line + column?
  default:
    throw std::runtime_error("Invalid opcode " + STR_HEX(opcode, 2) + " at " +
                             STR_HEX(start - 1 - bytecode_start, 8));
  }
}

inline void print_instruction(std::ostream &os, const uint8_t *start,
                              const BytecodeFile &bc) {
  uint8_t opcode = *start++;
  os << opcode_to_string(opcode);

  switch (opcode) {
    // 2 ints
  case BEGIN:
  case BEGINC:
  case FAIL: {
    os << " " << read_uint32(start);
    os << " " << read_uint32(start + 4);
    break;
  }
  // hex + int
  case CALL: {
    os << " " << STR_HEX(read_uint32(start), 8);
    os << " " << read_uint32(start + 4);
    break;
  }
  // int
  case CALLC:
  case CALL_ARRAY:
  case CONST:
  case STRING:
  case ARRAY:
  case LINE: {
    os << " " << read_uint32(start);
    break;
  }
  // (int)
  case ST_G:
  case ST_L:
  case ST_A:
  case ST_C:
  case LD_G:
  case LD_L:
  case LD_A:
  case LD_C: {
    os << "(" << read_uint32(start) << ")";
    break;
  }
  // string + int
  case SEXP:
  case TAG: {
    uint32_t string_tag_id = read_uint32(start);
    os << " " << bc.get_string(string_tag_id);
    os << " " << read_uint32(start + 4);
    break;
  }
  // hex
  case JMP:
  case CJMP_Z:
  case CJMP_NZ: {
    os << " " << STR_HEX(read_uint32(start), 8);
    break;
  }
  // hex + int + n designations [type + (int)]
  case CLOSURE: {
    os << " " << STR_HEX(read_uint32(start), 8);
    start += 4;
    uint32_t n = read_uint32(start);
    os << " " << n;
    start += 4;
    for (uint32_t i = 0; i < n; i++) {
      uint8_t designation_type = *start++;
      os << " " << designation_type_to_string(designation_type);
      os << "(" << read_uint32(start) << ")";
      start += 4;
    }
    break;
  }
  }
}

// opcode which is conditiona jump
inline bool is_conditional_jmp(uint8_t opcode) {
  return opcode == CJMP_Z || opcode == CJMP_NZ;
}

// opcode which is unconditional jump
inline bool is_unconditional_jmp(uint8_t opcode) { return opcode == JMP; }

// opcode which is any jump to a fixed offset
inline bool is_jmp(uint8_t opcode) {
  return is_unconditional_jmp(opcode) || is_conditional_jmp(opcode);
}

inline bool leads_to_label(uint8_t opcode) {
  return opcode == CALL || is_jmp(opcode) || opcode == CLOSURE;
}

inline bool falls_through(uint8_t opcode) {
  return !(opcode == END || opcode == RET || opcode == FAIL ||
           opcode == EOF_OPCODE || is_unconditional_jmp(opcode));
}