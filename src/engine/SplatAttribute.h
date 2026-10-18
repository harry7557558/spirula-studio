#pragma once

// One per-splat pool buffer the developer view can read
// (engine_splat_attributes, docs/notes/splat-inspector.md).

#include <string>

struct SplatAttribute {
    std::string code;   // what an expression calls it
    std::string pool;   // the pool buffer it is read from
    bool live = false;  // that buffer holds data
};
