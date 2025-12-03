# BGP Simulator

A high-performance BGP (Border Gateway Protocol) simulator that models internet routing announcements and propagation across an Autonomous System (AS) graph.

## Overview

This simulator implements a BGP routing protocol simulator that:
- Builds an AS graph from CAIDA relationship data
- Seeds routing announcements from CSV files
- Propagates announcements following BGP best path selection rules
- Implements Route Origin Validation (ROV) for security
- Outputs routing tables (RIBs) in CSV format

## Architecture & Design Decisions

### Core Components

#### 1. **ASGraph** (`include/ASGraph.h`, `src/ASGraph.cpp`)
- **Purpose**: Represents the internet AS topology as a directed graph
- **Storage**: Uses `std::unordered_map<uint32_t, std::shared_ptr<ASNode>>` for O(1) ASN lookups
- **Relationships**: Each ASNode maintains separate vectors for providers, customers, and peers
- **Design Choice**: Raw pointers for edges (`std::vector<ASNode*>`) to avoid circular dependency overhead while memory is managed by shared_ptr in the graph container

#### 2. **Announcement** (`include/Announcement.h`)
- **Structure**: Lightweight struct containing prefix, AS path, next hop, relationship, and ROV validity
- **AS Path**: Stored as `std::vector<uint32_t>` for efficient prepending operations
- **Design Choice**: Simple struct over class for minimal overhead in high-frequency operations

#### 3. **Policy System** (`include/Policy.h`)
- **BGP Policy**: Base policy class managing local RIB and received queue
- **ROV Policy**: Extends BGP to filter invalid announcements
- **Design Choice**: Inheritance-based policy system allows easy extension without modifying core propagation logic
- **Storage**: 
  - `local_rib`: `std::unordered_map<std::string, Announcement>` - O(1) prefix lookups
  - `received_queue`: `std::unordered_map<std::string, std::vector<Announcement>>` - Groups announcements by prefix

#### 4. **Propagation Engine** (`include/Propagation.h`, `src/Propagation.cpp`)
- **Purpose**: Encapsulates all BGP propagation logic in a separate module
- **Design Choice**: Separates propagation implementation from main orchestration code
- **Benefits**: 
  - Cleaner main.cpp focused on high-level flow
  - Easier to test propagation logic independently
  - Better code organization and maintainability
- **Structure**: Static class with three-phase propagation methods (UP, ACROSS, DOWN)

### Propagation Algorithm

The simulator uses a **three-phase propagation** approach that respects BGP's valley-free routing. All propagation logic is encapsulated in the `PropagationEngine` class for better code organization.

#### Phase 1: Propagate UP (Customers → Providers)
- **Rank-based processing**: Uses graph ranking to process from lowest (customers) to highest (providers)
- **Efficiency**: Processes all nodes at each rank before moving up, ensuring correct dependency order
- **Implementation**: Two-pass per rank - first process received announcements, then send to providers
- **Location**: `PropagationEngine::propagate_up()`

#### Phase 2: Propagate ACROSS (Peers)
- **Single-hop only**: Prevents valley routing by limiting peer propagation to one hop
- **Implementation**: All ASes send to peers, then all process received peer announcements
- **Design Choice**: Separate send-then-process phases prevent multi-hop peer propagation
- **Location**: `PropagationEngine::propagate_across()`

#### Phase 3: Propagate DOWN (Providers → Customers)
- **Reverse rank processing**: Processes from highest rank down to lowest
- **Efficiency**: Ensures providers have processed all announcements before sending to customers
- **Location**: `PropagationEngine::propagate_down()`

**Entry Point**: `PropagationEngine::run_propagation()` executes all three phases in sequence.

### Graph Ranking (Flattening)

The graph is "flattened" into ranks for efficient propagation:
- **Rank 0**: ASes with no customers (leaf nodes)
- **Rank N**: ASes whose customers are all at rank N-1 or lower
- **Efficiency**: Pre-computed ranking allows O(1) access to all ASes at a given level
- **Implementation**: BFS-based ranking algorithm in `getRankedASes()`

## Optimizations

### 1. **Efficient Data Structures**
- **Hash maps everywhere**: `std::unordered_map` for O(1) average-case lookups
  - ASN → ASNode mapping
  - Prefix → Announcement mapping
  - Prefix → Received announcements queue
