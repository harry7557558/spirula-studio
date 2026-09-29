// Regions of space on the device: a post-order program of primitives and
// boolean operations, and the label field -- labelled seed points with the
// direction they were seen from, queried for the nearest under a
// visibility-aware metric. Everything is float4 arrays so both backends pass
// them untyped; data/LabelField.cpp and data/RegionProgram.cpp are the host
// mirrors and region_parity holds them to this file. Layouts: program node =
// kRegionNodeStride float4 ((type, a, b, label) | center/normal + radius/
// offset | half extents | three rotation rows); field node = (lo, first|left)
// (hi, count), count 0 = inner with children first and first + 1; seed =
// (xyz, label | oct-direction << 8), direction all ones = seen from everywhere.

static const uint kRegionNodeStride = 6;
static const uint kRegionBox = 0;
static const uint kRegionSphere = 1;
static const uint kRegionHalfSpace = 2;
static const uint kRegionLabel = 3;
static const uint kRegionUnion = 4;
static const uint kRegionIntersection = 5;
static const uint kRegionDifference = 6;
static const uint kRegionComplement = 7;
static const uint kRegionAll = 8;
static const uint kRegionNone = 9;
static const uint kLabelNone = 255;
static const uint kSeedOmni = 0xFFFFFF;

// Behind a seed (relative to where it was seen from) counts kBehindWeight
// times the distance; a query normal leaning away from that direction (for a
// camera: from the camera) costs kOrientPenalty, past kOrientCutoff it is unseen.
static const float kBehindWeight = 8.0;
static const float kOrientPenalty = 20.0;
static const float kOrientCutoff = -0.5;

float3 oct_decode(uint packed) {
    float u = float(packed & 0xFFFu) / 4095.0 * 2.0 - 1.0;
    float v = float((packed >> 12) & 0xFFFu) / 4095.0 * 2.0 - 1.0;
    float3 n = float3(u, v, 1.0 - abs(u) - abs(v));
    float t = max(-n.z, 0.0);
    n.x += n.x >= 0.0 ? -t : t;
    n.y += n.y >= 0.0 ? -t : t;
    return normalize(n);
}

float aabb_d2(float3 p, float3 lo, float3 hi) {
    float3 d = max(max(lo - p, p - hi), float3(0.0));
    return dot(d, d);
}

// The metric between a query and one seed, negative for a seed that could
// not have seen the query; see the constants above.
float seed_d2(float3 p, float3 n, bool has_n, float4 seed) {
    float3 d = p - seed.xyz;
    float e2 = dot(d, d);
    uint packed = asuint(seed.w);
    uint dir_bits = packed >> 8;
    float3 dir;
    if (dir_bits != kSeedOmni) {
        dir = oct_decode(dir_bits);
        float behind = max(0.0, -dot(d, dir));
        e2 += (kBehindWeight * behind) * (kBehindWeight * behind);
    } else {
        dir = -d * rsqrt(max(e2, 1e-30));
    }
    if (has_n) {
        float a = dot(n, dir);
        if (a < kOrientCutoff) return -1.0;
        e2 *= 1.0 + kOrientPenalty * max(0.0, -a);
    }
    return e2;
}

// Label of the nearest seed, kLabelNone for an empty field.
[CudaDeviceExport]
uint label_field_nearest(float3 p, float3 n, bool has_n, float4* bvh, float4* seeds,
                         uint num_nodes) {
    if (num_nodes == 0) return kLabelNone;
    uint stack[48];
    uint sp = 0;
    stack[sp++] = 0;
    float best = 3.0e38;
    uint label = kLabelNone;
    while (sp > 0) {
        uint node = stack[--sp];
        float4 n0 = bvh[2 * node];
        float4 n1 = bvh[2 * node + 1];
        if (aabb_d2(p, n0.xyz, n1.xyz) >= best) continue;
        uint first = asuint(n0.w);
        uint count = asuint(n1.w);
        if (count > 0) {
            for (uint i = first; i < first + count; i++) {
                float4 s = seeds[i];
                float e2 = seed_d2(p, n, has_n, s);
                if (e2 >= 0.0 && e2 < best) {
                    best = e2;
                    label = asuint(s.w) & 0xFFu;
                }
            }
        } else {
            // Nearer child on top of the stack.
            float dl = aabb_d2(p, bvh[2 * first].xyz, bvh[2 * first + 1].xyz);
            float dr = aabb_d2(p, bvh[2 * first + 2].xyz, bvh[2 * first + 3].xyz);
            uint nearer = dl <= dr ? first : first + 1;
            uint farther = dl <= dr ? first + 1 : first;
            if (sp + 2 <= 48) {
                stack[sp++] = farther;
                stack[sp++] = nearer;
            }
        }
    }
    return label;
}

