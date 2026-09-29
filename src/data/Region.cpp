// Region.cpp -- see Region.h.

#include "data/Region.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "data/LabelField.h"

namespace spirula {

// ===========================================================================
// Region
// ===========================================================================

void Region::contains_many(const float* xyz, int64_t n, uint8_t* out) const {
#pragma omp parallel for schedule(static)
    for (int64_t i = 0; i < n; i++) {
        const double p[3] = {xyz[i * 3], xyz[i * 3 + 1], xyz[i * 3 + 2]};
        out[i] = contains(p) ? 1 : 0;
    }
}

void Region::contains_many(const double* xyz, int64_t n, uint8_t* out) const {
#pragma omp parallel for schedule(static)
    for (int64_t i = 0; i < n; i++) out[i] = contains(&xyz[i * 3]) ? 1 : 0;
}

namespace {

void write_vec(JsonWriter& w, const char* key, const double* v, int n) {
    w.key(key).array();
    for (int i = 0; i < n; i++) w.value(v[i]);
    w.end();
}

bool read_vec(const JsonValue& obj, const char* key, double* out, int n) {
    const JsonValue* a = obj.find(key);
    if (!a || !a->is_array() || (int)a->arr.size() != n) return false;
    for (int i = 0; i < n; i++) out[i] = a->arr[(size_t)i].as_double();
    return true;
}

}  // namespace

void region_write_json(JsonWriter& w, const Region& r) {
    w.object();
    w.field("type", r.kind());
    r.write_json(w);
    w.end();
}

std::string region_to_json(const Region& r) {
    JsonWriter w;
    region_write_json(w, r);
    return w.str();
}

std::unique_ptr<Region> region_from_json(const JsonValue& v, const std::string& base_dir,
                                         std::string& error) {
    if (!v.is_object()) { error = "region is not an object"; return nullptr; }
    const std::string type = v.find("type") ? v.find("type")->as_string() : "";
    if (type == "box") {
        auto r = std::make_unique<BoxRegion>();
        if (!read_vec(v, "center", r->center, 3) || !read_vec(v, "half", r->half, 3)) {
            error = "box needs center[3] and half[3]";
            return nullptr;
        }
        read_vec(v, "rotation", r->R, 9);
        return r;
    }
    if (type == "sphere") {
        auto r = std::make_unique<SphereRegion>();
        if (!read_vec(v, "center", r->center, 3) || !v.find("radius")) {
            error = "sphere needs center[3] and radius";
            return nullptr;
        }
        r->radius = v.find("radius")->as_double();
        return r;
    }
    if (type == "halfspace") {
        auto r = std::make_unique<HalfSpaceRegion>();
        if (!read_vec(v, "normal", r->normal, 3)) {
            error = "halfspace needs normal[3]";
            return nullptr;
        }
        r->offset = v.get_double("offset", 0.0);
        return r;
    }
    if (type == "mesh") {
        const JsonValue* xyz = v.find("vertices");
        const JsonValue* tri = v.find("triangles");
        if (!xyz || !tri || !xyz->is_array() || !tri->is_array()) {
            error = "mesh needs vertices[] and triangles[]";
            return nullptr;
        }
        std::vector<double> V;
        std::vector<uint32_t> F;
        for (const JsonValue& x : xyz->arr) V.push_back(x.as_double());
        for (const JsonValue& x : tri->arr) F.push_back((uint32_t)x.as_int());
        if (V.size() % 3 || F.size() % 3) {
            error = "mesh arrays are not multiples of three";
            return nullptr;
        }
        return std::make_unique<MeshRegion>(std::move(V), std::move(F));
    }
    if (type == "label") {
        const JsonValue* lv = v.find("label");
        const int label = lv ? (int)lv->as_int(-1) : -1;
        std::shared_ptr<LabelField> field;
        std::string file;
        if (const JsonValue* f = v.find("file")) {
            file = f->as_string();
            std::string path = file;
            if (!base_dir.empty() && !path.empty() && path[0] != '/' &&
                !(path.size() > 1 && path[1] == ':'))
                path = base_dir + "/" + path;
            try {
                field = std::make_shared<LabelField>(LabelField::read_file(path));
            } catch (const std::exception& e) {
                error = e.what();
                return nullptr;
            }
        } else if (const JsonValue* d = v.find("data")) {
            const std::string bytes = base64_decode(d->as_string());
            size_t used = 0;
            field = std::make_shared<LabelField>();
            if (!LabelField::read(bytes.data(), bytes.size(), used, *field)) {
                error = "label field data is not a field";
                return nullptr;
            }
        } else {
            error = "label needs file or data";
            return nullptr;
        }
        if (label < 0) { error = "label needs a label"; return nullptr; }
        return std::make_unique<LabelRegion>(field, label, file);
    }
    CsgOp op;
    if (csg_op_from_name(type, op)) {
        auto r = std::make_unique<CsgRegion>();
        r->op = op;
        const JsonValue* kids = v.find("children");
        if (!kids || !kids->is_array()) {
            error = type + " needs children[]";
            return nullptr;
        }
        for (const JsonValue& k : kids->arr) {
            std::unique_ptr<Region> c = region_from_json(k, base_dir, error);
            if (!c) return nullptr;
            r->children.emplace_back(std::move(c));
        }
        if (op == CsgOp::Complement && r->children.size() != 1) {
            error = "complement takes exactly one child";
            return nullptr;
        }
        return r;
    }
    error = "unknown region type '" + type + "'";
    return nullptr;
}

// ===========================================================================
// Primitives
// ===========================================================================

bool BoxRegion::contains(const double p[3]) const {
    const double d[3] = {p[0] - center[0], p[1] - center[1], p[2] - center[2]};
    for (int r = 0; r < 3; r++) {
        const double q = R[r * 3] * d[0] + R[r * 3 + 1] * d[1] + R[r * 3 + 2] * d[2];
        if (std::fabs(q) > half[r]) return false;
    }
    return true;
}

Aabb BoxRegion::bounds() const {
    Aabb b;
    for (int i = 0; i < 8; i++) {
        double p[3];
        for (int k = 0; k < 3; k++) {
            p[k] = center[k];
            for (int r = 0; r < 3; r++)
                p[k] += ((i >> r) & 1 ? half[r] : -half[r]) * R[r * 3 + k];
        }
        b.expand(p);
    }
    return b;
}

void BoxRegion::write_json(JsonWriter& w) const {
    write_vec(w, "center", center, 3);
    write_vec(w, "half", half, 3);
    write_vec(w, "rotation", R, 9);
}

bool SphereRegion::contains(const double p[3]) const {
    const double d[3] = {p[0] - center[0], p[1] - center[1], p[2] - center[2]};
    return d[0] * d[0] + d[1] * d[1] + d[2] * d[2] <= radius * radius;
}

Aabb SphereRegion::bounds() const {
    Aabb b;
    for (int k = 0; k < 3; k++) { b.lo[k] = center[k] - radius; b.hi[k] = center[k] + radius; }
    return b;
}

void SphereRegion::write_json(JsonWriter& w) const {
    write_vec(w, "center", center, 3);
    w.field("radius", radius);
}

bool HalfSpaceRegion::contains(const double p[3]) const {
    return normal[0] * p[0] + normal[1] * p[1] + normal[2] * p[2] + offset >= 0;
}

void HalfSpaceRegion::write_json(JsonWriter& w) const {
    write_vec(w, "normal", normal, 3);
    w.field("offset", offset);
}

// ---- mesh -----------------------------------------------------------------

namespace {

// A direction no mesh edge is likely to be parallel to, and the ray-triangle
// test along it (Moller-Trumbore, one-sided count of every hit ahead).
constexpr double kRayDir[3] = {0.5773502691896258, 0.5773502691896258 + 0.013, 0.5773502691896258 - 0.021};

bool ray_hits_triangle(const double o[3], const double* a, const double* b, const double* c) {
    const double e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    const double e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
    const double h[3] = {kRayDir[1] * e2[2] - kRayDir[2] * e2[1],
                         kRayDir[2] * e2[0] - kRayDir[0] * e2[2],
                         kRayDir[0] * e2[1] - kRayDir[1] * e2[0]};
    const double det = e1[0] * h[0] + e1[1] * h[1] + e1[2] * h[2];
    if (std::fabs(det) < 1e-14) return false;
    const double inv = 1.0 / det;
    const double s[3] = {o[0] - a[0], o[1] - a[1], o[2] - a[2]};
    const double u = inv * (s[0] * h[0] + s[1] * h[1] + s[2] * h[2]);
    if (u < 0.0 || u > 1.0) return false;
    const double q[3] = {s[1] * e1[2] - s[2] * e1[1], s[2] * e1[0] - s[0] * e1[2],
                         s[0] * e1[1] - s[1] * e1[0]};
    const double v = inv * (kRayDir[0] * q[0] + kRayDir[1] * q[1] + kRayDir[2] * q[2]);
    if (v < 0.0 || u + v > 1.0) return false;
    const double t = inv * (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]);
    return t > 1e-12;
}

bool ray_hits_box(const double o[3], const Aabb& b) {
    double t0 = 0, t1 = 1e300;
    for (int k = 0; k < 3; k++) {
        const double inv = 1.0 / kRayDir[k];
        double a = (b.lo[k] - o[k]) * inv, c = (b.hi[k] - o[k]) * inv;
        if (a > c) std::swap(a, c);
        t0 = std::max(t0, a);
        t1 = std::min(t1, c);
        if (t0 > t1) return false;
    }
    return true;
}

}  // namespace

