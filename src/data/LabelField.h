#pragma once

// A partition of space by labelled seed points: every point of space belongs
// to the nearest seed under a metric that knows which side a seed was seen
// from (shaders/region.slang has the constants and the layout; this is the
// host mirror). Density-adaptive and unbounded by construction, and a wall
// seen from one room does not claim the other room's air. Built once, then
// queried here or uploaded as two float4 arrays for the device.

#include "data/Region.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace spirula {

struct LabelField {
    static constexpr int kNone = 255;
    static constexpr uint32_t kOmni = 0xFFFFFFu;
    static constexpr float kBehindWeight = 8.0f;
    static constexpr float kOrientPenalty = 20.0f;
    static constexpr float kOrientCutoff = -0.5f;

    // Seeds [n,4]: xyz, and label | direction << 8 stored as float bits.
    // Nodes [m,8]: (lo, first) (hi, count); count 0 = inner, children first
    // and first + 1.
    std::vector<float> seeds;
    std::vector<float> nodes;

    bool empty() const { return nodes.empty(); }
    int64_t num_seeds() const { return (int64_t)seeds.size() / 4; }
    int64_t num_nodes() const { return (int64_t)nodes.size() / 8; }

    // `dirs` [n,3] unit vectors toward where each point was seen from, or
    // null; a zero vector marks a seed seen from everywhere.
    static LabelField build(const float* xyz, const int32_t* labels, const float* dirs,
                            int64_t n, int leaf_size = 8);

    // Nearest seed's label; `n` is the query's normal, null for none.
    int label(const double p[3], const double* n = nullptr) const;
    int label(float x, float y, float z) const {
        const double p[3] = {x, y, z};
        return label(p);
    }
    void labels_of(const float* xyz, int64_t n, int32_t* out) const;

    int num_labels() const;
    std::vector<int64_t> histogram() const;
    Aabb bounds() const;
    // Points on a grid of `per_axis` cells over the seeds' box padded by a
    // tenth, each with its label: the partition drawn where no point is.
    void lattice(int per_axis, std::vector<float>& xyz_out,
                 std::vector<int32_t>& label_out) const;

    static uint32_t pack(int label, const float dir[3]);
    static uint32_t oct_encode(const float dir[3]);
    static void oct_decode(uint32_t bits, float out[3]);

    // "SSLF", u32 version 1, u64 seeds, u64 nodes, then the two arrays.
    void write(std::string& out) const;
    static bool read(const char* data, size_t size, size_t& consumed, LabelField& out);
    void write_file(const std::string& path) const;
    static LabelField read_file(const std::string& path);
};

// The cells of one label, as a Region. Holds the field by shared pointer so
// many labels of one field share it.
class LabelRegion : public Region {
public:
    LabelRegion(std::shared_ptr<const LabelField> field, int label, std::string file = "")
        : _field(std::move(field)), _label(label), _file(std::move(file)) {}
    bool contains(const double p[3]) const override {
        return _field && _field->label(p) == _label;
    }
    Aabb bounds() const override { return Aabb::everything(); }
    const char* kind() const override { return "label"; }
    // {"file": ..., "label": k} when the field came from a file, the field
    // inline (base64) otherwise.
    void write_json(JsonWriter& w) const override;
    bool emit(RegionProgram& out, std::string& error) const override;
    int label() const { return _label; }
    const std::shared_ptr<const LabelField>& field() const { return _field; }

private:
    std::shared_ptr<const LabelField> _field;
    int _label;
    std::string _file;
};

std::string base64_encode(const std::string& bytes);
std::string base64_decode(const std::string& text);

}  // namespace spirula
