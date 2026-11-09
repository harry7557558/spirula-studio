// Local covariance losses and derivatives, shared by CUDA and Vulkan.
// Neighborhood search and objectives: docs/notes/pca-reg.md.
// Positions are float*, three floats a splat; quats are (w, x, y, z).

// Overridden by the Vulkan entries, whose float atomics are atomic_float.slang's.
#ifndef SS_PCA_REG_ATOMIC_ADD
#define SS_PCA_REG_ATOMIC_ADD(ptr, v) InterlockedAdd((ptr)[0], v)
#endif

// A neighbourhood holds at most this many centres, its own included.
static const uint kPcaMaxNeighbors = 32;
// A tree node over at most this many points is scanned, not descended.
static const uint kPcaLeafSize = 16;
static const uint kPcaLeafFlag = 0x80000000u;

static const uint kPcaModeTree = 0;
static const uint kPcaModeWindow = 1;
static const uint kPcaModeBest = 2;
static const uint kPcaModeList = 3;

static const float kPcaFar = 3.0e38;

float3 pca_load3(float* p, uint i) {
    return float3(p[3 * i], p[3 * i + 1], p[3 * i + 2]);
}

// Floats as uints that sort the same way, so a box reduces with integer atomics.
[CudaDeviceExport]
uint pca_order_key(float f) {
    uint u = asuint(f);
    return (u & 0x80000000u) != 0u ? ~u : (u | 0x80000000u);
}

[CudaDeviceExport]
float pca_order_float(uint u) {
    return asfloat((u & 0x80000000u) != 0u ? (u & 0x7fffffffu) : ~u);
}

uint pca_expand10(uint v) {
    v &= 0x3FFu;
    v = (v * 0x00010001u) & 0xFF0000FFu;
    v = (v * 0x00000101u) & 0x0F00F00Fu;
    v = (v * 0x00000011u) & 0xC30C30C3u;
    v = (v * 0x00000005u) & 0x49249249u;
    return v;
}

// 21 bits an axis of p in the cube at `lo` of side 1 / inv_side, as the
// (low, high) words of a 63-bit key: no 64-bit integer reaches the shader.
[CudaDeviceExport]
uint2 pca_morton63(float3 p, float3 lo, float inv_side) {
    float3 u = clamp((p - lo) * inv_side, float3(0.0), float3(1.0)) * 2097151.0;
    uint x = uint(u.x), y = uint(u.y), z = uint(u.z);
    uint w0 = pca_expand10(x) | (pca_expand10(y) << 1) | (pca_expand10(z) << 2) |
              (((x >> 10) & 1u) << 30) | (((y >> 10) & 1u) << 31);
    uint w1 = ((z >> 10) & 1u) | (pca_expand10(x >> 11) << 1) |
              (pca_expand10(y >> 11) << 2) | (pca_expand10(z >> 11) << 3);
    return uint2(w0, w1);
}

// The Morton key of p in the box the order reduced (order keys: min xyz, max xyz).
[CudaDeviceExport]
uint2 pca_reg_key(float3 p, uint* box) {
    float3 lo = float3(pca_order_float(box[0]), pca_order_float(box[1]),
                       pca_order_float(box[2]));
    float3 hi = float3(pca_order_float(box[3]), pca_order_float(box[4]),
                       pca_order_float(box[5]));
    float side = max(max(hi.x - lo.x, hi.y - lo.y), hi.z - lo.z);
    return pca_morton63(p, lo, 1.0 / max(side, 1e-30));
}

// sorted[r] = centre of splat perm[r]; perm_out (when write_perm) copies perm.
[CudaDeviceExport]
void pca_reg_gather(uint r, int* perm, float* means, float4* sorted, int* perm_out,
                    uint write_perm) {
    int id = perm[r];
    sorted[r] = float4(pca_load3(means, uint(id)), 0.0);
    if (write_perm != 0u) perm_out[r] = id;
}

// The search tree: a radix tree over the sorted keys, built bottom-up as in
// Apetrei 2014; node p splits [first, last] after point p. The layout of
// nodes / meta / ranges is docs/notes/pca-reg.md's.

// How far apart points i and i + 1 are: the key xor, then the index xor so
// equal keys still split; past either end it is the largest.
uint3 pca_reg_delta(uint i, uint n, float4* sorted, uint* box) {
    if (i >= n - 1) return uint3(0xffffffffu);
    uint2 a = pca_reg_key(sorted[i].xyz, box), b = pca_reg_key(sorted[i + 1].xyz, box);
    return uint3(a.y ^ b.y, a.x ^ b.x, i ^ (i + 1));
}

bool pca_reg_less(uint3 a, uint3 b) {
    return a.x != b.x ? a.x < b.x : (a.y != b.y ? a.y < b.y : a.z < b.z);
}

