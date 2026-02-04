#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

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

  EOF_OPCODE = 0xff,
  // TODO: extern, public, import
};

enum DesignationType : uint8_t {
  GLOBAL = 0,
  LOCAL = 1,
  ARGUMENT = 2,
  CLOSURE_V = 3
};

inline const char *designation_type_to_string(uint8_t designation_type) {
  switch (designation_type) {
  case GLOBAL:
    return "G";
  case LOCAL:
    return "L";
  case ARGUMENT:
    return "A";
  case CLOSURE_V:
    return "C";
  default:
    throw std::runtime_error("Unknown designation type: " +
                             std::to_string(designation_type));
  }
}

inline const char *opcode_to_string(uint8_t opcode) {
  switch (opcode) {
  case BINOP_PLUS:
    return "BINOP +";
  case BINOP_MINUS:
    return "BINOP -";
  case BINOP_MULTIPLY:
    return "BINOP *";
  case BINOP_DIVIDE:
    return "BINOP /";
  case BINOP_MODULO:
    return "BINOP %";
  case BINOP_LT:
    return "BINOP <";
  case BINOP_LTE:
    return "BINOP <=";
  case BINOP_GT:
    return "BINOP >";
  case BINOP_GTE:
    return "BINOP >=";
  case BINOP_EQ:
    return "BINOP ==";
  case BINOP_NE:
    return "BINOP !=";
  case BINOP_AND:
    return "BINOP &&";
  case BINOP_OR:
    return "BINOP !!";

  case CONST:
    return "CONST";
  case STRING:
    return "STRING";
  case SEXP:
    return "SEXP";

  case STI:
    return "STI";
  case STA:
    return "STA";

  case JMP:
    return "JMP";
  case END:
    return "END";
  case RET:
    return "RET";

  case DROP:
    return "DROP";
  case DUP:
    return "DUP";
  case SWAP:
    return "SWAP";
  case ELEM:
    return "ELEM";

  case LD_G:
    return "LD G";
  case LD_L:
    return "LD L";
  case LD_A:
    return "LD A";
  case LD_C:
    return "LD C";

  case LDA_G:
    return "LDA G";
  case LDA_L:
    return "LDA L";
  case LDA_A:
    return "LDA A";
  case LDA_C:
    return "LDA C";

  case ST_G:
    return "ST G";
  case ST_L:
    return "ST L";
  case ST_A:
    return "ST A";
  case ST_C:
    return "ST C";

  case CJMP_Z:
    return "CJMP_Z";
  case CJMP_NZ:
    return "CJMP_NZ";

  case CALL_READ:
    return "CALL_READ";
  case CALL_WRITE:
    return "CALL_WRITE";
  case CALL_LENGTH:
    return "CALL_LENGTH";
  case CALL_STRING:
    return "CALL_STRING";
  case CALL_ARRAY:
    return "CALL_ARRAY";

  case BEGIN:
    return "BEGIN";
  case BEGINC:
    return "BEGINC";

  case CLOSURE:
    return "CLOSURE";
  case CALLC:
    return "CALLC";
  case CALL:
    return "CALL";
  case TAG:
    return "TAG";
  case ARRAY:
    return "ARRAY";
  case FAIL:
    return "FAIL";
  case LINE:
    return "LINE";

  case PATT_STRCMP:
    return "PATT =str";
  case PATT_STRING:
    return "PATT #string";
  case PATT_ARRAY:
    return "PATT #array";
  case PATT_SEXP:
    return "PATT #sexp";
  case PATT_REF:
    return "PATT #ref";
  case PATT_VAL:
    return "PATT #val";
  case PATT_FUN:
    return "PATT #fun";

  case EOF_OPCODE:
    return "<end>";

  default:
    throw std::runtime_error("Unknown representation for opcode: " +
                             std::to_string(opcode));
  }
}