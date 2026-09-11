// The bundle adjustment problem: a 6-DOF pose per FRAME, a 6-DOF extrinsic
// per rig member (cam_from_rig, shared by every frame of the rig), and
// intrinsics per camera group; observations sorted by point, per-kernel
// observation lists, and the column layout of the reduced camera system.
// An image is its own frame with no member unless a rig says otherwise.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

#include "sfm/core/Log.h"

// Camera model registry; must match the entry points in sfm/shaders/ba/ba.slang.
// `n_intr` is how many parameters the kernel *reads* for this model (its
// ICameraModel::kNumIntr). How many of them bundle adjustment may *change* is
// a property of the problem, not the model -- see BAProblem::Group::n_intr and
// sfm/core/Camera.h camNumFreeParams (D50).
struct ModelDesc {
    const char* name;
    uint32_t n_intr;
    const char* cost_entry;
    const char* jac_entry;
};
static const ModelDesc kModels[] = {
    {"snavely", 3, "cost_snavely", "jac_snavely"},                       // BAL, log-focal
    {"snavely_f", 3, "cost_snavely_f", "jac_snavely_f"},                 // BAL, direct focal
    {"pinhole_radial", 5, "cost_pinhole_radial", "jac_pinhole_radial"},  // COLMAP RADIAL
    {"opencv", 8, "cost_opencv", "jac_opencv"},                          // COLMAP OPENCV (D29)
    {"simple_pinhole", 3, "cost_simple_pinhole", "jac_simple_pinhole"},  // COLMAP SIMPLE_PINHOLE
    {"pinhole", 4, "cost_pinhole", "jac_pinhole"},                       // COLMAP PINHOLE
    {"opencv_fisheye", 8, "cost_opencv_fisheye", "jac_opencv_fisheye"},  // COLMAP OPENCV_FISHEYE (D29-C)
    {"full_opencv", 12, "cost_full_opencv", "jac_full_opencv"},          // COLMAP FULL_OPENCV (D34)
    {"thin_prism_fisheye", 12, "cost_thin_prism_fisheye", "jac_thin_prism_fisheye"},  // COLMAP THIN_PRISM_FISHEYE (D34)
    {"equirect", 2, "cost_equirect", "jac_equirect"},                    // COLMAP EQUIRECTANGULAR (D49)
};
static const int kNumModels = sizeof(kModels) / sizeof(kModels[0]);
// 6 frame + 6 member extrinsic + up to 12 intrinsics; must match ba.slang. A
// problem without rigs never exceeds kMaxPlainDof and pays for nothing wider.
static const uint32_t kMaxCamDof = 24;
static const uint32_t kMaxPlainDof = 18;
static const uint32_t kNoMember = 0xFFFFFFFFu;

struct BAProblem {
    uint32_t num_images = 0, num_points = 0, num_obs = 0;
    // Pose blocks. Without rigs num_frames == num_images and image_frame is
    // the identity; with them, images must be ordered by frame (finalizeTables
    // checks), which is what lets the host solver own a frame's rows per task.
    uint32_t num_frames = 0;
    std::vector<uint32_t> image_frame;    // per image
    std::vector<uint32_t> image_member;   // per image; kNoMember = no extrinsic
    struct Member {
        uint32_t ext_offset;  // into `exts` (6 entries)
        uint32_t ext_col;     // column base; unused when n_free == 0
        uint32_t n_free;      // 0 (held) or 6 (refined)
    };
    std::vector<Member> members;

    // observations, sorted by (point, image)
    std::vector<uint32_t> obs_image, obs_point;
    std::vector<double> obs_xy;          // 2 per obs
    std::vector<uint32_t> obs_ranges;    // num_points+1

    // camera groups
    struct Group {
        uint32_t intr_offset;  // into flat intrinsics array (kModels[model].n_intr entries)
        uint32_t intr_col;     // column base in the reduced system
        uint32_t n_intr;       // *free* params: columns owned, and the dof beyond the
                               // pose. <= kModels[model].n_intr; the parameters
                               // past it are read by the kernels and held
                               // constant (D50). The kernels still read all
                               // kModels[model].n_intr from intr_offset.
        uint32_t model;
    };
    std::vector<uint32_t> image_group;   // per image
    std::vector<Group> groups;

    // parameters (host copy, double)
    std::vector<double> poses;   // 6 per frame
    std::vector<double> exts;    // 6 per member (cam_from_rig)
    std::vector<double> intr;    // flat
    std::vector<double> points;  // 3 per point

