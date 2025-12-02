#pragma once
#include <vector>
#include <unordered_map>
#include <memory>
#include <iostream>
#include <cstdint>

// Represents a single Autonomous System (Node)
struct ASNode {
    uint32_t asn; //

    // Relationships
    // Using raw pointers for edges to avoid circular dependency overhead
    // The Graph class owns the memory via shared_ptr.
    std::vector<ASNode*> providers; 
    std::vector<ASNode*> customers;
    std::vector<ASNode*> peers;

    ASNode(uint32_t id) : asn(id) {}
};

class ASGraph {
private:
    // Main storage: Map ASN -> Shared Pointer to Node
    std::unordered_map<uint32_t, std::shared_ptr<ASNode>> nodes;

    // Helper for cycle detection
    bool hasProviderCycleDFS(ASNode* node, std::unordered_map<uint32_t, bool>& visited, std::unordered_map<uint32_t, bool>& recursionStack);

public:
    // Get or create a node
    ASNode* getOrCreateNode(uint32_t asn);

    // Add a relationship line from CAIDA
    void addRelationship(uint32_t as1, uint32_t as2, int relationship);

    // Check for provider cycles
    bool detectProviderCycles();
    
    // Get total node count (for verification)
    size_t getNumNodes() const { return nodes.size(); }
};