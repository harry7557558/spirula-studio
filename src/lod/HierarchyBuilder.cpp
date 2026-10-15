#include "lod/HierarchyBuilder.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <numeric>
#include <vector>

namespace spirula {
// ================
// Helpers
// ================
static float sigmoid(float x) { return 1.0f / (1.0f + std::exp(-x)); }

static float logit(float p) {
    p = std::clamp(p, 1e-6f, 1.0f - 1e-6f);
    return std::log(p / (1.0f - p));
}

// Quaternion (w,x,y,z) → 3x3 rotation stored row-major.
static void quat_to_mat(const float q[4], float R[9]) {
    float w = q[0], x = q[1], y = q[2], z = q[3];
    R[0] = 1 - 2*(y*y + z*z); R[1] = 2*(x*y - w*z);     R[2] = 2*(x*z + w*y);
    R[3] = 2*(x*y + w*z);     R[4] = 1 - 2*(x*x + z*z); R[5] = 2*(y*z - w*x);
    R[6] = 2*(x*z - w*y);     R[7] = 2*(y*z + w*x);     R[8] = 1 - 2*(x*x + y*y);
}

// Covariance from log-scale + quaternion: Sigma = R diag(s^2) R^T.
static void cov_from_params(const float log_scale[3], const float quat[4], float S[6]) {
    float R[9];
    quat_to_mat(quat, R);
    float s0 = std::exp(log_scale[0]), s1 = std::exp(log_scale[1]), s2 = std::exp(log_scale[2]);
    float d[3] = {s0*s0, s1*s1, s2*s2};
    // S stored as xx, xy, xz, yy, yz, zz
    S[0] = R[0]*R[0]*d[0] + R[1]*R[1]*d[1] + R[2]*R[2]*d[2];
    S[1] = R[0]*R[3]*d[0] + R[1]*R[4]*d[1] + R[2]*R[5]*d[2];
    S[2] = R[0]*R[6]*d[0] + R[1]*R[7]*d[1] + R[2]*R[8]*d[2];
    S[3] = R[3]*R[3]*d[0] + R[4]*R[4]*d[1] + R[5]*R[5]*d[2];
    S[4] = R[3]*R[6]*d[0] + R[4]*R[7]*d[1] + R[5]*R[8]*d[2];
    S[5] = R[6]*R[6]*d[0] + R[7]*R[7]*d[1] + R[8]*R[8]*d[2];
}

// 3-sigma extent along each axis from symmetric cov (xx,xy,xz,yy,yz,zz).
static void sigma3_extent(const float S[6], float out[3]) {
    out[0] = 3.0f * std::sqrt(std::max(S[0], 1e-12f));
    out[1] = 3.0f * std::sqrt(std::max(S[3], 1e-12f));
    out[2] = 3.0f * std::sqrt(std::max(S[5], 1e-12f));
}
// ================
// Eigendecomposition (Jacobi)
// ================
static void sym3_eigen(const float M[6], float evals[3], float evecs[9]) {
    // M: xx, xy, xz, yy, yz, zz → full 3x3 A
    float A[3][3] = {
        {M[0], M[1], M[2]},
        {M[1], M[3], M[4]},
        {M[2], M[4], M[5]}
    };
    // evecs = I
    std::memset(evecs, 0, 9 * sizeof(float));
    evecs[0] = evecs[4] = evecs[8] = 1.0f;

    for (int iter = 0; iter < 50; iter++) {
        // find largest off-diagonal
        int p = 0, q = 1;
        float mx = std::abs(A[0][1]);
        if (std::abs(A[0][2]) > mx) { mx = std::abs(A[0][2]); p = 0; q = 2; }
        if (std::abs(A[1][2]) > mx) { mx = std::abs(A[1][2]); p = 1; q = 2; }
        if (mx < 1e-10f) break;

        float theta = 0.5f * std::atan2(2.0f * A[p][q], A[p][p] - A[q][q]);
        float c = std::cos(theta), s = std::sin(theta);

        // Givens rotation on A
        float Ap[3], Aq[3];
        for (int i = 0; i < 3; i++) {
            Ap[i] = c * A[i][p] + s * A[i][q];
            Aq[i] = -s * A[i][p] + c * A[i][q];
        }
        for (int i = 0; i < 3; i++) { A[i][p] = Ap[i]; A[i][q] = Aq[i]; }
        for (int i = 0; i < 3; i++) {
            Ap[i] = c * A[p][i] + s * A[q][i];
            Aq[i] = -s * A[p][i] + c * A[q][i];
        }
        for (int i = 0; i < 3; i++) { A[p][i] = Ap[i]; A[q][i] = Aq[i]; }

        // accumulate eigenvectors
        for (int i = 0; i < 3; i++) {
            float vp = evecs[i * 3 + p], vq = evecs[i * 3 + q];
            evecs[i * 3 + p] = c * vp + s * vq;
            evecs[i * 3 + q] = -s * vp + c * vq;
        }
    }
    evals[0] = A[0][0];
    evals[1] = A[1][1];
    evals[2] = A[2][2];
}

// Extract log-scale and quaternion from a 3x3 symmetric covariance.
static void decompose_cov(const float S[6], float log_scale[3], float quat[4]) {
    float evals[3], evecs[9]; // evecs row-major, each row is an eigenvector column
    sym3_eigen(S, evals, evecs);

    for (int i = 0; i < 3; i++) {
        float v = std::max(evals[i], 1e-12f);
        log_scale[i] = 0.5f * std::log(v); // scale = sqrt(eigenvalue)
    }

    // sym3_eigen stores eigenvectors in columns matching rotation matrix R.
    float R[9];
    std::memcpy(R, evecs, 9 * sizeof(float));

    // ensure proper rotation
    float det = R[0]*(R[4]*R[8]-R[5]*R[7]) - R[1]*(R[3]*R[8]-R[5]*R[6]) + R[2]*(R[3]*R[7]-R[4]*R[6]);
    if (det < 0) {
        for (int i = 0; i < 3; i++) R[i * 3 + 2] = -R[i * 3 + 2];
    }

    // rotation matrix to quaternion
    float tr = R[0] + R[4] + R[8];
    if (tr > 0) {
        float s = 0.5f / std::sqrt(tr + 1.0f);
        quat[0] = 0.25f / s;
        quat[1] = (R[7] - R[5]) * s;
        quat[2] = (R[2] - R[6]) * s;
        quat[3] = (R[3] - R[1]) * s;
    } else if (R[0] > R[4] && R[0] > R[8]) {
        float s = 2.0f * std::sqrt(1.0f + R[0] - R[4] - R[8]);
        quat[0] = (R[7] - R[5]) / s;
        quat[1] = 0.25f * s;
        quat[2] = (R[1] + R[3]) / s;
        quat[3] = (R[2] + R[6]) / s;
    } else if (R[4] > R[8]) {
        float s = 2.0f * std::sqrt(1.0f + R[4] - R[0] - R[8]);
        quat[0] = (R[2] - R[6]) / s;
        quat[1] = (R[1] + R[3]) / s;
        quat[2] = 0.25f * s;
        quat[3] = (R[5] + R[7]) / s;
    } else {
        float s = 2.0f * std::sqrt(1.0f + R[8] - R[0] - R[4]);
        quat[0] = (R[3] - R[1]) / s;
        quat[1] = (R[2] + R[6]) / s;
        quat[2] = (R[5] + R[7]) / s;
        quat[3] = 0.25f * s;
    }
    // normalize
    float n = std::sqrt(quat[0]*quat[0] + quat[1]*quat[1] + quat[2]*quat[2] + quat[3]*quat[3]);
    if (n > 0) for (int i = 0; i < 4; i++) quat[i] /= n;
    if (quat[0] < 0) for (int i = 0; i < 4; i++) quat[i] = -quat[i]; // canonical w > 0
}
// ================
// k-d Tree Build
// ================
struct BuildCtx {
    const SplatCloud& cloud;
    const LodBuildConfig& cfg;
    const LodBuildLog& log;