    // per-(model, rig) observation lists (concatenated) for specialized dispatches
    struct ModelRange { uint32_t model, offset, count; bool rig; };
    std::vector<uint32_t> model_obs;
    std::vector<ModelRange> model_ranges;

    // Jc block pool offsets (element index; block is 2 x dof). Jp, Y and the
    // residual are fixed-stride per observation and need no table.
    std::vector<uint32_t> jc_off;
    uint64_t jc_total = 0;

    // pair-aggregated Schur tables: for each unordered image pair {a,b} seen
    // together by at least one point, the list of (obs_a, obs_b) index pairs
    // (one per shared point; a==b entries are the per-obs diagonal terms).
    // Grouped contiguously per pair, split into chunks; chunks with bit 31 of
    // the count set share their pair with other chunks and must use atomics.
    // Built on demand by buildPairTables (the solver skips it on the pure-CG
    // path, where the entry lists would only waste host+device memory).
    std::vector<uint32_t> pair_entries;  // 2 per entry
    std::vector<uint32_t> pair_chunks;   // 2 per chunk: offset, count|flag
    uint32_t num_pair_chunks = 0;
    bool use_pair_schur = false;

    // observations grouped by image (CSR), for the CG path's per-camera
    // kernels; built on demand by buildCamTables. cam_chunks splits each
    // camera's list into fixed-size pieces (one warp each; camera blocks are
    // accumulated with atomics) for occupancy and load balance.
    std::vector<uint32_t> cam_obs_ranges;  // num_images+1
    std::vector<uint32_t> cam_obs;         // num_obs
    std::vector<uint32_t> cam_chunks;      // 3 per chunk: image, start, count
    uint32_t num_cam_chunks = 0;

    // Block-Jacobi preconditioner blocks for the CG path: a partition of the
    // n_dim camera columns, 4 uints per block (col0, len0, col1, len1) -- at
    // most two contiguous ranges, which is all a camera block ever needs (its
    // pose columns and its group's intrinsics columns). Built by
    // buildPrecBlocks; see there for why the partition depends on sharing.
    std::vector<uint32_t> prec_blocks;
    uint32_t num_prec_blocks = 0;
    bool prec_exclusive = true;

    // Column layout: [frame poses | free member extrinsics | free intrinsics].
    uint32_t pose_dim = 0;   // 6 * num_frames
    uint32_t ext_dim = 0;    // 6 * free members
    uint32_t total_intr = 0;  // intr.size(): parameters stored (free ones first)
    uint32_t free_intr = 0;   // of those, the ones with columns
    uint32_t n_dim = 0;      // camera-side system dimension

    bool hasRigs() const { return num_frames != num_images || ext_dim != 0; }
    uint32_t memberFree(uint32_t img) const {
        const uint32_t m = image_member[img];
        return m == kNoMember ? 0 : members[m].n_free;
    }
    // A problem built without rigs: every image its own frame.
    void identityFrames() {
        num_frames = num_images;
        image_frame.resize(num_images);
        for (uint32_t i = 0; i < num_images; i++) image_frame[i] = i;
        image_member.assign(num_images, kNoMember);
        members.clear();
        exts.clear();
        ext_dim = 0;
    }
};

// No column of the reduced system owned by more than one image, which the
// pair-Schur kernel requires. A group with no free parameters owns no column;
// a refined member or a multi-image frame is shared (README.md, "Rigs").
inline bool exclusiveGroups(const BAProblem& P) {
    if (P.hasRigs()) return false;
    std::vector<uint32_t> guse(P.groups.size(), 0);
    for (uint32_t g : P.image_group)
        if (++guse[g] > 1 && P.groups[g].n_intr) return false;
    return true;
}

// Preconditioner partition (README.md "Implicit-Schur PCG"): exclusive = one
// block per image over [pose | intrinsics]; shared = one 6x6 per frame, one per
// member (index num_frames + m), one per group (num_frames + members + g).
inline void buildPrecBlocks(BAProblem& P, bool exclusive) {
    P.prec_exclusive = exclusive;
    P.prec_blocks.clear();
    P.prec_blocks.reserve(4 * (P.num_frames + P.members.size() + P.groups.size()));
    auto push = [&](uint32_t c0, uint32_t l0, uint32_t c1, uint32_t l1) {
        P.prec_blocks.push_back(c0);
        P.prec_blocks.push_back(l0);
        P.prec_blocks.push_back(c1);
        P.prec_blocks.push_back(l1);
    };
    if (exclusive) {
        for (uint32_t i = 0; i < P.num_images; i++) {
            const BAProblem::Group& g = P.groups[P.image_group[i]];
            push(6 * i, 6, g.intr_col, g.n_intr);
        }
    } else {
        for (uint32_t f = 0; f < P.num_frames; f++) push(6 * f, 6, 0, 0);
        for (const BAProblem::Member& m : P.members) push(m.ext_col, m.n_free, 0, 0);
        for (const BAProblem::Group& g : P.groups) push(g.intr_col, g.n_intr, 0, 0);
    }
    P.num_prec_blocks = (uint32_t)(P.prec_blocks.size() / 4);
}

