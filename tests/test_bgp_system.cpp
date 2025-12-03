#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include "ASGraph.h"
#include "Announcement.h"
#include "Policy.h"
#include "Propagation.h"
#include "parse_caida.h"

/**
 * System tests for BGP propagation
 * These tests create mini-graphs, seed announcements, run propagation,
 * and verify the output matches expected results.
 */

// Helper function to check if a specific ASN has a specific prefix in its RIB
bool has_prefix_in_rib(ASGraph& graph, uint32_t asn, const std::string& prefix) {
    ASNode* node = graph.getOrCreateNode(asn);
    if (!node) return false;
    
    BGP* policy = dynamic_cast<BGP*>(node->policy.get());
    if (!policy) return false;
    
    return policy->local_rib.find(prefix) != policy->local_rib.end();
}

// Helper function to get AS path for a prefix at an ASN
std::vector<uint32_t> get_as_path(ASGraph& graph, uint32_t asn, const std::string& prefix) {
    ASNode* node = graph.getOrCreateNode(asn);
    if (!node) return {};
    
    BGP* policy = dynamic_cast<BGP*>(node->policy.get());
    if (!policy) return {};
    
    auto it = policy->local_rib.find(prefix);
    if (it == policy->local_rib.end()) return {};
    
    return it->second.as_path;
}

/**
 * Test 1: Single announcement with tiny graph
 * Graph: 1 -> 2 (1 is provider of 2)
 * Seed: AS 1 announces prefix 1.2.0.0/16
 * Expected: Both AS 1 and AS 2 should have the prefix
 */
void test_single_announcement_tiny_graph() {
    std::cout << "\n=== Test: Single Announcement (Tiny Graph) ===" << std::endl;
    
    ASGraph graph;
    graph.addRelationship(1, 2, -1); // 1 is provider of 2
    
    // Seed announcement at AS 1
    ASNode* node1 = graph.getOrCreateNode(1);
    Announcement ann("1.2.0.0/16", {1}, 1, Relationship::ORIGIN, false);
    BGP* policy1 = dynamic_cast<BGP*>(node1->policy.get());
    policy1->local_rib["1.2.0.0/16"] = ann;
    
    // Run propagation
    PropagationEngine::run_propagation(graph);
    
    // Verify AS 1 has the prefix (origin)
    if (!has_prefix_in_rib(graph, 1, "1.2.0.0/16")) {
        std::cerr << "FAILED: AS 1 should have prefix 1.2.0.0/16" << std::endl;
        return;
    }
    
    // Verify AS 2 has the prefix (received from provider)
    if (!has_prefix_in_rib(graph, 2, "1.2.0.0/16")) {
        std::cerr << "FAILED: AS 2 should have prefix 1.2.0.0/16" << std::endl;
        return;
    }
    
    // Verify AS paths
    auto path1 = get_as_path(graph, 1, "1.2.0.0/16");
    if (path1.size() != 1 || path1[0] != 1) {
        std::cerr << "FAILED: AS 1 should have path (1)" << std::endl;
        return;
    }
    
    auto path2 = get_as_path(graph, 2, "1.2.0.0/16");
    if (path2.size() != 2 || path2[0] != 2 || path2[1] != 1) {
        std::cerr << "FAILED: AS 2 should have path (2, 1)" << std::endl;
        return;
    }
    
    std::cout << "PASSED: Single announcement propagated correctly" << std::endl;
}

/**
 * Test 2: Larger graph with multiple ASes
 * Graph: 1 -> 2 -> 3, 1 -> 4, 2 <-> 5 (peer)
 * Seed: AS 1 announces prefix 1.2.0.0/16
 * Expected: All ASes should receive the announcement
 */