// Point i climbs while it is the second child to arrive, merging boxes. The
// sibling's box and reference are read with atomics: written by another
// thread, possibly another multiprocessor, whose cache this one does not see.
[CudaDeviceExport]
void pca_reg_build(uint i, uint n, float4* sorted, uint* box, float4* nodes, uint* node_bits,
                   int4* meta, int* ranges) {
    uint first = i, last = i;
    int current = int(i | kPcaLeafFlag);
    float3 lo = sorted[i].xyz, hi = lo;
    uint3 dl = pca_reg_delta(i - 1, n, sorted, box), dr = pca_reg_delta(i, n, sorted, box);
    for (;;) {
        bool left = pca_reg_less(dr, dl);
        uint p = left ? last : first - 1;
        if (left) meta[p].x = current;
        else meta[p].y = current;
        DeviceMemoryBarrier();
        int other;
        InterlockedCompareExchange(ranges[p], -1, int(left ? first : last), other);
        if (other == -1) return;
        if (left) last = uint(other);
        else first = uint(other);
        DeviceMemoryBarrier();
        int sib;
        if (left) InterlockedOr(meta[p].y, 0, sib);
        else InterlockedOr(meta[p].x, 0, sib);
        if ((uint(sib) & kPcaLeafFlag) != 0u) {
            float3 q = sorted[uint(sib) & ~kPcaLeafFlag].xyz;
            lo = min(lo, q);
            hi = max(hi, q);
        } else {
            uint b0, b1, b2, b4, b5, b6;
            InterlockedOr(node_bits[8 * uint(sib)], 0u, b0);
            InterlockedOr(node_bits[8 * uint(sib) + 1], 0u, b1);
            InterlockedOr(node_bits[8 * uint(sib) + 2], 0u, b2);
            InterlockedOr(node_bits[8 * uint(sib) + 4], 0u, b4);
            InterlockedOr(node_bits[8 * uint(sib) + 5], 0u, b5);
            InterlockedOr(node_bits[8 * uint(sib) + 6], 0u, b6);
            lo = min(lo, float3(asfloat(b0), asfloat(b1), asfloat(b2)));
            hi = max(hi, float3(asfloat(b4), asfloat(b5), asfloat(b6)));
        }
        nodes[2 * p] = float4(lo, 0.0);
        nodes[2 * p + 1] = float4(hi, 0.0);
        meta[p].z = int(first);
        meta[p].w = int(last);
        current = int(p);
        if (first == 0 && last == n - 1) {
            ranges[n - 1] = int(p);
            return;
        }
        if (left) dr = pca_reg_delta(last, n, sorted, box);
        else dl = pca_reg_delta(first - 1, n, sorted, box);
    }
}

float pca_box_d2(float3 p, float4 lo, float4 hi) {
    float3 d = max(max(lo.xyz - p, p - hi.xyz), float3(0.0));
    return dot(d, d);
}

// The k nearest so far, ascending, in registers: K is a compile-time capacity
// >= k and every index is static, so an insert is a compare-swap chain. Indexed
// by a running count it spilled to local memory: best-of-window ran 3x slower.
struct PcaKnn<let K : int> {
    float d[K];
    uint id[K];
    float worst;  // the k-th distance

    [mutating]
    void reset() {
        [ForceUnroll]
        for (int j = 0; j < K; j++) {
            d[j] = kPcaFar;
            id[j] = 0;
        }
        worst = kPcaFar;
    }

    [mutating]
    void insert(uint k, float dist, uint i) {
        [ForceUnroll]
        for (int j = 0; j < K; j++) {
            bool take = dist < d[j];
            float td = d[j];
            uint ti = id[j];
            d[j] = take ? dist : td;
            id[j] = take ? i : ti;
            dist = take ? td : dist;
            i = take ? ti : i;
        }
        [ForceUnroll]
        for (int j = 0; j < K; j++)
            if (uint(j) + 1 == k) worst = d[j];
    }

    uint count(uint k) {
        uint c = 0;
        [ForceUnroll]
        for (int j = 0; j < K; j++)
            if (uint(j) < k && d[j] < kPcaFar) c++;
        return c;
    }

    // Candidates [first, last) except the query and the already-scanned [skip0, skip1).
    [mutating]
    void scan(float3 q, uint self, uint first, uint last, uint skip0, uint skip1,
              float4* sorted, uint k) {
        for (uint i = first; i < last; i++) {
            float3 e = sorted[i].xyz - q;
            bool skip = i == self || (i >= skip0 && i < skip1);
            float dist = skip ? kPcaFar : dot(e, e);
            if (dist < worst) insert(k, dist, i);
        }
    }
};

// Votes over the whole wave: every lane calls them, the launch clamping the
// lanes past n. On CUDA the full mask also reconverges the warp after a scan.
bool pca_wave_any(bool c) {
    __target_switch {
    case cuda: __intrinsic_asm "(__any_sync(0xffffffffu, $0) != 0)";
    default: return WaveActiveAnyTrue(c);
    }
}

uint pca_wave_count(bool c) {
    __target_switch {
    case cuda: __intrinsic_asm "__popc(__ballot_sync(0xffffffffu, $0))";
    default: return WaveActiveCountBits(c);
    }
}

