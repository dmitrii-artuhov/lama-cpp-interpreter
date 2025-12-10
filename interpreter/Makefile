# Makefile for Lama bytecode interpreter

# Detect OS
UNAME_S := $(shell uname -s)

# Compiler settings
ifeq ($(UNAME_S),Linux)
    CXX = g++
else ifeq ($(UNAME_S),Darwin)
    CXX = clang++
    ARCH = -arch x86_64
endif

# Directories
SRC_DIR = src
BUILD_DIR = build
RUNTIME_DIR = ../runtime

# Source files (all .cpp files in src/)
SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# Target executable
TARGET = $(BUILD_DIR)/interpreter

# Runtime library
RUNTIME_LIB = $(RUNTIME_DIR)/runtime.a

# Compiler flags
CXXFLAGS = -g -fstack-protector-all $(ARCH) -std=c++17 -Wall -Wextra
CPPFLAGS = -I$(RUNTIME_DIR) -I$(SRC_DIR)

# Default target
.PHONY: all
all: $(TARGET)

# Build the interpreter executable
$(TARGET): $(OBJECTS) $(RUNTIME_LIB) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS) $(RUNTIME_LIB)

# Compile .cpp files to .o files in build directory
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

# Create build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Ensure runtime library exists
$(RUNTIME_LIB):
	$(MAKE) -C $(RUNTIME_DIR)

# Clean build artifacts
.PHONY: clean
clean:
	$(RM) -r $(BUILD_DIR)
