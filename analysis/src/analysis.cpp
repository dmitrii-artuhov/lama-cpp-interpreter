#include <algorithm>
#include <iostream>
#include <queue>

#include "logger.h"
#include "parsing.h"
#include "utils.h"

namespace {
const uint32_t OFFSET_MAX = 0x7FFFFFFF;
const uint32_t DOUBLE_INSN_MASK = 0x80000000;

struct SmartInstructionRange {
  // we are agreed to have bytecode file <= 1GB, so we only use 31 lower bits of
  // the offset one more bit we can use to mark, whether the range is a single
  // or double instruction
  uint32_t smart_offset;
  // count which is used for counting the number of the matching instructions
  uint32_t cnt;

  uint32_t offset() const { return smart_offset & OFFSET_MAX; }

  bool is_single() const { return (smart_offset & DOUBLE_INSN_MASK) == 0; }

  uint32_t length(const BytecodeFile &bc) const {
    uint32_t insn_offset = offset();
    uint32_t insn_length =
        instruction_length(bc.get_bytecode() + insn_offset, bc);

    // if double, then add a second instruction length
    if (!is_single()) {
      insn_length +=
          instruction_length(bc.get_bytecode() + insn_offset + insn_length, bc);
    }

    return insn_length;
  }

  // return true if a < b, false otherwise
  static bool lexicographical_compare(const SmartInstructionRange &a,
                                      const SmartInstructionRange &b,
                                      const BytecodeFile &bc) {
    const uint8_t *bytecode = bc.get_bytecode();
    uint32_t a_length = a.length(bc);
    uint32_t b_length = b.length(bc);
    int result = std::memcmp(bytecode + a.offset(), bytecode + b.offset(),
                             std::min(a_length, b_length));

    if (result == 0) {
      return a_length < b_length;
    }
    return result < 0;
  }
};

std::string instruction_range_to_string(const SmartInstructionRange &range,
                                        BytecodeFile &bc) {
  std::stringstream ss;
  uint32_t offset = range.offset();
  uint32_t length = range.length(bc);

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

  void analyse() {
    std::cout << "Analyzing bytecode..." << std::endl;

    // collect instruction ranges of length 1..2 from basic blocks
    std::vector<SmartInstructionRange> ranges = get_ranges();

    // 1. sort ranges by lexicographical order of the instruction ranges
    std::sort(ranges.begin(), ranges.end(),
              [this](const auto &a, const auto &b) {
                return SmartInstructionRange::lexicographical_compare(a, b, bc);
              });
    LOG(log() << "Sorted ranges of 1..2 instructions:" << std::endl);
    for (const auto &range : ranges) {
      LOG(log() << "  " << instruction_range_to_string(range, bc) << std::endl);
    }

    // 2. count the number of matching instruction ranges
    // the counting is done in the same vector, where the ranges are stored,
    // we just increment the count of the current range (the first one from the
    // matching span)
    const uint8_t *bytecode = bc.get_bytecode();
    for (uint32_t current = 0; current < ranges.size();) {
      auto &current_range = ranges[current];
      uint32_t current_length = current_range.length(bc);
      current_range.cnt++; // set the current count to 1 right away

      uint32_t next = current + 1;
      for (; next < ranges.size(); ++next) {
        const auto &next_range = ranges[next];
        uint32_t next_length = next_range.length(bc);

        bool equal =
            current_length == next_length &&
            (std::memcmp(bytecode + current_range.offset(),
                         bytecode + next_range.offset(), current_length) == 0);

        if (!equal) {
          break;
        }
        current_range.cnt++;
      }

      current = next;
    }

    // 3. sort buckets by the count in descending order
    std::sort(
        ranges.begin(), ranges.end(), [this](const auto &a, const auto &b) {
          if (a.cnt == b.cnt) {
            // for matching counts, sort by the lexicographical order of
            // the instruction ranges
            return SmartInstructionRange::lexicographical_compare(a, b, bc);
          }
          return a.cnt > b.cnt;
        });

    // 4. print buckets, skipping zeros -- they are duplicates
    std::cout << "Counts of instructions:" << std::endl;
    for (const auto &range : ranges) {
      if (range.cnt == 0) {
        break;
      }
      std::cout << "  " << range.cnt << "  "
                << instruction_range_to_string(range, bc) << std::endl;
    }
  }

private:
  std::vector<SmartInstructionRange> get_ranges() {
    auto [reachable, labels] = get_transitive_closure();
    // each element in range is a
    // { offset | 32-nd bit = { 0=single, 1=double }, cnt }
    // so each range takes 8 bytes, the maximum number of ranges is
    // twice the bytecode size, so we get <=16x of memory usage
    std::vector<SmartInstructionRange> ranges;

    for (uint32_t offset = 0; offset < reachable.size();) {
      if (!reachable[offset]) {
        offset++;
        continue;
      }

      // collect instruction ranges of length 1 and 2
      ranges.push_back({offset, 0}); // count is zero by default
      uint32_t length = instruction_length(bc.get_bytecode() + offset, bc);

      bool can_be_double =
          (offset + length < reachable.size() && reachable[offset + length] &&
           !labels[offset + length]);

      if (can_be_double) {
        // add a double instruction range if the second instruction is also
        // reachable
        ranges.push_back({
            offset | DOUBLE_INSN_MASK, // set 32-nd bit
            0                          // count is zero by default
        });
      }

      // shift by the current instruction size
      // then for loop will go to the next reachable instruction by itself
      offset += length;
    }

    return ranges;
  }

  // Returns a pair of marked vectors: reachable offsets and labels
  std::pair<std::vector<bool>, std::vector<bool>> get_transitive_closure() {
    std::vector<bool> reachable(bc.get_bytecode_size(), false);
    std::vector<bool> labels(bc.get_bytecode_size(), false);
    std::queue<uint32_t> work_set;

    // mark the public symbols
    for (const auto &symbol : bc.get_public_symbols()) {
      if (reachable[symbol.second])
        continue; // if a duplicate is found, skip it
      reachable[symbol.second] = true;
      work_set.push(symbol.second);
    }

    while (!work_set.empty()) {
      // work set offsets are already marked as reachable
      uint32_t offset = work_set.front();
      work_set.pop();
      uint8_t opcode = *(bc.get_bytecode() + offset);

      if (leads_to_label(opcode)) {
        // all instructions have the offset right after the opcode
        uint32_t target_offset = read_uint32(bc.get_bytecode() + offset + 1);
        validate_offset(target_offset);

        if (!reachable[target_offset]) {
          reachable[target_offset] = true;
          labels[target_offset] = true;
          work_set.push(target_offset);
        }
      }

      if (falls_through(opcode)) {
        uint32_t next_offset =
            offset + instruction_length(bc.get_bytecode() + offset, bc);
        validate_offset(next_offset);

        if (!reachable[next_offset]) {
          reachable[next_offset] = true;
          work_set.push(next_offset);
        }
      }
    }

    // print reachable offsets and labels
    LOG(log() << "Reachable offsets:" << std::endl);
    for (uint32_t i = 0; i < reachable.size(); ++i) {
      if (reachable[i]) {
        LOG(log() << "  " << STR_HEX(i, 8) << ":    ");
        LOG(log() << instruction_range_to_string(SmartInstructionRange{i, 0},
                                                 bc)
                  << std::endl);
      }
    }
    LOG(log() << "Labels:" << std::endl);
    for (uint32_t i = 0; i < labels.size(); ++i) {
      if (labels[i]) {
        LOG(log() << "  " << STR_HEX(i, 8) << std::endl);
      }
    }

    return {reachable, labels};
  }

  void validate_offset(uint32_t offset) {
    if (offset >= bc.get_bytecode_size()) {
      throw std::runtime_error("Invalid offset: " + STR_HEX(offset, 8) +
                               " >= " + STR_HEX(bc.get_bytecode_size(), 8));
    }
  }
};

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <bytecode_file.bc> [log_file]"
              << std::endl;
    return 1;
  }

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

    analyser.analyse();

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
