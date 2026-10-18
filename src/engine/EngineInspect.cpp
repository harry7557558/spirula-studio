// The developer view of per-splat pool buffers: what can be read and how
// (kSources), and an expression evaluated over it. docs/notes/splat-inspector.md.

#include "engine/Engine.h"
#include "engine/EngineState.h"

#include "core/ExprProgram.h"
#include "shaders/splat_expr.h"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

enum Bounds : uint8_t {
    kNone,
    kPerCells,    // one bound per 256 cells
    kPerSplats,   // one per 256 splats, over all of their cells
};

// A pool buffer an attribute can be read from. Rows sharing a name are the
// layouts it may be stored in; a run allocates one, and the first live is read.
struct Source {
    const char* name;    // the code, or its prefix when `comps` names components
    const char* comps;   // space-separated; "*" = the L2 norm of the SH bands
    int stride;          // cells per splat; 0 = one per component
    PoolSlot slot;
    uint8_t codec;
    uint8_t bits;        // 0 with SS_EXPR_ADAM_*: engine().optim.sh_optim_bits
    uint8_t bounds;
    uint8_t layout;
};

constexpr const char* kXyz = "x y z";
constexpr const char* kWxyz = "w x y z";
constexpr const char* kRgb = "r g b";
constexpr const char* kSh = "*";

constexpr Source plain(const char* name, const char* comps, PoolSlot slot,
                       uint8_t codec = SS_EXPR_F32, int stride = 0) {
    return {name, comps, stride, slot, codec, 0, kNone, SS_EXPR_AOS};
}
constexpr Source quant(const char* name, const char* comps, PoolSlot slot, uint8_t codec,
                       uint8_t bits, uint8_t bounds, uint8_t layout = SS_EXPR_AOS) {
    return {name, comps, 0, slot, codec, bits, bounds, layout};
}

using S = PoolSlot;
constexpr uint8_t kM = SS_EXPR_ADAM_M, kV = SS_EXPR_ADAM_V;

