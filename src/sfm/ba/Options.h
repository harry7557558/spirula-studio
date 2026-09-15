// Solver-facing configuration shared by the GPU driver (sfm/ba/Solver.h) and
// the host fallback (sfm/ba/SolverCpu.h).
#pragma once

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
    BAOverBudget(double need, double budget)
        : std::runtime_error("bundle adjustment needs more device memory than the budget allows"),
          need_mb(need), budget_mb(budget) {}
    double need_mb, budget_mb;
};

struct SolverOptions {
    RealCfg real = RealCfg::F64;
    float loss_param = 1.0f;      // Huber delta / Cauchy c (unused by trivial loss)
    int max_iters = 50;
    double init_damping = 1e-2;
    double rtol = 1e-6;
    int patience = 10;
    SolverSel solver = SolverSel::Auto;
    double vram_budget_mb = 0;    // 0 = 90% of the device-local heap (host: half the RAM)
    // Throw BAOverBudget instead of warning and trying anyway. For a caller
    // that can split the problem; the default keeps the old behaviour, since a
    // caller that cannot split is better served by an attempt than by a refusal.
    bool over_budget_throws = false;
    int cg_max_iters = 100;       // CG iteration cap per LM step
    double cg_tol = 0.1;          // relative residual tolerance eta
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
};

struct SolverStats {
    double initial_cost = 0, final_cost = 0;
    int iterations = 0, accepted = 0;
    double solve_seconds = 0;
    double vram_mb = 0;           // host RAM on the CPU path
    const char* solver = "dense";
    double cg_iters_total = 0;    // CG iterations summed over LM solves
    int cg_solves = 0;
    int cg_fallbacks = 0;         // LM iterations re-solved densely
};
