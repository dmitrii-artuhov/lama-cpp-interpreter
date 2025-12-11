#include "logger.h"

// Static variables
static std::ofstream log_file;
static NullStream null_stream;

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

// TODO: wrap calls to log() in macros, so it is properly cut-out when
std::ostream &log() {
#ifdef ENABLE_LOG
  if (log_file.is_open()) {
    return log_file;
  } else {
    return std::cout;
  }
#else
  return null_stream;
#endif
}