- **Vectors for relationships**: `std::vector<ASNode*>` for edge storage (cache-friendly, minimal overhead)

### 2. **Memory Management**
- **Shared pointers**: `std::shared_ptr<ASNode>` ensures automatic cleanup and prevents memory leaks
- **Raw pointers for edges**: Reduces overhead while maintaining safety through shared_ptr ownership
- **Policy polymorphism**: `std::shared_ptr<Policy>` allows BGP/ROV switching without copying

### 3. **Propagation Optimizations**
- **Rank-based processing**: Eliminates redundant checks by processing in dependency order
- **Batch processing**: Process all announcements at a rank before moving to next rank
- **Early filtering**: ROV checks happen during `process_announcements()` to avoid unnecessary propagation

### 4. **Best Path Selection**
- **Three-tier comparison**: Relationship → Path Length → Next Hop ASN
- **Efficiency**: Early termination when relationship differs (most common case)
- **Implementation**: Single comparison function with relationship score map (O(1) lookup)

### 5. **CSV Parsing**
- **Streaming reads**: Processes files line-by-line without loading entire file into memory
- **Error handling**: Continues processing on malformed lines with warnings
- **Minimal allocations**: Reuses string parsing objects

### 6. **Output Formatting**
- **In-place string building**: Constructs AS path tuples during iteration
- **Single file write**: Buffered I/O for efficient disk writes
- **Python tuple format**: Matches expected output format exactly (including trailing comma for single-element tuples)

## Performance Characteristics

- **Time Complexity**:
  - Graph building: O(E) where E is number of relationships
  - Propagation: O(V × P × D) where V=vertices, P=prefixes, D=average degree
  - Best path selection: O(1) per comparison (early termination)
  
- **Space Complexity**:
  - Graph storage: O(V + E)
  - RIB storage: O(V × P) in worst case
  - Propagation queues: O(V × P × D) during active propagation

## Usage

### Compilation

```bash
g++ src/*.cpp -Iinclude -o bgp_simulator -std=c++17 -lcurl
```

### Running the Simulator

```bash
./bgp_simulator --relationships <caida_file> --announcements <announcements.csv> --rov-asns <rov_asns.csv>
```

**Arguments:**
- `--relationships`: CAIDA AS relationship file (e.g., `CAIDAASGraphCollector_2025.10.16.txt`)
- `--announcements`: CSV file with format: `seed_asn,prefix,rov_invalid`
- `--rov-asns`: Text file with one ASN per line that deploy ROV

### Example

```bash
./bgp_simulator \
  --relationships ../bench/many/CAIDAASGraphCollector_2025.10.16.txt \
  --announcements ../bench/many/anns.csv \
  --rov-asns ../bench/many/rov_asns.csv
```

### Output

The simulator generates `ribs.csv` with format:
```
asn,prefix,as_path
1,10.0.0.0/24,"(1,)"
23477,10.3.0.0/24,"(23477, 401707, 19151, 226, 4)"
```

**Note**: AS paths are formatted as Python tuples with proper trailing commas for single-element tuples.

### Comparing Output

Use the provided comparison script:
```bash
./bench/compare_output.sh <expected_ribs.csv> <actual_ribs.csv>
```

The script sorts both files and compares them, ignoring whitespace differences.

## File Structure

```
cse3150_final_project/
├── src/
│   ├── main.cpp              # Main orchestration and high-level flow
│   ├── Propagation.cpp       # BGP propagation engine (three-phase logic)
│   ├── ASGraph.cpp           # Graph implementation and ranking
│   ├── parse_caida.cpp      # CAIDA file parsing
│   └── download_CADIA.cpp   # CAIDA data download utilities
├── include/
│   ├── Propagation.h         # PropagationEngine class interface
│   ├── ASGraph.h             # Graph and ASNode definitions
│   ├── Announcement.h        # Announcement struct and Relationship enum
│   ├── Policy.h              # BGP and ROV policy classes
│   └── parse_caida.h         # Parsing function declarations
├── tests/
│   ├── test_as_graph.cpp     # Unit tests for AS graph creation
│   ├── test_bgp_system.cpp   # System tests for BGP propagation
│   └── TESTING.md            # Testing documentation
├── README.md                 # This file
└── bgp_simulator             # Compiled executable
```

### Code Organization Philosophy

The codebase follows a **separation of concerns** principle:

