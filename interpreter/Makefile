# Makefile for Lama bytecode interpreter

# Detect OS
UNAME_S := $(shell uname -s)

# Compiler settings
ifeq ($(UNAME_S),Linux)
    CXX = clang++
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

COMMON_CXXFLAGS = $(ARCH) -std=c++17 -Wall -Wextra
CPPFLAGS = -I$(RUNTIME_DIR) -I$(SRC_DIR)

SANITIZER_FLAGS = -fsanitize=address -fsanitize=undefined
DEBUG_CXXFLAGS = -g -O0 -fno-omit-frame-pointer -fstack-protector-all $(SANITIZER_FLAGS) -DENABLE_LOG -DENABLE_PRINT_STACKS
RELEASE_CXXFLAGS = -O3

# Default to debug build
CXXFLAGS = $(COMMON_CXXFLAGS) $(DEBUG_CXXFLAGS)

# Default target (debug build)
.PHONY: all
all: $(TARGET)

# Release target (does a clean build to ensure all files use release flags)
.PHONY: release
release: CXXFLAGS = $(COMMON_CXXFLAGS) $(RELEASE_CXXFLAGS)
release: clean $(TARGET)
	@echo "Release build complete: $(TARGET)"

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

# Run tests from regression folder
.PHONY: test
test: $(TARGET)
	@echo "Running tests from ../regression..."
	@./run_tests.sh ../regression

# Clean test results
.PHONY: clean_test
clean_test:
	$(RM) -r test_results
