#pragma once
#include <string>

// We need to forward declare the class or include the header
// so the compiler knows what 'ASGraph' is.
#include "ASGraph.h"

// Updated signature: accepts the filename AND the graph reference
void parse_caida(const std::string& bz2_filename, ASGraph& graph);