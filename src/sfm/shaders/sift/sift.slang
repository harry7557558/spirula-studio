// GPU SIFT feature extraction (src/sfm/README.md, extraction stage).
//
// A hand-written DoG/SIFT pipeline that follows VLFeat's algorithm and COLMAP's
// default parameters (SiftExtractionOptions): first_octave -1 (2x upsample),
// octave_resolution S = 3, sigma0 = 1.6, peak_threshold 0.02/3, edge_threshold
// 10, up to 2 orientations per keypoint, 128-D 4x4x8 descriptor, RootSIFT
// normalization quantized to uint8.  The host driver (sfm/feature/Sift.h)
// owns the pyramid layout and issues one dispatch per (octave, level); this
// module is stateless beyond its buffers.
//
// The whole Gaussian scale space lives in one flat `gauss` buffer and the DoG
// in `dog`; per-level (offset,width,height) come from the `glev`/`dlev` tables
// so a single push constant only carries indices and thresholds.  Everything is
// plain float -- SIFT does not need the Real/Loss matrix the BA kernels do, so
// this compiles to a single `sift` blob.
//
// COMPILE-TIME CONSTANTS BELOW MUST MATCH sfm/feature/Sift.h.

static const int   S            = 3;        // octave_resolution (scales/octave)
static const int   GAUSS_PER_OCT = S + 3;   // 6 Gaussian levels per octave
static const int   DOG_PER_OCT   = S + 2;   // 5 DoG levels per octave
static const float SIGMA0       = 1.6;      // base scale at level 0 of an octave
static const int   FIRST_OCTAVE = -1;       // -1 upsamples the input 2x
static const float PI2          = 6.2831853071795864;

static const int NBP  = 4;   // descriptor spatial bins per axis (4x4)
static const int NBO  = 8;   // descriptor orientation bins
static const int NORI = 36;  // orientation-assignment histogram bins
static const float MAGNIF = 3.0;  // descriptor magnification (bin size = MAGNIF*sigma)

// Raw keypoint stride (floats): [x_oct, y_oct, octave, s_cont, sigma_oct, resp]
static const int KP_STRIDE  = 6;
// Oriented keypoint stride (floats):
//   [x_orig, y_orig, scale_orig, orientation, x_oct, y_oct, sigma_oct, octave]
static const int OKP_STRIDE = 8;

[[vk::push_constant]] cbuffer PC {
    uint p0; uint p1; uint p2; uint p3; uint p4; uint p5; uint p6; uint p7;
};

[[vk::binding(0)]]  StructuredBuffer<float>   img_in;   // original grayscale [0,1]
[[vk::binding(1)]]  RWStructuredBuffer<float> gauss;    // all Gaussian levels
[[vk::binding(2)]]  RWStructuredBuffer<float> dog;      // all DoG levels
[[vk::binding(3)]]  RWStructuredBuffer<float> tmpH;     // separable-blur scratch
[[vk::binding(4)]]  StructuredBuffer<float>   weights;  // concatenated blur kernels
[[vk::binding(5)]]  StructuredBuffer<uint4>   glev;     // (offset,w,h,_) per Gaussian level
[[vk::binding(6)]]  StructuredBuffer<uint4>   dlev;     // (offset,w,h,octave) per DoG level
[[vk::binding(7)]]  RWStructuredBuffer<float> kp;       // raw keypoints
[[vk::binding(8)]]  RWStructuredBuffer<uint>  kpcount;  // [0] = raw keypoint count
[[vk::binding(9)]]  RWStructuredBuffer<float> okp;      // oriented keypoints
[[vk::binding(10)]] RWStructuredBuffer<uint>  okpcount; // [0] = oriented count
[[vk::binding(11)]] RWStructuredBuffer<uint>  desc;     // 32 uint (128 uint8) per kept kp
[[vk::binding(12)]] RWStructuredBuffer<uint>  scalehist;// log2(scale) histogram (top-K)
[[vk::binding(13)]] RWStructuredBuffer<float> fokp;     // selected oriented keypoints (dense)
[[vk::binding(14)]] RWStructuredBuffer<uint>  selcount; // [0] = selected count

