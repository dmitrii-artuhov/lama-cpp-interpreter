#!/bin/bash

# Benchmark script for Lama interpreters
# Usage: ./benchmark.sh <test_folder> [test_name]
#   test_folder: folder containing .lama files
#   test_name: optional, run only this specific test (without .lama extension)

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LAMAC="${LAMAC:-lamac}"
INTERPRETER="${INTERPRETER:-${SCRIPT_DIR}/build/interpreter}"

# Totals (in nanoseconds for precision, converted at the end)
TOTAL_LAMA_TIME_NS=0
TOTAL_CPP_TIME_NS=0
TOTAL_TESTS=0
PASSED_TESTS=0
SKIPPED_TESTS=0

# Temp files
SHARED_BC_FILE=""
cleanup() {
    rm -f "$SHARED_BC_FILE" 2>/dev/null
}
trap cleanup EXIT INT TERM HUP QUIT

# Helper: convert nanoseconds to milliseconds with 2 decimal places
ns_to_ms() {
    awk "BEGIN { printf \"%.2f\", $1 / 1000000 }"
}

# Helper: calculate speedup ratio
calc_speedup() {
    local lama_ns="$1"
    local cpp_ns="$2"
    if [ "$cpp_ns" -gt 0 ]; then
        awk "BEGIN { printf \"%.2f\", $lama_ns / $cpp_ns }"
    else
        echo "N/A"
    fi
}

