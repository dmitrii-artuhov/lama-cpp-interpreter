#pragma once
#include <fstream>
#include <iostream>
#include <string>

// Initialize logger with filename
void init_logger(const std::string &filename = "interpreter.log");

std::ostream &log();

#ifdef ENABLE_LOG
#define LOG(stmt)                                                              \
  do {                                                                         \
    stmt;                                                                      \
  } while (0)
#else
#define LOG(stmt)                                                              \
  do {                                                                         \
  } while (0)
#endif
