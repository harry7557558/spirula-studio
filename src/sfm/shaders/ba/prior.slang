// Camera-side priors (sfm/ba/Priors.h): sparse 6x6 blocks over the frame pose
// columns, assembled on the host and added to what the observation kernels
// built -- the packed S and g on the dense path, the preconditioner blocks and
// the matrix-free product on the CG path. Included at the end of ba.slang.

[[vk::binding(47)]] StructuredBuffer<uint> u_prior_rows;   // (num_frames+1) CSR offsets
[[vk::binding(48)]] StructuredBuffer<uint> u_prior_cols;   // col frame per entry
[[vk::binding(49)]] StructuredBuffer<uint> u_prior_erow;   // row frame per entry
[[vk::binding(50)]] StructuredBuffer<Real> u_prior_blk;    // 36 per entry, [6a+b]: y[6 row+a] += blk x[6 col+b]
[[vk::binding(51)]] StructuredBuffer<Real> u_prior_g;      // pose_dim

// S += blocks, lower triangle: every pair is listed both ways, so an entry
// writes only when its row frame is at or below its column frame. u0 = entries
[shader("compute")] [numthreads(256, 1, 1)]
void prior_add_s(uint3 t : SV_DispatchThreadID) {
    uint e = t.x;
    if (e >= u0) return;
    uint r = u_prior_erow[e], c = u_prior_cols[e];
    if (r < c) return;
    for (uint a = 0; a < 6; a++)
        for (uint b = 0; b < 6; b++) {
            if (r == c && b > a) continue;
            uint idx = pidx(6 * r + a, 6 * c + b);
            u_S[idx] = u_S[idx] + u_prior_blk[36 * e + 6 * a + b];
        }
}

// g += prior gradient. u0 = pose_dim
[shader("compute")] [numthreads(256, 1, 1)]
void prior_add_g(uint3 t : SV_DispatchThreadID) {
    uint i = t.x;
    if (i >= u0) return;
    u_g[i] = u_g[i] + u_prior_g[i];
}

// Diagonal blocks into the block-Jacobi preconditioner. Frame f's block is
// block f on either partition (an exclusive problem has one image per frame),
// with the pose rows first. u0 = entries
[shader("compute")] [numthreads(256, 1, 1)]
void prior_add_m(uint3 t : SV_DispatchThreadID) {
    uint e = t.x;
    if (e >= u0) return;
    uint r = u_prior_erow[e];
    if (u_prior_cols[e] != r) return;
    uint cb = kCamBlk * r;
    for (uint a = 0; a < 6; a++)
        for (uint b = 0; b <= a; b++)
            u_cg_M[cb + pidx(a, b)] = u_cg_M[cb + pidx(a, b)] + u_prior_blk[36 * e + 6 * a + b];
}

// Sp += prior blocks * p over the pose rows, one thread per frame row, after
// cg_scatter has finished with those rows. u0 = num_frames
[shader("compute")] [numthreads(256, 1, 1)]
void prior_matvec(uint3 t : SV_DispatchThreadID) {
    if (cgDone()) return;
    uint f = t.x;
    if (f >= u0) return;
    uint e0 = u_prior_rows[f], e1 = u_prior_rows[f + 1];
    if (e0 == e1) return;
    Real acc[6];
    for (uint a = 0; a < 6; a++) acc[a] = Real(0.0);
    for (uint e = e0; e < e1; e++) {
        uint c = 6 * u_prior_cols[e];
        for (uint a = 0; a < 6; a++)
            for (uint b = 0; b < 6; b++)
                acc[a] = acc[a] + u_prior_blk[36 * e + 6 * a + b] * u_cg_p[c + b];
    }
    for (uint a = 0; a < 6; a++)
        u_cg_sp[6 * f + a] = u_cg_sp[6 * f + a] + acc[a];
}
