#include <algorithm>
#include <iostream>

#include "logger.h"
#include "parsing.h"
#include "utils.h"

namespace {
struct InstructionRange {
  uint32_t offset; // in the bytecode file
  uint32_t length; // in bytes including all parameters
};

std::string instruction_range_to_string(const InstructionRange &range,
                                        BytecodeFile &bc) {
  std::stringstream ss;
  uint32_t offset = range.offset;
  uint32_t length = range.length;

  const uint8_t *bytecode_start = bc.get_bytecode();
  const uint8_t *ip = bytecode_start + offset;

  if (offset + length > bc.get_bytecode_size()) {
    throw std::runtime_error(
        "Instruction range out of bounds: " + std::to_string(offset) + " + " +
        std::to_string(length) + " > " +
        std::to_string(bc.get_bytecode_size()));
  }

  while (ip < bytecode_start + offset + length) {
    print_instruction(ss, ip, bc);
    ip += instruction_length(ip, bc);
    if (ip < bytecode_start + offset + length) {
      ss << " | "; // separator between instructions
    }
  }

  return ss.str();
}

} // namespace

class Analyser {
  BytecodeFile &bc;

public:
  explicit Analyser(BytecodeFile &bc) : bc(bc) {}

  void analyse(int k) {
    LOG(log() << "Analyzing bytecode..." << std::endl);

    // collect basic blocks
    std::vector<std::vector<InstructionRange>> basic_blocks =
        get_basic_blocks();

    // print basic blocks
    for (size_t bb_index = 0; bb_index < basic_blocks.size(); bb_index++) {
      const auto &block = basic_blocks[bb_index];
      LOG(log() << "Basic block: " << bb_index << std::endl);
      for (const auto &instruction : block) {
        LOG(log() << "  " << instruction_range_to_string(instruction, bc)
                  << std::endl);
      }
    }

    // count instructions of length 1, ..., k
    for (int i = 1; i <= k; ++i) {
      std::vector<InstructionRange> ranges;

      for (const auto &block : basic_blocks) {
        // collect instruction ranges of length i
        for (size_t j = 0; j < block.size() - i + 1; ++j) {
          uint32_t offset = block[j].offset;
          uint32_t length = 0;
          // all instructions are followed one by another, so we can just sum up
          // the lengths
          for (int l = 0; l < i; ++l) {
            length += block[j + l].length;
          }

          ranges.push_back({offset, length});
        }
      }

      /*
        we have all ranges of length i for all basic blocks
        1. sort them by lexicographical order of the instruction ranges
        2. count the number of matching ones:
          a. if 2 neighbouring sequences are the same, do +=1 to the current
          counter
          b. if neighbours do not match, save the prev bucket {
          InstructionRange, count } and set count to 1. The 'InstructionRange'
          can be taken any of the matched values, because as byte-sequences they
          are equal
        3. the resulting bucket vector of { InstructionRange, count } pairs,
           sort by the count this is the answer for the current i
      */

      // 1. sort ranges by lexicographical order of the instruction ranges
      std::sort(ranges.begin(), ranges.end(),
                [this](const InstructionRange &a, const InstructionRange &b) {
                  const uint8_t *bytecode = bc.get_bytecode();
                  int result =
                      std::memcmp(bytecode + a.offset, bytecode + b.offset,
                                  std::min(a.length, b.length));
                  if (result == 0) {
                    return a.length < b.length;
                  }
                  return result < 0;
                });
      LOG(log() << "Sorted ranges of " << i << " instructions:" << std::endl);
      for (const auto &range : ranges) {
        LOG(log() << "  " << instruction_range_to_string(range, bc)
                  << std::endl);
      }

      // 2. count the number of matching ones
      std::vector<std::pair<InstructionRange, int>> counts = {{ranges[0], 1}};
      for (size_t j = 1; j < ranges.size(); ++j) {
        const InstructionRange &curr = ranges[j];
        const InstructionRange &prev = ranges[j - 1];
        const uint8_t *bytecode = bc.get_bytecode();

        bool equal = curr.length == prev.length &&
                     (std::memcmp(bytecode + curr.offset,
                                  bytecode + prev.offset, curr.length) == 0);

        if (equal) {
          counts.back().second++;
        } else {
          counts.push_back({curr, 1});
        }
      }

      // 3. sort buckets by the count
      std::sort(counts.begin(), counts.end(),
                [](const std::pair<InstructionRange, int> &a,
                   const std::pair<InstructionRange, int> &b) {
                  return a.second > b.second;
                });

      // 4. print buckets
      LOG(log() << "Counts of " << i << " instructions:" << std::endl);
      for (const auto &count : counts) {
        LOG(log() << "  " << count.second << "\t"
                  << instruction_range_to_string(count.first, bc) << std::endl);
      }
    }
  }

  std::vector<std::vector<InstructionRange>> get_basic_blocks() {
    // Identify leaders (first instructions of basic blocks).
    //
    // An instruction is a leader if it is:
    //   1. the first instruction of the call;
    //   2. any instruction that is the target of a conditional or unconditional
    //   jump;
    //   3. instruction immediately following a conditional if jump (since
    //   control may fall through)

    std::vector<uint32_t> leaders;
    uint32_t bytecode_size = bc.get_bytecode_size();
    const uint8_t *bytecode_start = bc.get_bytecode();
    const uint8_t *ip = bytecode_start;

    while (ip < bytecode_start + bytecode_size) {
      uint8_t opcode = *ip;
      uint32_t length = instruction_length(ip, bc);

      if (is_any_begin(opcode)) {
        // first instruction of method call
        leaders.push_back(ip - bytecode_start);
      } else if (is_any_jmp(opcode)) {
        uint32_t target_offset = read_uint32(ip + 1);
        // target of the jump
        leaders.push_back(target_offset);

        if (is_conditional_jmp(opcode)) {
          uint32_t next_offset = ip + length - bytecode_start;
          if (next_offset < bytecode_size) {
            // fall-through instruction of the conditional jump
            leaders.push_back(next_offset);
          }
        }
      }

      ip += length;
    }

    std::sort(leaders.begin(), leaders.end());

    std::vector<std::vector<InstructionRange>> basic_blocks;
    for (size_t i = 0; i < leaders.size(); ++i) {
      std::vector<InstructionRange> block;
      uint32_t start_offset = leaders[i];
      uint32_t end_offset = (i + 1 < leaders.size())
                                ? leaders[i + 1]
                                : bytecode_size; // exclusive

      uint32_t offset = start_offset;
      while (offset < end_offset) {
        uint32_t length = instruction_length(bytecode_start + offset, bc);
        block.push_back({offset, length});
        offset += length;
      }
      if (offset != end_offset) {
        throw std::runtime_error("Basic block " + std::to_string(i) +
                                 " is not terminated correctly, its end does "
                                 "not match expected end " +
                                 std::to_string(offset) + "/" +
                                 std::to_string(end_offset));
      }

      basic_blocks.push_back(block);
    }

    return basic_blocks;
  }
};

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <bytecode_file.bc> [log_file]"
              << std::endl;
    return 1;
  }

  // Use provided log file path or default to "interpreter.log"
  std::string log_file = (argc >= 3) ? argv[2] : "analysis.log";
  init_logger(log_file);

  try {
    BytecodeFile bc(argv[1]);
    Analyser analyser(bc);

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

    LOG(log() << "Analyzed bytecode:" << std::endl);
    analyser.analyse(2);

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
