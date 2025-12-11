#pragma once
#include <fstream>
#include <iostream>
#include <string>

// Simple null stream that discards output
class NullStream : public std::ostream {
public:
  NullStream() : std::ostream(nullptr) {}
  template <typename T> NullStream &operator<<(const T &) { return *this; }
  NullStream &operator<<(std::ostream &(*)(std::ostream &)) { return *this; }
};

// Initialize logger with filename
void init_logger(const std::string &filename = "interpreter.log");

// Simple log function
std::ostream &log();
