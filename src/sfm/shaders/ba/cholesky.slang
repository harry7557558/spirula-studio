// Dense SPD solver, written from scratch, operating fully in place on the
// packed lower-triangular matrix u_S (row-major, row r starts at r*(r+1)/2)
// and the RHS u_g. No extra matrix storage is allocated.
//
// Blocked right-looking Cholesky with block size kBs:
//   for k in 0..nb-1:  chol_diag(k); chol_panel(k); chol_update(k)
// then the two triangular solves run blockwise on u_g in place.
//
// Push constants: u0 = n (matrix dim), u1 = k (block index),
//                 u2 = item count for the dispatch (tiles or rows),
//                 u4 = first tile of a launch split into ranges.
// Included at the end of ba.slang; shares its bindings and push constants.

static const uint kBs = 32;

// Pivot from trailing diagonal x (NaN included): at least 1e-30; with u3 set, one
// under f0 of the row's original diagonal (in u_y) becomes that diagonal, which
// bounds the factor however indefinite rounding made it (cg.slang's coarse one).
Real pivot(Real x, uint row) {
    if (u3 == 0) return sqrt(x > Real(1e-30) ? x : Real(1e-30));
    Real d = u_y[row];
    return sqrt(x > Real(f0) * d ? x : max(d, Real(1e-30)));
}
static const uint kPitch = 33;           // padded shared row pitch (bank-conflict-free)
static const uint kShV = kSh2 + 32 * 33; // RHS vector slot in the shared pool

// Factor diagonal block k in place. One workgroup of kBs threads.
[shader("compute")] [numthreads(kBs, 1, 1)]
void chol_diag(uint3 t : SV_DispatchThreadID) {
    uint r = t.x;
    uint n = u0;
    uint base = u1 * kBs;
    uint m = min(kBs, n - base);

    if (r < m)
        for (uint c = 0; c <= r; c++)
            g_chol[r * kPitch + c] = u_S[pidx(base + r, base + c)];

    for (uint j = 0; j < m; j++) {
        GroupMemoryBarrierWithGroupSync();
        if (r == j)
            g_chol[j * kPitch + j] = pivot(g_chol[j * kPitch + j], base + j);
        GroupMemoryBarrierWithGroupSync();
        if (r > j && r < m)
            g_chol[r * kPitch + j] = g_chol[r * kPitch + j] / g_chol[j * kPitch + j];
        GroupMemoryBarrierWithGroupSync();
        if (r > j && r < m)
            for (uint c = j + 1; c <= r; c++)
                g_chol[r * kPitch + c] = g_chol[r * kPitch + c]
                    - g_chol[r * kPitch + j] * g_chol[c * kPitch + j];
    }

    GroupMemoryBarrierWithGroupSync();
    if (r < m)
        for (uint c = 0; c <= r; c++)
            u_S[pidx(base + r, base + c)] = g_chol[r * kPitch + c];
}

// Triangular solve of the panel below diagonal block k:
// L_ik = A_ik L_kk^-T. One workgroup (kBs threads = kBs rows) per row tile;
// grid.x = number of row tiles below block k.
[shader("compute")] [numthreads(kBs, 1, 1)]
void chol_panel(uint3 t : SV_DispatchThreadID, uint3 wg : SV_GroupID, uint gi : SV_GroupIndex) {
    uint n = u0;
    uint k = u1;
    uint r = gi;
    uint base = k * kBs;
    uint rbase = (k + 1 + wg.x + u4) * kBs;
    uint gr = rbase + r;
    bool valid = gr < n;

    for (uint c = 0; c <= r; c++)
        g_chol[r * kBs + c] = u_S[pidx(base + r, base + c)];
    for (uint c = 0; c < kBs; c++)
        g_chol[kSh2 + r * kPitch + c] = valid ? u_S[pidx(gr, base + c)] : Real(0.0);
    GroupMemoryBarrierWithGroupSync();

    for (uint j = 0; j < kBs; j++) {
        Real v = g_chol[kSh2 + r * kPitch + j];
        for (uint mm = 0; mm < j; mm++)
            v = v - g_chol[kSh2 + r * kPitch + mm] * g_chol[j * kBs + mm];
        g_chol[kSh2 + r * kPitch + j] = v / g_chol[j * kBs + j];
    }

    if (valid)
        for (uint c = 0; c < kBs; c++)
            u_S[pidx(gr, base + c)] = g_chol[kSh2 + r * kPitch + c];
}