int clampi(int v, int lo, int hi) { return max(lo, min(hi, v)); }

// =====================================================================
// Pyramid construction
// =====================================================================

// 2x bilinear upsample of the original image into tmpH, which blur_dog then
// blurs into gauss(0,0).
// p1=inW  p2=inH  p3=outW  p4=outH
[shader("compute")] [numthreads(16, 16, 1)]
void upsample(uint3 t : SV_DispatchThreadID) {
    uint ox = t.x, oy = t.y;
    if (ox >= p3 || oy >= p4) return;
    int inW = int(p1), inH = int(p2);
    // Destination pixel center maps to source coordinate (x-0.5)/2 ... align so
    // even output pixels land on source samples (standard 2x upsample).
    float fx = 0.5 * float(ox) - 0.25;
    float fy = 0.5 * float(oy) - 0.25;
    int x0 = int(floor(fx)), y0 = int(floor(fy));
    float wx = fx - float(x0), wy = fy - float(y0);
    int x0c = clampi(x0, 0, inW - 1), x1c = clampi(x0 + 1, 0, inW - 1);
    int y0c = clampi(y0, 0, inH - 1), y1c = clampi(y0 + 1, 0, inH - 1);
    float a = img_in[y0c * inW + x0c], b = img_in[y0c * inW + x1c];
    float c = img_in[y1c * inW + x0c], d = img_in[y1c * inW + x1c];
    float top = a + (b - a) * wx, bot = c + (d - c) * wx;
    tmpH[oy * p3 + ox] = top + (bot - top) * wy;
}

// One separable Gaussian step and the DoG level it completes, per 32x32 output
// tile staged in shared memory: the same sums in the same order as separate
// h / v / difference passes. Border = clamp (replicate).
static const int BT = 32;
static const int BT_R = 13;  // the largest of SiftExtractor::kBlurRadii
static const int BT_IN = BT + 2 * BT_R;
groupshared float btIn[BT_IN * BT_IN];
groupshared float btMid[BT_IN * BT];

// A compile-time radius keeps the taps in registers and unrolled: 2.1x on a
// 2-CU RADV iGPU and 1.3x on an RTX 5070 against one runtime-radius kernel.
void blurTile<let R : int>(uint3 g, uint3 gt) {
    int W = int(p2), H = int(p3);
    int x0 = int(g.x) * BT, y0 = int(g.y) * BT;
    int tx = int(gt.x), ty = int(gt.y);
    static const int N = BT + 2 * R;
    float w[2 * R + 1];
    [unroll] for (int k = 0; k <= 2 * R; k++) w[k] = weights[p4 + uint(k)];
    for (int ly = ty; ly < N; ly += 8) {
        uint row = uint(clampi(y0 - R + ly, 0, H - 1)) * uint(W);
        for (int lx = tx; lx < N; lx += 32) {
            uint si = row + uint(clampi(x0 - R + lx, 0, W - 1));
            btIn[ly * BT_IN + lx] = p7 != 0u ? tmpH[si] : gauss[p0 + si];
        }
    }
    GroupMemoryBarrierWithGroupSync();
    for (int ly = ty; ly < N; ly += 8) {
        float acc = 0.0;
        [unroll] for (int k = 0; k <= 2 * R; k++)
            acc += w[k] * btIn[ly * BT_IN + tx + k];
        btMid[ly * BT + tx] = acc;
    }
    GroupMemoryBarrierWithGroupSync();
    // Four adjacent output rows per thread share their 2R+4 loads.
    float acc[4] = {0.0, 0.0, 0.0, 0.0};
    int r0 = 4 * ty;
    [unroll] for (int m = 0; m <= 2 * R + 3; m++) {
        float v = btMid[(r0 + m) * BT + tx];
        [unroll] for (int j = 0; j < 4; j++) {
            if (m - j >= 0 && m - j <= 2 * R) acc[j] += w[m - j] * v;
        }
    }
    int x = x0 + tx;
    [unroll] for (int j = 0; j < 4; j++) {
        int y = y0 + r0 + j;
        if (x < W && y < H) {
            uint o = uint(y) * uint(W) + uint(x);
            gauss[p1 + o] = acc[j];
            if (p6 != 0xffffffffu) dog[p6 + o] = acc[j] - btIn[(r0 + j + R) * BT_IN + tx + R];
        }
    }
}