const Source kSources[] = {
    // ---- world: log scales, logit opacity ----
    plain("mean", kXyz, S::WorldMeans),
    plain("quat", kWxyz, S::WorldQuats),
    plain("scale", kXyz, S::WorldScales),
    plain("opacity", "", S::WorldOpacities),
    plain("dc", kRgb, S::WorldFeaturesDc),
    plain("sh_norm", kSh, S::WorldFeaturesSh),
    quant("sh_norm", kSh, S::EngWorldShVq8, SS_EXPR_QLIN, 8, kPerCells),
    quant("sh_norm", kSh, S::EngWorldShVq16, SS_EXPR_QLIN, 16, kPerCells),
    quant("sh_norm", kSh, S::EngWorldShVq8Fpbo, SS_EXPR_QLIN, 8, kPerSplats, SS_EXPR_SH_PAIR),
    quant("sh_norm", kSh, S::EngWorldShVq16Fpbo, SS_EXPR_QLIN, 16, kPerSplats, SS_EXPR_SH_PAIR),
    // ---- gradients ----
    plain("v_mean", kXyz, S::EngVMeans),
    quant("v_mean", kXyz, S::EngVMeansQ, SS_EXPR_GRADQ, 16, kPerSplats),
    plain("v_quat", kWxyz, S::EngVQuats),
    quant("v_quat", kWxyz, S::EngVQuatsQ, SS_EXPR_GRADQ, 16, kPerSplats),
    plain("v_scale", kXyz, S::EngVScales),
    quant("v_scale", kXyz, S::EngVScalesQ, SS_EXPR_GRADQ, 16, kPerSplats),
    plain("v_opacity", "", S::EngVOpacities),
    quant("v_opacity", "", S::EngVOpacitiesQ, SS_EXPR_GRADQ, 16, kPerSplats),
    plain("v_dc", kRgb, S::EngVFeaturesDc),
    quant("v_dc", kRgb, S::EngVFeaturesDcQ, SS_EXPR_GRADQ, 16, kPerSplats),
    plain("v_sh_norm", kSh, S::EngVFeaturesSh),
    quant("v_sh_norm", kSh, S::EngVFeaturesShQ, SS_EXPR_GRADQ, 8, kPerSplats),
    // ---- Adam moments ----
    plain("g1_mean", kXyz, S::EngG1Means),
    quant("g1_mean", kXyz, S::EngMeansQfpbo, kM, 16, kPerSplats),
    plain("g2_mean", kXyz, S::EngG2Means),
    quant("g2_mean", kXyz, S::EngMeansQfpbo, kV, 16, kPerSplats),
    plain("g1_quat", kWxyz, S::EngG1Quats),
    quant("g1_quat", kWxyz, S::EngQuatsQfpbo, kM, 16, kPerSplats),
    plain("g2_quat", kWxyz, S::EngG2Quats),
    quant("g2_quat", kWxyz, S::EngQuatsQfpbo, kV, 16, kPerSplats),
    plain("g1_scale", kXyz, S::EngG1Scales),
    quant("g1_scale", kXyz, S::EngScalesQfpbo, kM, 16, kPerSplats),
    plain("g2_scale", kXyz, S::EngG2Scales),
    quant("g2_scale", kXyz, S::EngScalesQfpbo, kV, 16, kPerSplats),
    plain("g1_opacity", "", S::EngG1Opacities),
    quant("g1_opacity", "", S::EngOpacitiesQfpbo, kM, 16, kPerSplats),
    plain("g2_opacity", "", S::EngG2Opacities),
    quant("g2_opacity", "", S::EngOpacitiesQfpbo, kV, 16, kPerSplats),
    plain("g1_dc", kRgb, S::EngG1FeaturesDc),
    quant("g1_dc", kRgb, S::EngFeaturesDcQfpbo, kM, 16, kPerSplats),
    plain("g2_dc", kRgb, S::EngG2FeaturesDc),
    quant("g2_dc", kRgb, S::EngFeaturesDcQfpbo, kV, 16, kPerSplats),
    plain("g1_sh_norm", kSh, S::EngG1FeaturesSh),
    quant("g1_sh_norm", kSh, S::EngShQuant, kM, 0, kPerCells),
    quant("g1_sh_norm", kSh, S::EngShQuantFpbo, kM, 0, kPerSplats, SS_EXPR_SH_PAIR),
    plain("g2_sh_norm", kSh, S::EngG2FeaturesSh),
    quant("g2_sh_norm", kSh, S::EngShQuant, kV, 0, kPerCells),
    quant("g2_sh_norm", kSh, S::EngShQuantFpbo, kV, 0, kPerSplats, SS_EXPR_SH_PAIR),
    // ---- densification; Avg's planar 2N accumulators read as their numerator ----
    plain("radii", "", S::EngRadii),
    plain("accum", "score count", S::EngAccumBuffer),
    plain("sample", "score count", S::EngDensifySampleScore),
    plain("bias_steps", "", S::EngBiasCorrectionSteps, SS_EXPR_I32),
    plain("visit", "renders idle", S::EngVisitCounters, SS_EXPR_U16),  // core/SplatVisitState.h
    plain("world_grad_score", "", S::EngDensifyWorldGradScore),
    plain("oversize", "", S::EngDensifyOversize),
    plain("oversize_weight", "", S::EngDensifyOversizeWeight, SS_EXPR_F32, 2),
    plain("region_weight", "", S::EngRegionWeight),
    plain("raster_accum", "", S::RasterBwdAccumWeight),
    plain("subbatch_accum", "", S::EngSubbatchAccumWeightSum),
    plain("mesh_radii", "", S::MeshingRenderRadii),
    // ---- refine scratch, as the last refine left it ----
    plain("score_gather", "", S::DensifyScoreGather),
    plain("fpbo_cam_bounds", "", S::FusedProjBwdCamBounds, SS_EXPR_I32),
    plain("wswr_sorting_values", "", S::DensifyWswrSortingValues, SS_EXPR_I32),
    plain("wswr_keys_out", "", S::DensifyWswrKeysOut, SS_EXPR_I32),
    plain("wswr_indices_in", "", S::DensifyWswrIndicesIn, SS_EXPR_I32),
    plain("wswr_indices_out", "", S::DensifyWswrIndicesOut, SS_EXPR_I32),
    plain("wswr_out_idx", "", S::DensifyWswrOutIdx, SS_EXPR_I32),
    plain("reloc_mask", "", S::DensifyRelocMask, SS_EXPR_I32),
    plain("reloc_dst_indices", "", S::DensifyRelocDstIndices, SS_EXPR_I32),
    plain("mcmc_sample_probs", "", S::DensifyMcmcSampleProbs),
    plain("mcmc_sample_probs_cumsum", "", S::DensifyMcmcSampleProbsCumsum),
    plain("mcmc_index_map", "", S::DensifyMcmcIndexMap, SS_EXPR_I32),
    plain("mcmc_n_idx_buffer", "", S::DensifyMcmcNIdxBuffer, SS_EXPR_I32),
    plain("mcmc_add_sample_probs", "", S::DensifyMcmcAddSampleProbs),
    plain("mcmc_add_sample_probs_cumsum", "", S::DensifyMcmcAddSampleProbsCumsum),
    plain("mcmc_add_index_map", "", S::DensifyMcmcAddIndexMap, SS_EXPR_I32),
    plain("mcmc_add_n_idx_buffer", "", S::DensifyMcmcAddNIdxBuffer, SS_EXPR_I32),
};

// One expression variable: a source, and which of its components.
struct Attr {
    std::string code;
    const Source* src;
    int comp;     // -1: the SH bands
    int ncomp;
};