// Trailing update: A_ij -= L_ik L_jk^T for all tile pairs k < j <= i.
// One workgroup (kBs x kBs threads) per tile pair, 1D grid over the
// triangular tile-pair count (u2), decoded to (i, j). The workgroup that owns
// the next diagonal tile (ii == jj == 0) exclusively owns its elements, so it
// additionally factors that tile in place, replacing a standalone chol_diag
// dispatch per step (chol_diag proper only runs for k = 0).
[shader("compute")] [numthreads(kBs, kBs, 1)]
void chol_update(uint3 wg : SV_GroupID, uint3 lt : SV_GroupThreadID) {
    uint tid = wg.x + u4;
    if (tid >= u2) return;
    uint n = u0;
    uint k = u1;

    uint ii = uint((sqrt(8.0f * float(tid) + 1.0f) - 1.0f) * 0.5f);
    while ((ii + 1) * (ii + 2) / 2 <= tid) ii++;
    while (ii * (ii + 1) / 2 > tid) ii--;
    uint jj = tid - ii * (ii + 1) / 2;
    uint bi = (k + 1 + ii) * kBs; // row tile base
    uint bj = (k + 1 + jj) * kBs; // col tile base
    uint bk = k * kBs;

    uint r = lt.y, c = lt.x;
    g_chol[r * kBs + c] = (bi + r < n) ? u_S[pidx(bi + r, bk + c)] : Real(0.0);
    g_chol[kSh2 + r * kPitch + c] = (bj + r < n) ? u_S[pidx(bj + r, bk + c)] : Real(0.0);
    GroupMemoryBarrierWithGroupSync();

    uint gr = bi + r, gc = bj + c;
    Real acc = Real(0.0);
    bool live = gr < n && gc <= gr;
    if (live) {
        for (uint m = 0; m < kBs; m++)
            acc = acc + g_chol[r * kBs + m] * g_chol[kSh2 + c * kPitch + m];
    }

    if (tid != 0) {
        if (live) {
            uint idx = pidx(gr, gc);
            u_S[idx] = u_S[idx] - acc;
        }
        return;
    }

    // diagonal tile of step k+1: apply the update in shared and factor it
    uint m2 = min(kBs, n - bi);
    GroupMemoryBarrierWithGroupSync();
    if (live)
        g_chol[r * kPitch + c] = u_S[pidx(gr, gc)] - acc;
    for (uint j = 0; j < m2; j++) {
        GroupMemoryBarrierWithGroupSync();
        if (r == j && c == j)
            g_chol[j * kPitch + j] = pivot(g_chol[j * kPitch + j], bi + j);
        GroupMemoryBarrierWithGroupSync();
        if (c == j && r > j && r < m2)
            g_chol[r * kPitch + j] = g_chol[r * kPitch + j] / g_chol[j * kPitch + j];
        GroupMemoryBarrierWithGroupSync();
        if (c > j && c <= r && r < m2)
            g_chol[r * kPitch + c] = g_chol[r * kPitch + c]
                - g_chol[r * kPitch + j] * g_chol[c * kPitch + j];
    }
    GroupMemoryBarrierWithGroupSync();
    if (live)
        u_S[pidx(gr, gc)] = g_chol[r * kPitch + c];
}

// ------------- triangular solves, fused one dispatch per block -------------
//
// Forward: y = L^-1 b. Step k reads b (u_g, unmodified for block k during the
// dispatch): every workgroup redundantly solves the 32x32 diagonal system in
// shared memory (no cross-workgroup ordering exists inside a dispatch), then
// workgroup 0 writes y_k to the scratch u_y while all threads update their
// rows of u_g below the block. Backward mirrors it: x_k is solved from u_y,
// written to u_g (not read by anyone in the backward pass), and rows of u_y
// above the block are updated. The final solution ends in u_g as before.
//
// u0 = n, u1 = k, u2 = update row count (0 for the last/first block).

static const uint kTriWg = 256;

[shader("compute")] [numthreads(kTriWg, 1, 1)]
void tri_fwd(uint3 wg : SV_GroupID, uint gi : SV_GroupIndex) {
    uint n = u0;
    uint base = u1 * kBs;
    uint m = min(kBs, n - base);
    uint r = gi;

    if (r < m) {
        for (uint c = 0; c <= r; c++)
            g_chol[r * kPitch + c] = u_S[pidx(base + r, base + c)];
        g_chol[kShV + r] = u_g[base + r];
    }
    for (uint j = 0; j < m; j++) {
        GroupMemoryBarrierWithGroupSync();
        if (r == j)
            g_chol[kShV + j] = g_chol[kShV + j] / g_chol[j * kPitch + j];
        GroupMemoryBarrierWithGroupSync();
        if (r > j && r < m)
            g_chol[kShV + r] = g_chol[kShV + r] - g_chol[r * kPitch + j] * g_chol[kShV + j];
    }
    GroupMemoryBarrierWithGroupSync();
    if (wg.x == 0 && r < m)
        u_y[base + r] = g_chol[kShV + r];

    uint row = wg.x * kTriWg + gi;
    if (row < u2) {
        uint gr = base + kBs + row;
        Real acc = Real(0.0);
        for (uint j = 0; j < kBs; j++)
            acc = acc + u_S[pidx(gr, base + j)] * g_chol[kShV + j];
        u_g[gr] = u_g[gr] - acc;
    }
}

[shader("compute")] [numthreads(kTriWg, 1, 1)]
void tri_bwd(uint3 wg : SV_GroupID, uint gi : SV_GroupIndex) {
    uint n = u0;
    uint base = u1 * kBs;
    uint m = min(kBs, n - base);
    uint r = gi;

    if (r < m) {
        for (uint c = 0; c <= r; c++)
            g_chol[r * kPitch + c] = u_S[pidx(base + r, base + c)];
        g_chol[kShV + r] = u_y[base + r];
    }
    for (int j = int(m) - 1; j >= 0; j--) {
        GroupMemoryBarrierWithGroupSync();
        if (int(r) == j)
            g_chol[kShV + j] = g_chol[kShV + j] / g_chol[j * kPitch + j];
        GroupMemoryBarrierWithGroupSync();
        if (int(r) < j && r < m)
            g_chol[kShV + r] = g_chol[kShV + r] - g_chol[uint(j) * kPitch + r] * g_chol[kShV + j];
    }
    GroupMemoryBarrierWithGroupSync();
    if (wg.x == 0 && r < m)
        u_g[base + r] = g_chol[kShV + r];

    uint gr = wg.x * kTriWg + gi;
    if (gr < u2) {
        Real acc = Real(0.0);
        for (uint j = 0; j < m; j++)
            acc = acc + u_S[pidx(base + j, gr)] * g_chol[kShV + j];
        u_y[gr] = u_y[gr] - acc;
    }
}
