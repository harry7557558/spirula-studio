// HierarchyFile.cpp -- .slod serialization.

#include "lod/HierarchyFile.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>

namespace spirula {

namespace {

int64_t splat_payload_floats(int sh_degree, int sb_lobes) {
    int64_t coeffs = 0;
    if (sh_degree > 0 || sb_lobes > 0) {
        int64_t sh = (int64_t)(sh_degree + 1) * (sh_degree + 1) - 1;
        coeffs = sh + sb_lobes * 3;  // 3 floats per beta lobe field
    }
    // means(3) + quats(4) + scales(3) + opacity(1) + dc(3) + sh(coeffs*3)
    return 3 + 4 + 3 + 1 + 3 + coeffs * 3;
}

}  // namespace

void write_slod(const GaussianHierarchy& h, const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("write_slod: cannot open " + path);

    const int32_t n_nodes = (int32_t)h.nodes.size();

    // Count leaves and total summary Gaussians.
    int32_t n_chunks = 0;
    int64_t total_summary = 0;
    for (const auto& nd : h.nodes) {
        if (nd.is_leaf()) n_chunks++;
        total_summary += (int64_t)nd.summary.size();
    }

    // Build header.
    SlodFileHeader hdr;
    hdr.root_node = h.root;
    hdr.max_depth = h.max_depth;
    hdr.num_splats = h.num_splats;
    hdr.num_nodes = n_nodes;
    hdr.num_chunks = n_chunks;
    hdr.sh_degree = h.sh_degree;
    hdr.sb_lobes = h.sb_lobes;
    hdr.total_summary = total_summary;
    hdr.node_table_offset = sizeof(SlodFileHeader);
    hdr.summary_offset = hdr.node_table_offset + (int64_t)n_nodes * sizeof(SlodNodeRecord);
    hdr.chunk_table_offset = hdr.summary_offset + total_summary * (int64_t)sizeof(SummaryGaussian);
    hdr.payload_offset = hdr.chunk_table_offset + (int64_t)n_chunks * sizeof(SlodChunkEntry);

    out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));

    // Node records + assign summary offsets and chunk indices.
    int32_t summary_cursor = 0;
    int32_t chunk_cursor = 0;
    std::vector<SlodNodeRecord> records((size_t)n_nodes);
    for (int32_t i = 0; i < n_nodes; i++) {
        const auto& nd = h.nodes[(size_t)i];
        auto& r = records[(size_t)i];
        for (int j = 0; j < 3; j++) { r.bounds_lo[j] = nd.bounds.lo[j]; r.bounds_hi[j] = nd.bounds.hi[j]; }
        r.error_radius = nd.error_radius;
        r.parent = nd.parent;
        r.child_left = nd.child_left;
        r.child_right = nd.child_right;
        r.depth = nd.depth;
        r.splat_offset = nd.splat_offset;
        r.splat_count = nd.splat_count;
        r.total_descendant_splats = nd.total_descendant_splats;
        r.summary_offset = summary_cursor;
        r.summary_count = (int32_t)nd.summary.size();
        summary_cursor += r.summary_count;
        r.chunk_index = nd.is_leaf() ? chunk_cursor++ : -1;
    }
    out.write(reinterpret_cast<const char*>(records.data()),
              (std::streamsize)(records.size() * sizeof(SlodNodeRecord)));

    // Summary Gaussians.
    for (const auto& nd : h.nodes)
        if (!nd.summary.empty())
            out.write(reinterpret_cast<const char*>(nd.summary.data()),
                      (std::streamsize)(nd.summary.size() * sizeof(SummaryGaussian)));

    // Chunk table: compute offsets into the payload section.
    const int64_t floats_per = splat_payload_floats(h.sh_degree, h.sb_lobes);
    int64_t payload_cursor = hdr.payload_offset;
    std::vector<SlodChunkEntry> chunks((size_t)n_chunks);
    chunk_cursor = 0;
    for (int32_t i = 0; i < n_nodes; i++) {
        const auto& nd = h.nodes[(size_t)i];
        if (!nd.is_leaf()) continue;
        auto& ce = chunks[(size_t)chunk_cursor];
        ce.file_offset = payload_cursor;
        ce.byte_size = nd.splat_count * floats_per * (int64_t)sizeof(float);
        ce.node_index = i;
        ce.splat_count = (int32_t)nd.splat_count;
        payload_cursor += ce.byte_size;
        chunk_cursor++;
    }
    out.write(reinterpret_cast<const char*>(chunks.data()),
              (std::streamsize)(chunks.size() * sizeof(SlodChunkEntry)));

    // Gaussian payloads: write each leaf's splats interleaved.
    const int64_t coeffs = h.coeffs();
    std::vector<float> row((size_t)floats_per);
    for (int32_t ci = 0; ci < n_chunks; ci++) {
        const auto& ce = chunks[(size_t)ci];
        const auto& nd = h.nodes[(size_t)ce.node_index];
        for (int64_t s = 0; s < nd.splat_count; s++) {
            const int64_t si = nd.splat_offset + s;
            float* p = row.data();
            std::copy_n(&h.means[(size_t)si * 3], 3, p); p += 3;
            std::copy_n(&h.quats[(size_t)si * 4], 4, p); p += 4;
            std::copy_n(&h.scales[(size_t)si * 3], 3, p); p += 3;
            *p++ = h.opacities[(size_t)si];
            std::copy_n(&h.features_dc[(size_t)si * 3], 3, p); p += 3;
            if (coeffs > 0)
                std::copy_n(&h.features_sh[(size_t)si * coeffs * 3], coeffs * 3, p);
            out.write(reinterpret_cast<const char*>(row.data()),
                      (std::streamsize)(row.size() * sizeof(float)));
        }
    }
}