// pair-Schur entry count = sum over points of t(t+1)/2 (cheap; used for VRAM
// estimates before deciding whether to build the tables at all)
inline uint64_t pairEntryCount(const BAProblem& P) {
    uint64_t total = 0;
    for (uint32_t p = 0; p < P.num_points; p++) {
        uint64_t t = P.obs_ranges[p + 1] - P.obs_ranges[p];
        total += t * (t + 1) / 2;
    }
    return total;
}

static const uint64_t kMaxPairEntries = 400ull << 20;  // 3.2 GB of entry data

// Group observations by image (CSR) for the CG path's per-camera kernels.
inline void buildCamTables(BAProblem& P) {
    P.cam_obs_ranges.assign(P.num_images + 1, 0);
    for (uint32_t o = 0; o < P.num_obs; o++) P.cam_obs_ranges[P.obs_image[o] + 1]++;
    for (uint32_t i = 0; i < P.num_images; i++) P.cam_obs_ranges[i + 1] += P.cam_obs_ranges[i];
    P.cam_obs.resize(P.num_obs);
    std::vector<uint32_t> fill(P.cam_obs_ranges.begin(), P.cam_obs_ranges.end() - 1);
    for (uint32_t o = 0; o < P.num_obs; o++) P.cam_obs[fill[P.obs_image[o]]++] = o;

    const uint32_t kChunk = 1024;
    P.cam_chunks.clear();
    for (uint32_t img = 0; img < P.num_images; img++)
        for (uint32_t o = P.cam_obs_ranges[img]; o < P.cam_obs_ranges[img + 1]; o += kChunk) {
            P.cam_chunks.push_back(img);
            P.cam_chunks.push_back(o);
            P.cam_chunks.push_back(std::min(kChunk, P.cam_obs_ranges[img + 1] - o));
        }
    P.num_cam_chunks = (uint32_t)(P.cam_chunks.size() / 3);
}

// The columns one image's observations touch, in Jacobian order:
// [frame 6 | member extrinsic n_free | group intrinsics n_intr].
inline uint32_t imageColumns(const BAProblem& P, uint32_t img, uint32_t* cols) {
    uint32_t k = 0;
    const uint32_t f = P.image_frame[img];
    for (uint32_t i = 0; i < 6; i++) cols[k++] = 6 * f + i;
    const uint32_t m = P.image_member[img];
    if (m != kNoMember)
        for (uint32_t i = 0; i < P.members[m].n_free; i++) cols[k++] = P.members[m].ext_col + i;
    const BAProblem::Group& g = P.groups[P.image_group[img]];
    for (uint32_t i = 0; i < g.n_intr; i++) cols[k++] = g.intr_col + i;
    return k;
}

