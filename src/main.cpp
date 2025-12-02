#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstdlib>

// Integrate our helper modules
#include "ASGraph.h"
#include "parse_caida.h"
#include "Announcement.h"
#include "Policy.h"

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
    // For this example, we'll manually seed a single announcement
    // instead of parsing the announcements file.
    std::cout << "\n[Step 4] Seeding initial announcement...\n";

    uint32_t seed_asn = 1;
    ASNode* target_as = graph.getOrCreateNode(seed_asn);

    if (target_as) {
        std::string prefix = "1.2.0.0/16";
        Announcement seed_announcement(
            prefix,
            {seed_asn},
            seed_asn,
            Relationship::ORIGIN
        );

        BGP* bgp_policy = dynamic_cast<BGP*>(target_as->policy.get());
        if (bgp_policy) {
            bgp_policy->local_rib[seed_announcement.prefix] = seed_announcement;
            std::cout << "Successfully seeded prefix " << prefix << " at ASN " << seed_asn << ".\n";
            // Optional: Verify it's there
            bgp_policy->local_rib.find(prefix)->second.print();
        } else {
            std::cerr << "Error: Could not retrieve BGP policy for ASN " << seed_asn << std::endl;
        }
    } else {
        std::cerr << "Error: Could not find or create ASN " << seed_asn << " for seeding." << std::endl;
    }



    // ---------------------------------------------------------
    // 5. Run Propagation (Phase 3.5)
    // ---------------------------------------------------------
    std::cout << "\n[Step 5] Running BGP propagation...\n";

    // Get the ranked graph structure for propagation
    auto ranked_ases = graph.getRankedASes();
    int max_rank = ranked_ases.size() - 1;

    // --- PHASE 1: PROPAGATE UP (Customers to Providers) ---
    std::cout << "  - Propagating UP from customers to providers...\n";
    for (int rank = 0; rank <= max_rank; ++rank) {
        // First, all nodes at this rank process their received announcements
        for (ASNode* node : ranked_ases[rank]) {
            BGP* policy = dynamic_cast<BGP*>(node->policy.get());
            if (!policy || policy->received_queue.empty()) continue;

            for (auto const& [prefix, announcements] : policy->received_queue) {
                // Ignoring conflicts: just process the first one received for a prefix
                if (!announcements.empty()) {
                    Announcement new_ann = announcements[0];
                    new_ann.as_path.insert(new_ann.as_path.begin(), node->asn); // Prepend
                    policy->local_rib[prefix] = new_ann;
                }
            }
            policy->received_queue.clear();
        }

        // Second, all nodes at this rank send from their local RIB to providers
        for (ASNode* node : ranked_ases[rank]) {
            BGP* policy = dynamic_cast<BGP*>(node->policy.get());
            if (!policy) continue;

            for (auto const& [prefix, ann] : policy->local_rib) {
                for (ASNode* provider : node->providers) {
                    BGP* provider_policy = dynamic_cast<BGP*>(provider->policy.get());
                    if (provider_policy) {
                        Announcement prop_ann = ann;
                        prop_ann.next_hop_asn = node->asn;
                        prop_ann.received_from_relationship = Relationship::CUSTOMER;
                        provider_policy->received_queue[prefix].push_back(prop_ann);
                    }
                }
            }
        }
    }

    // --- PHASE 2: PROPAGATE ACROSS (to Peers) ---
    std::cout << "  - Propagating ACROSS to peers...\n";
    // First, all ASes send to their peers
    for (auto const& [asn, node_ptr] : graph.getNodes()) {
        ASNode* node = node_ptr.get();
        BGP* policy = dynamic_cast<BGP*>(node->policy.get());
        if (!policy) continue;

        for (auto const& [prefix, ann] : policy->local_rib) {
            for (ASNode* peer : node->peers) {
                BGP* peer_policy = dynamic_cast<BGP*>(peer->policy.get());
                if (peer_policy) {
                    Announcement prop_ann = ann;
                    prop_ann.next_hop_asn = node->asn;
                    prop_ann.received_from_relationship = Relationship::PEER;
                    peer_policy->received_queue[prefix].push_back(prop_ann);
                }
            }
        }
    }

    // Second, all ASes process announcements received from peers
    for (auto const& [asn, node_ptr] : graph.getNodes()) {
        ASNode* node = node_ptr.get();
        BGP* policy = dynamic_cast<BGP*>(node->policy.get());
        if (!policy || policy->received_queue.empty()) continue;

        for (auto const& [prefix, announcements] : policy->received_queue) {
            if (!announcements.empty()) {
                Announcement new_ann = announcements[0];
                new_ann.as_path.insert(new_ann.as_path.begin(), node->asn); // Prepend
                policy->local_rib[prefix] = new_ann;
            }
        }
        policy->received_queue.clear();
    }

    // --- PHASE 3: PROPAGATE DOWN (Providers to Customers) ---
    std::cout << "  - Propagating DOWN from providers to customers...\n";
    for (int rank = max_rank; rank >= 0; --rank) {
        // First, process any announcements received from the previous (higher) rank
         for (ASNode* node : ranked_ases[rank]) {
            BGP* policy = dynamic_cast<BGP*>(node->policy.get());
            if (!policy || policy->received_queue.empty()) continue;

            for (auto const& [prefix, announcements] : policy->received_queue) {
                if (!announcements.empty()) {
                    Announcement new_ann = announcements[0];
                    new_ann.as_path.insert(new_ann.as_path.begin(), node->asn); // Prepend
                    policy->local_rib[prefix] = new_ann;
                }
            }
            policy->received_queue.clear();
        }

        // Second, send from local RIB to all customers
        for (ASNode* node : ranked_ases[rank]) {
            BGP* policy = dynamic_cast<BGP*>(node->policy.get());
            if (!policy) continue;

            for (auto const& [prefix, ann] : policy->local_rib) {
                for (ASNode* customer : node->customers) {
                    BGP* customer_policy = dynamic_cast<BGP*>(customer->policy.get());
                    if (customer_policy) {
                        Announcement prop_ann = ann;
                        prop_ann.next_hop_asn = node->asn;
                        prop_ann.received_from_relationship = Relationship::PROVIDER;
                        customer_policy->received_queue[prefix].push_back(prop_ann);
                    }
                }
            }
        }
    }
    
    std::cout << "[Info] Propagation complete.\n";



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
    out_file << "asn,prefix,as path\n";

    // Iterate through all AS nodes and their Local RIBs to dump data
    for (const auto& pair : graph.getNodes()) {
        ASNode* node = pair.second.get();
        BGP* policy = dynamic_cast<BGP*>(node->policy.get());

        if (policy) {
            for (const auto& rib_entry : policy->local_rib) {
                const Announcement& ann = rib_entry.second;
                
                // Format the AS path
                std::string path_str;
                for (size_t i = 0; i < ann.as_path.size(); ++i) {
                    path_str += std::to_string(ann.as_path[i]);
                    if (i < ann.as_path.size() - 1) {
                        path_str += "-";
                    }
                }

                // Write to file
                out_file << node->asn << "," << ann.prefix << "," << path_str << "\n";
            }
        }
    }

    out_file.close();
    std::cout << "[Success] ribs.csv generated successfully.\n";

    return 0;
}