// Source is tmpH when p7 != 0, else gauss[p0]; p6 == ~0u writes no DoG.
// p0=srcOff  p1=dstOff  p2=W  p3=H  p4=weightOff  p5=radius  p6=dogOff  p7=srcIsTmp
[shader("compute")] [numthreads(32, 8, 1)]
void blur_dog(uint3 g : SV_GroupID, uint3 gt : SV_GroupThreadID) {
    switch (p5) {  // SiftExtractor::kBlurRadii
    case 5: blurTile<5>(g, gt); break;
    case 7: blurTile<7>(g, gt); break;
    case 8: blurTile<8>(g, gt); break;
    case 10: blurTile<10>(g, gt); break;
    case 13: blurTile<13>(g, gt); break;
    }
}

// Decimate by 2 (nearest): gauss[srcOff] (srcW x srcH) -> gauss[dstOff].
// p0=srcOff  p1=dstOff  p2=srcW  p3=dstW  p4=dstH
[shader("compute")] [numthreads(16, 16, 1)]
void downsample(uint3 t : SV_DispatchThreadID) {
    uint x = t.x, y = t.y;
    if (x >= p3 || y >= p4) return;
    gauss[p1 + y * p3 + x] = gauss[p0 + (2 * y) * p2 + (2 * x)];
}

// =====================================================================
// Extrema detection + subpixel refinement
// =====================================================================

float dogAt(uint off, uint W, int x, int y) { return dog[off + uint(y) * W + uint(x)]; }

