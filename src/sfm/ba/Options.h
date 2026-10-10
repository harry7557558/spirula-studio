// Solver-facing configuration shared by the GPU driver (sfm/ba/Solver.h) and
// the host fallback (sfm/ba/SolverCpu.h).
#pragma once

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

// The arithmetic the solver runs in. `CPU` is double precision on the host, for
// devices that can run none of the kernels (see realSupportedByDevice).
enum class RealCfg { F32, F64, DF64, CPU };

inline RealCfg realCfgFromName(const std::string& s) {
    return s == "float"  ? RealCfg::F32
         : s == "df"     ? RealCfg::DF64
         : s == "cpu"    ? RealCfg::CPU
                         : RealCfg::F64;
}

inline const char* realCfgName(RealCfg c) {
    switch (c) {
        case RealCfg::F32: return "float";
        case RealCfg::F64: return "double";
        case RealCfg::CPU: return "cpu";
        default: return "df";
    }
}
inline size_t realSize(RealCfg c) { return c == RealCfg::F32 ? 4 : 8; }

inline void packReals(std::vector<uint8_t>& out, const double* v, size_t n, RealCfg cfg) {
    out.resize(n * realSize(cfg));
    if (cfg == RealCfg::F32) {
        float* p = (float*)out.data();
        for (size_t i = 0; i < n; i++) p[i] = (float)v[i];
    } else if (cfg == RealCfg::DF64) {
        float* p = (float*)out.data();
        for (size_t i = 0; i < n; i++) {
            float hi = (float)v[i];
            p[2 * i] = hi;
            p[2 * i + 1] = (float)(v[i] - hi);
        }
    } else {
        memcpy(out.data(), v, n * 8);
    }
}

inline void unpackReals(std::vector<double>& out, const uint8_t* v, size_t n, RealCfg cfg) {
    out.resize(n);
    if (cfg == RealCfg::F32) {
        const float* p = (const float*)v;
        for (size_t i = 0; i < n; i++) out[i] = p[i];
    } else if (cfg == RealCfg::DF64) {
        const float* p = (const float*)v;
        for (size_t i = 0; i < n; i++) out[i] = (double)p[2 * i] + (double)p[2 * i + 1];
    } else {
        memcpy(out.data(), v, n * 8);
    }
}

enum class SolverSel { Auto, Dense, CG };
enum class CgFallback { Auto, On, Off };

// Raised, before anything is allocated, when the chosen path does not fit the
// memory budget and the caller asked to be told rather than to find out from
// the driver. There is nothing below CG to fall back to -- its footprint is the
// problem data plus a few vectors -- so the only answer is a smaller problem,
// and only the caller knows how to make one (Mapper::jointRefine splits its
// models into batches).
struct BAOverBudget : std::runtime_error {
    BAOverBudget(double need, double budget, const char* resource = "device")
        : std::runtime_error(std::string("bundle adjustment needs more ") + resource +
                             " memory than the budget allows"),
          need_mb(need), budget_mb(budget) {}
    double need_mb, budget_mb;
};

struct BAIndexCapacity : std::runtime_error {
    BAIndexCapacity(uint64_t need, uint64_t limit, const char* name)
        : std::runtime_error(std::string("bundle adjustment ") + name + " needs " +
                             std::to_string(need) + " indexed elements; capacity is " +
                             std::to_string(limit)),
          need_elements(need), limit_elements(limit), pool(name) {}
    uint64_t need_elements, limit_elements;
    std::string pool;
};

inline void checkBAIndexCapacity(uint64_t need, uint64_t limit = UINT32_MAX,
                                 const char* pool = "Jc pool") {
    if (limit > UINT32_MAX) limit = UINT32_MAX;
    if (need > limit) throw BAIndexCapacity(need, limit, pool);
}

// Where a device solve had got to: its parameters are in the problem's host
// vectors as of `iterations` LM iterations, so a restart after a device failure
// resumes from here instead of from the start.
struct SolverCheckpoint {
    int iterations = 0;
    double damping = 0, cost = 0;
};