MeshRegion::MeshRegion(std::vector<double> xyz, std::vector<uint32_t> tri)
    : _xyz(std::move(xyz)), _tri(std::move(tri)) {
    const uint32_t nf = (uint32_t)(_tri.size() / 3);
    _order.resize(nf);
    for (uint32_t i = 0; i < nf; i++) _order[i] = i;
    for (size_t i = 0; i + 2 < _xyz.size(); i += 3) _bounds.expand(&_xyz[i]);
    if (nf == 0) return;
    _nodes.reserve(2 * nf);
    _nodes.emplace_back();
    build(0, 0, nf, 0);
}

void MeshRegion::build(uint32_t node, uint32_t lo, uint32_t hi, int depth) {
    Node& n = _nodes[node];
    n.box = Aabb();
    for (uint32_t i = lo; i < hi; i++)
        for (int k = 0; k < 3; k++) n.box.expand(&_xyz[_tri[_order[i] * 3 + k] * 3]);
    if (hi - lo <= 8 || depth > 40) {
        n.first = lo;
        n.count = hi - lo;
        return;
    }
    int axis = 0;
    for (int k = 1; k < 3; k++)
        if (n.box.hi[k] - n.box.lo[k] > n.box.hi[axis] - n.box.lo[axis]) axis = k;
    auto centroid = [&](uint32_t t) {
        return (_xyz[_tri[t * 3] * 3 + axis] + _xyz[_tri[t * 3 + 1] * 3 + axis] +
                _xyz[_tri[t * 3 + 2] * 3 + axis]) / 3.0;
    };
    const uint32_t mid = (lo + hi) / 2;
    std::nth_element(_order.begin() + lo, _order.begin() + mid, _order.begin() + hi,
                     [&](uint32_t a, uint32_t b) { return centroid(a) < centroid(b); });
    const uint32_t left = (uint32_t)_nodes.size();
    _nodes.emplace_back();
    _nodes.emplace_back();
    _nodes[node].left = left;
    _nodes[node].count = 0;
    build(left, lo, mid, depth + 1);
    build(left + 1, mid, hi, depth + 1);
}