    // sorted splat indices
    std::vector<int32_t> indices;

    // output
    std::vector<HierarchyNode> nodes;

    int64_t coeffs;
    int sh_degree;
    int sb_lobes;

    int32_t allocate_node() {
        int32_t id = (int32_t)nodes.size();
        nodes.emplace_back();
        return id;
    }
};

// Build k-d tree, returns node index. [begin, end) is range into ctx.indices.
static int32_t build_kdtree(BuildCtx& ctx, int32_t begin, int32_t end, int32_t depth) {
    int32_t count = end - begin;
    int32_t nid = ctx.allocate_node();
    HierarchyNode& node = ctx.nodes[nid];
    node.depth = depth;
    node.splat_offset = begin;
    node.splat_count = count;
    node.total_descendant_splats = count;

    if (count <= ctx.cfg.max_leaf_size) {
        // leaf
        return nid;
    }

    // find bounding box of means in this range
    Aabb3 box;
    for (int32_t i = begin; i < end; i++) {
        int32_t si = ctx.indices[i];
        box.expand(ctx.cloud.means[si*3], ctx.cloud.means[si*3+1], ctx.cloud.means[si*3+2]);
    }

    int axis = box.widest_axis();
    int32_t mid = begin + count / 2;

    // partial sort by the widest axis
    std::nth_element(
        ctx.indices.begin() + begin,
        ctx.indices.begin() + mid,
        ctx.indices.begin() + end,
        [&](int32_t a, int32_t b) {
            return ctx.cloud.means[a*3+axis] < ctx.cloud.means[b*3+axis];
        });

    int32_t left = build_kdtree(ctx, begin, mid, depth + 1);
    // re-fetch after recursive calls may have grown nodes
    int32_t right = build_kdtree(ctx, mid, end, depth + 1);

    ctx.nodes[nid].child_left = left;
    ctx.nodes[nid].child_right = right;
    ctx.nodes[left].parent = nid;
    ctx.nodes[right].parent = nid;
    ctx.nodes[nid].total_descendant_splats =
        ctx.nodes[left].total_descendant_splats + ctx.nodes[right].total_descendant_splats;

    return nid;
}
// ================
// Moment Matching
// ================
// Get mean position for a leaf's Gaussian subset.
static void leaf_mean_pos(const BuildCtx& ctx, const HierarchyNode& node, float out[3]) {
    out[0] = out[1] = out[2] = 0;
    float total_w = 0;
    for (int64_t i = node.splat_offset; i < node.splat_offset + node.splat_count; i++) {
        int32_t si = ctx.indices[i];
        float w = sigmoid(ctx.cloud.opacities[si]);
        out[0] += w * ctx.cloud.means[si*3];
        out[1] += w * ctx.cloud.means[si*3+1];
        out[2] += w * ctx.cloud.means[si*3+2];
        total_w += w;
    }
    if (total_w > 0) { out[0] /= total_w; out[1] /= total_w; out[2] /= total_w; }
}

// Build one summary Gaussian from a set of leaf Gaussians.
static SummaryGaussian summarize_leaf(const BuildCtx& ctx, const HierarchyNode& node) {
    SummaryGaussian sg{};

    float total_w = 0;
    float mean[3] = {};
    float cov[6] = {}; // xx, xy, xz, yy, yz, zz

    // first pass: weighted mean position + combined opacity
    float survive = 1.0f;
    for (int64_t i = node.splat_offset; i < node.splat_offset + node.splat_count; i++) {
        int32_t si = ctx.indices[i];
        float w = sigmoid(ctx.cloud.opacities[si]);
        mean[0] += w * ctx.cloud.means[si*3];
        mean[1] += w * ctx.cloud.means[si*3+1];
        mean[2] += w * ctx.cloud.means[si*3+2];
        total_w += w;
        survive *= (1.0f - w);
    }
    if (total_w > 1e-10f) { for (int k = 0; k < 3; k++) mean[k] /= total_w; }
    sg.mean[0] = mean[0]; sg.mean[1] = mean[1]; sg.mean[2] = mean[2];
    sg.opacity = logit(1.0f - survive);

    // second pass: moment-matched covariance
    for (int64_t i = node.splat_offset; i < node.splat_offset + node.splat_count; i++) {
        int32_t si = ctx.indices[i];
        float w = sigmoid(ctx.cloud.opacities[si]);
        float dx = ctx.cloud.means[si*3]   - mean[0];
        float dy = ctx.cloud.means[si*3+1] - mean[1];
        float dz = ctx.cloud.means[si*3+2] - mean[2];

        // child covariance
        float cS[6];
        cov_from_params(&ctx.cloud.scales[si*3], &ctx.cloud.quats[si*4], cS);

        // add w * (child_cov + outer(d, d))
        cov[0] += w * (cS[0] + dx*dx);
        cov[1] += w * (cS[1] + dx*dy);
        cov[2] += w * (cS[2] + dx*dz);
        cov[3] += w * (cS[3] + dy*dy);
        cov[4] += w * (cS[4] + dy*dz);
        cov[5] += w * (cS[5] + dz*dz);
    }
    if (total_w > 1e-10f) { for (int k = 0; k < 6; k++) cov[k] /= total_w; }

    decompose_cov(cov, sg.scale, sg.quat);

    // DC color: opacity-weighted average
    float dc[3] = {};
    for (int64_t i = node.splat_offset; i < node.splat_offset + node.splat_count; i++) {
        int32_t si = ctx.indices[i];
        float w = sigmoid(ctx.cloud.opacities[si]);
        dc[0] += w * ctx.cloud.features_dc[si*3];
        dc[1] += w * ctx.cloud.features_dc[si*3+1];
        dc[2] += w * ctx.cloud.features_dc[si*3+2];
    }
    if (total_w > 1e-10f) { for (int k = 0; k < 3; k++) dc[k] /= total_w; }
    sg.dc[0] = dc[0]; sg.dc[1] = dc[1]; sg.dc[2] = dc[2];

    return sg;
}

// Summarize an internal node from its two children's summaries.
static SummaryGaussian summarize_children(const SummaryGaussian& L, const SummaryGaussian& R) {
    SummaryGaussian sg{};

    float wL = sigmoid(L.opacity), wR = sigmoid(R.opacity);
    float total_w = wL + wR;

    // weighted mean
    if (total_w > 1e-10f) {
        for (int k = 0; k < 3; k++)
            sg.mean[k] = (wL * L.mean[k] + wR * R.mean[k]) / total_w;
    }

    // opacity: 1 - (1-wL)(1-wR)
    sg.opacity = logit(1.0f - (1.0f - wL) * (1.0f - wR));

    // moment-matched covariance
    float cov[6] = {};
    float SL[6], SR[6];
    cov_from_params(L.scale, L.quat, SL);
    cov_from_params(R.scale, R.quat, SR);

    float dL[3], dR[3];
    for (int k = 0; k < 3; k++) { dL[k] = L.mean[k] - sg.mean[k]; dR[k] = R.mean[k] - sg.mean[k]; }

    // indices into symmetric cov: {0,0}→0, {0,1}→1, {0,2}→2, {1,1}→3, {1,2}→4, {2,2}→5
    auto outer = [](const float d[3], int i, int j) -> float { return d[i] * d[j]; };
    int ci[6][2] = {{0,0},{0,1},{0,2},{1,1},{1,2},{2,2}};
    for (int k = 0; k < 6; k++) {
        int i = ci[k][0], j = ci[k][1];
        cov[k] = (total_w > 1e-10f) ?
            (wL * (SL[k] + outer(dL, i, j)) + wR * (SR[k] + outer(dR, i, j))) / total_w :
            0;
    }
    decompose_cov(cov, sg.scale, sg.quat);

    // DC color
    if (total_w > 1e-10f) {
        for (int k = 0; k < 3; k++)
            sg.dc[k] = (wL * L.dc[k] + wR * R.dc[k]) / total_w;
    }

    return sg;
}
// ================
// Error Radius & Bounds
// ================
static void compute_leaf_error_and_bounds(BuildCtx& ctx, int32_t nid) {
    HierarchyNode& node = ctx.nodes[nid];

    // weighted centroid
    float center[3] = {};
    float total_w = 0;
    for (int64_t i = node.splat_offset; i < node.splat_offset + node.splat_count; i++) {
        int32_t si = ctx.indices[i];
        float w = sigmoid(ctx.cloud.opacities[si]);
        center[0] += w * ctx.cloud.means[si*3];
        center[1] += w * ctx.cloud.means[si*3+1];
        center[2] += w * ctx.cloud.means[si*3+2];
        total_w += w;
    }
    if (total_w > 1e-10f) { for (int k = 0; k < 3; k++) center[k] /= total_w; }

    // error_radius: max distance from center to child means
    float max_r2 = 0;
    Aabb3 box;
    for (int64_t i = node.splat_offset; i < node.splat_offset + node.splat_count; i++) {
        int32_t si = ctx.indices[i];
        float dx = ctx.cloud.means[si*3]   - center[0];
        float dy = ctx.cloud.means[si*3+1] - center[1];
        float dz = ctx.cloud.means[si*3+2] - center[2];
        max_r2 = std::max(max_r2, dx*dx + dy*dy + dz*dz);

        // bounds: position ± 3-sigma
        float S[6], ext[3];
        cov_from_params(&ctx.cloud.scales[si*3], &ctx.cloud.quats[si*4], S);
        sigma3_extent(S, ext);
        float mx = ctx.cloud.means[si*3], my = ctx.cloud.means[si*3+1], mz = ctx.cloud.means[si*3+2];
        box.expand(mx - ext[0], my - ext[1], mz - ext[2]);
        box.expand(mx + ext[0], my + ext[1], mz + ext[2]);
    }
    node.error_radius = std::sqrt(max_r2);
    node.bounds = box;
}

static void compute_inner_error_and_bounds(BuildCtx& ctx, int32_t nid) {
    HierarchyNode& node = ctx.nodes[nid];
    const HierarchyNode& L = ctx.nodes[node.child_left];
    const HierarchyNode& R = ctx.nodes[node.child_right];

    // bounds: union of children
    node.bounds = L.bounds;
    node.bounds.expand(R.bounds);

    // error_radius: bounding sphere of both children's summary means
    if (!node.summary.empty()) {
        float c[3] = {node.summary[0].mean[0], node.summary[0].mean[1], node.summary[0].mean[2]};
        float max_r = 0;

        auto check_child = [&](const HierarchyNode& child) {
            if (child.is_leaf()) {
                for (int64_t i = child.splat_offset; i < child.splat_offset + child.splat_count; i++) {
                    int32_t si = ctx.indices[i];
                    float dx = ctx.cloud.means[si*3]   - c[0];
                    float dy = ctx.cloud.means[si*3+1] - c[1];
                    float dz = ctx.cloud.means[si*3+2] - c[2];
                    max_r = std::max(max_r, std::sqrt(dx*dx + dy*dy + dz*dz));
                }
            } else {
                for (auto& sg : child.summary) {
                    float dx = sg.mean[0] - c[0], dy = sg.mean[1] - c[1], dz = sg.mean[2] - c[2];
                    max_r = std::max(max_r, std::sqrt(dx*dx + dy*dy + dz*dz));
                }
            }
            // also include child's own error radius
            max_r = std::max(max_r, child.error_radius);
        };
        check_child(L);
        check_child(R);
        node.error_radius = max_r;
    }
}
// ================
// Bottom-Up Pass
// ================
static void bottom_up(BuildCtx& ctx, int32_t nid) {
    HierarchyNode& node = ctx.nodes[nid];

    if (node.is_leaf()) {
        node.summary.push_back(summarize_leaf(ctx, node));
        compute_leaf_error_and_bounds(ctx, nid);
        return;
    }

    bottom_up(ctx, node.child_left);
    bottom_up(ctx, node.child_right);

    const auto& Lsummary = ctx.nodes[node.child_left].summary;
    const auto& Rsummary = ctx.nodes[node.child_right].summary;

    // Summarize children into a single consolidated Gaussian without duplication
    SummaryGaussian sg = summarize_children(
        Lsummary.empty() ? SummaryGaussian{} : Lsummary[0],
        Rsummary.empty() ? SummaryGaussian{} : Rsummary[0]);
    node.summary.push_back(sg);

    compute_inner_error_and_bounds(ctx, nid);
}

// ================
// GaussianHierarchy Helpers
// ================

int64_t GaussianHierarchy::coeffs() const {
    ColorBasis b{sh_degree, sb_lobes};
    return b.coeffs();
}

int64_t GaussianHierarchy::total_summary_gaussians() const {
    int64_t n = 0;
    for (auto& nd : nodes) n += (int64_t)nd.summary.size();
    return n;
}

// ================
// Builder Entry Point
// ================

GaussianHierarchy build_gaussian_hierarchy(
    const SplatCloud& cloud,
    const LodBuildConfig& cfg,
    const LodBuildLog& log)
{
    auto emit = [&](const std::string& msg) { if (log) log(msg); };

    emit("building LOD hierarchy for " + std::to_string(cloud.num) + " splats");

    BuildCtx ctx{cloud, cfg, log, {}, {}, cloud.coeffs(), cloud.sh_degree, cloud.sb_lobes};

    // initialize index array
    ctx.indices.resize(cloud.num);
    std::iota(ctx.indices.begin(), ctx.indices.end(), 0);

    // reserve: a binary tree over N leaves has ~2N-1 nodes
    ctx.nodes.reserve(2 * cloud.num);

    emit("building k-d tree...");
    int32_t root = build_kdtree(ctx, 0, (int32_t)cloud.num, 0);

    emit("computing summaries bottom-up (" + std::to_string(ctx.nodes.size()) + " nodes)...");

    // bottom-up pass
    bottom_up(ctx, root);

    // compute max depth
    int32_t max_depth = 0;
    for (auto& nd : ctx.nodes) max_depth = std::max(max_depth, nd.depth);

    // assemble output
    GaussianHierarchy hier;
    hier.root = root;
    hier.max_depth = max_depth;
    hier.num_splats = cloud.num;
    hier.sh_degree = cloud.sh_degree;
    hier.sb_lobes = cloud.sb_lobes;
    hier.nodes = std::move(ctx.nodes);

    // reorder splat data by the k-d tree order
    int64_t N = cloud.num;
    int64_t K = cloud.coeffs();
    hier.means.resize(N * 3);
    hier.quats.resize(N * 4);
    hier.scales.resize(N * 3);
    hier.opacities.resize(N);
    hier.features_dc.resize(N * 3);
    if (K > 0) hier.features_sh.resize(N * K * 3);

    #pragma omp parallel for schedule(static)
    for (int64_t i = 0; i < N; i++) {
        int32_t si = ctx.indices[i];
        std::memcpy(&hier.means[i*3], &cloud.means[si*3], 3 * sizeof(float));
        std::memcpy(&hier.quats[i*4], &cloud.quats[si*4], 4 * sizeof(float));
        std::memcpy(&hier.scales[i*3], &cloud.scales[si*3], 3 * sizeof(float));
        hier.opacities[i] = cloud.opacities[si];
        std::memcpy(&hier.features_dc[i*3], &cloud.features_dc[si*3], 3 * sizeof(float));
        if (K > 0)
            std::memcpy(&hier.features_sh[i*K*3], &cloud.features_sh[si*K*3], K * 3 * sizeof(float));
    }


    emit("hierarchy built: " + std::to_string(hier.nodes.size()) + " nodes, depth "
         + std::to_string(max_depth) + ", " + std::to_string(hier.total_summary_gaussians())
         + " summary Gaussians");

    return hier;
}

}  // namespace spirula
