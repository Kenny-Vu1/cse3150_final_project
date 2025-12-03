# Testing Documentation

This document describes the testing strategy and test coverage for the BGP Simulator.

## Test Structure

The project includes two test suites:

### Unit Tests (`tests/test_as_graph.cpp`)
Tests for AS graph creation and validation:
- **Simple Graph**: Basic provider-customer relationships
- **Provider Cycle Detection**: Verifies cycle detection works correctly
- **Peer Relationships**: Ensures peer relationships don't trigger cycle detection
- **Complex Graph**: Tests larger graphs with mixed relationships

**Run with:**
```bash
g++ tests/test_as_graph.cpp src/ASGraph.cpp -Iinclude -o test_as_graph -std=c++17
./test_as_graph
```

### System Tests (`tests/test_bgp_system.cpp`)
End-to-end tests for BGP propagation:
- **Single Announcement (Tiny Graph)**: Tests basic propagation with minimal graph
- **Larger Graph**: Tests propagation through multiple ASes with various relationships
- **Multiple Announcements (Same Prefix)**: Tests best path selection when multiple ASes announce the same prefix
- **Customer vs Provider Preference**: Verifies BGP relationship preference rules
- **Output Format**: Validates CSV output format matches expected specification

**Run with:**
```bash
g++ tests/test_bgp_system.cpp src/Propagation.cpp src/ASGraph.cpp src/parse_caida.cpp -Iinclude -o test_bgp_system -std=c++17 -lcurl
./test_bgp_system
```

## Test Coverage

### Requirements Coverage

#### Testing the AS Graph
- **Unit tests for graph creation**: `tests/test_as_graph.cpp` includes tests for:
  - Node creation and relationship addition
  - Cycle detection
  - Peer vs provider-customer relationships
  - Complex graph structures

- **System tests with mini-graphs**: `tests/test_bgp_system.cpp` includes:
  - Tiny graph (2 nodes)
  - Larger graph (5+ nodes)
  - Various relationship configurations

#### Output and Tests
- **Single announcement test**: `test_single_announcement_tiny_graph()` seeds one announcement and verifies propagation
- **Larger test graph**: `test_larger_graph()` tests with multiple ASes and relationships
- **Multiple announcements for same prefix**: `test_multiple_announcements_same_prefix()` tests best path selection
- **Output format verification**: `test_output_format()` validates CSV format including Python tuple syntax

## Test Execution

### Running All Tests

```bash
# Compile and run unit tests
g++ tests/test_as_graph.cpp src/ASGraph.cpp -Iinclude -o test_as_graph -std=c++17
./test_as_graph

# Compile and run system tests
g++ tests/test_bgp_system.cpp src/Propagation.cpp src/ASGraph.cpp src/parse_caida.cpp -Iinclude -o test_bgp_system -std=c++17 -lcurl
./test_bgp_system
```

### Expected Output

Both test suites should output:
- Individual test results (PASSED/FAILED)
- Summary of all tests completed

## Test Design Principles

### Unit Tests
- **Isolated**: Each test is independent and can run in any order
- **Fast**: Tests complete quickly (< 1 second)
- **Focused**: Each test verifies one specific aspect of graph functionality

### System Tests
- **Realistic**: Use actual BGP propagation logic
- **Comprehensive**: Cover main use cases and edge cases
- **Verifiable**: Check both intermediate state (RIBs) and final output format

## Integration with Benchmark Tests

In addition to unit and system tests, the simulator is validated against provided benchmark datasets:

- `bench/many/`: Large-scale test (~2.9M routing entries)
- `bench/prefix/`: Medium-scale test
- `bench/subprefix/`: Small-scale test

These are verified using the `compare_output.sh` script to ensure output matches expected results.

## Future Test Improvements

Potential additions:
- **Performance tests**: Measure propagation time for various graph sizes
- **ROV tests**: Verify ROV filtering works correctly
- **Edge case tests**: Empty graphs, disconnected components, etc.
- **Property-based tests**: Generate random graphs and verify invariants
- **Regression tests**: Capture known bugs and ensure they don't reappear