void test_larger_graph() {
    std::cout << "\n=== Test: Larger Graph ===" << std::endl;
    
    ASGraph graph;
    graph.addRelationship(1, 2, -1); // 1 is provider of 2
    graph.addRelationship(2, 3, -1); // 2 is provider of 3
    graph.addRelationship(1, 4, -1);  // 1 is provider of 4
    graph.addRelationship(2, 5, 0);   // 2 and 5 are peers
    
    // Seed announcement at AS 1
    ASNode* node1 = graph.getOrCreateNode(1);
    Announcement ann("1.2.0.0/16", {1}, 1, Relationship::ORIGIN, false);
    BGP* policy1 = dynamic_cast<BGP*>(node1->policy.get());
    policy1->local_rib["1.2.0.0/16"] = ann;
    
    // Run propagation
    PropagationEngine::run_propagation(graph);
    
    // Verify all ASes have the prefix
    uint32_t ases[] = {1, 2, 3, 4, 5};
    for (uint32_t asn : ases) {
        if (!has_prefix_in_rib(graph, asn, "1.2.0.0/16")) {
            std::cerr << "FAILED: AS " << asn << " should have prefix 1.2.0.0/16" << std::endl;
            return;
        }
    }
    
    std::cout << "PASSED: Larger graph propagation works correctly" << std::endl;
}

/**
 * Test 3: Two announcements for same prefix from different ASes
 * Graph: 1 -> 2 <- 3 (2 is customer of both 1 and 3)
 * Seed: AS 1 and AS 3 both announce 1.2.0.0/16
 * Expected: AS 2 should choose the better path (customer relationship preferred)
 */
void test_multiple_announcements_same_prefix() {
    std::cout << "\n=== Test: Multiple Announcements (Same Prefix) ===" << std::endl;
    
    ASGraph graph;
    graph.addRelationship(1, 2, -1); // 1 is provider of 2
    graph.addRelationship(3, 2, -1); // 3 is provider of 2
    
    // Seed announcement at AS 1
    ASNode* node1 = graph.getOrCreateNode(1);
    Announcement ann1("1.2.0.0/16", {1}, 1, Relationship::ORIGIN, false);
    BGP* policy1 = dynamic_cast<BGP*>(node1->policy.get());
    policy1->local_rib["1.2.0.0/16"] = ann1;
    
    // Seed announcement at AS 3
    ASNode* node3 = graph.getOrCreateNode(3);
    Announcement ann3("1.2.0.0/16", {3}, 3, Relationship::ORIGIN, false);
    BGP* policy3 = dynamic_cast<BGP*>(node3->policy.get());
    policy3->local_rib["1.2.0.0/16"] = ann3;
    
    // Run propagation
    PropagationEngine::run_propagation(graph);
    
    // AS 2 should have the prefix
    if (!has_prefix_in_rib(graph, 2, "1.2.0.0/16")) {
        std::cerr << "FAILED: AS 2 should have prefix 1.2.0.0/16" << std::endl;
        return;
    }
    
    // AS 2 should choose one of the paths (both are from customers, so path length or ASN decides)
    auto path2 = get_as_path(graph, 2, "1.2.0.0/16");
    if (path2.size() != 2) {
        std::cerr << "FAILED: AS 2 should have path length 2" << std::endl;
        return;
    }
    
    // Both paths are equivalent (same relationship, same length), so lower next hop ASN wins
    // Since both are from customers, the one with lower ASN in path should win
    // Path would be (2, 1) or (2, 3) - lower next hop ASN (1 < 3) means (2, 1) should win
    if (path2[1] != 1) {
        std::cerr << "FAILED: AS 2 should prefer path from AS 1 (lower next hop ASN)" << std::endl;
        return;
    }
    
    std::cout << "PASSED: Best path selection works correctly" << std::endl;
}

/**
 * Test 4: Customer vs Provider preference
 * Graph: 1 -> 2 <- 3 (2 is customer of 1, provider of 3)
 * Seed: AS 1 and AS 3 both announce 1.2.0.0/16
 * Expected: AS 2 should prefer customer (AS 3) over provider (AS 1)
 */
