#pragma once

// A region of 3-space answering one question -- is this point inside -- for
// every use that needs one: which part of a split scene owns a splat, which
// splats a region of interest keeps while training, what a selection tool
// drew. Leaves are the primitives; a CsgRegion combines them with boolean
// operations; every kind serializes to JSON and back through region_from_json.
// Coordinates are whatever frame the caller's points are in.

#include "core/Similarity.h"
#include "data/Json.h"
#include "data/JsonWrite.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace spirula {

struct RegionProgram;

struct Aabb {
    double lo[3] = {1e300, 1e300, 1e300};
    double hi[3] = {-1e300, -1e300, -1e300};
    bool empty() const { return lo[0] > hi[0]; }
    bool unbounded() const { return lo[0] <= -1e299 || hi[0] >= 1e299; }
    void expand(const double p[3]) {
        for (int k = 0; k < 3; k++) {
            lo[k] = p[k] < lo[k] ? p[k] : lo[k];
            hi[k] = p[k] > hi[k] ? p[k] : hi[k];
        }
    }
    void expand(const Aabb& o) {
        if (o.empty()) return;
        expand(o.lo);
        expand(o.hi);
    }
    bool contains(const double p[3]) const {
        return p[0] >= lo[0] && p[0] <= hi[0] && p[1] >= lo[1] && p[1] <= hi[1] &&
               p[2] >= lo[2] && p[2] <= hi[2];
    }
    static Aabb everything() {
        Aabb a;
        for (int k = 0; k < 3; k++) { a.lo[k] = -1e300; a.hi[k] = 1e300; }
        return a;
    }
};

class Region {
public:
    virtual ~Region() = default;
    virtual bool contains(const double p[3]) const = 0;
    // Where `contains` can be true. Conservative: may be larger than the
    // region, never smaller. Aabb::everything() for an unbounded one.
    virtual Aabb bounds() const = 0;
    virtual const char* kind() const = 0;
    // Adds this region's fields to an object the caller has opened.
    virtual void write_json(JsonWriter& w) const = 0;
    // Appends this region to a device program (data/RegionProgram.h); false
    // with `error` set for a kind the device does not evaluate.
    virtual bool emit(RegionProgram& out, std::string& error) const;

    // One flag per point of an [n,3] float array, in parallel.
    void contains_many(const float* xyz, int64_t n, uint8_t* out) const;
    void contains_many(const double* xyz, int64_t n, uint8_t* out) const;
    // `contains` spelled out, since the array overload hides this in a subclass.
    bool inside(double x, double y, double z) const {
        const double p[3] = {x, y, z};
        return contains(p);
    }
};

// The object form, {"type": kind, ...}, on its own line-free.
std::string region_to_json(const Region& r);
void region_write_json(JsonWriter& w, const Region& r);
// Null and a message in `error` when the JSON names no region this build
// knows. `base_dir` resolves any file a region refers to.
std::unique_ptr<Region> region_from_json(const JsonValue& v, const std::string& base_dir,
                                         std::string& error);

// ---- primitives -----------------------------------------------------------

