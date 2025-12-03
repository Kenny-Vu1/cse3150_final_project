#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstdlib>
#include <sstream>
#include <unordered_set>

// Integrate our helper modules
#include "ASGraph.h"
#include "parse_caida.h"
#include "Announcement.h"
#include "Policy.h"
#include "Propagation.h" 

void print_usage(const char* prog_name) {
    std::cerr << "Usage: " << prog_name 
              << " --relationships <file> --announcements <file> --rov-asns <file>\n";
}

// Propagation logic has been moved to Propagation.cpp/Propagation.h
// This keeps main.cpp focused on orchestration rather than implementation details


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
    std::cout << "\n[Step 3] Configuring ROV policies...\n";
    std::unordered_set<uint32_t> rov_asns;
    
    std::ifstream rov_stream(rov_file);
    if (!rov_stream.is_open()) {
        std::cerr << "Error: Could not open ROV ASNs file: " << rov_file << std::endl;
        return 1;
    }
    
    std::string rov_line;
    // Skip header if present
    if (std::getline(rov_stream, rov_line)) {
        // Check if it's a header (contains non-numeric characters)
        bool is_header = false;
        for (char c : rov_line) {
            if (!std::isdigit(c) && c != '\r' && c != '\n') {
                is_header = true;
                break;
            }
        }
        if (!is_header) {
            // First line is not a header, process it
            uint32_t asn = std::stoul(rov_line);
            rov_asns.insert(asn);
        }
    }
    
    // Read remaining ROV ASNs
    while (std::getline(rov_stream, rov_line)) {
        if (rov_line.empty()) continue;
        // Remove any trailing whitespace
        while (!rov_line.empty() && (rov_line.back() == '\r' || rov_line.back() == '\n' || rov_line.back() == ' ')) {
            rov_line.pop_back();
        }
        if (rov_line.empty()) continue;
        
        try {
            uint32_t asn = std::stoul(rov_line);
            rov_asns.insert(asn);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Could not parse ROV ASN: " << rov_line << std::endl;
        }
    }
    rov_stream.close();
    
    // Apply ROV policies to the specified ASNs
    for (uint32_t rov_asn : rov_asns) {
        ASNode* node = graph.getOrCreateNode(rov_asn);
        if (node) {
            node->policy = std::make_shared<ROV>();
        }
    }
    
    std::cout << "[Info] ROV policies applied to " << rov_asns.size() << " ASNs.\n";


    // ---------------------------------------------------------
    // 4. Seed Announcements (Phase 3.4)
    // ---------------------------------------------------------
    std::cout << "\n[Step 4] Seeding announcements from file...\n";
    
    std::ifstream ann_stream(ann_file);
    if (!ann_stream.is_open()) {
        std::cerr << "Error: Could not open announcements file: " << ann_file << std::endl;
        return 1;
    }
    
    std::string ann_line;
    // Skip header line
    std::getline(ann_stream, ann_line);
    
    int seeded_count = 0;
    while (std::getline(ann_stream, ann_line)) {
        if (ann_line.empty()) continue;
        
        // Parse CSV: seed_asn,prefix,rov_invalid
        std::istringstream iss(ann_line);
        std::string seed_asn_str, prefix, rov_invalid_str;
        
        if (!std::getline(iss, seed_asn_str, ',') ||
            !std::getline(iss, prefix, ',') ||
            !std::getline(iss, rov_invalid_str, ',')) {
            std::cerr << "Warning: Could not parse announcement line: " << ann_line << std::endl;
            continue;
        }
        
        // Remove any whitespace
        seed_asn_str.erase(0, seed_asn_str.find_first_not_of(" \t\r\n"));
        seed_asn_str.erase(seed_asn_str.find_last_not_of(" \t\r\n") + 1);
        prefix.erase(0, prefix.find_first_not_of(" \t\r\n"));
        prefix.erase(prefix.find_last_not_of(" \t\r\n") + 1);
        rov_invalid_str.erase(0, rov_invalid_str.find_first_not_of(" \t\r\n"));
        rov_invalid_str.erase(rov_invalid_str.find_last_not_of(" \t\r\n") + 1);
        
        try {
            uint32_t seed_asn = std::stoul(seed_asn_str);
            bool rov_invalid = (rov_invalid_str == "True" || rov_invalid_str == "true" || rov_invalid_str == "1");
            
            ASNode* target_as = graph.getOrCreateNode(seed_asn);
            if (target_as) {
                Announcement seed_announcement(
                    prefix,
                    {seed_asn},
                    seed_asn,
                    Relationship::ORIGIN,
                    rov_invalid
                );
                
                BGP* bgp_policy = dynamic_cast<BGP*>(target_as->policy.get());
                if (bgp_policy) {
                    bgp_policy->local_rib[seed_announcement.prefix] = seed_announcement;
                    seeded_count++;
                } else {
                    std::cerr << "Error: Could not retrieve BGP policy for ASN " << seed_asn << std::endl;
                }
            } else {
                std::cerr << "Error: Could not find or create ASN " << seed_asn << " for seeding." << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Warning: Could not parse announcement line: " << ann_line << " (" << e.what() << ")" << std::endl;
        }
    }
    ann_stream.close();
    
    std::cout << "[Info] Successfully seeded " << seeded_count << " announcements.\n";



    // ---------------------------------------------------------
    // 5. Run Propagation (Phase 3.5)
    // ---------------------------------------------------------
    // All propagation logic is abstracted into PropagationEngine
    // This includes the three phases: UP, ACROSS, and DOWN
    PropagationEngine::run_propagation(graph);



    // ---------------------------------------------------------
    // 6. Output Results (Phase 3.7)
    // ---------------------------------------------------------
    std::cout << "\n[Step 6] Writing results to ribs.csv...\n";
    std::ofstream out_file("ribs.csv");
    if (!out_file.is_open()) {
        std::cerr << "Error: Could not open ribs.csv for writing.\n";
        return 1;
    }

    // Write Header
    out_file << "asn,prefix,as_path\n";

    // Iterate through all AS nodes and their Local RIBs to dump data
    for (const auto& pair : graph.getNodes()) {
        ASNode* node = pair.second.get();
        // ROV extends BGP, so dynamic_cast<BGP*> will work for both BGP and ROV policies
        BGP* policy = dynamic_cast<BGP*>(node->policy.get());

        if (policy) {
            for (const auto& rib_entry : policy->local_rib) {
                const Announcement& ann = rib_entry.second;
                
                // Format the AS path as a Python tuple: "(1, 2, 3)" or "(1,)" for single element
                std::string path_str = "(";
                for (size_t i = 0; i < ann.as_path.size(); ++i) {
                    path_str += std::to_string(ann.as_path[i]);
                    if (i < ann.as_path.size() - 1) {
                        path_str += ", ";
                    } else if (ann.as_path.size() == 1) {
                        // Single-element tuple requires trailing comma in Python
                        path_str += ",";
                    }
                }
                path_str += ")";

                // Write to file (quote the path_str since it contains commas)
                out_file << node->asn << "," << ann.prefix << ",\"" << path_str << "\"\n";
            }
        }
    }

    out_file.close();
    std::cout << "[Success] ribs.csv generated successfully.\n";

    return 0;
}