// MoGe-3's Sparse3DUNet, read against sparse_unet.py and flex_sparse_blocks.py
// in MoGe, with FlexGEMM's submanifold convolution as one GEMM over each
// voxel's 27 gathered neighbours (nn::sparse_conv).
//
// The voxel bookkeeping is host work over one download per step. Level 0 is
// one voxel per pixel in pixel order, so the head's raw map is the input rows
// and the output is one log-depth delta per pixel, with no scatter either way.

#include "moge/model/Model.h"

#include "moge/Common.h"
#include "nn/Ops.h"
#include "nn/core/Parallel.h"
#include "nn/vk/Stream.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace moge {
namespace {

using nn::Act;
using nn::DType;
using nn::LinearOpts;
using nn::Tensor;

constexpr int kTaps = 27;

// One level of the voxel pyramid, sorted by (i, j, z).
struct Level {
    int64_t H = 0, W = 0;
    std::vector<int32_t> cell;      // [H*W + 1] first voxel of each (i, j)
    std::vector<int32_t> z;         // [M] depth bin
    std::vector<int32_t> nbr;       // [M, 27] neighbour index, -1 when absent
    std::vector<int32_t> parent;    // [M] voxel at the next level; empty at the last
    std::vector<int32_t> child_off; // [M + 1] into the previous level's `order`
    std::vector<int32_t> children;  // previous level's voxels grouped by parent
    int64_t size() const { return (int64_t)z.size(); }
};

// FlexGEMM's tap order: itertools.product over (di, dj, dz), each -1..1.
void build_neighbours(Level& L) {
    const int64_t M = L.size();
    L.nbr.assign((size_t)M * kTaps, -1);
    nn::parallel_for(L.H, [&](int64_t i0, int64_t i1) {
        for (int64_t i = i0; i < i1; ++i)
            for (int64_t j = 0; j < L.W; ++j)
                for (int32_t v = L.cell[(size_t)(i * L.W + j)];
                     v < L.cell[(size_t)(i * L.W + j + 1)]; ++v) {
                    int t = 0;
                    for (int di = -1; di <= 1; ++di)
                        for (int dj = -1; dj <= 1; ++dj) {
                            const int64_t ni = i + di, nj = j + dj;
                            const bool in = ni >= 0 && ni < L.H && nj >= 0 && nj < L.W;
                            const int32_t b = in ? L.cell[(size_t)(ni * L.W + nj)] : 0;
                            const int32_t e = in ? L.cell[(size_t)(ni * L.W + nj + 1)] : 0;
                            for (int dz = -1; dz <= 1; ++dz, ++t) {
                                const int32_t want = L.z[(size_t)v] + dz;
                                for (int32_t u = b; u < e; ++u)
                                    if (L.z[(size_t)u] == want) {
                                        L.nbr[(size_t)v * kTaps + t] = u;
                                        break;
                                    }
                            }
                        }
                }
    }, 8);
}

// SparsePool3d(kernel = stride = f): parent = floor(coord / f), and the mean
// over the children present. z is never negative, so / is floor.
Level pool(Level& fine, int f) {
    Level c;
    c.H = (fine.H + f - 1) / f;
    c.W = (fine.W + f - 1) / f;
    const int64_t cells = c.H * c.W, M = fine.size();

    std::vector<int32_t> pcell((size_t)M);
    std::vector<int32_t> bucket((size_t)cells + 1, 0);
    for (int64_t i = 0; i < fine.H; ++i)
        for (int64_t j = 0; j < fine.W; ++j) {
            const int32_t pc = (int32_t)((i / f) * c.W + j / f);
            for (int32_t v = fine.cell[(size_t)(i * fine.W + j)];
                 v < fine.cell[(size_t)(i * fine.W + j + 1)]; ++v) {
                pcell[(size_t)v] = pc;
                ++bucket[(size_t)pc + 1];
            }
        }
    for (int64_t k = 0; k < cells; ++k) bucket[(size_t)k + 1] += bucket[(size_t)k];
    std::vector<int32_t> order((size_t)M);
    {
        std::vector<int32_t> fill(bucket.begin(), bucket.end() - 1);
        for (int32_t v = 0; v < (int32_t)M; ++v) order[(size_t)fill[(size_t)pcell[(size_t)v]]++] = v;
    }

    // Sort each bucket by parent bin; the buckets then list the children in
    // the parents' own (cell, z) order, which is the CSR segment_mean reads.
    std::vector<int32_t> uniq((size_t)cells, 0);
    nn::parallel_for(cells, [&](int64_t k0, int64_t k1) {
        for (int64_t k = k0; k < k1; ++k) {
            auto b = order.begin() + bucket[(size_t)k], e = order.begin() + bucket[(size_t)k + 1];
            std::sort(b, e, [&](int32_t a, int32_t d) {
                const int32_t za = fine.z[(size_t)a] / f, zd = fine.z[(size_t)d] / f;
                return za != zd ? za < zd : a < d;
            });
            int32_t n = 0, last = -1;
            for (auto it = b; it != e; ++it)
                if (fine.z[(size_t)*it] / f != last) { last = fine.z[(size_t)*it] / f; ++n; }
            uniq[(size_t)k] = n;
        }
    }, 64);

    c.cell.assign((size_t)cells + 1, 0);
    for (int64_t k = 0; k < cells; ++k) c.cell[(size_t)k + 1] = c.cell[(size_t)k] + uniq[(size_t)k];
    const int64_t Mc = c.cell[(size_t)cells];
    c.z.resize((size_t)Mc);
    c.child_off.resize((size_t)Mc + 1);
    c.child_off[(size_t)Mc] = (int32_t)M;
    fine.parent.resize((size_t)M);
    nn::parallel_for(cells, [&](int64_t k0, int64_t k1) {
        for (int64_t k = k0; k < k1; ++k) {
            int32_t p = c.cell[(size_t)k] - 1, last = -1;
            for (int32_t s = bucket[(size_t)k]; s < bucket[(size_t)k + 1]; ++s) {
                const int32_t v = order[(size_t)s], zc = fine.z[(size_t)v] / f;
                if (zc != last) {
                    last = zc;
                    c.z[(size_t)++p] = zc;
                    c.child_off[(size_t)p] = s;
                }
                fine.parent[(size_t)v] = p;
            }
        }
    }, 64);
    c.children = std::move(order);
    return c;
}

Tensor upload_i32(nn::vk::Arena& a, const std::vector<int32_t>& v, int64_t rows,
                  int64_t cols = 1) {
    Tensor t = nn::arena_tensor(a, DType::I32, rows, cols);
    vk::Stream::get().upload(t.ptr, v.data(), v.size() * sizeof(int32_t));
    return t;
}

}  // namespace