# Check arguments
if [ $# -lt 1 ]; then
    echo "Usage: $0 <test_folder> [test_name]"
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
    echo -e "${RED}Error: lamac not found. Set LAMAC environment variable or ensure lamac is in PATH${NC}" >&2
    exit 1
fi

# Check if interpreter exists
if [ ! -f "$INTERPRETER" ]; then
    echo -e "${RED}Error: Interpreter not found at '$INTERPRETER'${NC}" >&2
    echo "Please build the interpreter first: cd interpreter && make release" >&2
    exit 1
fi

# Create shared temp file for bytecode
SHARED_BC_FILE="$(mktemp)"

# Function to run a single benchmark
run_benchmark() {
    local test_base="$1"
    local lama_file="${test_base}.lama"
    local input_file="${test_base}.input"
    local bc_file="$SHARED_BC_FILE"
    
    local lama_time_ns cpp_time_ns
    local lama_ok=false cpp_ok=false
    local lama_output cpp_output
    
    echo -n "$(basename "$test_base"): "
    
    # Step 1: Generate bytecode file
    local lama_base="$(basename "$lama_file" .lama)"
    local lama_file_abs="$(cd "$(dirname "$lama_file")" && pwd)/$(basename "$lama_file")"
    local test_folder_abs="$(cd "$TEST_FOLDER" && pwd)"
    local project_root="$(dirname "$test_folder_abs")"
    local generated_bc="${project_root}/${lama_base}.bc"
    
    local old_pwd="$(pwd)"
    cd "$project_root"
    
    if ! "$LAMAC" -b "$lama_file_abs" > /dev/null 2>&1; then
        cd "$old_pwd"
        echo -e "${YELLOW}SKIPPED${NC} (bytecode generation failed)"
        TOTAL_TESTS=$((TOTAL_TESTS + 1))
        SKIPPED_TESTS=$((SKIPPED_TESTS + 1))
        return 0
    fi
    
    if [ -f "$generated_bc" ]; then
        mv "$generated_bc" "$bc_file"
        cd "$old_pwd"
    else
        cd "$old_pwd"
        echo -e "${YELLOW}SKIPPED${NC} (bytecode not generated)"
        TOTAL_TESTS=$((TOTAL_TESTS + 1))
        SKIPPED_TESTS=$((SKIPPED_TESTS + 1))
        return 0
    fi
    
    # Step 2: Run Lama interpreter and measure time
    local start_lama end_lama
    start_lama=$(date +%s%N)
    if [ -f "$input_file" ]; then
        lama_output=$("$LAMAC" -i "$lama_file" < "$input_file" 2>&1) && lama_ok=true || true
    else
        lama_output=$("$LAMAC" -i "$lama_file" 2>&1) && lama_ok=true || true
    fi
    end_lama=$(date +%s%N)
    lama_time_ns=$((end_lama - start_lama))
    
    # Step 3: Run C++ interpreter and measure time
    local start_cpp end_cpp
    start_cpp=$(date +%s%N)
    if [ -f "$input_file" ]; then
        cpp_output=$("$INTERPRETER" "$bc_file" /dev/null < "$input_file" 2>&1) && cpp_ok=true || true
    else
        cpp_output=$("$INTERPRETER" "$bc_file" /dev/null 2>&1) && cpp_ok=true || true
    fi
    end_cpp=$(date +%s%N)
    cpp_time_ns=$((end_cpp - start_cpp))
    
    # Convert to ms for display
    local lama_time_ms cpp_time_ms
    lama_time_ms=$(ns_to_ms "$lama_time_ns")
    cpp_time_ms=$(ns_to_ms "$cpp_time_ns")
    
    # Step 4: Report results
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    if [ "$lama_ok" = true ] && [ "$cpp_ok" = true ]; then
        # Both succeeded - check output matches
        if [ "$lama_output" = "$cpp_output" ]; then
            PASSED_TESTS=$((PASSED_TESTS + 1))
            TOTAL_LAMA_TIME_NS=$((TOTAL_LAMA_TIME_NS + lama_time_ns))
            TOTAL_CPP_TIME_NS=$((TOTAL_CPP_TIME_NS + cpp_time_ns))
            
            # Calculate speedup
            local speedup
            speedup=$(calc_speedup "$lama_time_ns" "$cpp_time_ns")
            
            echo -e "${GREEN}OK${NC}  lama: ${CYAN}${lama_time_ms}ms${NC}  cpp: ${CYAN}${cpp_time_ms}ms${NC}  (${speedup}x)"
        else
            echo -e "${YELLOW}MISMATCH${NC}  lama: ${CYAN}${lama_time_ms}ms${NC}  cpp: ${CYAN}${cpp_time_ms}ms${NC}"
            echo "    Output differs between interpreters"
        fi
    elif [ "$lama_ok" = true ]; then
        echo -e "${RED}CPP FAILED${NC}  lama: ${CYAN}${lama_time_ms}ms${NC}  cpp: ${RED}error${NC}"
    elif [ "$cpp_ok" = true ]; then
        echo -e "${RED}LAMA FAILED${NC}  lama: ${RED}error${NC}  cpp: ${CYAN}${cpp_time_ms}ms${NC}"
    else
        echo -e "${RED}BOTH FAILED${NC}"
    fi
    
    return 0
}

# Main execution
echo -e "${BOLD}Lama Interpreter Benchmark${NC}"
echo "========================================="
echo ""

if [ -n "$TEST_NAME" ]; then
    # Run single test
    test_file="${TEST_FOLDER}/${TEST_NAME}.lama"
    if [ ! -f "$test_file" ]; then
        echo -e "${RED}Error: Test file '$test_file' not found${NC}" >&2
        exit 1
    fi
    test_base="${TEST_FOLDER}/${TEST_NAME}"
    run_benchmark "$test_base"
else
    # Run all tests in folder
    while IFS= read -r lama_file; do
        test_base="${lama_file%.lama}"
        run_benchmark "$test_base"
    done < <(find "$TEST_FOLDER" -name "*.lama" -type f | sort)
fi

# Print summary
echo ""
echo "========================================="
echo -e "${BOLD}Summary${NC}"
echo "========================================="
echo "  Total tests:  $TOTAL_TESTS"
echo "  Passed:       $PASSED_TESTS"
echo "  Skipped:      $SKIPPED_TESTS"
echo "  Failed:       $((TOTAL_TESTS - PASSED_TESTS - SKIPPED_TESTS))"
echo ""
if [ "$PASSED_TESTS" -gt 0 ]; then
    TOTAL_LAMA_MS=$(ns_to_ms "$TOTAL_LAMA_TIME_NS")
    TOTAL_CPP_MS=$(ns_to_ms "$TOTAL_CPP_TIME_NS")
    TOTAL_SPEEDUP=$(calc_speedup "$TOTAL_LAMA_TIME_NS" "$TOTAL_CPP_TIME_NS")
    
    echo -e "  ${BOLD}Total Lama time:${NC}  ${CYAN}${TOTAL_LAMA_MS}ms${NC}"
    echo -e "  ${BOLD}Total C++ time:${NC}   ${CYAN}${TOTAL_CPP_MS}ms${NC}"
    echo -e "  ${BOLD}Overall speedup:${NC}  ${GREEN}${TOTAL_SPEEDUP}x${NC}"
fi
echo "========================================="
