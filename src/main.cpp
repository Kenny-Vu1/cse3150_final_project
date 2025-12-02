#include "download_CADIA.h"
#include <iostream>

int main() {
    std::string file = download_caida(); // uses default 12 months back
    if (file.empty()) {
        std::cerr << "Failed to get CAIDA dataset\n";
        return 1;
    }

    std::cout << "Got CAIDA file: " << file << "\n";
    // TODO: call your part 2.3 parser/graph builder next
    return 0;
}
