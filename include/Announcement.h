#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Enum for the relationship from which an announcement is received
enum class Relationship {
    PROVIDER,
    PEER,
    CUSTOMER,
    ORIGIN // Special case for the origin of the announcement
};

struct Announcement {
    std::string prefix;
    std::vector<uint32_t> as_path;
    uint32_t next_hop_asn;
    Relationship received_from_relationship;
    bool rov_invalid;  // True if this announcement is invalid according to ROV

    // Default constructor
    Announcement() : next_hop_asn(0), received_from_relationship(Relationship::ORIGIN), rov_invalid(false) {}

    // Parameterized constructor for convenience
    Announcement(const std::string& _prefix, const std::vector<uint32_t>& _as_path, uint32_t _next_hop_asn, Relationship _rel, bool _rov_invalid = false)
        : prefix(_prefix), as_path(_as_path), next_hop_asn(_next_hop_asn), received_from_relationship(_rel), rov_invalid(_rov_invalid) {}

    // A helper to print the announcement for debugging
    void print() const {
        std::cout << "Prefix: " << prefix << ", Path: ";
        for (size_t i = 0; i < as_path.size(); ++i) {
            std::cout << as_path[i] << (i < as_path.size() - 1 ? "-" : "");
        }
        std::cout << ", Next Hop: " << next_hop_asn << std::endl;
    }
};