- **src/**: Production source code
  - **main.cpp**: High-level orchestration - argument parsing, file I/O, calling modules
  - **Propagation.cpp**: All BGP propagation logic - three-phase algorithm, best path selection
  - **ASGraph.cpp**: Graph structure and operations - ranking, cycle detection, node management
  - **parse_caida.cpp**: Input parsing - CAIDA file format handling
- **tests/**: Test code separated from production code
  - **test_as_graph.cpp**: Unit tests for graph functionality
  - **test_bgp_system.cpp**: System tests for BGP propagation
  - **TESTING.md**: Testing documentation

This organization makes the code:
- **More maintainable**: Each module has a clear responsibility, tests are separate from production code
- **Easier to test**: Modules can be tested independently, test files are organized in dedicated directory
- **More readable**: main.cpp shows the high-level flow without implementation details
- **More extensible**: New propagation algorithms can be added without touching main.cpp
- **Better organized**: Clear separation between production code (`src/`) and test code (`tests/`)

## Key Features

### 1. BGP Best Path Selection
Implements the three-tier BGP decision process:
1. **Relationship preference**: Customer > Peer > Provider
2. **AS Path length**: Shorter paths preferred
3. **Next Hop ASN**: Lower ASN preferred (tiebreaker)

### 2. Route Origin Validation (ROV)
- ASes with ROV policy drop announcements marked as `rov_invalid=true`
- Filtering happens during announcement processing to prevent invalid routes from propagating
- ROV ASNs are read from CSV and policies are applied before propagation

### 3. Cycle Detection
- Detects provider-customer cycles in the input topology
- Uses DFS-based cycle detection algorithm
- Program terminates with error message if cycles are found

### 4. Valley-Free Routing
- Enforces BGP's valley-free property:
  - Announcements go UP (customers → providers)
  - Then ACROSS (peers, one hop only)
  - Then DOWN (providers → customers)
- Prevents routing loops and ensures economic routing preferences

## Design Philosophy

### Clean Code Principles
- **Separation of concerns**: Graph, policies, propagation, and orchestration are separate modules
  - **Graph operations**: ASGraph handles topology and ranking
  - **Propagation logic**: PropagationEngine handles all BGP propagation
  - **Orchestration**: main.cpp coordinates the overall flow
- **Single responsibility**: Each class/function has one clear purpose
- **Modular design**: Propagation and graph ranking abstracted from main for better organization
- **Readable code**: Well-commented, descriptive variable names
- **Error handling**: Graceful error messages and validation

### Performance vs. Readability Trade-offs
- **Optimized where it matters**: Hash maps for frequent lookups, rank-based propagation
- **Readable where speed isn't critical**: Clear function names, comments, structured code
- **No premature optimization**: Simple, correct code first, then optimize bottlenecks

## Testing

The simulator includes comprehensive testing at multiple levels:

### Unit Tests (`tests/test_as_graph.cpp`)
Tests for AS graph creation and validation:
- Simple graph creation
- Provider cycle detection
- Peer relationship handling
- Complex graph structures

**Run:** `g++ tests/test_as_graph.cpp src/ASGraph.cpp -Iinclude -o test_as_graph -std=c++17 && ./test_as_graph`

### System Tests (`tests/test_bgp_system.cpp`)
End-to-end tests for BGP propagation:
- Single announcement with tiny graph
- Larger graph propagation
- Multiple announcements for same prefix (best path selection)
- Customer vs provider preference
- Output format verification

**Run:** `g++ tests/test_bgp_system.cpp src/Propagation.cpp src/ASGraph.cpp src/parse_caida.cpp -Iinclude -o test_bgp_system -std=c++17 -lcurl && ./test_bgp_system`

### Benchmark Tests
Validated against provided benchmark datasets:
- `bench/many/`: Large-scale test with ~2.9M routing entries
- `bench/prefix/`: Medium-scale test
- `bench/subprefix/`: Small-scale test

All benchmark tests pass the comparison script, confirming correct BGP propagation and output format.

See `TESTING.md` for detailed testing documentation.

## Future Improvements

Potential optimizations for larger datasets:
- **Parallel processing**: Propagate different prefixes in parallel
- **Incremental updates**: Only process changed announcements
- **Memory pooling**: Reuse announcement objects
- **Compressed storage**: Use more compact data structures for AS paths
- **Output streaming**: Write results incrementally instead of building in memory

## License

This project is part of CSE3150 coursework.

