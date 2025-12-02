#include <iostream>
#include "ASGraph.h"

void test_simple_graph() {
    std::cout << "--- Running test: Simple Graph ---" << std::endl;
    ASGraph graph;
    graph.addRelationship(1, 2, -1); // 1 is provider of 2
    graph.addRelationship(2, 3, -1); // 2 is provider of 3

    if (graph.getNumNodes() != 3) {
        std::cerr << "FAILED: Expected 3 nodes, but got " << graph.getNumNodes() << std::endl;
        return;
    }

    if (graph.detectProviderCycles()) {
        std::cerr << "FAILED: Detected a cycle where there should be none." << std::endl;
        return;
    }

    std::cout << "PASSED: Simple Graph test" << std::endl;
}

void test_provider_cycle() {
    std::cout << "--- Running test: Provider Cycle ---" << std::endl;
    ASGraph graph;
    graph.addRelationship(1, 2, -1); // 1 is provider of 2
    graph.addRelationship(2, 3, -1); // 2 is provider of 3
    graph.addRelationship(3, 1, -1); // 3 is provider of 1 (cycle)

    if (graph.getNumNodes() != 3) {
        std::cerr << "FAILED: Expected 3 nodes, but got " << graph.getNumNodes() << std::endl;
        return;
    }

    if (!graph.detectProviderCycles()) {
        std::cerr << "FAILED: Did not detect a provider cycle where one exists." << std::endl;
        return;
    }

    std::cout << "PASSED: Provider Cycle test" << std::endl;
}

void test_peer_relationship() {
    std::cout << "--- Running test: Peer Relationship ---" << std::endl;
    ASGraph graph;
    graph.addRelationship(1, 2, 0); // 1 and 2 are peers
    graph.addRelationship(2, 3, 0); // 2 and 3 are peers

    if (graph.getNumNodes() != 3) {
        std::cerr << "FAILED: Expected 3 nodes, but got " << graph.getNumNodes() << std::endl;
        return;
    }

    if (graph.detectProviderCycles()) {
        std::cerr << "FAILED: Detected a cycle where there should be none (peers shouldn't cause provider cycles)." << std::endl;
        return;
    }

    std::cout << "PASSED: Peer Relationship test" << std::endl;
}

void test_complex_graph_no_cycle() {
    std::cout << "--- Running test: Complex Graph (No Cycle) ---" << std::endl;
    ASGraph graph;
    graph.addRelationship(1, 2, -1);
    graph.addRelationship(1, 3, -1);
    graph.addRelationship(2, 4, -1);
    graph.addRelationship(3, 4, -1);
    graph.addRelationship(4, 5, 0);
    graph.addRelationship(5, 6, -1);

    if (graph.getNumNodes() != 6) {
        std::cerr << "FAILED: Expected 6 nodes, but got " << graph.getNumNodes() << std::endl;
        return;
    }

    if (graph.detectProviderCycles()) {
        std::cerr << "FAILED: Detected a cycle where there should be none." << std::endl;
        return;
    }

    std::cout << "PASSED: Complex Graph (No Cycle) test" << std::endl;
}


/*
int main() {
    test_simple_graph();
    test_provider_cycle();
    test_peer_relationship();
    test_complex_graph_no_cycle();

    return 0;
}
*/