// Build per-model obs lists and A_cp offsets from obs/image/group tables.
inline void finalizeTables(BAProblem& P) {
    if (P.image_frame.size() != P.num_images) P.identityFrames();
    if (P.image_member.size() != P.num_images) P.image_member.assign(P.num_images, kNoMember);
    for (uint32_t i = 1; i < P.num_images; i++)
        if (P.image_frame[i] < P.image_frame[i - 1])
            throw std::runtime_error("BA images must be ordered by frame");
    // Per-image bucket and dof, so the passes below index an array rather than
    // chase image -> group -> model per observation. Buckets are (model, rig):
    // a rigged observation runs the kernel that composes the member extrinsic.
    std::vector<uint8_t> img_bucket(P.num_images);
    std::vector<uint8_t> img_dof(P.num_images);
    const int nb = 2 * kNumModels;
    for (uint32_t i = 0; i < P.num_images; i++) {
        const BAProblem::Group& g = P.groups[P.image_group[i]];
        if (g.model >= (uint32_t)kNumModels)
            throw std::runtime_error("camera model index outside the registry");
        const bool rig = P.image_member[i] != kNoMember;
        img_bucket[i] = (uint8_t)(g.model + (rig ? kNumModels : 0));
        uint32_t dof = 6 + P.memberFree(i) + g.n_intr;
        if (dof > kMaxCamDof) throw std::runtime_error("camera dof exceeds kMaxCamDof");
        img_dof[i] = (uint8_t)dof;
    }

    // Bucket the observations in one counting pass: indices ascending within
    // each bucket, buckets in registry order, empty ones omitted.
    std::vector<uint32_t> cnt(nb, 0), off(nb, 0);
    for (uint32_t o = 0; o < P.num_obs; o++) cnt[img_bucket[P.obs_image[o]]]++;
    P.model_ranges.clear();
    uint32_t run = 0;
    for (int b = 0; b < nb; b++) {
        off[b] = run;
        if (cnt[b])
            P.model_ranges.push_back({(uint32_t)(b % kNumModels), run, cnt[b], b >= kNumModels});
        run += cnt[b];
    }
    P.model_obs.resize(P.num_obs);
    for (uint32_t o = 0; o < P.num_obs; o++) P.model_obs[off[img_bucket[P.obs_image[o]]]++] = o;

    P.jc_off.resize(P.num_obs);
    uint64_t acc = 0;
    for (uint32_t o = 0; o < P.num_obs; o++) {
        P.jc_off[o] = (uint32_t)acc;
        acc += 2 * (size_t)img_dof[P.obs_image[o]];
    }
    P.jc_total = acc;
    if (acc > 0xFFFFFFFFull) throw std::runtime_error("Jc pool exceeds 32-bit indexing");
    // note: the packed-triangle 32-bit limit (n_dim <~ 65k) applies only to
    // the dense solver and is checked when that path is selected
}

// Build the pair-aggregated Schur tables (see BAProblem). Requires exclusive
// per-image ownership of intrinsics columns (no shared groups); falls back to
// the per-observation atomic kernel otherwise, or when the entry list would
// be unreasonably large (very long tracks).
inline void buildPairTables(BAProblem& P) {
    P.pair_entries.clear();
    P.pair_chunks.clear();
    P.num_pair_chunks = 0;
    P.use_pair_schur = false;
    if (P.num_obs == 0) return;

    // exclusivity: each group referenced by at most one image
    if (!exclusiveGroups(P)) return;

    uint64_t total = pairEntryCount(P);
    if (total > kMaxPairEntries) {
        sfm::slog::diag(sfm::slog::Tag::Map, "[bal] pair-schur disabled (%llu entries)",
                   (unsigned long long)total);
        return;
    }

    // counting sort by pair key; within a point's track images are strictly
    // increasing, so obs i >= j implies image_i >= image_j
    auto key = [&](uint32_t oi, uint32_t oj) {
        uint64_t a = P.obs_image[oi], b = P.obs_image[oj];
        return a * (a + 1) / 2 + b;
    };
    uint64_t nkeys = (uint64_t)P.num_images * (P.num_images + 1) / 2;
    std::vector<uint32_t> cnt(nkeys + 1, 0);
    for (uint32_t p = 0; p < P.num_points; p++)
        for (uint32_t i = P.obs_ranges[p]; i < P.obs_ranges[p + 1]; i++)
            for (uint32_t j = P.obs_ranges[p]; j <= i; j++)
                cnt[key(i, j) + 1]++;
    for (uint64_t k = 0; k < nkeys; k++) cnt[k + 1] += cnt[k];
    std::vector<uint32_t> fill(cnt.begin(), cnt.end() - 1);
    P.pair_entries.resize(2 * total);
    for (uint32_t p = 0; p < P.num_points; p++)
        for (uint32_t i = P.obs_ranges[p]; i < P.obs_ranges[p + 1]; i++)
            for (uint32_t j = P.obs_ranges[p]; j <= i; j++) {
                uint32_t e = fill[key(i, j)]++;
                P.pair_entries[2 * e] = i;
                P.pair_entries[2 * e + 1] = j;
            }

    // chunk pairs; pairs longer than kChunk are split and flagged for atomics
    const uint32_t kChunk = 1024;
    for (uint64_t k = 0; k < nkeys; k++) {
        uint32_t off = cnt[k], n = cnt[k + 1] - cnt[k];
        if (!n) continue;
        uint32_t flag = n > kChunk ? 0x80000000u : 0;
        for (uint32_t o = 0; o < n; o += kChunk) {
            P.pair_chunks.push_back(off + o);
            P.pair_chunks.push_back(std::min(kChunk, n - o) | flag);
        }
    }
    P.num_pair_chunks = (uint32_t)(P.pair_chunks.size() / 2);
    P.use_pair_schur = true;
    sfm::slog::diag(sfm::slog::Tag::Map, "[bal] pair-schur: %llu entries, %u chunks",
               (unsigned long long)total, P.num_pair_chunks);
}


