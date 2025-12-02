#include "caida_downloader.h"
#include <iostream>

int main() {
    std::string file = download_caida(); // default 12 months back
    if (file.empty()) {
        std::cerr << "Failed to get CAIDA dataset\n";
        return 1;
    }
    std::cout << "Got CAIDA file: " << file << "\n";
    return 0;
}
