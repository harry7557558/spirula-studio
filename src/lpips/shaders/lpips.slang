// One LPIPS tap's distance, summed per workgroup over the window a tile owns:
// sum_c w[c] * (a[c] / sqrt(eps + |a|^2) - b[c] / sqrt(eps + |b|^2))^2, which is
// torchmetrics' _normalize_tensor followed by NetLinLayer. The host adds the
// partials in double and takes the spatial mean.

#include "_common.slang"

static const int WG = 256;

// torchmetrics' _normalize_tensor default.
static const float kEps = 1e-8f;

struct DistanceParams {
    float* partial;      // one sum per workgroup
    uint*  ref;          // [h, w, channels]
    uint*  pred;         // same shape
    uint*  weight;       // [channels]
    uint   w, channels;
    uint   x0, y0, ow, oh;
    uint   groups_per_row;
};

groupshared float sSum[WG];

[shader("compute")]
[numthreads(WG, 1, 1)]
void distance(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
              uniform DistanceParams p) {
    uint i = fold_index(gid, tid, p.groups_per_row, uint(WG));
    uint n = p.ow * p.oh;
    float d = 0.0f;
    if (i < n) {
        uint base = ((p.y0 + i / p.ow) * p.w + p.x0 + i % p.ow) * p.channels;
        float na = 0.0f, nb = 0.0f;
        for (uint c = 0; c < p.channels; ++c) {
            float a = load_at(p.ref, base + c, false);
            float b = load_at(p.pred, base + c, false);
            na += a * a;
            nb += b * b;
        }
        float sa = sqrt(kEps + na), sb = sqrt(kEps + nb);
        for (uint c = 0; c < p.channels; ++c) {
            float e = load_at(p.ref, base + c, false) / sa -
                      load_at(p.pred, base + c, false) / sb;
            d += load_at(p.weight, c, false) * (e * e);
        }
    }
    sSum[tid.x] = d;
    GroupMemoryBarrierWithGroupSync();
    for (uint s = uint(WG) / 2u; s > 0u; s >>= 1u) {
        if (tid.x < s) sSum[tid.x] += sSum[tid.x + s];
        GroupMemoryBarrierWithGroupSync();
    }
    // The last folded row is padding: its groups own no pixels.
    uint group = gid.y * p.groups_per_row + gid.x;
    if (tid.x == 0u && group * uint(WG) < n) store_at(p.partial, group, sSum[0]);
}