inline BAProblem loadBAL(const std::string& path, int model_id, bool shared_intrinsics) {
    if (model_id != 0 && model_id != 1)
        throw std::runtime_error("BAL loader supports snavely / snavely_f models");
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) throw std::runtime_error("cannot open " + path);
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::string text(len, 0);
    if (fread(text.data(), 1, len, fp) != (size_t)len) throw std::runtime_error("read failed");
    fclose(fp);

    char* s = text.data();
    auto nextInt = [&]() { return (uint32_t)strtoul(s, &s, 10); };
    auto nextDouble = [&]() { return strtod(s, &s); };

    BAProblem P;
    uint32_t nc = nextInt(), np = nextInt(), no = nextInt();
    P.num_images = nc;
    P.num_points = np;
    P.num_obs = no;
    sfm::slog::diag(sfm::slog::Tag::Map, "[bal] %u cameras, %u points, %u observations", nc, np,
                    no);

    std::vector<uint32_t> cam_idx(no), pnt_idx(no);
    std::vector<double> xy(2 * (size_t)no);
    for (uint32_t i = 0; i < no; i++) {
        cam_idx[i] = nextInt();
        pnt_idx[i] = nextInt();
        xy[2 * (size_t)i] = nextDouble();
        xy[2 * (size_t)i + 1] = nextDouble();
    }
    std::vector<double> cam9(9 * (size_t)nc);
    for (auto& v : cam9) v = nextDouble();
    P.points.resize(3 * (size_t)np);
    for (auto& v : P.points) v = nextDouble();

    // sort observations by (point, image)
    std::vector<uint32_t> order(no);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
        return pnt_idx[a] == pnt_idx[b] ? cam_idx[a] < cam_idx[b] : pnt_idx[a] < pnt_idx[b];
    });
    P.obs_image.resize(no);
    P.obs_point.resize(no);
    P.obs_xy.resize(2 * (size_t)no);
    for (uint32_t i = 0; i < no; i++) {
        uint32_t o = order[i];
        P.obs_image[i] = cam_idx[o];
        P.obs_point[i] = pnt_idx[o];
        P.obs_xy[2 * (size_t)i] = xy[2 * (size_t)o];
        P.obs_xy[2 * (size_t)i + 1] = xy[2 * (size_t)o + 1];
    }
    P.obs_ranges.assign(np + 1, 0);
    for (uint32_t i = 0; i < no; i++) P.obs_ranges[P.obs_point[i] + 1]++;
    for (uint32_t i = 0; i < np; i++) P.obs_ranges[i + 1] += P.obs_ranges[i];

    // poses + intrinsics; BAL camera = [angle-axis(3), t(3), f, k1, k2]
    P.poses.resize(6 * (size_t)nc);
    for (uint32_t c = 0; c < nc; c++)
        for (int j = 0; j < 6; j++) P.poses[6 * (size_t)c + j] = cam9[9 * (size_t)c + j];

    const uint32_t ni = kModels[model_id].n_intr;  // 3
    auto camIntr = [&](uint32_t c, int j) {
        double v = cam9[9 * (size_t)c + 6 + j];
        if (model_id == 0 && j == 0) v = std::log(v);  // log-focal parameterization
        return v;
    };
    if (shared_intrinsics) {
        P.groups.resize(1);
        P.image_group.assign(nc, 0);
        P.intr.assign(ni, 0.0);
        for (uint32_t c = 0; c < nc; c++)
            for (uint32_t j = 0; j < ni; j++) P.intr[j] += camIntr(c, j) / nc;
        P.groups[0] = {0, 0, ni, (uint32_t)model_id};
    } else {
        P.groups.resize(nc);
        P.image_group.resize(nc);
        P.intr.resize((size_t)ni * nc);
        for (uint32_t c = 0; c < nc; c++) {
            P.image_group[c] = c;
            for (uint32_t j = 0; j < ni; j++) P.intr[(size_t)ni * c + j] = camIntr(c, j);
            P.groups[c] = {ni * c, 0 /*fixed below*/, ni, (uint32_t)model_id};
        }
    }
    P.identityFrames();
    P.pose_dim = 6 * nc;
    P.total_intr = (uint32_t)P.intr.size();
    P.free_intr = P.total_intr;  // the BAL models refine everything they read
    P.n_dim = P.pose_dim + P.free_intr;
    for (auto& g : P.groups) g.intr_col = P.pose_dim + g.intr_offset;

    finalizeTables(P);
    return P;
}