// Exact: the 4(k + 1) Morton neighbours first, whose k-th distance prunes a
// nearest-first descent. The wave descends together into any node a lane
// needs; its queries are consecutive in Morton order (docs/notes/pca-reg.md).
void pca_knn_tree<let K : int>(uint r, uint n, uint k, float4* sorted, float4* nodes,
                               int4* meta, int* ranges, inout PcaKnn<K> h) {
    float3 q = sorted[r].xyz;
    uint w0 = r > 2 * (k + 1) ? r - 2 * (k + 1) : 0, w1 = min(r + 2 * (k + 1) + 1, n);
    h.scan(q, r, w0, w1, 0, 0, sorted, k);
    // A radix tree is at most 63 key bits plus 31 index bits deep; the stack
    // holds one entry a level, so 96 never overflows.
    uint stack[96];
    float stack_d2[96];
    stack[0] = uint(ranges[n - 1]);
    stack_d2[0] = 0.0;
    uint sp = 1;
    while (sp > 0) {
        sp--;
        uint node = stack[sp];
        if (!pca_wave_any(stack_d2[sp] < h.worst)) continue;
        int4 m = meta[node];
        // One scan site a step: five inlined copies of the insert chain
        // measured 9% slower on bicycle.
        uint a = uint(m.x), b = uint(m.y);
        float da = kPcaFar, db = kPcaFar;
        uint first = 0, last = 0;
        if (uint(m.w - m.z) < kPcaLeafSize) {
            first = uint(m.z);
            last = uint(m.w) + 1;
            a = b = 0;
        } else {
            bool la = (a & kPcaLeafFlag) != 0u, lb = (b & kPcaLeafFlag) != 0u;
            if (la) first = a & ~kPcaLeafFlag;
            else first = b & ~kPcaLeafFlag;
            last = (la || lb) ? (lb ? (b & ~kPcaLeafFlag) : first) + 1 : first;
            if (!la) da = pca_box_d2(q, nodes[2 * a], nodes[2 * a + 1]);
            if (!lb) db = pca_box_d2(q, nodes[2 * b], nodes[2 * b + 1]);
        }
        h.scan(q, r, first, last, w0, w1, sorted, k);
        bool need_a = pca_wave_any(da < h.worst);
        bool need_b = pca_wave_any(db < h.worst);
        bool a_near = 2 * pca_wave_count(da <= db) >= pca_wave_count(true);
        uint near_ = a_near ? a : b, far_ = a_near ? b : a;
        float dn = a_near ? da : db, df = a_near ? db : da;
        if (a_near ? need_b : need_a) {
            stack[sp] = far_;
            stack_d2[sp] = df;
            sp++;
        }
        if (a_near ? need_a : need_b) {
            stack[sp] = near_;
            stack_d2[sp] = dn;
            sp++;
        }
    }
}

// One Jacobi rotation zeroing a_pq; r is the third index (Numerical Recipes 11.1).
void pca_jacobi_rot(inout float app, inout float aqq, inout float apq, inout float arp,
                    inout float arq, inout float3 vp, inout float3 vq) {
    // The absolute floor keeps 2 a_pq normal: flushed to zero it made 0 / 0.
    if (!(abs(apq) > 1e-30) || abs(apq) <= 1e-12 * (abs(app) + abs(aqq))) return;
    float theta = (aqq - app) / (2.0 * apq);
    float t = (theta >= 0.0 ? 1.0 : -1.0) / (abs(theta) + sqrt(theta * theta + 1.0));
    float c = rsqrt(t * t + 1.0);
    float s = t * c;
    app -= t * apq;
    aqq += t * apq;
    apq = 0.0;
    float rp = arp, rq = arq;
    arp = c * rp - s * rq;
    arq = s * rp + c * rq;
    float3 p0 = vp, q0 = vq;
    vp = c * p0 - s * q0;
    vq = s * p0 + c * q0;
}

// Eigenpairs of [[d.x o.x o.y] [o.x d.y o.z] [o.y o.z d.z]], ascending.
void pca_sym3_eigen(float3 d, float3 o, out float3 ev, out float3 e0, out float3 e1,
                    out float3 e2) {
    float a00 = d.x, a11 = d.y, a22 = d.z, a01 = o.x, a02 = o.y, a12 = o.z;
    float3 v0 = float3(1, 0, 0), v1 = float3(0, 1, 0), v2 = float3(0, 0, 1);
    // Quadratic once the off-diagonal is small: five sweeps reach float rounding.
    for (int sweep = 0; sweep < 5; sweep++) {
        pca_jacobi_rot(a00, a11, a01, a02, a12, v0, v1);
        pca_jacobi_rot(a00, a22, a02, a01, a12, v0, v2);
        pca_jacobi_rot(a11, a22, a12, a01, a02, v1, v2);
    }
    float3 l = float3(a00, a11, a22);
    if (l.x > l.y) { l.xy = l.yx; float3 t = v0; v0 = v1; v1 = t; }
    if (l.y > l.z) { l.yz = l.zy; float3 t = v1; v1 = v2; v2 = t; }
    if (l.x > l.y) { l.xy = l.yx; float3 t = v0; v0 = v1; v1 = t; }
    ev = l;
    e0 = v0;
    e1 = v1;
    e2 = v2;
}

[Differentiable]
float3 pca_axis(float4 quat, no_diff uint axis) {
    float inv = rsqrt(max(dot(quat, quat), 1e-20));
    float w = quat.x * inv, x = quat.y * inv, y = quat.z * inv, z = quat.w * inv;
    if (axis == 0)
        return float3(1.0 - 2.0 * (y * y + z * z), 2.0 * (x * y + z * w), 2.0 * (x * z - y * w));
    if (axis == 1)
        return float3(2.0 * (x * y - z * w), 1.0 - 2.0 * (x * x + z * z), 2.0 * (y * z + x * w));
    return float3(2.0 * (x * z + y * w), 2.0 * (y * z - x * w), 1.0 - 2.0 * (x * x + y * y));
}

[Differentiable]
float pca_axis_align(float4 quat, float3 v, no_diff uint axis) {
    return abs(dot(pca_axis(quat, axis), v));
}

float4 pca_load4(float* p, uint i) {
    return float4(p[4 * i], p[4 * i + 1], p[4 * i + 2], p[4 * i + 3]);
}

float3x3 pca_outer(float3 a, float3 b) {
    return float3x3(a.x * b, a.y * b, a.z * b);
}

[Differentiable]
float pca_opacity(float logit) {
    float e = exp(logit >= 0.0 ? -logit : logit);
    return logit >= 0.0 ? 1.0 / (1.0 + e) : e / (1.0 + e);
}