// One thread per pixel of DoG level d in [1,S] of octave `oct`.
// p0=oct  p1=d  p2=dogBaseOff (offset of dog(oct,0))  p3=W  p4=H  p5=kpCap
// p6=asuint(peak_threshold)  p7=asuint(edge_threshold)
[shader("compute")] [numthreads(16, 16, 1)]
void extrema(uint3 t : SV_DispatchThreadID) {
    int W = int(p3), H = int(p4);
    int x = int(t.x), y = int(t.y);
    if (x < 1 || y < 1 || x >= W - 1 || y >= H - 1) return;
    float peakThresh = asfloat(p6);
    float edgeThresh = asfloat(p7);
    uint layer = p3 * p4;                 // pixels per DoG level in this octave
    uint offC = p2 + p1 * layer;          // dog(oct,d)
    uint offL = offC - layer;             // dog(oct,d-1)
    uint offU = offC + layer;             // dog(oct,d+1)

    float v = dogAt(offC, p3, x, y);
    if (abs(v) < 0.8 * peakThresh) return;

    // Strict extremum among the 26 neighbors across three DoG levels.
    bool isMax = true, isMin = true;
    [unroll] for (int dy = -1; dy <= 1; dy++)
        [unroll] for (int dx = -1; dx <= 1; dx++) {
            float n0 = dogAt(offL, p3, x + dx, y + dy);
            float n1 = dogAt(offC, p3, x + dx, y + dy);
            float n2 = dogAt(offU, p3, x + dx, y + dy);
            if (!(dx == 0 && dy == 0)) { if (n1 >= v) isMax = false; if (n1 <= v) isMin = false; }
            if (n0 >= v) isMax = false; if (n0 <= v) isMin = false;
            if (n2 >= v) isMax = false; if (n2 <= v) isMin = false;
        }
    if (!isMax && !isMin) return;

    // Subpixel refinement: up to 5 relocations of a 3D quadratic fit.
    int xi = x, yi = y, si = int(p1);
    float ox = 0, oy = 0, os = 0;
    float Dval = v;
    bool ok = false;
    for (int it = 0; it < 5; it++) {
        uint oC = p2 + uint(si) * layer, oL = oC - layer, oU = oC + layer;
        float c   = dogAt(oC, p3, xi, yi);
        float dx  = 0.5 * (dogAt(oC, p3, xi + 1, yi) - dogAt(oC, p3, xi - 1, yi));
        float dy  = 0.5 * (dogAt(oC, p3, xi, yi + 1) - dogAt(oC, p3, xi, yi - 1));
        float ds  = 0.5 * (dogAt(oU, p3, xi, yi)     - dogAt(oL, p3, xi, yi));
        float dxx = dogAt(oC, p3, xi + 1, yi) + dogAt(oC, p3, xi - 1, yi) - 2 * c;
        float dyy = dogAt(oC, p3, xi, yi + 1) + dogAt(oC, p3, xi, yi - 1) - 2 * c;
        float dss = dogAt(oU, p3, xi, yi)     + dogAt(oL, p3, xi, yi)     - 2 * c;
        float dxy = 0.25 * (dogAt(oC, p3, xi + 1, yi + 1) - dogAt(oC, p3, xi - 1, yi + 1)
                          - dogAt(oC, p3, xi + 1, yi - 1) + dogAt(oC, p3, xi - 1, yi - 1));
        float dxs = 0.25 * (dogAt(oU, p3, xi + 1, yi) - dogAt(oU, p3, xi - 1, yi)
                          - dogAt(oL, p3, xi + 1, yi) + dogAt(oL, p3, xi - 1, yi));
        float dys = 0.25 * (dogAt(oU, p3, xi, yi + 1) - dogAt(oU, p3, xi, yi - 1)
                          - dogAt(oL, p3, xi, yi + 1) + dogAt(oL, p3, xi, yi - 1));
        // Solve H * offset = -grad (3x3), via cofactor inverse.
        float A00 = dxx, A01 = dxy, A02 = dxs;
        float A11 = dyy, A12 = dys, A22 = dss;
        float c00 = A11 * A22 - A12 * A12;
        float c01 = A02 * A12 - A01 * A22;
        float c02 = A01 * A12 - A02 * A11;
        float det = A00 * c00 + A01 * c01 + A02 * c02;
        if (abs(det) < 1e-18) return;
        float idet = 1.0 / det;
        float c11 = A00 * A22 - A02 * A02;
        float c12 = A02 * A01 - A00 * A12;
        float c22 = A00 * A11 - A01 * A01;
        // offset = -Hinv * grad
        ox = -idet * (c00 * dx + c01 * dy + c02 * ds);
        oy = -idet * (c01 * dx + c11 * dy + c12 * ds);
        os = -idet * (c02 * dx + c12 * dy + c22 * ds);
        Dval = c + 0.5 * (dx * ox + dy * oy + ds * os);
        if (abs(ox) < 0.5 && abs(oy) < 0.5 && abs(os) < 0.5) { ok = true; break; }
        xi += int(round(clamp(ox, -1.0, 1.0)));
        yi += int(round(clamp(oy, -1.0, 1.0)));
        si += int(round(clamp(os, -1.0, 1.0)));
        if (xi < 1 || yi < 1 || xi >= W - 1 || yi >= H - 1 || si < 1 || si > S) return;
    }
    if (!ok) return;
    if (abs(Dval) < peakThresh) return;

    // Edge rejection using the 2x2 spatial Hessian at the refined site.
    {
        uint oC = p2 + uint(si) * layer;
        float c   = dogAt(oC, p3, xi, yi);
        float dxx = dogAt(oC, p3, xi + 1, yi) + dogAt(oC, p3, xi - 1, yi) - 2 * c;
        float dyy = dogAt(oC, p3, xi, yi + 1) + dogAt(oC, p3, xi, yi - 1) - 2 * c;
        float dxy = 0.25 * (dogAt(oC, p3, xi + 1, yi + 1) - dogAt(oC, p3, xi - 1, yi + 1)
                          - dogAt(oC, p3, xi + 1, yi - 1) + dogAt(oC, p3, xi - 1, yi - 1));
        float tr = dxx + dyy, det = dxx * dyy - dxy * dxy;
        if (det <= 0) return;
        float r = edgeThresh;
        if (tr * tr * r >= (r + 1.0) * (r + 1.0) * det) return;
    }

    float s_cont = float(si) + os;
    uint slot;
    InterlockedAdd(kpcount[0], 1u, slot);
    if (slot >= p5) return;
    uint b = slot * uint(KP_STRIDE);
    kp[b + 0] = float(xi) + ox;
    kp[b + 1] = float(yi) + oy;
    kp[b + 2] = float(p0);
    kp[b + 3] = s_cont;
    kp[b + 4] = SIGMA0 * exp2(s_cont / float(S));
    kp[b + 5] = abs(Dval);
}

