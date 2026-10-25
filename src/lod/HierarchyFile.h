#pragma once

// .slod binary container for hierarchy topology, metadata, and chunked splats.
// Coarse levels stream independently from leaf payloads.
// Layout: FileHeader (128B), NodeRecords, SummaryGaussians, ChunkTable, payloads.

#include "lod/HierarchyNode.h"

#include <cstdint>
#include <string>

namespace spirula {

constexpr uint32_t kSlodMagic   = 0x444F4C53;  // "SLOD" little-endian
constexpr uint32_t kSlodVersion = 1;

struct SlodFileHeader {
    uint32_t magic = kSlodMagic;
    uint32_t version = kSlodVersion;
    int32_t  root_node = 0;
    int32_t  max_depth = 0;
    int64_t  num_splats = 0;
    int32_t  num_nodes = 0;
    int32_t  num_chunks = 0;        // == number of leaf nodes
    int32_t  sh_degree = 0;
    int32_t  sb_lobes = 0;
    int64_t  total_summary = 0;     // total SummaryGaussian count
    int64_t  node_table_offset = 0;
    int64_t  summary_offset = 0;
    int64_t  chunk_table_offset = 0;
    int64_t  payload_offset = 0;
    uint8_t  reserved[48] = {};     // pad to 128 bytes
};
static_assert(sizeof(SlodFileHeader) == 128, "header must be 128 bytes");

struct SlodNodeRecord {
    float    bounds_lo[3];
    float    bounds_hi[3];
    float    error_radius;
    int32_t  parent;
    int32_t  child_left;
    int32_t  child_right;
    int32_t  depth;
    int64_t  splat_offset;
    int64_t  splat_count;
    int64_t  total_descendant_splats;
    int32_t  summary_offset;    // into the summary array
    int32_t  summary_count;
    int32_t  chunk_index;       // -1 for internal nodes
    int32_t  pad = 0;
};

struct SlodChunkEntry {
    int64_t file_offset;
    int64_t byte_size;
    int32_t node_index;
    int32_t splat_count;
};

// Write a GaussianHierarchy to a .slod file.
void write_slod(const GaussianHierarchy& h, const std::string& path);

// Read a .slod file into a GaussianHierarchy.  When `metadata_only` is true,
// only the node tree and summary Gaussians are loaded; leaf Gaussian data is
// left empty (for streaming / inspection).
GaussianHierarchy read_slod(const std::string& path, bool metadata_only = false);

// Validate a .slod file without fully loading it: checks magic, version,
// offset ranges, node connectivity, and chunk bounds. Returns "" on success
// or a human-readable error.
std::string validate_slod(const std::string& path);

}  // namespace spirula