float3x3 pca_sigma(float4 q, float3 ls) {
    float3 a = pca_axis(q, 0), b = pca_axis(q, 1), c = pca_axis(q, 2);
    float3 s2 = exp(2.0 * ls);
    return s2.x * pca_outer(a, a) + s2.y * pca_outer(b, b) + s2.z * pca_outer(c, c);
}

[Differentiable]
float pca_sigma_contract(float4 q, float3 ls, no_diff float3x3 G) {
    float3 a = pca_axis(q, 0), b = pca_axis(q, 1), c = pca_axis(q, 2);
    float3 s2 = exp(2.0 * ls);
    return s2.x * dot(a, mul(G, a)) + s2.y * dot(b, mul(G, b)) + s2.z * dot(c, mul(G, c));
}

// The orthographic footprint is the peak along the line, not a 3D density integral.
[Differentiable]
float pca_ray_alpha(float3 d, float4 q, float3 ls, float logit, float3 normal) {
    float3 a = pca_axis(q, 0), b = pca_axis(q, 1), c = pca_axis(q, 2);
    float3 inv_s = exp(-ls);
    float3 u = float3(dot(a, d), dot(b, d), dot(c, d)) * inv_s;
    float3 v = float3(dot(a, normal), dot(b, normal), dot(c, normal)) * inv_s;
    float t = dot(u, v) / max(dot(v, v), 1e-30);
    float3 residual = u - float3(t) * v;
    return min(pca_opacity(logit) * exp(-0.5 * dot(residual, residual)), 0.999);
}

void pca_add3(float* dst, uint id, float3 v) {
    SS_PCA_REG_ATOMIC_ADD(dst + 3 * id, v.x);
    SS_PCA_REG_ATOMIC_ADD(dst + 3 * id + 1, v.y);
    SS_PCA_REG_ATOMIC_ADD(dst + 3 * id + 2, v.z);
}

void pca_add4(float* dst, uint id, float4 v) {
    for (uint c = 0; c < 4; c++) SS_PCA_REG_ATOMIC_ADD(dst + 4 * id + c, v[c]);
}

// Probability smoothing is relative to trace, preserving similarity invariance.
float pca_erank_loss(float3 ev, out float3 grad) {
    float3 x = max(ev, float3(0.0));
    float trace = x.x + x.y + x.z;
    grad = float3(0.0);
    if (!(trace > 1e-30)) return 0.0;
    float3 p = (x / trace + float3(1e-8)) / (1.0 + 3e-8);
    float3 lp = log(p);
    float rank = exp(-dot(p, lp));
    float excess = max(rank - 2.0, 0.0);
    float3 h = -(lp + float3(1.0)) / (1.0 + 3e-8);
    grad = (2.0 * excess * rank / trace) * (h - float3(dot(h, x / trace)));
    grad *= float3(ev.x > 0.0 ? 1.0 : 0.0, ev.y > 0.0 ? 1.0 : 0.0, ev.z > 0.0 ? 1.0 : 0.0);
    return excess * excess;
}

// A neighbourhood's eigenpairs and G, the loss's adjoint dL/dC.
struct PcaSolved {
    bool ok, directional;
    float3 ev, e0, e1, e2, v_normal;
    float inv_t, align_loss;
    float3x3 G;
};

// C is (xx, yy, zz), (xy, xz, yz). Adds the owner's normal and surfel gradients.
PcaSolved pca_reg_solve<let SA : bool>(float3 cd, float3 co, uint self, float* quats, float* scales,
                                       float* v_quats, float* v_scales, uint terms, float w_plane,
                                       float w_tangent, float w_normal, float w_surfel,
                                       float w_erank, inout float2 extra) {
    PcaSolved s;
    s.ok = false;
    s.directional = false;
    s.v_normal = float3(0.0);
    s.align_loss = 0.0;
    pca_sym3_eigen(cd, co, s.ev, s.e0, s.e1, s.e2);
    if (!all(isfinite(s.ev)) || !all(isfinite(s.e0))) return s;
    s.ok = true;
    float3 ev = s.ev;
    float trace = max(ev.x + ev.y + ev.z, 0.0);
    s.inv_t = SA ? (trace > 1e-30 ? 1.0 / trace : 0.0) : 1.0;
    float3 ge = s.inv_t * float3(w_plane, -w_tangent, w_tangent);
    if (ev.y == ev.z) ge.yz = float2(0.0);
    if ((terms & 4u) != 0u) {
        float3 rank_grad;
        extra.x = pca_erank_loss(ev, rank_grad);
        ge += w_erank * rank_grad;
    }
    s.G = ge.x * pca_outer(s.e0, s.e0) + ge.y * pca_outer(s.e1, s.e1) + ge.z * pca_outer(s.e2, s.e2);
    s.directional = ev.y - ev.x > 1e-6 * max(trace, 1e-30);
    if ((terms & 1u) != 0u || (terms & 2u) != 0u) {
        float3 ls = pca_load3(scales, self);
        uint axis = ls.x <= ls.y ? (ls.x <= ls.z ? 0u : 2u) : (ls.y <= ls.z ? 1u : 2u);
        if ((terms & 1u) != 0u && s.directional) {
            float4 quat = pca_load4(quats, self);
            s.align_loss = 1.0 - pca_axis_align(quat, s.e0, axis);
            var dq = diffPair(quat);
            var dn = diffPair(s.e0);
            bwd_diff(pca_axis_align)(dq, dn, axis, -w_normal);
            pca_add4(v_quats, self, dq.d);
            s.v_normal += dn.d;
        }
        if ((terms & 2u) != 0u) {
            float smin = ls[axis];
            if (SA) smin -= max(ls.x, max(ls.y, ls.z));
            SS_PCA_REG_ATOMIC_ADD(v_scales + 3 * self + axis, w_surfel * exp(smin));
        }
    }
    return s;
}