// =====================================================================
// Gradient sampling (shared by orientation + descriptor)
// =====================================================================

// Central-difference gradient of a Gaussian level; returns (mag, angle[0,2pi)).
void gradAt(uint off, int W, int H, int x, int y, out float mag, out float ang) {
    int xa = clampi(x + 1, 0, W - 1), xb = clampi(x - 1, 0, W - 1);
    int ya = clampi(y + 1, 0, H - 1), yb = clampi(y - 1, 0, H - 1);
    float gx = 0.5 * (gauss[off + uint(y) * W + xa] - gauss[off + uint(y) * W + xb]);
    float gy = 0.5 * (gauss[off + uint(ya) * W + x] - gauss[off + uint(yb) * W + x]);
    mag = sqrt(gx * gx + gy * gy);
    float a = atan2(gy, gx);
    if (a < 0) a += PI2;
    ang = a;
}

// Nearest Gaussian level within an octave for a continuous scale s_cont.
uint gaussLevelIndex(uint oct, float s_cont) {
    int s = clampi(int(round(s_cont)), 0, GAUSS_PER_OCT - 1);
    return oct * uint(GAUSS_PER_OCT) + uint(s);
}

// =====================================================================
// Orientation assignment
// =====================================================================

// One thread per raw keypoint of [p3, p0). Emits up to p1 oriented keypoints.
// p0=kpEnd  p1=maxOri  p2=okpCap  p3=kpBegin
[shader("compute")] [numthreads(64, 1, 1)]
void orient(uint3 t : SV_DispatchThreadID) {
    uint ki = t.x + p3;
    if (ki >= p0) return;
    uint b = ki * uint(KP_STRIDE);
    float x_oct = kp[b + 0], y_oct = kp[b + 1];
    uint oct = uint(kp[b + 2]);
    float s_cont = kp[b + 3], sigma_oct = kp[b + 4];

    uint gi = gaussLevelIndex(oct, s_cont);
    uint4 L = glev[gi];
    uint off = L.x; int W = int(L.y), H = int(L.z);

    float sigmaw = 1.5 * sigma_oct;
    int R = int(floor(3.0 * sigmaw));
    int xi = int(round(x_oct)), yi = int(round(y_oct));

    float hist[NORI];
    [unroll] for (int i = 0; i < NORI; i++) hist[i] = 0.0;
    float inv2s2 = 1.0 / (2.0 * sigmaw * sigmaw);
    for (int dy = -R; dy <= R; dy++)
        for (int dx = -R; dx <= R; dx++) {
            int px = xi + dx, py = yi + dy;
            if (px < 0 || py < 0 || px >= W || py >= H) continue;
            float r2 = float(dx * dx + dy * dy);
            if (r2 > float(R * R) + 0.5) continue;
            float mag, ang;
            gradAt(off, W, H, px, py, mag, ang);
            float w = exp(-r2 * inv2s2) * mag;
            int bin = int(floor(ang / PI2 * float(NORI))) % NORI;
            hist[bin] += w;
        }

    // Smooth the histogram 6x with a circular 3-tap box (VLFeat).
    for (int pass = 0; pass < 6; pass++) {
        float prev = hist[NORI - 1];
        float first = hist[0];
        [unroll] for (int i = 0; i < NORI; i++) {
            float next = (i + 1 < NORI) ? hist[i + 1] : first;
            float cur = hist[i];
            hist[i] = (prev + cur + next) / 3.0;
            prev = cur;
        }
    }

    float maxh = 0.0;
    [unroll] for (int i = 0; i < NORI; i++) maxh = max(maxh, hist[i]);
    if (maxh <= 0.0) return;
    float thresh = 0.8 * maxh;

    // Collect peaks (parabolic interpolation), keep the strongest p1.
    // Only the first nbest entries are ever read; zeroed to keep slangc quiet.
    float bestVal[4] = {0.0, 0.0, 0.0, 0.0};
    float bestAng[4] = {0.0, 0.0, 0.0, 0.0};
    int nbest = 0;
    int cap = int(min(p1, 4u));
    for (int i = 0; i < NORI; i++) {
        float h0 = hist[(i + NORI - 1) % NORI], h1 = hist[i], h2 = hist[(i + 1) % NORI];
        if (h1 <= h0 || h1 <= h2 || h1 < thresh) continue;
        float di = 0.5 * (h0 - h2) / (h0 - 2.0 * h1 + h2);
        float ang = PI2 * (float(i) + di) / float(NORI);
        if (ang < 0) ang += PI2; if (ang >= PI2) ang -= PI2;
        // insertion into a tiny top-k by peak height
        int pos = nbest;
        while (pos > 0 && bestVal[pos - 1] < h1) {
            if (pos < cap) { bestVal[pos] = bestVal[pos - 1]; bestAng[pos] = bestAng[pos - 1]; }
            pos--;
        }
        if (pos < cap) {
            bestVal[pos] = h1; bestAng[pos] = ang;
            if (nbest < cap) nbest++;
        }
    }

    float octSize = exp2(float(int(oct) + FIRST_OCTAVE));
    for (int i = 0; i < nbest; i++) {
        uint slot;
        InterlockedAdd(okpcount[0], 1u, slot);
        if (slot >= p2) return;
        uint ob = slot * uint(OKP_STRIDE);
        okp[ob + 0] = x_oct * octSize;
        okp[ob + 1] = y_oct * octSize;
        okp[ob + 2] = sigma_oct * octSize;
        okp[ob + 3] = bestAng[i];
        okp[ob + 4] = x_oct;
        okp[ob + 5] = y_oct;
        okp[ob + 6] = sigma_oct;
        okp[ob + 7] = float(oct);
    }
}

