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
    // p' = xf_scale * p + xf_shift, applied by push() in double before the
    // narrowing to float: a geo-referenced region does not survive float.
    double xf_scale = 1.0;
    double xf_shift[3] = {0, 0, 0};
    int num_nodes() const { return (int)(nodes.size() / (4 * kNodeStride)); }
    bool empty() const { return nodes.empty(); }

    // Appends one node; the leaf types fill the slots the shader reads.
    void push(int type, const double* center_or_normal = nullptr, double radius_or_offset = 0,
              const double* half = nullptr, const double* rotation = nullptr, int label = 0);
    // A prism node and the whole nodes its polygon fills after it, which
    // the evaluator skips; the vertex count is in the header's second slot.
    void push_prism(const double* center, const double* rotation, double half_height,
                    const std::vector<double>& polygon);
    // Nodes the polygon of a prism with `num_vertices` fills.
    static int payload_nodes(int num_vertices) {
        return (2 * num_vertices + 4 * kNodeStride - 1) / (4 * kNodeStride);
    }
    // Moves the compiled program to p' = scale * p + shift (scale > 0), in
    // float; compile_region takes the same similarity in double.
    void apply_similarity(double scale, const double shift[3]);
    // The label field's half of that.
    void move_field(double scale, const double shift[3]);
};

// False with `error` set when the region has a part no program can hold (a
// mesh) or refers to two different label fields. `scale` and `shift` take it
// to the frame p' = scale * p + shift on the way, in double.
bool compile_region(const Region& r, RegionProgram& out, std::string& error,
                    double scale = 1.0, const double* shift = nullptr);

// Host evaluation, the mirror of region_contains in shaders/region.slang.
bool program_contains(const RegionProgram& p, const double point[3], const double* normal = nullptr);

// The splat's short axis pointed along `toward`, the mirror of splat_normal
// in shaders/region.slang: quat (w, x, y, z), log scales.
void splat_normal(const float quat[4], const float log_scale[3], const double toward[3],
                  double out[3]);

}  // namespace spirula