// Whether `p` (with normal `n` when has_n) is inside the program's region.
[CudaDeviceExport]
bool region_contains(float3 p, float3 n, bool has_n, float4* program, uint num_prog,
                     float4* bvh, float4* seeds, uint num_field_nodes) {
    bool stack[32] = {};
    uint sp = 0;
    for (uint i = 0; i < num_prog; i++) {
        float4 h = program[i * kRegionNodeStride];
        uint type = uint(h.x);
        bool v = false;
        if (type == kRegionBox) {
            float4 c = program[i * kRegionNodeStride + 1];
            float4 half_ = program[i * kRegionNodeStride + 2];
            float3 d = p - c.xyz;
            v = true;
            for (uint r = 0; r < 3; r++) {
                float4 row = program[i * kRegionNodeStride + 3 + r];
                float q = dot(row.xyz, d);
                if (abs(q) > half_[r]) v = false;
            }
        } else if (type == kRegionSphere) {
            float4 c = program[i * kRegionNodeStride + 1];
            float3 d = p - c.xyz;
            v = dot(d, d) <= c.w * c.w;
        } else if (type == kRegionHalfSpace) {
            float4 c = program[i * kRegionNodeStride + 1];
            v = dot(c.xyz, p) + c.w >= 0.0;
        } else if (type == kRegionLabel) {
            v = label_field_nearest(p, n, has_n, bvh, seeds, num_field_nodes) == uint(h.w);
        } else if (type == kRegionAll) {
            v = true;
        } else if (type == kRegionNone) {
            v = false;
        } else if (type == kRegionComplement) {
            if (sp > 0) v = !stack[--sp];
        } else {
            bool b = sp > 0 ? stack[--sp] : false;
            bool a = sp > 0 ? stack[--sp] : false;
            if (type == kRegionUnion) v = a || b;
            else if (type == kRegionIntersection) v = a && b;
            else v = a && !b;
        }
        if (sp < 32) stack[sp++] = v;
    }
    return sp > 0 ? stack[sp - 1] : false;
}

// The splat's short axis, the column of its rotation with the smallest
// scale, pointed toward `toward` (a camera the splat was trained from).
[CudaDeviceExport]
float3 splat_normal(float4 quat, float3 log_scale, float3 mean, float3 toward) {
    float w = quat.x, x = quat.y, y = quat.z, z = quat.w;
    float inv = rsqrt(max(w * w + x * x + y * y + z * z, 1e-20));
    w *= inv; x *= inv; y *= inv; z *= inv;
    float3 c0 = float3(1.0 - 2.0 * (y * y + z * z), 2.0 * (x * y + z * w), 2.0 * (x * z - y * w));
    float3 c1 = float3(2.0 * (x * y - z * w), 1.0 - 2.0 * (x * x + z * z), 2.0 * (y * z + x * w));
    float3 c2 = float3(2.0 * (x * z + y * w), 2.0 * (y * z - x * w), 1.0 - 2.0 * (x * x + y * y));
    float3 axis = c0;
    if (log_scale.y < log_scale.x && log_scale.y <= log_scale.z) axis = c1;
    else if (log_scale.z < log_scale.x && log_scale.z < log_scale.y) axis = c2;
    if (dot(axis, toward - mean) < 0.0) axis = -axis;
    return axis;
}