// =====================================================================
// Top-K selection by scale (GPU histogram-select + compaction)
// =====================================================================
//
// COLMAP keeps the largest-scale features when more than max_num_features are
// found. Rather than a full sort (the phase-0 GPU radix sort does not exist
// yet), bin log2(scale) into a histogram, let the host pick a scale threshold
// that keeps ~K, then compact the survivors into a dense `fokp` list so the
// expensive descriptor pass only runs on kept keypoints. The host applies the
// exact K cut afterwards on the (already small) survivor list.

// One thread per oriented keypoint -> log2(scale) histogram.
// p0=okpCount  p1=numBins  p2=asuint(logMin)  p3=asuint(invLogRange)
[shader("compute")] [numthreads(64, 1, 1)]
void scale_hist(uint3 t : SV_DispatchThreadID) {
    uint i = t.x;
    if (i >= p0) return;
    float scale = okp[i * uint(OKP_STRIDE) + 2];
    int b = int(floor((log2(max(scale, 1e-8)) - asfloat(p2)) * asfloat(p3) * float(p1)));
    b = clampi(b, 0, int(p1) - 1);
    uint old;
    InterlockedAdd(scalehist[uint(b)], 1u, old);
}

// Compact oriented keypoints with scale >= threshold into `fokp`.
// p0=okpCount  p1=asuint(threshold)  p2=cap
[shader("compute")] [numthreads(64, 1, 1)]
void select_topk(uint3 t : SV_DispatchThreadID) {
    uint i = t.x;
    if (i >= p0) return;
    uint ib = i * uint(OKP_STRIDE);
    if (okp[ib + 2] < asfloat(p1)) return;
    uint slot;
    InterlockedAdd(selcount[0], 1u, slot);
    if (slot >= p2) return;
    uint ob = slot * uint(OKP_STRIDE);
    [unroll] for (int k = 0; k < OKP_STRIDE; k++) fokp[ob + k] = okp[ib + k];
}

// =====================================================================
// 128-D descriptor
// =====================================================================

