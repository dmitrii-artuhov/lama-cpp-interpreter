# Makefile for byterun

# Detect OS
UNAME_S := $(shell uname -s)

# Compiler settings
CC = gcc
RUNTIME_DIR = ../runtime
RUNTIME_LIB = $(RUNTIME_DIR)/runtime.a

# Debug flags for better error reporting
DEBUG_FLAGS = -g -O0 -fno-omit-frame-pointer
SANITIZER_FLAGS = -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer
STACK_PROTECTION = -fstack-protector-all

# Default: build with debug symbols
CFLAGS = $(DEBUG_FLAGS) $(STACK_PROTECTION) -Wall -Wextra -I$(RUNTIME_DIR)

# Target executable
TARGET = byterun

# Default target
.PHONY: all
all: $(TARGET)

# Build byterun with debug symbols
$(TARGET): byterun.c $(RUNTIME_LIB)
	$(CC) $(CFLAGS) -o $@ $< $(RUNTIME_LIB)

# Build with AddressSanitizer (catches heap corruption)
.PHONY: sanitize
sanitize: CFLAGS += $(SANITIZER_FLAGS)
sanitize: clean $(TARGET)
	@echo "Built with AddressSanitizer - heap corruption will be detected"

# Build with all debug flags
.PHONY: debug
debug: CFLAGS += -DDEBUG -g3 -ggdb
debug: clean $(TARGET)
	@echo "Built with full debug symbols"

# Ensure runtime library exists
$(RUNTIME_LIB):
	$(MAKE) -C $(RUNTIME_DIR)

# Clean build artifacts
.PHONY: clean
clean:
	$(RM) $(TARGET) core core.* *.core

# Enable core dumps (run this before executing)
.PHONY: enable-coredump
enable-coredump:
	@echo "Enabling core dumps..."
	@ulimit -c unlimited
	@echo "Core dumps enabled. Run: gdb $(TARGET) core"
	@echo "Or use: gdb $(TARGET) -c core"

# Run with gdb
.PHONY: gdb
gdb: $(TARGET)
	@echo "Starting gdb..."
	@ulimit -c unlimited
	gdb --args $(TARGET) $$ARGS

# Run with valgrind (detects memory errors)
# Usage: make valgrind ARGS="<arguments>"
.PHONY: valgrind
valgrind: $(TARGET)
	@if [ -z "$$ARGS" ]; then \
		echo "Error: ARGS not set. Usage: make valgrind ARGS=\"<bytecode_file>\"" >&2; \
		exit 1; \
	fi
	@echo "Running with valgrind..."
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
		--log-file=valgrind.log ./$(TARGET) $$ARGS
	@echo "Valgrind log saved to valgrind.log"