void Model::refine(const Features& f, const Tensor& raw, int steps) {
    const Hparams& h = hp();
    NN_CHECK(h.has_refiner, "this checkpoint has no refiner");
    const int64_t Hf = raw.shape[0], Wf = raw.shape[1], M0 = Hf * Wf;
    const int L = h.refiner_levels;
    const int64_t D = h.embed_dim, np = f.gh * f.gw;
    const float bins = h.refiner_depth_resolution;

    std::vector<float> coord((size_t)M0 * 3);
    nn::tensor_to_host(raw, coord.data(), raw.numel());
    std::vector<float> delta((size_t)M0);

    for (int step = 0; step < steps; ++step) {
        const double t0 = nn::now_ms();
        // ---- voxels: one per pixel at a log-depth bin, offset to start at 0 --
        std::vector<Level> lv((size_t)L);
        lv[0].H = Hf;
        lv[0].W = Wf;
        lv[0].z.resize((size_t)M0);
        lv[0].cell.resize((size_t)M0 + 1);
        int32_t zmin = INT32_MAX;
        for (int64_t p = 0; p < M0; ++p) {
            // torch.round in fp32: half to even, which nearbyint is by default.
            const float lz = coord[(size_t)p * 3 + 2];
            const float q = std::isfinite(lz) ? std::nearbyint(lz * bins) : 0.0f;
            lv[0].z[(size_t)p] = (int32_t)std::max(-1e9f, std::min(1e9f, q));
            zmin = std::min(zmin, lv[0].z[(size_t)p]);
            lv[0].cell[(size_t)p] = (int32_t)p;
        }
        lv[0].cell[(size_t)M0] = (int32_t)M0;
        for (int32_t& z : lv[0].z) z -= zmin;
        for (int l = 0; l + 1 < L; ++l) lv[(size_t)l + 1] = pool(lv[(size_t)l], h.refiner_pool[l]);
        for (Level& lev : lv) build_neighbours(lev);
        const Level& top = lv[(size_t)L - 1];
        NN_CHECK(top.H == f.gh && top.W == f.gw,
                 "the refiner's last level is %lldx%lld, the patch grid %lldx%lld",
                 (long long)top.H, (long long)top.W, (long long)f.gh, (long long)f.gw);
        std::vector<int32_t> enc_ids((size_t)top.size());
        for (int64_t i = 0; i < top.H; ++i)
            for (int64_t j = 0; j < top.W; ++j)
                for (int32_t v = top.cell[(size_t)(i * top.W + j)];
                     v < top.cell[(size_t)(i * top.W + j + 1)]; ++v)
                    enc_ids[(size_t)v] = (int32_t)(i * top.W + j);

        const double t_host = nn::now_ms() - t0;

        // ---- the arena, now that the voxels are counted --------------------
        int64_t floats = M0 * 4 + top.size() * (D + 2 + 4 * h.refiner_ch[L - 1]) + np * (D + 2);
        int64_t ints = 0, widest = 0;
        for (int l = 0; l < L; ++l) {
            const int64_t m = lv[(size_t)l].size(), c = h.refiner_ch[l];
            floats += m * c;   // the level's features, kept as the skip
            widest = std::max(widest, 2 * m * c);
            ints += m * (kTaps + 3) + 2;
        }
        refine_arena.reset();
        refine_arena.reserve((uint64_t)(floats + widest + ints) * 4 + (32ull << 20));
        const uint64_t reserved = refine_arena.capacity();
        vk::ArenaScope root(refine_arena);
        nn::vk::Arena& a = refine_arena;

        std::vector<Tensor> nbr((size_t)L), parent((size_t)L), child_off((size_t)L),
            children((size_t)L);
        for (int l = 0; l < L; ++l) {
            const Level& lev = lv[(size_t)l];
            nbr[(size_t)l] = upload_i32(a, lev.nbr, lev.size(), kTaps);
            if (l + 1 < L) parent[(size_t)l] = upload_i32(a, lev.parent, lev.size());
            if (l > 0) {
                child_off[(size_t)l] = upload_i32(a, lev.child_off, lev.size() + 1);
                children[(size_t)l] = upload_i32(a, lev.children, lv[(size_t)l - 1].size());
            }
        }

        // ---- the network ---------------------------------------------------
        auto lin = [&](const Tensor& out, const Tensor& x, const std::string& w,
                       Act act = Act::None, const Tensor& residual = {}) {
            LinearOpts lo;
            lo.bias = weights.get(w + ".bias");
            lo.act = act;
            lo.residual = residual;
            nn::linear(out, x, weights.get(w + ".weight"), lo);
        };
        // A submanifold 3x3x3 convolution; `residual` adds into `out` in place.
        auto conv = [&](const Tensor& out, const Tensor& x, const Tensor& nb,
                        const std::string& w, Act act, bool residual) {
            LinearOpts lo;
            lo.bias = weights.get(w + ".bias");
            lo.act = act;
            if (residual) lo.residual = out;
            nn::sparse_conv(out, x, nb, weights.get(w + ".weight"), lo);
        };
        // SparseResBlock3d: x += conv2(silu(conv1(silu(LN(x))))).
        auto block = [&](const Tensor& x, int l, const std::string& p) {
            vk::ArenaScope scope(a);
            const int64_t M = x.rows(), C = x.cols();
            Tensor t = nn::arena_tensor(a, DType::F32, M, C);
            nn::layer_norm(t, x, weights.get(p + ".norm1.weight"), weights.get(p + ".norm1.bias"),
                           Hparams::kNormEps);
            nn::unary(t, t, Act::Silu);
            Tensor u = nn::arena_tensor(a, DType::F32, M, C);
            conv(u, t, nbr[(size_t)l], p + ".conv1", Act::Silu, false);
            conv(x, u, nbr[(size_t)l], p + ".conv2", Act::None, true);
        };
        auto stage = [&](const Tensor& x, int l, const std::string& name) {
            const int nb = weights.refinerBlocks(name);
            for (int b = 0; b < nb; ++b) block(x, l, "refiner." + name + "." + std::to_string(b));
        };

        std::vector<Tensor> x((size_t)L);
        {
            Tensor in = nn::arena_tensor(a, DType::F32, M0, 3);
            nn::tensor_from_host(in, coord.data(), M0 * 3);
            x[0] = nn::arena_tensor(a, DType::F32, M0, h.refiner_ch[0]);
            lin(x[0], in, "refiner.input_proj");
        }
        for (int l = 0; l < L; ++l) {
            stage(x[(size_t)l], l, "down_stages." + std::to_string(l));
            if (l + 1 == L) break;
            const int64_t m = lv[(size_t)l + 1].size();
            x[(size_t)l + 1] = nn::arena_tensor(a, DType::F32, m, h.refiner_ch[l + 1]);
            vk::ArenaScope scope(a);
            Tensor pooled = nn::arena_tensor(a, DType::F32, m, h.refiner_ch[l]);
            nn::segment_mean(pooled, x[(size_t)l], child_off[(size_t)l + 1],
                             children[(size_t)l + 1]);
            lin(x[(size_t)l + 1], pooled, "refiner.downsample_blocks." + std::to_string(l) + ".linear");
        }

        // The bottleneck fuses the encoder's patch-grid features, uv included,
        // sampled at each voxel's (i, j).
        {
            const Tensor& xb = x[(size_t)L - 1];
            const int64_t m = xb.rows(), C = xb.cols();
            vk::ArenaScope scope(a);
            Tensor table = nn::arena_tensor(a, DType::F32, np, D + 2);
            nn::strided_copy(table, f.map.view(np, D), np, D, D, D + 2);
            nn::strided_copy(table.offsetElems(D), uv[0].view(np, 2), np, 2, 2, D + 2);
            if (step == 0 && dump_enabled())
                dump_tensor("refiner_table", table, {f.gh, f.gw, D + 2});
            Tensor ids = upload_i32(a, enc_ids, m);
            Tensor enc = nn::arena_tensor(a, DType::F32, m, D + 2);
            nn::gather_rows(enc, table, ids);
            Tensor cat = nn::arena_tensor(a, DType::F32, m, 2 * C);
            nn::strided_copy(cat, xb, m, C, C, 2 * C);
            Tensor fused = nn::arena_tensor(a, DType::F32, m, C);
            lin(fused, enc, "refiner.encoder_fuse");
            nn::strided_copy(cat.offsetElems(C), fused, m, C, C, 2 * C);
            lin(fused, cat, "refiner.fuse_proj.0", Act::Silu);
            lin(xb, fused, "refiner.fuse_proj.2");
        }
        stage(x[(size_t)L - 1], L - 1, "bottleneck_stage");

        // NearestUp: project, copy each parent to its children, add the skip.
        for (int i = 0; i + 1 < L; ++i) {
            const int t = L - 2 - i;
            const Tensor& skip = x[(size_t)t];
            {
                vk::ArenaScope scope(a);
                Tensor y = nn::arena_tensor(a, DType::F32, x[(size_t)t + 1].rows(), skip.cols());
                lin(y, x[(size_t)t + 1], "refiner.upsample_blocks." + std::to_string(i) + ".linear");
                Tensor up = nn::arena_tensor(a, DType::F32, skip.rows(), skip.cols());
                nn::gather_rows(up, y, parent[(size_t)t]);
                nn::add(skip, skip, up);
            }
            stage(skip, t, "up_stages." + std::to_string(i));
        }

        Tensor out = nn::arena_tensor(a, DType::F32, M0, 1, 1, 1, 2);
        lin(out, x[0], "refiner.out_proj");
        nn::tensor_to_host(out, delta.data(), M0);
        NN_CHECK(refine_arena.capacity() == reserved,
                 "the refiner arena grew mid-pass; its plan is wrong");
        for (int64_t p = 0; p < M0; ++p) coord[(size_t)p * 3 + 2] += delta[(size_t)p];
        NN_LOG_DEBUG("[moge] refine step %d: %lld voxels at level 0, %lld at the bottleneck, "
                     "%.0f ms host + %.0f ms device\n",
                     step + 1, (long long)M0, (long long)top.size(), t_host,
                     nn::now_ms() - t0 - t_host);
    }
    nn::tensor_from_host(raw, coord.data(), M0 * 3);
}

}  // namespace moge