// 32 threads per selected keypoint of [p1, p0): lane q < 25 owns the unit square
// [qx-1,qx) x [qy-1,qy) of the 5x5 bin grid, so each sample is read once. 8x one
// thread per keypoint on an RTX 5070, 5.6x on a 2-CU RADV iGPU.
static const int DESC_KP_PER_GROUP = 2;
static const int NSQ = NBP + 1;
// Loop extent, x1 - x0 + 1 <= 2*ext + 3 < 21 while sigma_oct < SIGMA0*2^(3.5/S).
// Compile-time, as per-thread trip counts hang Intel Arc B390 (drivers
// 101.8992 / 101.9033, issue #122).
static const int SQ_BOX = 20;
groupshared float descPart[DESC_KP_PER_GROUP * NSQ * NSQ * 4 * NBO];
groupshared float descSh[DESC_KP_PER_GROUP * 128];
groupshared float descScal[DESC_KP_PER_GROUP * 3];

[shader("compute")] [numthreads(64, 1, 1)]
void descriptor(uint3 t : SV_DispatchThreadID, uint3 gt : SV_GroupThreadID) {
    uint lane = gt.x & 31u, slot = gt.x >> 5;
    uint ki = t.x / 32u + p1;
    bool valid = ki < p0;
    // Corner c of a square is cell (qx-1 + (c&1), qy-1 + (c>>1)).
    float h[4 * NBO];
    [unroll] for (int i = 0; i < 4 * NBO; i++) h[i] = 0.0;
    if (valid && lane < uint(NSQ * NSQ)) {
        uint ob = ki * uint(OKP_STRIDE);
        float theta = fokp[ob + 3];
        float x_oct = fokp[ob + 4], y_oct = fokp[ob + 5], sigma_oct = fokp[ob + 6];
        uint oct = uint(fokp[ob + 7]);
        // sigma_oct = SIGMA0 * 2^(s_cont/S)  ->  s_cont = S*log2(sigma_oct/SIGMA0)
        float s_cont = float(S) * log2(sigma_oct / SIGMA0);
        uint gi = gaussLevelIndex(oct, s_cont);
        uint4 L = glev[gi];
        uint off = L.x; int W = int(L.y), H = int(L.z);

        float binSize = MAGNIF * sigma_oct;
        // Support radius: cover NBP+1 bins, rotated (sqrt2 factor), +0.5 round.
        int R = int(floor(1.41421356 * binSize * float(NBP + 1) * 0.5 + 0.5));
        float ct = cos(theta), st = sin(theta);
        int xi = int(round(x_oct)), yi = int(round(y_oct));
        float sigScale = float(NBP) * 0.5;
        float inv2s2 = 1.0 / (2.0 * sigScale * sigScale);

        int qx = int(lane % uint(NSQ)), qy = int(lane / uint(NSQ));
        float rxc = (float(qx) - 2.0) * binSize, ryc = (float(qy) - 2.0) * binSize;
        float ccx = ct * rxc - st * ryc, ccy = st * rxc + ct * ryc;
        float ext = 0.5 * binSize * (abs(ct) + abs(st)) + 1.0;
        int x0 = max(max(-R, -xi), int(floor(ccx - ext)));
        int x1 = min(min(R, W - 1 - xi), int(ceil(ccx + ext)));
        int y0 = max(max(-R, -yi), int(floor(ccy - ext)));
        int y1 = min(min(R, H - 1 - yi), int(ceil(ccy + ext)));

        for (int iy = 0; iy < SQ_BOX; iy++)
            for (int ix = 0; ix < SQ_BOX; ix++) {
                int dx = x0 + ix, dy = y0 + iy;
                if (dx > x1 || dy > y1) continue;
                // Rotate sample offset into the descriptor frame, in bin units.
                float rx = (ct * float(dx) + st * float(dy)) / binSize;
                float ry = (-st * float(dx) + ct * float(dy)) / binSize;
                // continuous bin coordinates centered so bins are at 0..NBP-1
                float bx = rx + float(NBP) * 0.5 - 0.5;
                float by = ry + float(NBP) * 0.5 - 0.5;
                if (bx <= -1.0 || bx >= float(NBP) || by <= -1.0 || by >= float(NBP)) continue;
                int bx0 = int(floor(bx)), by0 = int(floor(by));
                if (bx0 != qx - 1 || by0 != qy - 1) continue;

                float mag, ang;
                gradAt(off, W, H, xi + dx, yi + dy, mag, ang);
                float w = exp(-(rx * rx + ry * ry) * inv2s2) * mag;
                float ro = ang - theta;  // both in [0, 2pi)
                if (ro < 0) ro += PI2;
                if (ro >= PI2) ro -= PI2;
                float ob_c = ro / PI2 * float(NBO);
                int ob0 = int(floor(ob_c));
                float fx = bx - float(bx0), fy = by - float(by0), fo = ob_c - float(ob0);
                int o0 = ob0 % NBO, o1 = (ob0 + 1) % NBO;
                // trilinear: 2x2 cells, 2 orientation bins
                [unroll] for (int c = 0; c < 4; c++) {
                    float wx = (c & 1) == 0 ? (1.0 - fx) : fx;
                    float wy = (c & 2) == 0 ? (1.0 - fy) : fy;
                    float a = w * wx * wy;
                    float v0 = a * (1.0 - fo), v1 = a * fo;
                    [unroll] for (int k = 0; k < NBO; k++) {
                        if (k == o0) h[c * NBO + k] += v0;
                        if (k == o1) h[c * NBO + k] += v1;
                    }
                }
            }
    }
    uint pb = slot * uint(NSQ * NSQ * 4 * NBO);
    if (lane < uint(NSQ * NSQ))
        [unroll] for (int i = 0; i < 4 * NBO; i++)
            descPart[pb + lane * uint(4 * NBO) + uint(i)] = h[i];
    GroupMemoryBarrierWithGroupSync();

    // Cell (cx, cy) gathers corner j of the squares qx in {cx+1, cx}, qy in {cy+1, cy}.
    uint sb = slot * 128u;
    if (lane < uint(NBP * NBP)) {
        int cx = int(lane) % NBP, cy = int(lane) / NBP;
        [unroll] for (int k = 0; k < NBO; k++) {
            float sum = 0.0;
            [unroll] for (int j = 0; j < 4; j++) {
                int qx = cx + 1 - (j & 1), qy = cy + 1 - (j >> 1);
                sum += descPart[pb + uint((qy * NSQ + qx) * 4 * NBO + j * NBO + k)];
            }
            descSh[sb + lane * uint(NBO) + uint(k)] = sum;
        }
    }
    GroupMemoryBarrierWithGroupSync();

    // Normalize: L2, clip to 0.2*||d||, L2 again (VLFeat), then RootSIFT
    // (L1-normalize + sqrt), then quantize round(512*x) clamped to 255 (COLMAP).
    if (lane == 0u) {
        float norm = 0.0;
        for (int i = 0; i < 128; i++) norm += descSh[sb + i] * descSh[sb + i];
        norm = sqrt(norm) + 1e-12;
        float clip = 0.2 * norm;
        float norm2 = 0.0;
        for (int i = 0; i < 128; i++) { float v = min(descSh[sb + i], clip); norm2 += v * v; }
        norm2 = sqrt(norm2) + 1e-12;
        float l1 = 0.0;
        for (int i = 0; i < 128; i++) l1 += min(descSh[sb + i], clip) / norm2;
        descScal[slot * 3 + 0] = clip;
        descScal[slot * 3 + 1] = norm2;
        descScal[slot * 3 + 2] = l1 + 1e-12;
    }
    GroupMemoryBarrierWithGroupSync();
    if (!valid || lane >= uint(NBP * NBP)) return;
    float clip = descScal[slot * 3 + 0], norm2 = descScal[slot * 3 + 1], l1 = descScal[slot * 3 + 2];
    uint base = ki * 32u + lane * 2u;
    [unroll] for (int j = 0; j < 2; j++) {
        uint packed = 0;
        [unroll] for (int k = 0; k < 4; k++) {
            float v = sqrt(min(descSh[sb + lane * 8u + uint(j * 4 + k)], clip) / norm2 / l1);
            int q = int(round(512.0 * v));
            q = clampi(q, 0, 255);
            packed |= uint(q) << (8 * k);
        }
        desc[base + uint(j)] = packed;
    }
}