struct SolverOptions {
    RealCfg real = RealCfg::F64;
    float loss_param = 1.0f;      // Huber delta / Cauchy c (unused by trivial loss)
    int max_iters = 50;
    double init_damping = 1e-2;
    double rtol = 1e-6;
    int patience = 10;
    // A step under rtol that still cuts the prior cost by prior_rtol shrinks the
    // damping, up to prior_patience times, when absolute centres are present. Canopy drone capture:
    // at damping 3e-3 a step cut its GPS prior < 2e-5; 1e-6 is 2.5x f32 noise.
    double prior_rtol = 1e-6;
    int prior_patience = 15;
    // Stop at an accepted point whose Ceres gradient max-norm (sfm/ba/GradientNorm.h)
    // is at or under this; 0 = off. Its lengths are metres, metres_per_unit to a model unit.
    double gradient_tol = 0;
    double metres_per_unit = 1;
    SolverSel solver = SolverSel::Auto;
    double vram_budget_mb = 0;    // 0 = 90% of the device-local heap (host: half the RAM)
    size_t host_budget_bytes = 0;
    // Throw BAOverBudget instead of warning and trying anyway. For a caller
    // that can split the problem; the default keeps the old behaviour, since a
    // caller that cannot split is better served by an attempt than by a refusal.
    bool over_budget_throws = false;
    int cg_max_iters = 100;       // CG iteration cap per LM step
    double cg_tol = 0.1;          // relative residual tolerance eta
    // ... and CG also stops once a step improves the quadratic model by under
    // this fraction of the total so far (Nash-Sofer; 0 = off). It settles for a
    // residual near sqrt of it, so a caller that wants the exact step turns it off.
    double cg_model_tol = 0.1;
    CgFallback cg_fallback = CgFallback::Auto;
    // The kernels are compiled per (real, loss); `loss` selects the embedded
    // blob "ba_<real>_<loss>". spv_path overrides it with a module from disk
    // (a hand-compiled shader, for iteration without relinking).
    std::string loss = "trivial";
    std::string spv_path;
    // Canonical uuid:<hex> of the device to run on, "" for the shared
    // precedence (explicit --device, then VK_DEVICE, then Auto).
    std::string device_selector;
    int device = -1;
    // Host worker threads for the CPU path; 0 = every core. Caps the tasks one
    // solve splits into, not the shared pool's width (bacpu::Pool).
    int threads = 0;
    bool validate = false;
    bool verbose = true;
    bool profile = false;
    // Written by a device solve every few seconds of accepted progress, along
    // with the problem's parameters. Null = no checkpoints.
    SolverCheckpoint* checkpoint = nullptr;
};

// What an accepted LM step does, in both solvers' loops: an improvement shrinks
// the damping and resets patience, a tie counts toward patience and leaves it.
enum class LmAccept { Improved, PriorImproved, Tie };

inline LmAccept classifyAccept(const SolverOptions& o, double cost, double newCost,
                               bool absCentres, double prior, double newPrior, int priorSteps) {
    if (newCost / cost < 1.0 - o.rtol) return LmAccept::Improved;
    if (absCentres && prior > 0 && newPrior <= prior * (1.0 - o.prior_rtol) &&
        priorSteps < o.prior_patience)
        return LmAccept::PriorImproved;
    return LmAccept::Tie;
}

struct SolverStats {
    double initial_cost = 0, final_cost = 0;
    int iterations = 0, accepted = 0;
    double solve_seconds = 0;
    double vram_mb = 0;           // host RAM on the CPU path
    const char* solver = "dense";
    bool jac32 = false;           // the Jacobians were stored at fp32 (Solver.h)
    double cg_iters_total = 0;    // CG iterations summed over LM solves
    int cg_solves = 0;
    int cg_fallbacks = 0;         // LM iterations re-solved densely
    double prior_initial = 0, prior_final = 0;  // the priors' share of the cost
    int prior_steps = 0;          // ties whose prior decrease shrank the damping
    double final_damping = 0;
    // The gradient max-norm at each accepted point it was measured at (gradient_tol > 0),
    // and whether the last of them stopped the solve.
    std::vector<double> gradient_norms;
    bool gradient_stop = false;
};
