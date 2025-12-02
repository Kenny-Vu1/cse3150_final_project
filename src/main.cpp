#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstdlib>

// Integrate our helper modules
#include "ASGraph.h"
#include "parse_caida.h"

// TODO: Future headers
// #include "Simulation.h" 

void print_usage(const char* prog_name) {
    std::cerr << "Usage: " << prog_name 
              << " --relationships <file> --announcements <file> --rov-asns <file>\n";
}

int main(int argc, char* argv[]) {
    // ---------------------------------------------------------
    // 1. Argument Parsing
    // ---------------------------------------------------------
    std::string rel_file;
    std::string ann_file;
    std::string rov_file;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--relationships") {
            if (i + 1 < argc) rel_file = argv[++i];
            else { std::cerr << "Error: --relationships requires a file path.\n"; return 1; }
        } else if (arg == "--announcements") {
            if (i + 1 < argc) ann_file = argv[++i];
            else { std::cerr << "Error: --announcements requires a file path.\n"; return 1; }
        } else if (arg == "--rov-asns") {
            if (i + 1 < argc) rov_file = argv[++i];
            else { std::cerr << "Error: --rov-asns requires a file path.\n"; return 1; }
        }
    }

    if (rel_file.empty() || ann_file.empty() || rov_file.empty()) {
        std::cerr << "Error: Missing required arguments.\n";
        print_usage(argv[0]);
        return 1;
    }

    std::cout << "Starting Simulation...\n";
    std::cout << "Relationships File: " << rel_file << "\n";
    std::cout << "Announcements File: " << ann_file << "\n";
    std::cout << "ROV ASNs File:      " << rov_file << "\n";

    // ---------------------------------------------------------
    // 2. Build the AS Graph (Phase 1)
    // ---------------------------------------------------------
    std::cout << "\n[Step 1] Building AS Graph...\n";
    
    // Instantiate the Graph
    ASGraph graph;

    // Parse the CAIDA file and populate the graph
    // Passing 'graph' by reference so it gets filled
    parse_caida(rel_file, graph);

    // Check for Provider/Customer cycles
    // The spec requires the program to output a reasonable print statement and end 
    // if a cycle is detected.
    if (graph.detectProviderCycles()) {
        std::cerr << "CRITICAL ERROR: Provider cycle detected in input topology. Aborting.\n";
        return 1;
    }

    std::cout << "[Info] AS Graph built successfully (" << graph.getNumNodes() << " nodes).\n";


    // ---------------------------------------------------------
    // 3. Configure ROV (Phase 4)
    // ---------------------------------------------------------
    // TODO: Read rov_file. For every ASN in this file, set their policy to ROV.
    
    std::cout << "[Info] ROV policies applied (Placeholder).\n";


    // ---------------------------------------------------------
    // 4. Seed Announcements (Phase 3.4)
    // ---------------------------------------------------------
    // TODO: Parse ann_file. Format: ASN, Prefix, rov_invalid.
    // Seed these into the Local RIB of the specific ASNs.
    
    std::cout << "[Info] Announcements seeded (Placeholder).\n";


    // ---------------------------------------------------------
    // 5. Run Propagation (Phase 3.5)
    // ---------------------------------------------------------
    // TODO: Execute the propagation stages: Up, Across, Down.
    
    std::cout << "[Info] Propagation complete (Placeholder).\n";


    // ---------------------------------------------------------
    // 6. Output Results (Phase 3.7)
    // ---------------------------------------------------------
    std::ofstream out_file("ribs.csv");
    if (!out_file.is_open()) {
        std::cerr << "Error: Could not open ribs.csv for writing.\n";
        return 1;
    }

    // Write Header: "asn", "prefix", "as path"
    out_file << "asn,prefix,as path\n";

    // TODO: Iterate through all AS nodes and their Local RIBs to dump data
    // for (const auto& pair : graph.getNodes()) { // Assuming accessor exists
    //      auto node = pair.second;
    //      write_rib_to_file(out_file, node);
    // }

    out_file.close();
    std::cout << "[Success] ribs.csv generated successfully.\n";

    return 0;
}