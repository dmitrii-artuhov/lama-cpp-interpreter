#include "logger.h"

// Static variables
static std::ofstream log_file;

void init_logger(const std::string &filename) {
#ifdef ENABLE_LOG
  log_file.open(filename, std::ios::trunc);
  if (!log_file.is_open()) {
    std::cerr << "Warning: Could not open log file: " << filename << std::endl;
  }
#else
  (void)filename; // Suppress unused parameter warning
#endif
}

std::ostream &log() {
#ifdef ENABLE_LOG
  if (log_file.is_open()) {
    return log_file;
  } else {
    return std::cout;
  }
#else
  return std::cout;
#endif
}