const std::vector<Attr>& attrs() {
    static const std::vector<Attr> all = [] {
        std::vector<Attr> out;
        for (const Source& s : kSources) {
            if (std::strcmp(s.comps, kSh) == 0) {
                out.push_back({s.name, &s, -1, 0});
                continue;
            }
            std::vector<std::string> comps;
            std::istringstream in(s.comps);
            for (std::string t; in >> t;) comps.push_back(t);
            if (comps.empty()) out.push_back({s.name, &s, 0, 1});
            for (size_t c = 0; c < comps.size(); c++)
                out.push_back({std::string(s.name) + "_" + comps[c], &s, (int)c, (int)comps.size()});
        }
        return out;
    }();
    return all;
}

struct Live {
    void* data = nullptr;
    void* bounds = nullptr;
    size_t bytes = 0;
};

bool live(const Source& s, Live& out) {
    DevicePool& pool = DevicePool::global();
    size_t nb = 0;
    if (s.bounds == kNone) return pool.lookup(s.slot, kSubMain, out.data, out.bytes);
    return pool.lookup(s.slot, kSubPacked, out.data, out.bytes) &&
           pool.lookup(s.slot, kSubBounds, out.bounds, nb);
}

uint32_t cell_bits(uint32_t codec, uint32_t bits) {
    switch (codec) {
        case SS_EXPR_U16:   return 16;
        case SS_EXPR_QLIN:
        case SS_EXPR_GRADQ: return bits;
        case SS_EXPR_ADAM_M:
        case SS_EXPR_ADAM_V: return 2 * bits;   // (u, log_s) per cell
        default:            return 32;
    }
}

// False when the buffer cannot be read the way the row says: an SH row of no
// cells, or a quantized Adam state in a run that is not quantizing SH.
bool describe(const Attr& a, const Live& l, SplatExprVar& v) {
    const Source& s = *a.src;
    const EngineState& e = engine();
    v = SplatExprVar{};
    v.data = (uint64_t)l.data;
    v.bounds = (uint64_t)(l.bounds ? l.bounds : l.data);
    v.codec = s.codec;
    v.bits = s.bits ? s.bits : (uint32_t)e.optim.sh_optim_bits;
    v.layout = s.layout;
    if (a.comp < 0) {
        v.stride = 3u * (uint32_t)e.num_sh;
        v.count = 3u * (uint32_t)std::max(e.num_sh - 2 * e.sb_lobes, 0);
    } else {
        v.stride = (uint32_t)(s.stride ? s.stride : a.ncomp);
        v.offset = (uint32_t)a.comp;
    }
    if (s.bounds != kNone && v.bits != 4 && v.bits != 8 && v.bits != 16) return false;
    const uint32_t splat_cells = s.layout == SS_EXPR_SH_PAIR ? (v.stride + 1) / 2 * 2 : v.stride;
    if (splat_cells == 0) return false;
    const uint64_t cells = (uint64_t)l.bytes * 8 / cell_bits(v.codec, v.bits);
    v.rows = (uint32_t)std::min<uint64_t>(cells / splat_cells, UINT32_MAX);
    v.bounds_stride = s.bounds == kPerSplats ? 256u * splat_cells : 256u;
    return true;
}

}  // namespace

std::vector<SplatAttribute> engine_splat_attributes() {
    std::vector<SplatAttribute> out;
    std::unordered_map<std::string, size_t> at;
    for (const Attr& a : attrs()) {
        Live l;
        const bool on = live(*a.src, l);
        auto [it, fresh] = at.emplace(a.code, out.size());
        if (fresh) {
            out.push_back({a.code, slot_name(a.src->slot), on});
        } else if (on && !out[it->second].live) {
            out[it->second].pool = slot_name(a.src->slot);
            out[it->second].live = true;
        }
    }
    return out;
}

std::vector<float> engine_eval_splat_expr(const std::string& expr) {
    auto is_var = [](const std::string& name) {
        return std::any_of(attrs().begin(), attrs().end(),
                           [&](const Attr& a) { return a.code == name; });
    };
    spirula::ExprProgram prog;
    std::string error;
    if (!spirula::compile_expr(expr, is_var, prog, error))
        throw std::runtime_error(error);

    std::vector<SplatExprVar> vars;
    for (const std::string& name : prog.vars) {
        bool found = false;
        for (const Attr& a : attrs()) {
            Live l;
            SplatExprVar v;
            if (a.code != name || !live(*a.src, l) || !describe(a, l, v)) continue;
            vars.push_back(v);
            found = true;
            break;
        }
        if (!found) throw std::runtime_error("'" + name + "' is empty");
    }

    const int64_t n = std::max<int64_t>(engine().cur_num_splats, 0);
    std::vector<float> out((size_t)n);
    if (n == 0) return out;
    DeviceVector<float> d;
    d.resize(PoolSlot::ViewerSplatExpr, n);
    splat_expr_eval_tensor(n, vars, prog.code, d.data_ptr());
    backend::memcpy_sync(out.data(), d.data_ptr(), (size_t)n * sizeof(float),
                         backend::MemcpyKind::DeviceToHost);
    if (const char* err = backend::last_error())
        throw std::runtime_error(std::string("splat expression: ") + err);
    return out;
}