GaussianHierarchy read_slod(const std::string& path, bool metadata_only) {
    std::string err = validate_slod(path);
    if (!err.empty())
        throw std::runtime_error("read_slod: " + err + " in " + path);

    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("read_slod: cannot open " + path);

    SlodFileHeader hdr;
    in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));

    GaussianHierarchy h;
    h.root = hdr.root_node;
    h.max_depth = hdr.max_depth;
    h.num_splats = hdr.num_splats;
    h.sh_degree = hdr.sh_degree;
    h.sb_lobes = hdr.sb_lobes;
    h.nodes.resize((size_t)hdr.num_nodes);

    // Node records.
    std::vector<SlodNodeRecord> records((size_t)hdr.num_nodes);
    in.seekg(hdr.node_table_offset);
    in.read(reinterpret_cast<char*>(records.data()),
            (std::streamsize)(records.size() * sizeof(SlodNodeRecord)));

    // Summary Gaussians.
    std::vector<SummaryGaussian> all_summary((size_t)hdr.total_summary);
    in.seekg(hdr.summary_offset);
    in.read(reinterpret_cast<char*>(all_summary.data()),
            (std::streamsize)(all_summary.size() * sizeof(SummaryGaussian)));

    for (int32_t i = 0; i < hdr.num_nodes; i++) {
        const auto& r = records[(size_t)i];
        auto& nd = h.nodes[(size_t)i];
        for (int j = 0; j < 3; j++) { nd.bounds.lo[j] = r.bounds_lo[j]; nd.bounds.hi[j] = r.bounds_hi[j]; }
        nd.error_radius = r.error_radius;
        nd.parent = r.parent;
        nd.child_left = r.child_left;
        nd.child_right = r.child_right;
        nd.depth = r.depth;
        nd.splat_offset = r.splat_offset;
        nd.splat_count = r.splat_count;
        nd.total_descendant_splats = r.total_descendant_splats;
        if (r.summary_count > 0)
            nd.summary.assign(all_summary.begin() + r.summary_offset,
                              all_summary.begin() + r.summary_offset + r.summary_count);
    }

    if (metadata_only) return h;

    // Read chunk table.
    std::vector<SlodChunkEntry> chunks((size_t)hdr.num_chunks);
    in.seekg(hdr.chunk_table_offset);
    in.read(reinterpret_cast<char*>(chunks.data()),
            (std::streamsize)(chunks.size() * sizeof(SlodChunkEntry)));

    // Allocate and read Gaussian payloads.
    const int64_t coeffs = h.coeffs();
    const int64_t floats_per = splat_payload_floats(h.sh_degree, h.sb_lobes);
    h.means.resize((size_t)(h.num_splats * 3));
    h.quats.resize((size_t)(h.num_splats * 4));
    h.scales.resize((size_t)(h.num_splats * 3));
    h.opacities.resize((size_t)h.num_splats);
    h.features_dc.resize((size_t)(h.num_splats * 3));
    if (coeffs > 0) h.features_sh.resize((size_t)(h.num_splats * coeffs * 3));

    std::vector<float> row((size_t)floats_per);
    for (const auto& ce : chunks) {
        in.seekg(ce.file_offset);
        const auto& nd = h.nodes[(size_t)ce.node_index];
        for (int64_t s = 0; s < nd.splat_count; s++) {
            in.read(reinterpret_cast<char*>(row.data()),
                    (std::streamsize)(row.size() * sizeof(float)));
            const int64_t si = nd.splat_offset + s;
            const float* p = row.data();
            std::copy_n(p, 3, &h.means[(size_t)si * 3]); p += 3;
            std::copy_n(p, 4, &h.quats[(size_t)si * 4]); p += 4;
            std::copy_n(p, 3, &h.scales[(size_t)si * 3]); p += 3;
            h.opacities[(size_t)si] = *p++;
            std::copy_n(p, 3, &h.features_dc[(size_t)si * 3]); p += 3;
            if (coeffs > 0)
                std::copy_n(p, coeffs * 3, &h.features_sh[(size_t)si * coeffs * 3]);
        }
    }

    if (!in) throw std::runtime_error("read_slod: truncated file " + path);
    return h;
}

