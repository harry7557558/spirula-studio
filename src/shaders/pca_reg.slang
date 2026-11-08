// Local-PCA regularizer (PCAsplat, arXiv 2610.11011): each splat's centre and
// its nearest centres, the eigenspectrum of their covariance, and the
// gradients of L_PCA and L_normal. Device math for both backends; the methods
// that pick the neighbours and what each costs are docs/notes/pca-reg.md.
// Positions are float*, three floats a splat; quats are (w, x, y, z).

// Overridden by the Vulkan entries, whose float atomics are atomic_float.slang's.
#ifndef PCA_REG_ATOMIC_ADD
#define PCA_REG_ATOMIC_ADD(ptr, v) InterlockedAdd((ptr)[0], v)
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

// |<column `axis` of the rotation of quat, v>|.
[Differentiable]
float pca_axis_align(float4 quat, no_diff float3 v, no_diff uint axis) {
    float inv = rsqrt(max(dot(quat, quat), 1e-20));
    float w = quat.x * inv, x = quat.y * inv, y = quat.z * inv, z = quat.w * inv;
    float3 c;
    if (axis == 0)
        c = float3(1.0 - 2.0 * (y * y + z * z), 2.0 * (x * y + z * w), 2.0 * (x * z - y * w));
    else if (axis == 1)
        c = float3(2.0 * (x * y - z * w), 1.0 - 2.0 * (x * x + z * z), 2.0 * (y * z + x * w));
    else
        c = float3(2.0 * (x * z + y * w), 2.0 * (y * z - x * w), 1.0 - 2.0 * (x * x + y * y));
    return abs(dot(c, v));
}

// Rank r's neighbourhood (its splat, perm[r], and k < K others): adds the
// gradients of the terms in docs/notes/pca-reg.md (SA: scale-agnostic) into
// v_* and returns its unweighted (lambda_1, lambda_3 - lambda_2, 1 - |<v_1, n>|).
float3 pca_reg_owner<let K : int, let SA : bool>(
    uint r, uint mode, uint n, uint k, uint window, float* means, float* quats,
    float* scales, int* perm, float4* sorted, float4* nodes, int4* meta, int* ranges,
    int* nbrs, uint store_list, float* radii, uint gate_means, float* v_means, float* v_quats,
    float* v_scales, float w_plane, float w_tangent, float w_normal, float w_surfel) {
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

    // Moments about the query, which is a member: the shift costs no precision.
    float3 s1 = float3(0.0), sd = float3(0.0), so = float3(0.0);
    [ForceUnroll]
    for (int j = 0; j < K; j++) {
        if (uint(j) >= k || !(h.d[j] < kPcaFar)) continue;
        float3 d = (by_rank ? sorted[h.id[j]].xyz : pca_load3(means, h.id[j])) - q;
        s1 += d;
        sd += d * d;
        so += float3(d.x * d.y, d.x * d.z, d.y * d.z);
    }
    float inv_k = 1.0 / float(cnt + 1);
    float3 mean = s1 * inv_k;
    float3 cd = sd * inv_k - mean * mean;
    float3 co = so * inv_k - float3(mean.x * mean.y, mean.x * mean.z, mean.y * mean.z);
    float3 ev, e0, e1, e2;
    pca_sym3_eigen(cd, co, ev, e0, e1, e2);
    if (!all(isfinite(ev)) || !all(isfinite(e0))) return float3(0.0);
    float inv_t = 1.0;
    if (SA) {
        // Held fixed. A spread under the centres' float rounding has no shape.
        float t = ev.x + ev.y + ev.z;
        inv_t = t > 1e-12 * dot(q, q) + 1e-30 ? 1.0 / t : 0.0;
    }

    // d lambda_m / d mu_j = (2/K) <v_m, mu_j - mean> v_m, so each member's
    // gradient is G (mu_j - mean) with G the weighted sum of v_m v_m^T.
    float g = 2.0 * inv_k * inv_t;
    float3x3 G = (g * w_plane) * float3x3(e0.x * e0, e0.y * e0, e0.z * e0) +
                 (g * w_tangent) * (float3x3(e2.x * e2, e2.y * e2, e2.z * e2) -
                                    float3x3(e1.x * e1, e1.y * e1, e1.z * e1));
    float3 v = mul(G, -mean);
    bool scatter = w_plane != 0.0 || w_tangent != 0.0;
    // The scale-agnostic mean step divides by the splat's screen radius: one
    // this step did not project would take a step of ~1/eps.
    if (scatter && (gate_means == 0u || radii[self] > 0.0)) {
        PCA_REG_ATOMIC_ADD(v_means + 3 * self, v.x);
        PCA_REG_ATOMIC_ADD(v_means + 3 * self + 1, v.y);
        PCA_REG_ATOMIC_ADD(v_means + 3 * self + 2, v.z);
    }
    [ForceUnroll]
    for (int j = 0; j < K; j++) {
        if (!scatter || uint(j) >= k || !(h.d[j] < kPcaFar)) continue;
        uint id = by_rank ? uint(perm[h.id[j]]) : h.id[j];
        if (gate_means != 0u && !(radii[id] > 0.0)) continue;
        float3 d = (by_rank ? sorted[h.id[j]].xyz : pca_load3(means, h.id[j])) - q - mean;
        v = mul(G, d);
        PCA_REG_ATOMIC_ADD(v_means + 3 * id, v.x);
        PCA_REG_ATOMIC_ADD(v_means + 3 * id + 1, v.y);
        PCA_REG_ATOMIC_ADD(v_means + 3 * id + 2, v.z);
    }

    float3 ls = pca_load3(scales, self);
    uint axis = ls.x <= ls.y ? (ls.x <= ls.z ? 0u : 2u) : (ls.y <= ls.z ? 1u : 2u);
    float4 quat = float4(quats[4 * self], quats[4 * self + 1], quats[4 * self + 2],
                         quats[4 * self + 3]);
    float align = pca_axis_align(quat, e0, axis);
    // Only the owner's thread writes these rows.
    if (w_normal != 0.0) {
        var dq = diffPair(quat);
        bwd_diff(pca_axis_align)(dq, e0, axis, -w_normal);
        for (uint c = 0; c < 4; c++) v_quats[4 * self + c] += dq.d[c];
    }
    if (w_surfel != 0.0) {
        float smin = axis == 0 ? ls.x : (axis == 1 ? ls.y : ls.z);
        if (SA) smin -= max(ls.x, max(ls.y, ls.z));
        v_scales[3 * self + axis] += w_surfel * exp(smin);
    }
    return float3(ev.x * inv_t, (ev.z - ev.y) * inv_t, 1.0 - align);
}

