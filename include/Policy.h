#pragma once

#include "Announcement.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

class Policy {
public:
    virtual ~Policy() = default;
    // This will be the main interface for processing announcements.
    // We can add virtual functions here later, for example:
    // virtual void process_incoming_announcement(const Announcement& ann) = 0;
};

class BGP : public Policy {
public:
    // The Local RIB stores the best announcement for each prefix.
    // Key: prefix (e.g., "8.8.8.0/24")
    // Value: The best announcement object for that prefix.
    std::unordered_map<std::string, Announcement> local_rib;

    // The received_queue stores all announcements received from neighbors
    // before they are processed by the policy.
    // Key: prefix
    // Value: A list of all announcements received for that prefix.
    std::unordered_map<std::string, std::vector<Announcement>> received_queue;
};

// ROV (Route Origin Validation) policy extends BGP
// ROV ASes drop announcements with rov_invalid=true
class ROV : public BGP {
public:
    // ROV inherits all BGP functionality but filters invalid announcements
    // This is handled in process_announcements by checking rov_invalid
};