std::string validate_slod(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return "cannot open file";

    in.seekg(0, std::ios::end);
    const int64_t file_size = in.tellg();
    in.seekg(0);

    if (file_size < (int64_t)sizeof(SlodFileHeader))
        return "file too small for header";

    SlodFileHeader hdr;
    in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (hdr.magic != kSlodMagic) return "bad magic";
    if (hdr.version != kSlodVersion)
        return "unsupported version " + std::to_string(hdr.version);
    if (hdr.num_nodes <= 0 || hdr.num_nodes > 50000000) return "invalid node count";
    if (hdr.num_splats < 0 || hdr.num_splats > 2000000000LL) return "invalid splat count";
    if (hdr.total_summary < 0 || hdr.total_summary > 1000000000LL) return "invalid summary count";
    if (hdr.num_chunks < 0 || hdr.num_chunks > hdr.num_nodes) return "invalid chunk count";
    if (hdr.root_node < 0 || hdr.root_node >= hdr.num_nodes) return "root out of range";

    const int64_t node_end = hdr.node_table_offset + (int64_t)hdr.num_nodes * sizeof(SlodNodeRecord);
    if (node_end > file_size) return "node table extends past EOF";
    const int64_t summary_end = hdr.summary_offset + hdr.total_summary * (int64_t)sizeof(SummaryGaussian);
    if (summary_end > file_size) return "summary data extends past EOF";
    const int64_t chunk_end = hdr.chunk_table_offset + (int64_t)hdr.num_chunks * sizeof(SlodChunkEntry);
    if (chunk_end > file_size) return "chunk table extends past EOF";

    // Validate node connectivity and offsets.
    std::vector<SlodNodeRecord> records((size_t)hdr.num_nodes);
    in.seekg(hdr.node_table_offset);
    in.read(reinterpret_cast<char*>(records.data()),
            (std::streamsize)(records.size() * sizeof(SlodNodeRecord)));
    for (int32_t i = 0; i < hdr.num_nodes; i++) {
        const auto& r = records[(size_t)i];
        if (r.child_left >= 0 && (r.child_left >= hdr.num_nodes || r.child_right >= hdr.num_nodes))
            return "child index out of range at node " + std::to_string(i);
        if (r.parent >= hdr.num_nodes)
            return "parent index out of range at node " + std::to_string(i);
        if (r.summary_count < 0 || r.summary_offset < 0 ||
            (int64_t)r.summary_offset + r.summary_count > hdr.total_summary)
            return "invalid summary offset/count at node " + std::to_string(i);
        if (r.splat_count < 0 || r.splat_offset < 0 ||
            r.splat_offset + r.splat_count > hdr.num_splats)
            return "invalid splat offset/count at node " + std::to_string(i);
    }

    // Validate chunks.
    std::vector<SlodChunkEntry> chunks((size_t)hdr.num_chunks);
    in.seekg(hdr.chunk_table_offset);
    in.read(reinterpret_cast<char*>(chunks.data()),
            (std::streamsize)(chunks.size() * sizeof(SlodChunkEntry)));
    for (int32_t ci = 0; ci < hdr.num_chunks; ci++) {
        const auto& ce = chunks[(size_t)ci];
        if (ce.file_offset < 0 || ce.byte_size < 0 || ce.file_offset + ce.byte_size > file_size)
            return "chunk " + std::to_string(ci) + " extends past EOF";
        if (ce.node_index < 0 || ce.node_index >= hdr.num_nodes)
            return "chunk " + std::to_string(ci) + " has invalid node index";
        if (ce.splat_count < 0 || ce.splat_count > hdr.num_splats)
            return "chunk " + std::to_string(ci) + " has invalid splat count";
    }

    return "";
}

}  // namespace spirula

