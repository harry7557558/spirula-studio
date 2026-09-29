#pragma once

// A Region compiled for the device: the post-order node list
// shaders/region.slang evaluates, plus the one label field it may refer to.
// data/RegionProgram.cpp also evaluates it on the host, which is what the
// parity test compares the kernel against.

#include <memory>
#include <string>
#include <vector>

namespace spirula {

class Region;
struct LabelField;

struct RegionProgram {
    static constexpr int kNodeStride = 6;   // float4 per node
    std::vector<float> nodes;               // [num_nodes, 6, 4]
    std::shared_ptr<const LabelField> field;
    int num_nodes() const { return (int)(nodes.size() / (4 * kNodeStride)); }
    bool empty() const { return nodes.empty(); }

    // Appends one node; the leaf types fill the slots the shader reads.
    void push(int type, const double* center_or_normal = nullptr, double radius_or_offset = 0,
              const double* half = nullptr, const double* rotation = nullptr, int label = 0);
    // Moves the program to the frame p' = scale * p + shift (scale > 0): the
    // trainer's, from the dataset's the region was drawn in.
    void apply_similarity(double scale, const double shift[3]);
};

// False with `error` set when the region has a part no program can hold (a
// mesh) or refers to two different label fields.
bool compile_region(const Region& r, RegionProgram& out, std::string& error);

// Host evaluation, the mirror of region_contains in shaders/region.slang.
bool program_contains(const RegionProgram& p, const double point[3], const double* normal = nullptr);

// The splat's short axis pointed along `toward`, the mirror of splat_normal
// in shaders/region.slang: quat (w, x, y, z), log scales.
void splat_normal(const float quat[4], const float log_scale[3], const double toward[3],
                  double out[3]);

}  // namespace spirula
