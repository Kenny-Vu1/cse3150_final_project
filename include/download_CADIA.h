#pragma once

#include <string>

// Main function:
// Searches backward month-by-month:
//   1) Check if the local file exists
//   2) If not, check if the remote URL exists
//   3) If exists, download
//   4) Repeat for up to max_months_back
// Returns:
//   - filename (e.g., "20250901.as-rel2.txt.bz2") on success
//   - "" on failure
std::string download_caida(int max_months_back = 12);
