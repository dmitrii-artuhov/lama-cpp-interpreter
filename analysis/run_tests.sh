#!/bin/bash

# Test runner for Lama bytecode analyzer
# Usage: ./run_tests.sh [--silent] <test_folder> [test_name]
#   --silent: don't create folders or save output files to disk
#   test_folder: folder containing .lama files
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
ANALYZER="${ANALYZER:-${SCRIPT_DIR}/build/analysis}"
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
    echo "  test_folder: folder containing .lama files"
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

# Check if analyzer exists
if [ ! -f "$ANALYZER" ]; then
    echo -e "${RED}Error: Analyzer not found at '$ANALYZER'${NC}" >&2
    echo "Please build the analyzer first: cd analysis && make" >&2
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
    local bc_file log_file result_file
    
    # Set up output files/variables based on mode
    if [ "$SILENT_MODE" = true ]; then
        # Silent mode: reuse shared bc file, discard log and result
        bc_file="$SHARED_BC_FILE"
        log_file="/dev/null"
        result_file="/dev/null"
    else
        bc_file="${TEST_RESULTS_DIR}/$(basename "${test_base}").bc"
        log_file="${TEST_RESULTS_DIR}/$(basename "${test_base}").log"
        result_file="${TEST_RESULTS_DIR}/$(basename "${test_base}").result"
    fi
    
    echo -n "Analyzing $(basename "$test_base")... "
    
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
    
    # Step 2: Run analyzer (redirect stdout to result file, stderr to terminal)
    if ! "$ANALYZER" "$bc_file" "$log_file" > "$result_file" 2>&1; then
        echo -e "${RED}FAILED${NC}"
        echo "  Error: Analyzer failed (exit code: $?)" >&2
        return 1
    fi
    
    echo -e "${GREEN}OK${NC}"
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