#define PCA_REG_OWNER_ARGS                                                          \
    uint r, uint mode, uint n, uint k, uint window, float* means, float* quats,     \
        float* scales, int* perm, float4* sorted, float4* nodes, int4* meta,        \
        int* ranges, int* nbrs, uint store_list, float* radii, uint gate_means,     \
        float* v_means, float* v_quats, float* v_scales,                            \
        float w_plane, float w_tangent, float w_normal, float w_surfel
#define PCA_REG_OWNER_PASS                                                          \
    r, mode, n, k, window, means, quats, scales, perm, sorted, nodes, meta, ranges, \
        nbrs, store_list, radii, gate_means, v_means, v_quats, v_scales, w_plane,   \
        w_tangent, w_normal,                                                        \
        w_surfel

// One per capacity, smallest >= k (pca_reg_capacity in kernels/loss/PcaReg.cuh),
// and per scale mode, _sa the scale-agnostic one.
#define PCA_REG_OWNER_EXPORT(NAME, K, SA)                                          \
    [CudaDeviceExport]                                                              \
    float3 NAME(PCA_REG_OWNER_ARGS) { return pca_reg_owner<K, SA>(PCA_REG_OWNER_PASS); }
PCA_REG_OWNER_EXPORT(pca_reg_owner_k8, 8, false)
PCA_REG_OWNER_EXPORT(pca_reg_owner_k16, 16, false)
PCA_REG_OWNER_EXPORT(pca_reg_owner_k32, 32, false)
PCA_REG_OWNER_EXPORT(pca_reg_owner_k8_sa, 8, true)
PCA_REG_OWNER_EXPORT(pca_reg_owner_k16_sa, 16, true)
PCA_REG_OWNER_EXPORT(pca_reg_owner_k32_sa, 32, true)
