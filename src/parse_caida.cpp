#include "parse_caida.h"
#include "ASGraph.h" // Include the graph definition

#include <iostream>
#include <cstdio>
#include <cstring>
#include <string>
#include <cstdint>
#include <cstdlib> // for strtoul

// Change signature to accept Graph reference
void parse_caida(const std::string& bz2_filename, ASGraph& graph) {
    std::string cmd;
    if (bz2_filename.size() > 4 && bz2_filename.substr(bz2_filename.size() - 4) == ".bz2") {
        cmd = "bzcat \"" + bz2_filename + "\"";
    } else {
        cmd = "cat \"" + bz2_filename + "\"";
    }
    FILE* fp = popen(cmd.c_str(), "r");
    
    if (!fp) {
        std::cerr << "Error: failed to run command on file: " << bz2_filename << "\n";
        return;
    }

    char line[512];
    uint64_t line_count = 0;

    std::cout << "Parsing CAIDA file... ";
    
    while (std::fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\0') continue;

        // Fast newline strip
        size_t len = std::strlen(line);
        if (len > 0 && line[len-1] == '\n') line[--len] = '\0';
        
        char* saveptr = nullptr;
        char* t1 = strtok_r(line, "|", &saveptr);
        char* t2 = strtok_r(nullptr, "|", &saveptr);
        char* t3 = strtok_r(nullptr, "|", &saveptr);

        if (!t1 || !t2 || !t3) continue;

        uint32_t as1 = static_cast<uint32_t>(std::strtoul(t1, nullptr, 10));
        uint32_t as2 = static_cast<uint32_t>(std::strtoul(t2, nullptr, 10));
        int rel = std::strtol(t3, nullptr, 10);

        // Populate Graph instead of printing
        graph.addRelationship(as1, as2, rel);
        
        line_count++;
    }

    pclose(fp);
    std::cout << "Done. Parsed " << line_count << " lines.\n";
}