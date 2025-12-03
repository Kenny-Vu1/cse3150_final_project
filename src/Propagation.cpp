#include "Propagation.h"
#include "ASGraph.h"
#include "Policy.h"
#include <iostream>

// Relationship scores for conflict resolution
const std::unordered_map<Relationship, int> PropagationEngine::REL_SCORES = {
    {Relationship::ORIGIN, 3},
    {Relationship::CUSTOMER, 2},
    {Relationship::PEER, 1},
    {Relationship::PROVIDER, 0}
};

bool PropagationEngine::is_better_announcement(
    const Announcement& ann1, 
    const Announcement& ann2, 
    const std::unordered_map<Relationship, int>& rel_scores
) {
    // Rule 1: Relationship (Customer > Peer > Provider)
    int score1 = rel_scores.at(ann1.received_from_relationship);
    int score2 = rel_scores.at(ann2.received_from_relationship);
    if (score1 != score2) {
        return score1 > score2;
    }

    // Rule 2: AS Path Length
    if (ann1.as_path.size() != ann2.as_path.size()) {
        return ann1.as_path.size() < ann2.as_path.size();
    }

    // Rule 3: Next Hop ASN (lower is better)
    return ann1.next_hop_asn < ann2.next_hop_asn;
}

void PropagationEngine::process_announcements(
    ASNode* node, 
    const std::unordered_map<Relationship, int>& rel_scores
) {
    BGP* policy = dynamic_cast<BGP*>(node->policy.get());
    if (!policy || policy->received_queue.empty()) {
        return;
    }

    // Check if this node uses ROV policy
    ROV* rov_policy = dynamic_cast<ROV*>(node->policy.get());
    bool is_rov = (rov_policy != nullptr);

    for (auto const& [prefix, received_anns] : policy->received_queue) {
        // Find the current best announcement for this prefix (if it exists)
        Announcement* current_best = nullptr;
        auto it = policy->local_rib.find(prefix);
        if (it != policy->local_rib.end()) {
            current_best = &it->second;
        }

        // Compare against all newly received announcements
        for (const auto& new_ann_const : received_anns) {
            // ROV ASes drop announcements with rov_invalid=true
            if (is_rov && new_ann_const.rov_invalid) {
                continue;  // Drop this announcement
            }

            // The new path if we adopt this announcement includes our own ASN
            Announcement potential_new_ann = new_ann_const;
            potential_new_ann.as_path.insert(potential_new_ann.as_path.begin(), node->asn);

            if (current_best == nullptr || is_better_announcement(potential_new_ann, *current_best, rel_scores)) {
                // If the new one is better, it becomes the new best.
                // We update the local_rib directly, and our pointer to it.
                policy->local_rib[prefix] = potential_new_ann;
                current_best = &policy->local_rib[prefix];
            }
        }
    }

    policy->received_queue.clear();
}

void PropagationEngine::propagate_up(
    ASGraph& graph,
    const std::vector<std::vector<ASNode*>>& ranked_ases,
    int max_rank
) {
    std::cout << "  - Propagating UP from customers to providers...\n";
    
    for (int rank = 0; rank <= max_rank; ++rank) {
        // First, all nodes at this rank process announcements they have received
        for (ASNode* node : ranked_ases[rank]) {
            process_announcements(node, REL_SCORES);
        }

        // Second, all nodes at this rank send from their updated local RIB to providers
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
}

void PropagationEngine::propagate_across(ASGraph& graph) {
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
        process_announcements(node_ptr.get(), REL_SCORES);
    }
}

void PropagationEngine::propagate_down(
    ASGraph& graph,
    const std::vector<std::vector<ASNode*>>& ranked_ases,
    int max_rank
) {
    std::cout << "  - Propagating DOWN from providers to customers...\n";
    
    for (int rank = max_rank; rank >= 0; --rank) {
        // First, process any announcements received from the previous (higher) rank or peers
        for (ASNode* node : ranked_ases[rank]) {
            process_announcements(node, REL_SCORES);
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
}

void PropagationEngine::run_propagation(ASGraph& graph) {
    std::cout << "\n[Step 5] Running BGP propagation...\n";

    // Get the ranked graph structure for propagation
    auto ranked_ases = graph.getRankedASes();
    int max_rank = ranked_ases.size() - 1;

    // Execute the three phases of BGP propagation
    propagate_up(graph, ranked_ases, max_rank);
    propagate_across(graph);
    propagate_down(graph, ranked_ases, max_rank);
    
    std::cout << "[Info] Propagation complete.\n";
}