int MeshRegion::crossings(const double p[3], uint32_t node) const {
    const Node& n = _nodes[node];
    if (!ray_hits_box(p, n.box)) return 0;
    if (n.count > 0) {
        int c = 0;
        for (uint32_t i = n.first; i < n.first + n.count; i++) {
            const uint32_t* t = &_tri[_order[i] * 3];
            if (ray_hits_triangle(p, &_xyz[t[0] * 3], &_xyz[t[1] * 3], &_xyz[t[2] * 3])) c++;
        }
        return c;
    }
    return crossings(p, n.left) + crossings(p, n.left + 1);
}

bool MeshRegion::contains(const double p[3]) const {
    if (_nodes.empty() || !_bounds.contains(p)) return false;
    return (crossings(p, 0) & 1) != 0;
}

void MeshRegion::write_json(JsonWriter& w) const {
    w.key("vertices").array();
    for (double v : _xyz) w.value(v);
    w.end();
    w.key("triangles").array();
    for (uint32_t t : _tri) w.value((long long)t);
    w.end();
}

// ===========================================================================
// CSG
// ===========================================================================

const char* csg_op_name(CsgOp op) {
    switch (op) {
        case CsgOp::Union: return "union";
        case CsgOp::Intersection: return "intersection";
        case CsgOp::Difference: return "difference";
        case CsgOp::Complement: return "complement";
    }
    return "union";
}

bool csg_op_from_name(const std::string& s, CsgOp& out) {
    for (CsgOp op : {CsgOp::Union, CsgOp::Intersection, CsgOp::Difference, CsgOp::Complement})
        if (s == csg_op_name(op)) { out = op; return true; }
    return false;
}

const char* CsgRegion::kind() const { return csg_op_name(op); }

int CsgRegion::op_code() const {
    switch (op) {
        case CsgOp::Union: return 4;
        case CsgOp::Intersection: return 5;
        case CsgOp::Difference: return 6;
        case CsgOp::Complement: return 7;
    }
    return 4;
}

bool CsgRegion::contains(const double p[3]) const {
    switch (op) {
        case CsgOp::Union:
            for (const auto& c : children) if (c->contains(p)) return true;
            return false;
        case CsgOp::Intersection:
            for (const auto& c : children) if (!c->contains(p)) return false;
            return !children.empty();
        case CsgOp::Difference:
            if (children.empty() || !children[0]->contains(p)) return false;
            for (size_t i = 1; i < children.size(); i++)
                if (children[i]->contains(p)) return false;
            return true;
        case CsgOp::Complement:
            return !children.empty() && !children[0]->contains(p);
    }
    return false;
}

Aabb CsgRegion::bounds() const {
    switch (op) {
        case CsgOp::Union: {
            Aabb b;
            for (const auto& c : children) b.expand(c->bounds());
            return b;
        }
        case CsgOp::Intersection: {
            Aabb b = Aabb::everything();
            for (const auto& c : children) {
                const Aabb o = c->bounds();
                for (int k = 0; k < 3; k++) {
                    b.lo[k] = std::max(b.lo[k], o.lo[k]);
                    b.hi[k] = std::min(b.hi[k], o.hi[k]);
                }
            }
            return b;
        }
        case CsgOp::Difference:
            return children.empty() ? Aabb() : children[0]->bounds();
        case CsgOp::Complement:
            return Aabb::everything();
    }
    return Aabb::everything();
}

void CsgRegion::write_json(JsonWriter& w) const {
    w.key("children").array();
    for (const auto& c : children) region_write_json(w, *c);
    w.end();
}

}  // namespace spirula