// Eigenvector VJP; the smallest eigenspace is unique only across a resolved gap.
void pca_reg_finish(inout PcaSolved s, uint terms) {
    if (!s.directional || (terms & 9u) == 0u) return;
    float3 u = (dot(s.e1, s.v_normal) / (s.ev.x - s.ev.y)) * s.e1 +
               (dot(s.e2, s.v_normal) / (s.ev.x - s.ev.z)) * s.e2;
    s.G += 0.5 * (pca_outer(u, s.e0) + pca_outer(s.e0, u));
}

float3 pca_solved_losses(PcaSolved s) {
    return float3(s.ev.x * s.inv_t, (s.ev.z - s.ev.y) * s.inv_t, s.align_loss);
}

float3 pca_reg_owner<let K : int, let SA : bool>(
    uint r, uint mode, uint n, uint k, uint window, float* means, float* quats,
    float* scales, float* opacities, uint covariance, uint terms, int* perm, float4* sorted, float4* nodes, int4* meta, int* ranges,
    int* nbrs, uint store_list, float* radii, uint gate_means, float* v_means, float* v_quats,
    float* v_scales, float* v_opacities, float w_plane, float w_tangent, float w_normal,
    float w_surfel, float w_erank, float w_transparency, out float2 extra) {
    extra = float2(0.0);
    PcaKnn<K> h;
    h.reset();
    bool by_rank = mode != kPcaModeList;
    uint self = uint(perm[r]);
    float3 q = by_rank ? sorted[r].xyz : pca_load3(means, self);

    // h.id holds ranks, or splat ids in list mode; h.d < kPcaFar marks a member.
    if (mode == kPcaModeTree) {
        pca_knn_tree<K>(r, n, k, sorted, nodes, meta, ranges, h);
    } else if (mode == kPcaModeBest) {
        uint half_ = max(window, k + 1) / 2;
        uint first = r > half_ ? r - half_ : 0;
        h.scan(q, r, first, min(r + half_ + 1, n), 0, 0, sorted, k);
    } else if (mode == kPcaModeWindow) {
        uint first = r > k / 2 ? r - k / 2 : 0;
        if (first + k + 1 > n) first = n > k + 1 ? n - (k + 1) : 0;
        [ForceUnroll]
        for (int j = 0; j < K; j++) {
            uint i = first + uint(j);
            i += i >= r ? 1 : 0;
            if (uint(j) < k && i < n) {
                h.d[j] = 0.0;
                h.id[j] = i;
            }
        }
    } else {
        [ForceUnroll]
        for (int j = 0; j < K; j++) {
            int id = uint(j) < k ? nbrs[r * k + uint(j)] : -1;
            if (id >= 0) {
                h.d[j] = 0.0;
                h.id[j] = uint(id);
            }
        }
    }
    uint cnt = h.count(k);
    if (store_list != 0u) {
        [ForceUnroll]
        for (int j = 0; j < K; j++)
            if (uint(j) < k)
                nbrs[r * k + uint(j)] =
                    h.d[j] < kPcaFar ? (by_rank ? perm[h.id[j]] : int(h.id[j])) : -1;
    }
    if (cnt < 2) return float3(0.0);

    float total = covariance == 0u ? 1.0 : pca_opacity(opacities[self]);
    float3 mean = float3(0.0);
    [ForceUnroll]
    for (int j = 0; j < K; j++) {
        if (uint(j) >= k || !(h.d[j] < kPcaFar)) continue;
        uint id = by_rank ? uint(perm[h.id[j]]) : h.id[j];
        float weight = covariance == 0u ? 1.0 : pca_opacity(opacities[id]);
        mean += weight * ((by_rank ? sorted[h.id[j]].xyz : pca_load3(means, id)) - q);
        total += weight;
    }
    if (!(total > 1e-20)) return float3(0.0);
    mean /= total;
    float3x3 C = float3x3(0.0);
    [ForceUnroll]
    for (int j = 0; j <= K; j++) {
        if (j < K && (uint(j) >= k || !(h.d[j] < kPcaFar))) continue;
        uint id = j == K ? self : (by_rank ? uint(perm[h.id[j]]) : h.id[j]);
        float weight = covariance == 0u ? 1.0 : pca_opacity(opacities[id]);
        float3 point = j == K ? q : (by_rank ? sorted[h.id[j]].xyz : pca_load3(means, id));
        float3 d = point - q - mean;
        float3x3 moment = pca_outer(d, d);
        if (covariance == 2u) moment += pca_sigma(pca_load4(quats, id), pca_load3(scales, id));
        C += (weight / total) * moment;
    }
    PcaSolved st = pca_reg_solve<SA>(float3(C[0][0], C[1][1], C[2][2]),
                                     float3(C[0][1], C[0][2], C[1][2]), self, quats, scales,
                                     v_quats, v_scales, terms, w_plane, w_tangent, w_normal,
                                     w_surfel, w_erank, extra);
    if (!st.ok) return float3(0.0);
    if ((terms & 8u) != 0u && st.directional) {
        float log_t = 0.0;
        [ForceUnroll]
        for (int j = 0; j <= K; j++) {
            if (j < K && (uint(j) >= k || !(h.d[j] < kPcaFar))) continue;
            uint id = j == K ? self : (by_rank ? uint(perm[h.id[j]]) : h.id[j]);
            float a = pca_ray_alpha(pca_load3(means, id) - q, pca_load4(quats, id),
                                    pca_load3(scales, id), opacities[id], st.e0);
            log_t += log(1.0 - a);
        }
        extra.y = exp(log_t);
        float3 query_grad = float3(0.0);
        [ForceUnroll]
        for (int j = 0; j <= K; j++) {
            if (j < K && (uint(j) >= k || !(h.d[j] < kPcaFar))) continue;
            uint id = j == K ? self : (by_rank ? uint(perm[h.id[j]]) : h.id[j]);
            var dd = diffPair(pca_load3(means, id) - q);
            var dq = diffPair(pca_load4(quats, id));
            var ds = diffPair(pca_load3(scales, id));
            var da = diffPair(opacities[id]);
            var dn = diffPair(st.e0);
            float a = pca_ray_alpha(dd.p, dq.p, ds.p, da.p, dn.p);
            bwd_diff(pca_ray_alpha)(dd, dq, ds, da, dn, -w_transparency * extra.y / (1.0 - a));
            if (gate_means == 0u || radii[id] > 0.0) pca_add3(v_means, id, dd.d);
            query_grad -= dd.d;
            pca_add4(v_quats, id, dq.d);
            pca_add3(v_scales, id, ds.d);
            SS_PCA_REG_ATOMIC_ADD(v_opacities + id, da.d);
            st.v_normal += dn.d;
        }
        if (gate_means == 0u || radii[self] > 0.0) pca_add3(v_means, self, query_grad);
    }
    pca_reg_finish(st, terms);
    float3x3 G = st.G;
    if (w_plane != 0.0 || w_tangent != 0.0 || w_normal != 0.0 || w_erank != 0.0 || w_transparency != 0.0) {
        float gc = dot(G[0], C[0]) + dot(G[1], C[1]) + dot(G[2], C[2]);
        [ForceUnroll]
        for (int j = 0; j <= K; j++) {
            if (j < K && (uint(j) >= k || !(h.d[j] < kPcaFar))) continue;
            uint id = j == K ? self : (by_rank ? uint(perm[h.id[j]]) : h.id[j]);
            float weight = covariance == 0u ? 1.0 : pca_opacity(opacities[id]);
            float beta = weight / total;
            float3 point = j == K ? q : (by_rank ? sorted[h.id[j]].xyz : pca_load3(means, id));
            float3 d = point - q - mean;
            if (gate_means == 0u || radii[id] > 0.0) pca_add3(v_means, id, (2.0 * beta) * mul(G, d));
            float moment = dot(d, mul(G, d));
            if (covariance == 2u) {
                var dq = diffPair(pca_load4(quats, id));
                var ds = diffPair(pca_load3(scales, id));
                moment += pca_sigma_contract(dq.p, ds.p, G);
                bwd_diff(pca_sigma_contract)(dq, ds, G, beta);
                pca_add4(v_quats, id, dq.d);
                pca_add3(v_scales, id, ds.d);
            }
            if (covariance != 0u)
                SS_PCA_REG_ATOMIC_ADD(v_opacities + id, beta * (1.0 - weight) * (moment - gc));
        }
    }
    return pca_solved_losses(st);
}

