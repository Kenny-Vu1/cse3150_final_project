#include "ASGraph.h"
#include <queue>
#include <algorithm>

// Retrieve a node or create it if it doesn't exist
ASNode* ASGraph::getOrCreateNode(uint32_t asn) {
    if (nodes.find(asn) == nodes.end()) {
        nodes[asn] = std::make_shared<ASNode>(asn);
    }
    return nodes[asn].get();
}

// [cite: 85] Extract relationships
// as1 | as2 | rel
// rel = -1: as1 is provider of as2
// rel =  0: as1 is peer of as2
void ASGraph::addRelationship(uint32_t as1, uint32_t as2, int relationship) {
    ASNode* u = getOrCreateNode(as1);
    ASNode* v = getOrCreateNode(as2);

    if (relationship == -1) {
        // as1 is provider of as2
        v->providers.push_back(u); // as2 adds as1 as provider
        u->customers.push_back(v); // as1 adds as2 as customer
    } else if (relationship == 0) {
        // Peers
        u->peers.push_back(v);
        v->peers.push_back(u);
    }
}

//  Check specifically that there are no provider cycles
// Standard DFS cycle detection
bool ASGraph::detectProviderCycles() {
    std::cout << "Running cycle detection on " << nodes.size() << " nodes...\n";
    
    std::unordered_map<uint32_t, bool> visited;
    std::unordered_map<uint32_t, bool> recursionStack;

    for (auto& pair : nodes) {
        ASNode* node = pair.second.get();
        if (hasProviderCycleDFS(node, visited, recursionStack)) {
            std::cerr << "CYCLE DETECTED involving ASN: " << node->asn << "\n";
            return true;
        }
    }
    std::cout << "No provider cycles found.\n";
    return false;
}

bool ASGraph::hasProviderCycleDFS(ASNode* node, 
                                  std::unordered_map<uint32_t, bool>& visited, 
                                  std::unordered_map<uint32_t, bool>& recursionStack) {
    
    uint32_t asn = node->asn;

    // If currently in recursion stack, we found a cycle
    if (recursionStack[asn]) return true;
    
    // If already fully processed, no cycle here
    if (visited[asn]) return false;

    // Mark current
    visited[asn] = true;
    recursionStack[asn] = true;

    // Traverse ONLY providers for this check 
    for (ASNode* provider : node->providers) {
        if (hasProviderCycleDFS(provider, visited, recursionStack)) {
            return true;
        }
    }

    // Unmark from recursion stack
    recursionStack[asn] = false;
    return false;
}

std::vector<std::vector<ASNode*>> ASGraph::getRankedASes() {
    std::queue<ASNode*> q;
    std::unordered_map<uint32_t, size_t> customer_counts;
    int max_rank = 0;

    // Initialize ranks to 0 and calculate initial customer counts.
    for (auto const& [asn, node_ptr] : nodes) {
        node_ptr->propagation_rank = 0;
        size_t count = node_ptr->customers.size();
        customer_counts[asn] = count;
        if (count == 0) {
            q.push(node_ptr.get());
        }
    }

    // Process nodes in topological order (customer to provider).
    while (!q.empty()) {
        ASNode* customer_node = q.front();
        q.pop();

        for (ASNode* provider_node : customer_node->providers) {
            // A provider's rank is the max of its customers' ranks + 1.
            provider_node->propagation_rank = std::max(
                provider_node->propagation_rank,
                customer_node->propagation_rank + 1
            );
            max_rank = std::max(max_rank, provider_node->propagation_rank);

            // This provider has one less customer to be processed.
            customer_counts[provider_node->asn]--;

            // If all customers of this provider are processed, it's ready to be a "customer" for its own providers.
            if (customer_counts[provider_node->asn] == 0) {
                q.push(provider_node);
            }
        }
    }

    // Now that ranks are assigned, create the flattened vector structure.
    std::vector<std::vector<ASNode*>> ranked_ases(max_rank + 1);
    for (auto const& [asn, node_ptr] : nodes) {
        ranked_ases[node_ptr->propagation_rank].push_back(node_ptr.get());
    }

    return ranked_ases;
}