void test_customer_vs_provider_preference() {
    std::cout << "\n=== Test: Customer vs Provider Preference ===" << std::endl;
    
    ASGraph graph;
    graph.addRelationship(1, 2, -1); // 1 is provider of 2
    graph.addRelationship(2, 3, -1); // 2 is provider of 3 (so 3 is customer of 2)
    
    // Seed announcement at AS 1
    ASNode* node1 = graph.getOrCreateNode(1);
    Announcement ann1("1.2.0.0/16", {1}, 1, Relationship::ORIGIN, false);
    BGP* policy1 = dynamic_cast<BGP*>(node1->policy.get());
    policy1->local_rib["1.2.0.0/16"] = ann1;
    
    // Seed announcement at AS 3
    ASNode* node3 = graph.getOrCreateNode(3);
    Announcement ann3("1.2.0.0/16", {3}, 3, Relationship::ORIGIN, false);
    BGP* policy3 = dynamic_cast<BGP*>(node3->policy.get());
    policy3->local_rib["1.2.0.0/16"] = ann3;
    
    // Run propagation
    PropagationEngine::run_propagation(graph);
    
    // AS 2 should have the prefix
    if (!has_prefix_in_rib(graph, 2, "1.2.0.0/16")) {
        std::cerr << "FAILED: AS 2 should have prefix 1.2.0.0/16" << std::endl;
        return;
    }
    
    // AS 2 should prefer customer (AS 3) over provider (AS 1)
    auto path2 = get_as_path(graph, 2, "1.2.0.0/16");
    if (path2.size() != 2 || path2[1] != 3) {
        std::cerr << "FAILED: AS 2 should prefer customer (AS 3) over provider (AS 1)" << std::endl;
        std::cerr << "  Got path: (";
        for (size_t i = 0; i < path2.size(); ++i) {
            std::cerr << path2[i] << (i < path2.size() - 1 ? ", " : "");
        }
        std::cerr << ")" << std::endl;
        return;
    }
    
    std::cout << "PASSED: Customer relationship preferred over provider" << std::endl;
}

/**
 * Test 5: Output format verification
 * Creates a simple graph, runs propagation, and verifies output format
 */
void test_output_format() {
    std::cout << "\n=== Test: Output Format ===" << std::endl;
    
    ASGraph graph;
    graph.addRelationship(1, 2, -1);
    
    // Seed announcement
    ASNode* node1 = graph.getOrCreateNode(1);
    Announcement ann("1.2.0.0/16", {1}, 1, Relationship::ORIGIN, false);
    BGP* policy1 = dynamic_cast<BGP*>(node1->policy.get());
    policy1->local_rib["1.2.0.0/16"] = ann;
    
    PropagationEngine::run_propagation(graph);
    
    // Write output to temporary file
    std::ofstream out("test_output.csv");
    out << "asn,prefix,as_path\n";
    
    for (const auto& pair : graph.getNodes()) {
        ASNode* node = pair.second.get();
        BGP* policy = dynamic_cast<BGP*>(node->policy.get());
        if (policy) {
            for (const auto& rib_entry : policy->local_rib) {
                const Announcement& ann = rib_entry.second;
                
                std::string path_str = "(";
                for (size_t i = 0; i < ann.as_path.size(); ++i) {
                    path_str += std::to_string(ann.as_path[i]);
                    if (i < ann.as_path.size() - 1) {
                        path_str += ", ";
                    } else if (ann.as_path.size() == 1) {
                        path_str += ",";
                    }
                }
                path_str += ")";
                
                out << node->asn << "," << ann.prefix << ",\"" << path_str << "\"\n";
            }
        }
    }
    out.close();
    
    // Verify file was created and has correct format
    std::ifstream in("test_output.csv");
    std::string line;
    std::getline(in, line); // Skip header
    
    bool found_single_element = false;
    bool found_multi_element = false;
    
    while (std::getline(in, line)) {
        if (line.find("\"(1,)\"") != std::string::npos) {
            found_single_element = true;
        }
        if (line.find("\"(2, 1)\"") != std::string::npos) {
            found_multi_element = true;
        }
    }
    in.close();
    
    if (!found_single_element) {
        std::cerr << "FAILED: Output should contain single-element tuple format (1,)" << std::endl;
        return;
    }
    
    if (!found_multi_element) {
        std::cerr << "FAILED: Output should contain multi-element tuple format (2, 1)" << std::endl;
        return;
    }
    
    // Clean up
    std::remove("test_output.csv");
    
    std::cout << "PASSED: Output format is correct" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "BGP Simulator System Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    
    test_single_announcement_tiny_graph();
    test_larger_graph();
    test_multiple_announcements_same_prefix();
    test_customer_vs_provider_preference();
    test_output_format();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "All system tests completed!" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}