#define SS_PCA_REG_OWNER_ARGS                                                          \
    uint r, uint mode, uint n, uint k, uint window, float* means, float* quats,     \
        float* scales, float* opacities, uint covariance, uint terms, int* perm, float4* sorted, float4* nodes, int4* meta,        \
        int* ranges, int* nbrs, uint store_list, float* radii, uint gate_means,     \
        float* v_means, float* v_quats, float* v_scales, float* v_opacities,                            \
        float w_plane, float w_tangent, float w_normal, float w_surfel, \
        float w_erank, float w_transparency, out float2 extra
#define SS_PCA_REG_OWNER_PASS                                                          \
    r, mode, n, k, window, means, quats, scales, opacities, covariance, terms, perm, sorted, nodes, meta, ranges, \
        nbrs, store_list, radii, gate_means, v_means, v_quats, v_scales, v_opacities, w_plane,   \
        w_tangent, w_normal,                                                        \
        w_surfel, w_erank, w_transparency, extra

// One per capacity, smallest >= k (pca_reg_capacity in kernels/loss/PcaReg.cuh),
// and per scale mode, _sa the scale-agnostic one.
#define SS_PCA_REG_OWNER_EXPORT(NAME, K, SA)                                          \
    [CudaDeviceExport]                                                              \
    float3 NAME(SS_PCA_REG_OWNER_ARGS) { return pca_reg_owner<K, SA>(SS_PCA_REG_OWNER_PASS); }
SS_PCA_REG_OWNER_EXPORT(pca_reg_owner_k8, 8, false)
SS_PCA_REG_OWNER_EXPORT(pca_reg_owner_k16, 16, false)
SS_PCA_REG_OWNER_EXPORT(pca_reg_owner_k32, 32, false)
SS_PCA_REG_OWNER_EXPORT(pca_reg_owner_k8_sa, 8, true)
SS_PCA_REG_OWNER_EXPORT(pca_reg_owner_k16_sa, 16, true)
SS_PCA_REG_OWNER_EXPORT(pca_reg_owner_k32_sa, 32, true)

// ================
// Best-of-window, one workgroup at a time
// ================
// Scatter costs one flush per target instead of 11 atomics per neighbour;
// design and measurements: docs/notes/pca-reg.md. Launch with exactly kPcaGroup threads.
static const uint kPcaGroup = 128;
static const uint kPcaMaxHalf = 64;  // pca_reg_uses_slab in kernels/loss/PcaReg.cuh
static const uint kPcaSlabMax = 256;
// A packed key at or above this is empty, or a non-finite distance.
static const uint kPcaKeyFar = 0x7f800000u;

