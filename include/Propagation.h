#pragma once

#include "ASGraph.h"
#include "Policy.h"
#include "Announcement.h"
#include <unordered_map>
#include <vector>

/**
 * BGP Propagation Engine
 * 
 * Handles all BGP announcement propagation logic following the three-phase
 * valley-free routing model:
 * 1. UP: Customers to Providers
 * 2. ACROSS: Peers (single hop)
 * 3. DOWN: Providers to Customers
 */
class PropagationEngine {
private:
    // Relationship scores for best path selection
    // Customer > Peer > Provider > Origin (though Origin is highest priority)
    static const std::unordered_map<Relationship, int> REL_SCORES;

    /**
     * Returns true if ann1 is better than ann2, based on BGP best path selection
     * Rules (in order):
     * 1. Relationship (Customer > Peer > Provider)
     * 2. AS Path Length (shorter is better)
     * 3. Next Hop ASN (lower is better)
     */
    static bool is_better_announcement(
        const Announcement& ann1, 
        const Announcement& ann2, 
        const std::unordered_map<Relationship, int>& rel_scores
    );

    /**
     * Processes all announcements in a node's received_queue, resolves conflicts,
     * and updates its local_rib with the best announcement for each prefix.
     * 
     * Also handles ROV filtering - ROV ASes drop announcements with rov_invalid=true
     */
    static void process_announcements(
        ASNode* node, 
        const std::unordered_map<Relationship, int>& rel_scores
    );

    /**
     * Propagate announcements UP the provider-customer hierarchy
     */
    static void propagate_up(
        ASGraph& graph,
        const std::vector<std::vector<ASNode*>>& ranked_ases,
        int max_rank
    );

    /**
     * Propagate announcements ACROSS peer relationships (single hop only)
     */
    static void propagate_across(ASGraph& graph);

    /**
     * Propagate announcements DOWN the provider-customer hierarchy
     */
    static void propagate_down(
        ASGraph& graph,
        const std::vector<std::vector<ASNode*>>& ranked_ases,
        int max_rank
    );

public:
    /**
     * Run the complete BGP propagation process
     * 
     * This executes all three phases of BGP propagation:
     * 1. UP: From customers to providers
     * 2. ACROSS: To peers (single hop)
     * 3. DOWN: From providers to customers
     * 
     * @param graph The AS graph to propagate announcements through
     */
    static void run_propagation(ASGraph& graph);
};