// An oriented box: |R^T (p - c)| <= half, componentwise. R row-major, the
// box's axes as its rows.
class BoxRegion : public Region {
public:
    double center[3] = {0, 0, 0};
    double half[3] = {1, 1, 1};
    double R[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    bool contains(const double p[3]) const override;
    Aabb bounds() const override;
    const char* kind() const override { return "box"; }
    void write_json(JsonWriter& w) const override;
    bool emit(RegionProgram& out, std::string& error) const override;
};

class SphereRegion : public Region {
public:
    double center[3] = {0, 0, 0};
    double radius = 1;
    bool contains(const double p[3]) const override;
    Aabb bounds() const override;
    const char* kind() const override { return "sphere"; }
    void write_json(JsonWriter& w) const override;
    bool emit(RegionProgram& out, std::string& error) const override;
};

// The box's frame and half extents, inside where sum (q_k / half_k)^2 <= 1.
class EllipsoidRegion : public Region {
public:
    double center[3] = {0, 0, 0};
    double half[3] = {1, 1, 1};
    double R[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    bool contains(const double p[3]) const override;
    Aabb bounds() const override;
    const char* kind() const override { return "ellipsoid"; }
    void write_json(JsonWriter& w) const override;
    bool emit(RegionProgram& out, std::string& error) const override;
};

// An elliptic cylinder along the frame's third axis: (q0/half0)^2 +
// (q1/half1)^2 <= 1 and |q2| <= half2.
class CylinderRegion : public Region {
public:
    double center[3] = {0, 0, 0};
    double half[3] = {1, 1, 1};
    double R[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    bool contains(const double p[3]) const override;
    Aabb bounds() const override;
    const char* kind() const override { return "cylinder"; }
    void write_json(JsonWriter& w) const override;
    bool emit(RegionProgram& out, std::string& error) const override;
};

// A polygon in the frame's first two axes, extruded over |q2| <= half_height.
// Even-odd rule, so an outline that crosses itself still has an answer.
class PrismRegion : public Region {
public:
    double center[3] = {0, 0, 0};
    double R[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    double half_height = 1;
    std::vector<double> polygon;   // x,y pairs in the frame, about `center`
    bool contains(const double p[3]) const override;
    Aabb bounds() const override;
    const char* kind() const override { return "prism"; }
    void write_json(JsonWriter& w) const override;
    bool emit(RegionProgram& out, std::string& error) const override;
};

// Even-odd point in polygon, `poly` x,y pairs.
bool polygon_contains(const double* poly, size_t num_vertices, double x, double y);

// n . p + d >= 0 is inside.
class HalfSpaceRegion : public Region {
public:
    double normal[3] = {0, 0, 1};
    double offset = 0;
    bool contains(const double p[3]) const override;
    Aabb bounds() const override { return Aabb::everything(); }
    const char* kind() const override { return "halfspace"; }
    void write_json(JsonWriter& w) const override;
    bool emit(RegionProgram& out, std::string& error) const override;
};

// A closed triangle mesh, inside by ray parity. A mesh with holes gives
// parity's answer, which is wrong only for rays through the hole; the
// direction is fixed and irrational-ish so shared edges are not hit twice.
class MeshRegion : public Region {
public:
    // `xyz` [nv,3], `tri` [nf,3] vertex indices. Builds the BVH.
    MeshRegion(std::vector<double> xyz, std::vector<uint32_t> tri);
    bool contains(const double p[3]) const override;
    Aabb bounds() const override { return _bounds; }
    const char* kind() const override { return "mesh"; }
    void write_json(JsonWriter& w) const override;
    const std::vector<double>& vertices() const { return _xyz; }
    const std::vector<uint32_t>& triangles() const { return _tri; }

private:
    struct Node {
        Aabb box;
        uint32_t first = 0, count = 0;   // leaf: triangles [first, first+count)
        uint32_t left = 0;               // interior: children left, left + 1
    };
    void build(uint32_t node, uint32_t lo, uint32_t hi, int depth);
    int crossings(const double p[3], uint32_t node) const;
    std::vector<double> _xyz;
    std::vector<uint32_t> _tri;
    std::vector<uint32_t> _order;   // triangle indices, leaf-contiguous
    std::vector<Node> _nodes;
    Aabb _bounds;
};

// ---- combinations ---------------------------------------------------------

enum class CsgOp { Union, Intersection, Difference, Complement };

// Union / intersection over every child; Difference is the first child minus
// the rest; Complement negates its single child.
class CsgRegion : public Region {
public:
    CsgOp op = CsgOp::Union;
    std::vector<std::shared_ptr<const Region>> children;
    bool contains(const double p[3]) const override;
    Aabb bounds() const override;
    const char* kind() const override;
    void write_json(JsonWriter& w) const override;
    bool emit(RegionProgram& out, std::string& error) const override;
    // The shader's type code for this operation (shaders/region.slang).
    int op_code() const;
};

const char* csg_op_name(CsgOp op);
bool csg_op_from_name(const std::string& s, CsgOp& out);

}  // namespace spirula