groupshared float4 g_pca_pos[kPcaSlabMax];  // world xyz, w opacity weight
groupshared float4 g_pca_sd[kPcaSlabMax];   // Sigma xx, yy, zz
groupshared float4 g_pca_so[kPcaSlabMax];   // Sigma xy, xz, yz
groupshared uint g_pca_id[kPcaSlabMax];
groupshared float4 g_pca_hd[kPcaGroup];     // G / total xx, yy, zz; w = -(G : C) / total
groupshared float4 g_pca_ho[kPcaGroup];     // G / total xy, xz, yz
groupshared float4 g_pca_mn[kPcaGroup];     // weighted mean, from the owner's centre
groupshared uint g_pca_who[kPcaSlabMax * 4]; // per target: which owners chose it, a bit each

void pca_slab_fill(uint s, uint rank, uint covariance, int* perm, float4* sorted, float* quats,
                   float* scales, float* opacities) {
    uint id = uint(perm[rank]);
    g_pca_id[s] = id;
    for (uint w = 0; w < 4; w++) g_pca_who[4 * s + w] = 0u;
    g_pca_pos[s] = float4(sorted[rank].xyz, covariance == 0u ? 1.0 : pca_opacity(opacities[id]));
    if (covariance == 2u) {
        float3x3 S = pca_sigma(pca_load4(quats, id), pca_load3(scales, id));
        g_pca_sd[s] = float4(S[0][0], S[1][1], S[2][2], 0.0);
        g_pca_so[s] = float4(S[0][1], S[0][2], S[1][2], 0.0);
    }
}

float3 pca_slab_owner<let K : int, let SA : bool>(
    uint o, uint r, uint lo, uint n, uint k, uint half_, uint covariance, uint terms, int* nbrs,
    uint store_list, float* quats, float* scales, float* v_quats, float* v_scales, float w_plane,
    float w_tangent, float w_normal, float w_surfel, float w_erank, inout float2 extra) {
    uint sr = r - lo;
    float4 own = g_pca_pos[sr];
    float3 q = own.xyz;
    // Distance over slot, one register a neighbour: the ascending list is a
    // min/max chain. The 8 low mantissa bits only break near-ties.
    uint keys[K];
    [ForceUnroll]
    for (int j = 0; j < K; j++) keys[j] = 0xffffffffu;
    uint last = min(r + half_ + 1, n);
    for (uint c = r > half_ ? r - half_ : 0; c < last; c++) {
        if (c == r) continue;
        uint s = c - lo;
        float3 e = g_pca_pos[s].xyz - q;
        uint x = (asuint(dot(e, e)) & 0xffffff00u) | s;
        [ForceUnroll]
        for (int j = 0; j < K; j++) {
            uint t = min(keys[j], x);
            x = max(keys[j], x);
            keys[j] = t;
        }
    }
    uint cnt = 0;
    float total = own.w;
    float3 mean = float3(0.0);
    [ForceUnroll]
    for (int j = 0; j < K; j++) {
        if (uint(j) >= k) continue;
        bool live = keys[j] < kPcaKeyFar;
        uint s = keys[j] & 0xffu;
        if (store_list != 0u) nbrs[r * k + uint(j)] = live ? int(g_pca_id[s]) : -1;
        if (!live) continue;
        cnt++;
        float4 p = g_pca_pos[s];
        total += p.w;
        mean += p.w * (p.xyz - q);
    }
    if (cnt < 2 || !(total > 1e-20)) return float3(0.0);
    mean /= total;
    float3 cd = float3(0.0), co = float3(0.0);
    [ForceUnroll]
    for (int j = 0; j <= K; j++) {
        if (j < K && (uint(j) >= k || keys[j] >= kPcaKeyFar)) continue;
        uint s = j == K ? sr : keys[j] & 0xffu;
        float4 p = g_pca_pos[s];
        float3 d = p.xyz - q - mean;
        float beta = p.w / total;
        cd += beta * d * d;
        co += beta * float3(d.x * d.y, d.x * d.z, d.y * d.z);
        if (covariance == 2u) {
            cd += beta * g_pca_sd[s].xyz;
            co += beta * g_pca_so[s].xyz;
        }
    }
    PcaSolved st = pca_reg_solve<SA>(cd, co, g_pca_id[sr], quats, scales, v_quats, v_scales, terms,
                                     w_plane, w_tangent, w_normal, w_surfel, w_erank, extra);
    if (!st.ok) return float3(0.0);
    pca_reg_finish(st, terms);
    float3 gd = float3(st.G[0][0], st.G[1][1], st.G[2][2]);
    float3 go = float3(st.G[0][1], st.G[0][2], st.G[1][2]);
    g_pca_hd[o] = float4(gd / total, -(dot(gd, cd) + 2.0 * dot(go, co)) / total);
    g_pca_ho[o] = float4(go / total, 0.0);
    g_pca_mn[o] = float4(mean, 0.0);
    uint bit = 1u << (o & 31u), word = o >> 5;
    [ForceUnroll]
    for (int j = 0; j <= K; j++) {
        if (j < K && (uint(j) >= k || keys[j] >= kPcaKeyFar)) continue;
        uint s = j == K ? sr : keys[j] & 0xffu;
        InterlockedOr(g_pca_who[4 * s + word], bit);
    }
    return pca_solved_losses(st);
}

