#!/bin/bash

# Run all benchmark tests and compare outputs
# Usage: ./run_all_tests.sh
# Run from: cse3150_final_project/ directory

# Don't use set -e here because arithmetic expansion can return non-zero
# We handle errors explicitly in the test function

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Paths
BENCH_DIR="$PROJECT_ROOT/bench"
SIMULATOR="$SCRIPT_DIR/bgp_simulator"
COMPARE_SCRIPT="$BENCH_DIR/compare_output.sh"
OUTPUT_FILE="$SCRIPT_DIR/ribs.csv"

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "=========================================="
echo "BGP Simulator Benchmark Tests"
echo "=========================================="
echo ""

# Check if simulator exists
if [ ! -f "$SIMULATOR" ]; then
    echo -e "${RED}Error: bgp_simulator not found at $SIMULATOR${NC}"
    echo "Please compile first: g++ src/*.cpp -Iinclude -o bgp_simulator -std=c++17 -lcurl"
    exit 1
fi

# Function to run a single test
run_test() {
    local test_name=$1
    local bench_dir=$2
    
    echo -e "${YELLOW}Running test: $test_name${NC}"
    echo "----------------------------------------"
    
    # Run simulator
    "$SIMULATOR" \
        --relationships "$bench_dir/CAIDAASGraphCollector_2025.10.16.txt" \
        --announcements "$bench_dir/anns.csv" \
        --rov-asns "$bench_dir/rov_asns.csv"
    
    if [ $? -ne 0 ]; then
        echo -e "${RED}FAILED: Simulator error for $test_name${NC}"
        return 1
    fi
    
    # Compare output
    echo ""
    echo "Comparing output..."
    "$COMPARE_SCRIPT" "$bench_dir/ribs.csv" "$OUTPUT_FILE"
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}PASSED: $test_name${NC}"
        echo ""
        return 0
    else
        echo -e "${RED}FAILED: $test_name - Output does not match${NC}"
        echo ""
        return 1
    fi
}

# Track results
PASSED=0
FAILED=0

# Run all tests
# Test 1: many/
# Equivalent to: ./bgp_simulator --relationships ../bench/many/CAIDAASGraphCollector_2025.10.16.txt --announcements ../bench/many/anns.csv --rov-asns ../bench/many/rov_asns.csv
#              ./compare_output.sh ../bench/many/ribs.csv ribs.csv
echo "Test 1: many/"
if run_test "many" "$BENCH_DIR/many"; then
    PASSED=$((PASSED + 1))
else
    FAILED=$((FAILED + 1))
fi

# Test 2: prefix/
# Equivalent to: ./bgp_simulator --relationships ../bench/prefix/CAIDAASGraphCollector_2025.10.16.txt --announcements ../bench/prefix/anns.csv --rov-asns ../bench/prefix/rov_asns.csv
#              ./compare_output.sh ../bench/prefix/ribs.csv ribs.csv
echo "Test 2: prefix/"
if run_test "prefix" "$BENCH_DIR/prefix"; then
    PASSED=$((PASSED + 1))
else
    FAILED=$((FAILED + 1))
fi

# Test 3: subprefix/
# Equivalent to: ./bgp_simulator --relationships ../bench/subprefix/CAIDAASGraphCollector_2025.10.16.txt --announcements ../bench/subprefix/anns.csv --rov-asns ../bench/subprefix/rov_asns.csv
#              ./compare_output.sh ../bench/subprefix/ribs.csv ribs.csv
echo "Test 3: subprefix/"
if run_test "subprefix" "$BENCH_DIR/subprefix"; then
    PASSED=$((PASSED + 1))
else
    FAILED=$((FAILED + 1))
fi

# Summary
echo "=========================================="
echo "Test Summary"
echo "=========================================="
echo -e "${GREEN}Passed: $PASSED${NC}"
if [ $FAILED -gt 0 ]; then
    echo -e "${RED}Failed: $FAILED${NC}"
    exit 1
else
    echo -e "${GREEN}Failed: $FAILED${NC}"
    exit 0
fi

