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
COMMON_DIR = ../common

# Source files (all .cpp files in src/ and common/)
SRC_SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
COMMON_SOURCES = $(wildcard $(COMMON_DIR)/*.cpp)
SOURCES = $(SRC_SOURCES) $(COMMON_SOURCES)
SRC_OBJECTS = $(SRC_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
COMMON_OBJECTS = $(COMMON_SOURCES:$(COMMON_DIR)/%.cpp=$(BUILD_DIR)/%.o)
OBJECTS = $(SRC_OBJECTS) $(COMMON_OBJECTS)

# Target executable
TARGET = $(BUILD_DIR)/analysis

COMMON_CXXFLAGS = $(ARCH) -std=c++17 -Wall -Wextra
CPPFLAGS = -I$(SRC_DIR) -I$(COMMON_DIR)

SANITIZER_FLAGS = -fsanitize=address -fsanitize=undefined
DEBUG_CXXFLAGS = -g -O0 -fno-omit-frame-pointer -fstack-protector-all $(SANITIZER_FLAGS) -DENABLE_LOG
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

# Build the analysis executable
$(TARGET): $(OBJECTS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS)

# Compile .cpp files to .o files in build directory
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(COMMON_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

# Create build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Clean build artifacts
.PHONY: clean
clean:
	$(RM) -r $(BUILD_DIR)