// Sum what the group's owners published for one rank, then one flush.
void pca_slab_target(uint s, uint first_owner_slot, uint covariance, float* quats, float* scales,
                     float* radii, uint gate_means, float* v_means, float* v_quats,
                     float* v_scales, float* v_opacities) {
    float3 g = float3(0.0), hd = float3(0.0), ho = float3(0.0);
    float sum = 0.0;
    bool any_hit = false;
    float4 t = g_pca_pos[s];
    [ForceUnroll]
    for (int w = 0; w < 4; w++) {
        uint m = g_pca_who[4 * s + uint(w)];
        while (m != 0u) {
            uint o = 32u * uint(w) + firstbitlow(m);
            m &= m - 1u;
            float4 h0 = g_pca_hd[o], h1 = g_pca_ho[o];
            float3 d = t.xyz - g_pca_pos[first_owner_slot + o].xyz - g_pca_mn[o].xyz;
            float3 hv = float3(h0.x * d.x + h1.x * d.y + h1.y * d.z,
                               h1.x * d.x + h0.y * d.y + h1.z * d.z,
                               h1.y * d.x + h1.z * d.y + h0.z * d.z);
            g += hv;
            hd += h0.xyz;
            ho += h1.xyz;
            sum += dot(d, hv) + h0.w;
            any_hit = true;
        }
    }
    if (!any_hit) return;
    uint id = g_pca_id[s];
    float a = t.w;
    if (gate_means == 0u || radii[id] > 0.0) pca_add3(v_means, id, (2.0 * a) * g);
    if (covariance == 2u) {
        float3x3 h = float3x3(hd.x, ho.x, ho.y, ho.x, hd.y, ho.z, ho.y, ho.z, hd.z);
        var dq = diffPair(pca_load4(quats, id));
        var ds = diffPair(pca_load3(scales, id));
        bwd_diff(pca_sigma_contract)(dq, ds, h, a);
        pca_add4(v_quats, id, dq.d);
        pca_add3(v_scales, id, ds.d);
        sum += dot(hd, g_pca_sd[s].xyz) + 2.0 * dot(ho, g_pca_so[s].xyz);
    }
    if (covariance != 0u) SS_PCA_REG_ATOMIC_ADD(v_opacities + id, a * (1.0 - a) * sum);
}

float3 pca_reg_slab<let K : int, let SA : bool>(
    uint tid, uint group_base, uint n, uint k, uint window, float* quats, float* scales,
    float* opacities, uint covariance, uint terms, int* perm, float4* sorted, int* nbrs,
    uint store_list, float* radii, uint gate_means, float* v_means, float* v_quats,
    float* v_scales, float* v_opacities, float w_plane, float w_tangent, float w_normal,
    float w_surfel, float w_erank, out float2 extra) {
    extra = float2(0.0);
    uint half_ = max(window, k + 1) / 2;
    uint lo = group_base > half_ ? group_base - half_ : 0;
    uint hi = min(group_base + kPcaGroup + half_, n);
    uint owners_end = min(group_base + kPcaGroup, n);
    [ForceUnroll]
    for (int i = 0; i < 2; i++) {
        uint rank = lo + 2 * tid + uint(i);
        if (rank < hi)
            pca_slab_fill(rank - lo, rank, covariance, perm, sorted, quats, scales, opacities);
    }
    GroupMemoryBarrierWithGroupSync();
    float3 l = float3(0.0);
    uint r = group_base + tid;
    if (r < owners_end)
        l = pca_slab_owner<K, SA>(tid, r, lo, n, k, half_, covariance, terms, nbrs, store_list,
                                  quats, scales, v_quats, v_scales, w_plane, w_tangent, w_normal,
                                  w_surfel, w_erank, extra);
    GroupMemoryBarrierWithGroupSync();
    [ForceUnroll]
    for (int i = 0; i < 2; i++) {
        uint rank = lo + 2 * tid + uint(i);
        if (rank < hi)
            pca_slab_target(rank - lo, group_base - lo, covariance, quats, scales, radii,
                            gate_means, v_means, v_quats, v_scales, v_opacities);
    }
    return l;
}

#define SS_PCA_REG_SLAB_EXPORT(NAME, K, SA)                                            \
    [CudaDeviceExport]                                                                 \
    float3 NAME(uint tid, uint group_base, uint n, uint k, uint window, float* quats,  \
                float* scales, float* opacities, uint covariance, uint terms, int* perm, \
                float4* sorted, int* nbrs, uint store_list, float* radii, uint gate_means, \
                float* v_means, float* v_quats, float* v_scales, float* v_opacities,   \
                float w_plane, float w_tangent, float w_normal, float w_surfel,        \
                float w_erank, out float2 extra) {                                     \
        return pca_reg_slab<K, SA>(tid, group_base, n, k, window, quats, scales, opacities, \
                                   covariance, terms, perm, sorted, nbrs, store_list, radii, \
                                   gate_means, v_means, v_quats, v_scales, v_opacities, w_plane, \
                                   w_tangent, w_normal, w_surfel, w_erank, extra);     \
    }
SS_PCA_REG_SLAB_EXPORT(pca_reg_slab_k8, 8, false)
SS_PCA_REG_SLAB_EXPORT(pca_reg_slab_k16, 16, false)
SS_PCA_REG_SLAB_EXPORT(pca_reg_slab_k32, 32, false)
SS_PCA_REG_SLAB_EXPORT(pca_reg_slab_k8_sa, 8, true)
SS_PCA_REG_SLAB_EXPORT(pca_reg_slab_k16_sa, 16, true)
SS_PCA_REG_SLAB_EXPORT(pca_reg_slab_k32_sa, 32, true)
