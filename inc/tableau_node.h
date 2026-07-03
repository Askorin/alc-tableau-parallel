#pragma once
#include "concept.h"
#include <string_view>
#include <vector>
#include <algorithm>
#include <iostream>
#include <cstdint>


struct Edge {
    std::string_view role;
    size_t child_idx;
};

struct TableauNode {
    size_t label_offset = 0;
    size_t label_count = 0;
    size_t edge_offset = 0;
    size_t edge_count = 0;
    //bool isClashed = false;
    size_t parent_idx = SIZE_MAX; // SIZE_MAX indicates the mathematical root
};
