#!/bin/bash

# Test runner for Lama interpreter
# Usage: ./run_tests.sh [--silent] <test_folder> [test_name]
#   --silent: don't create folders or save output files to disk
#   test_folder: folder containing .lama/.input files
#   test_name: optional, run only this specific test (without .lama extension)

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LAMAC="${LAMAC:-lamac}"
INTERPRETER="${INTERPRETER:-${SCRIPT_DIR}/build/interpreter}"
TEST_RESULTS_DIR="${SCRIPT_DIR}/test_results"
SILENT_MODE=false
SHARED_BC_FILE=""

# Parse flags
while [[ $# -gt 0 ]]; do
    case "$1" in
        --silent)
            SILENT_MODE=true
            shift
            ;;
        -*)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
        *)
            break
            ;;
    esac
done

# Check arguments
if [ $# -lt 1 ]; then
    echo "Usage: $0 [--silent] <test_folder> [test_name]"
    echo "  --silent: don't create folders or save output files to disk"
    echo "  test_folder: folder containing .lama/.input files"
    echo "  test_name: optional, run only this specific test (without .lama extension)"
    exit 1
fi

TEST_FOLDER="$1"
TEST_NAME="$2"

# Validate test folder
if [ ! -d "$TEST_FOLDER" ]; then
    echo -e "${RED}Error: Test folder '$TEST_FOLDER' does not exist${NC}" >&2
    exit 1
fi

# Check if lamac exists
if ! command -v "$LAMAC" &> /dev/null; then
    echo -e "${RED}Error: lamac not found. Please set LAMAC environment variable or ensure lamac is in PATH${NC}" >&2
    exit 1
fi

# Check if interpreter exists
if [ ! -f "$INTERPRETER" ]; then
    echo -e "${RED}Error: Interpreter not found at '$INTERPRETER'${NC}" >&2
    echo "Please build the interpreter first: cd interpreter && make" >&2
    exit 1
fi

# Create test results directory (unless silent mode)
if [ "$SILENT_MODE" = false ]; then
    mkdir -p "$TEST_RESULTS_DIR"
else
    # Create single shared temp file for bytecode (reused across all tests)
    SHARED_BC_FILE="$(mktemp)"
    # Cleanup on any exit: normal, error, Ctrl+C, kill, hangup, etc.
    trap 'rm -f "$SHARED_BC_FILE" 2>/dev/null' EXIT INT TERM HUP QUIT
fi

# Function to run a single test
run_test() {
    local test_base="$1"
    local lama_file="${test_base}.lama"
    local input_file="${test_base}.input"
    local bc_file ref_output_file cpp_output_file cpp_log
    local ref_output_var cpp_output_var
    
    # Set up output files/variables based on mode
    if [ "$SILENT_MODE" = true ]; then
        # Silent mode: reuse shared bc file, compare outputs in memory
        bc_file="$SHARED_BC_FILE"
        cpp_log="/dev/null"
    else
        bc_file="${TEST_RESULTS_DIR}/$(basename "${test_base}").bc"
        ref_output_file="${TEST_RESULTS_DIR}/$(basename "${test_base}").ref.out"
        cpp_output_file="${TEST_RESULTS_DIR}/$(basename "${test_base}").cpp.out"
        cpp_log="${TEST_RESULTS_DIR}/$(basename "${test_base}").cpp.log"
    fi
    
    echo -n "Testing $(basename "$test_base")... "
    
    # Step 1: Generate bytecode file
    # lamac -b generates .bc file in the current working directory (project root)
    local lama_base="$(basename "$lama_file" .lama)"
    # Get absolute paths
    local lama_file_abs="$(cd "$(dirname "$lama_file")" && pwd)/$(basename "$lama_file")"
    local test_folder_abs="$(cd "$TEST_FOLDER" && pwd)"
    local project_root="$(dirname "$test_folder_abs")"
    local generated_bc="${project_root}/${lama_base}.bc"
    
    # Change to project root before running lamac
    local old_pwd="$(pwd)"
    cd "$project_root"
    
    if ! "$LAMAC" -b "$lama_file_abs" > /dev/null 2>&1; then
        cd "$old_pwd"
        echo -e "${RED}FAILED${NC}"
        echo "  Error: Failed to generate bytecode file" >&2
        return 1
    fi
    
    # Move generated .bc file to destination
    if [ -f "$generated_bc" ]; then
        mv "$generated_bc" "$bc_file"
        cd "$old_pwd"
    else
        cd "$old_pwd"
        echo -e "${RED}FAILED${NC}"
        echo "  Error: Bytecode file not generated: $generated_bc" >&2
        return 1
    fi
    
    # Step 2 & 3: Run interpreters and compare
    if [ "$SILENT_MODE" = true ]; then
        # Silent mode: capture outputs in variables (no temp files)
        if [ -f "$input_file" ]; then
            ref_output_var=$("$LAMAC" -i "$lama_file" < "$input_file" 2>&1) || {
                echo -e "${RED}FAILED${NC}"
                echo "  Error: Reference interpreter failed" >&2
                return 1
            }
            cpp_output_var=$("$INTERPRETER" "$bc_file" "$cpp_log" < "$input_file" 2>&1) || {
                echo -e "${RED}FAILED${NC}"
                echo "  Error: C++ interpreter failed" >&2
                return 1
            }
        else
            ref_output_var=$("$LAMAC" -i "$lama_file" 2>&1) || {
                echo -e "${RED}FAILED${NC}"
                echo "  Error: Reference interpreter failed" >&2
                return 1
            }
            cpp_output_var=$("$INTERPRETER" "$bc_file" "$cpp_log" 2>&1) || {
                echo -e "${RED}FAILED${NC}"
                echo "  Error: C++ interpreter failed" >&2
                return 1
            }
        fi
        
        # Compare in memory
        if [ "$ref_output_var" != "$cpp_output_var" ]; then
            echo -e "${RED}FAILED${NC}"
            echo "  Error: Output mismatch" >&2
            echo "  Diff:" >&2
            diff <(echo "$ref_output_var") <(echo "$cpp_output_var") >&2 || true
            return 1
        fi
    else
        # Normal mode: use files
        if [ -f "$input_file" ]; then
            if ! "$LAMAC" -i "$lama_file" < "$input_file" > "$ref_output_file" 2>&1; then
                echo -e "${RED}FAILED${NC}"
                echo "  Error: Reference interpreter failed" >&2
                return 1
            fi
            if ! "$INTERPRETER" "$bc_file" "$cpp_log" < "$input_file" > "$cpp_output_file" 2>&1; then
                echo -e "${RED}FAILED${NC}"
                echo "  Error: C++ interpreter failed (exit code: $?)" >&2
                return 1
            fi
        else
            if ! "$LAMAC" -i "$lama_file" > "$ref_output_file" 2>&1; then
                echo -e "${RED}FAILED${NC}"
                echo "  Error: Reference interpreter failed" >&2
                return 1
            fi
            if ! "$INTERPRETER" "$bc_file" "$cpp_log" > "$cpp_output_file" 2>&1; then
                echo -e "${RED}FAILED${NC}"
                echo "  Error: C++ interpreter failed (exit code: $?)" >&2
                return 1
            fi
        fi
        
        # Compare files
        if ! diff -q "$ref_output_file" "$cpp_output_file" > /dev/null 2>&1; then
            echo -e "${RED}FAILED${NC}"
            echo "  Error: Output mismatch" >&2
            echo "  Reference output saved to: $ref_output_file" >&2
            echo "  C++ output saved to: $cpp_output_file" >&2
            echo "  Diff:" >&2
            diff "$ref_output_file" "$cpp_output_file" >&2 || true
            return 1
        fi
    fi
    
    echo -e "${GREEN}PASSED${NC}"
    return 0
}

# Find all test files
if [ -n "$TEST_NAME" ]; then
    # Run single test
    test_file="${TEST_FOLDER}/${TEST_NAME}.lama"
    if [ ! -f "$test_file" ]; then
        echo -e "${RED}Error: Test file '$test_file' not found${NC}" >&2
        exit 1
    fi
    test_base="${TEST_FOLDER}/${TEST_NAME}"
    if ! run_test "$test_base"; then
        exit 1
    fi
else
    # Run all tests
    failed_tests=()
    total=0
    passed=0
    
    # Find all .lama files
    while IFS= read -r lama_file; do
        test_base="${lama_file%.lama}"
        total=$((total + 1))
        if ! run_test "$test_base"; then
            failed_tests+=("$(basename "$test_base")")
        else
            passed=$((passed + 1))
        fi
    done < <(find "$TEST_FOLDER" -name "*.lama" -type f | sort)
    
    # Summary
    echo ""
    echo "========================================="
    echo "Test Summary:"
    echo "  Total:  $total"
    echo "  Passed: $passed"
    echo "  Failed: $((total - passed))"
    echo "========================================="
    
    if [ ${#failed_tests[@]} -gt 0 ]; then
        echo -e "${RED}Failed tests:${NC}"
        for test in "${failed_tests[@]}"; do
            echo "  - $test"
        done
        exit 1
    else
        echo -e "${GREEN}All tests passed!${NC}"
        exit 0
    fi
fi

