// `spirula-sfm ba` -- the bundle adjuster driven directly on a BAL problem
// (Bundle Adjustment in the Large), for benchmarking and solver debugging.
// Its own translation unit because it shares nothing with the pipeline
// subcommands but the solver. See src/sfm/ba/README.md.
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>

#include "sfm/SfmConfig.h"
#include "sfm/ba/Problem.h"
#include "sfm/ba/Solver.h"
#include "sfm/core/Rig.h"
#include "sfm/map/Bundle.h"
#include "core/Env.h"
#include "i18n/catalog/SfmHelp.h"

// spirula::env with a default. (`getenv(x) ?: "d"` is a GNU extension MSVC
// lacks.)
static const char* env_or(const char* suffix, const char* fallback) {
    const char* v = spirula::env(suffix);
    return v ? v : fallback;
}

static void writePly(const char* path, const std::vector<double>& pts) {
    std::ofstream f(path, std::ios::binary);
    size_t n = pts.size() / 3;
    f << "ply\nformat binary_little_endian 1.0\nelement vertex " << n
      << "\nproperty float x\nproperty float y\nproperty float z\nend_header\n";
    for (size_t i = 0; i < n * 3; i++) {
        float v = (float)pts[i];
        f.write((const char*)&v, 4);
    }
}


// Printed by `spirula-sfm ba --help` and by `spirula-sfm help ba`. The solver's
// options are not in the SfmConfig table: nothing else drives them, and the
// pipeline's own bundle adjustment is configured through --ba-* on `map`.
void printBaHelp(FILE* out) {
    namespace H = spirula::i18n::msg::sfmhelp;
    using spirula::i18n::wrap;

    std::fprintf(out, "spirula-sfm ba -- %s\n\n", H::sum_ba.get());
    std::fprintf(out, "%s\n  spirula-sfm ba <BAL_PROBLEM.TXT|SPARSE_MODEL_DIR> "
                      "[options]\n\n", H::label_usage.get());

    std::fprintf(out, "%s\n", H::label_description.get());
    for (const spirula::i18n::Msg* m : {&H::ba_desc_1, &H::ba_desc_2}) {
        if (m != &H::ba_desc_1) std::fprintf(out, "\n");
        for (const std::string& line : wrap(m->get(), 94))
            std::fprintf(out, "  %s\n", line.c_str());
    }

    // One row per flag: the flag and its value syntax on the left (identifiers,
    // so untranslated), the sentence for it on the right, wrapped where the
    // language allows.
    struct Row { const char* flag; const char* def; const spirula::i18n::Msg* help; };
    static const Row kRows[] = {
        {"--real {float|double|df|cpu}", "double", &H::ba_opt_real},
        {"--loss {trivial|huber|cauchy}", "trivial", &H::ba_opt_loss},
        {"--loss-param X", "1", &H::ba_opt_loss_param},
        {"--model {snavely|snavely_f}", "snavely", &H::ba_opt_model},
        {"--shared-intrinsics", "", &H::ba_opt_shared_intrinsics},
        {"--rig PREFIX,PREFIX,...", "", &H::opt_rig},
        {"--solver {auto|dense|cg}", "auto", &H::ba_opt_solver},
        {"--max-iters N", "", &H::ba_opt_max_iters},
        {"--damping X", "", &H::ba_opt_damping},
        {"--rtol X", "", &H::ba_opt_rtol},
        {"--patience N", "", &H::ba_opt_patience},
        {"--cg-iters N", "", &H::ba_opt_cg_iters},
        {"--cg-tol X", "", &H::ba_opt_cg_tol},
        {"--cg-fallback {auto|on|off}", "", &H::ba_opt_cg_fallback},
        {"--vram-budget MB", "", &H::ba_opt_vram_budget},
        {"--ply PREFIX", "", &H::ba_opt_ply},
        {"-o, --output DIR", "", &H::ba_opt_output},
        {"--device N", "", &H::ba_opt_device},
        {"--validate", "", &H::ba_opt_validate},
        {"--profile", "", &H::ba_opt_profile},
        {"--quiet", "", &H::ba_opt_quiet},
        {"--spv-path FILE", "", &H::ba_opt_spv_path},
        {"-h, --help", "", &H::opt_help},
    };
    std::fprintf(out, "\n%s\n", H::label_options.get());
    for (const Row& r : kRows) {
        sfm::printOptionLine(out, r.flag, r.def, r.help->get());
    }

    std::fprintf(out,
        "\n%s\n"
        "  spirula-sfm ba problem-49-7776-pre.txt --real df --loss huber\n"
        "  spirula-sfm ba problem-1778-993923-pre.txt --solver cg --vram-budget 4096\n\n",
        H::label_examples.get());
    for (const std::string& line : wrap(H::ba_note.get(), 78))
        std::fprintf(out, "%s\n", line.c_str());
}

// The rig table over a written model's image names, with each member's
// extrinsic averaged from the poses -- what the mapper's calibration would
// have found, so the benchmark builds the same rigged problem it builds.
static sfm::RigTable rigTableForModel(sfm::Reconstruction& rec,
                                      const std::vector<sfm::RigDef>& defs) {
    uint32_t n = 0;
    for (const auto& kv : rec.images) n = std::max(n, kv.first + 1);
    std::vector<std::string> names(n);
    for (const auto& kv : rec.images) names[kv.first] = kv.second.name;
    sfm::RigTable rigs = sfm::buildRigTable(names, defs);
    rec.rigs.assign(rigs.rigs.size(), sfm::RigCalib{});
    const sfm::RigCalibOptions copt;
    for (size_t r = 0; r < rigs.rigs.size(); r++) {
        const sfm::RigSpec& spec = rigs.rigs[r];
        sfm::RigCalib& c = rec.rigs[r];
        c.resize(spec.members.size());
        c.ref = 0;
        c.established[0] = 1;
        auto regd = [&](uint32_t img) {
            auto it = img == sfm::kNoImage ? rec.images.end() : rec.images.find(img);
            return it != rec.images.end() && it->second.registered;
        };
        for (uint32_t m = 1; m < spec.members.size(); m++) {
            std::vector<sfm::Pose> rel;
            for (const auto& fr : spec.frames)
                if (regd(fr[0]) && regd(fr[m]))
                    rel.push_back(sfm::relativePose(rec.images.at(fr[0]).pose,
                                                    rec.images.at(fr[m]).pose));
            if ((int)rel.size() < copt.min_frames) continue;
            sfm::Pose avg;
            double spread = 0;
            const int inl = sfm::averageRelativePoses(rel, copt, avg, spread, nullptr);
            if (inl < copt.min_frames) continue;
            c.cam_from_rig[m] = avg;
            c.established[m] = 1;
            c.support[m] = (uint32_t)inl;
            c.spread_deg[m] = spread;
        }
    }
    return rigs;
}

static size_t calibratedMembers(const sfm::Reconstruction& rec) {
    size_t n = 0;
    for (const sfm::RigCalib& c : rec.rigs) n += c.numEstablished();
    return n;
}

int cmdBa(int argc, char** argv) {
    std::string file, ply_prefix, out_dir, loss = "trivial", model = "snavely";
    SolverOptions opt;
    bool shared_intr = false, loss_given = false;
    std::vector<sfm::RigDef> rig_defs;

    for (int i = 0; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&]() { return std::string(argv[++i]); };
        if (a == "--help" || a == "-h") { printBaHelp(stdout); return 0; }
        else if (a == "--real") opt.real = realCfgFromName(next());
        else if (a == "--loss") { loss = next(); loss_given = true; }
        else if (a == "--spv-path") opt.spv_path = next();
        else if (a == "--loss-param") { opt.loss_param = std::stof(next()); loss_given = true; }
        else if (a == "-o" || a == "--output") out_dir = next();
        else if (a == "--model") model = next();
        else if (a == "--shared-intrinsics") shared_intr = true;
        else if (a == "--rig") {
            sfm::RigDef d;
            const std::string err = sfm::parseRigArg(next(), d);
            if (!err.empty()) {
                fprintf(stderr, "spirula-sfm ba: error: %s\n", err.c_str());
                return 1;
            }
            rig_defs.push_back(std::move(d));
        }
        else if (a == "--max-iters") opt.max_iters = std::stoi(next());
        else if (a == "--damping") opt.init_damping = std::stod(next());
        else if (a == "--rtol") opt.rtol = std::stod(next());
        else if (a == "--patience") opt.patience = std::stoi(next());
        else if (a == "--ply") ply_prefix = next();
        else if (a == "--solver") {
            std::string s = next();
            opt.solver = s == "dense" ? SolverSel::Dense
                       : s == "cg"    ? SolverSel::CG
                                      : SolverSel::Auto;
        } else if (a == "--vram-budget") opt.vram_budget_mb = std::stod(next());
        else if (a == "--cg-iters") opt.cg_max_iters = std::stoi(next());
        else if (a == "--cg-tol") opt.cg_tol = std::stod(next());
        else if (a == "--cg-fallback") {
            std::string s = next();
            opt.cg_fallback = s == "on"  ? CgFallback::On
                            : s == "off" ? CgFallback::Off
                                         : CgFallback::Auto;
        }
        else if (a == "--device") opt.device = std::stoi(next());
        else if (a == "--validate") opt.validate = true;
        else if (a == "--profile") opt.profile = true;
        else if (a == "--quiet") opt.verbose = false;
        else if (a[0] != '-') file = a;
        else {
            fprintf(stderr, "spirula-sfm ba: error: unknown option %s\n", a.c_str());
            fprintf(stderr, "Try 'spirula-sfm ba --help' for more information.\n");
            return 1;
        }
    }

    if (file.empty()) {
        fprintf(stderr, "spirula-sfm ba: error: a BAL problem file or sparse model "
                        "directory is required\n");
        fprintf(stderr, "Try 'spirula-sfm ba --help' for more information.\n");
        return 1;
    }

    int model_id = -1;
    for (int m = 0; m < kNumModels; m++)
        if (model == kModels[m].name) model_id = m;
    if (model_id < 0) {
        fprintf(stderr, "spirula-sfm ba: error: unknown --model %s\n", model.c_str());
        return 1;
    }

    // both debug hooks pin the solver selection they need
    if (spirula::env("SFM_DUMP_SG")) opt.solver = SolverSel::Dense;
    const bool cmp_step = spirula::env("SFM_CMP_STEP") != nullptr;
    if (cmp_step) {
        opt.solver = SolverSel::CG;
        opt.cg_fallback = CgFallback::On;
    }

    // A directory is a COLMAP sparse model: build the same problem the mapper's
    // global BA builds and drive the solver on it. This is how the solver is
    // profiled on real captures rather than on BAL. The mapper's loss defaults
    // come along with it, since matching what the pipeline runs is the point.
    std::error_code dir_ec;
    const bool is_model = std::filesystem::is_directory(file, dir_ec);

    auto t0 = std::chrono::high_resolution_clock::now();
    sfm::Reconstruction rec;
    sfm::BundleLayout layout;
    sfm::RigTable rigs;
    if (is_model) {
        sfm::BundleOptions bopt;
        if (!loss_given) { loss = bopt.loss; opt.loss_param = bopt.loss_param; }
        rec = sfm::Reconstruction::readBinary(file);
        bopt.real = opt.real;
        bopt.verbose = opt.verbose;
        bopt.device = opt.device;
        if (!rig_defs.empty()) {
            try {
                rigs = rigTableForModel(rec, rig_defs);
            } catch (const std::exception& e) {
                fprintf(stderr, "spirula-sfm ba: error: %s\n", e.what());
                return 1;
            }
            bopt.rigs = &rigs;
            fprintf(stderr, "[model] %zu rig(s), %zu member(s) calibrated from the poses\n",
                    rigs.rigs.size(), (size_t)calibratedMembers(rec));
        }
        layout = sfm::buildBundle(rec, bopt);
        if (layout.P.num_images < 2) {
            fprintf(stderr, "spirula-sfm ba: error: %s holds no registered model\n", file.c_str());
            return 1;
        }
        fprintf(stderr, "[model] %u images, %u points, %u observations\n", layout.P.num_images,
                layout.P.num_points, layout.P.num_obs);
    }
    opt.loss = loss;
    BAProblem P = is_model ? std::move(layout.P) : loadBAL(file, model_id, shared_intr);
    BundleSolver solver(P, opt);
    solver.init();
    auto t1 = std::chrono::high_resolution_clock::now();
    double t_pre = std::chrono::duration<double>(t1 - t0).count();

    if (!ply_prefix.empty()) writePly((ply_prefix + "_before.ply").c_str(), P.points);

    if (spirula::env("SFM_DUMP_SG")) {
        solver.debugAssemble(atof(env_or("SFM_DUMP_SG_LAMBDA", "0.01")));
        std::string base = spirula::env("SFM_DUMP_SG");
        std::vector<double> v = solver.debugPackedS();
        std::ofstream fs(base + "_S.bin", std::ios::binary);
        fs.write((const char*)v.data(), v.size() * 8);
        v = solver.debugG();
        std::ofstream fg(base + "_g.bin", std::ios::binary);
        fg.write((const char*)v.data(), v.size() * 8);
        return 0;
    }

    // SS_SFM_CMP_STEP: solve one assembly with both CG and dense, print the
    // step difference (use --cg-tol / --cg-iters to control CG accuracy)
    if (cmp_step) {
        double lam = atof(env_or("SFM_CMP_STEP_LAMBDA", "0.01"));
        solver.debugCompareStep((float)lam);
        return 0;
    }

    solver.solve();
    solver.downloadParams();

    if (!ply_prefix.empty()) writePly((ply_prefix + "_after.ply").c_str(), P.points);
    if (is_model && !out_dir.empty()) {
        sfm::writeBundle(rec, layout, P);
        std::filesystem::create_directories(out_dir);
        rec.writeBinary(out_dir);
    }

    const SolverStats& st = solver.stats();
    printf("real=%s loss=%s model=%s solver=%s%s\n", realCfgName(solver.real()), loss.c_str(),
           model.c_str(), st.solver, shared_intr ? " shared-intrinsics" : "");
    {
        namespace H = spirula::i18n::msg::sfmhelp;
        using spirula::i18n::format;
        auto num = [](double v, int dec) {
            char b[64];
            std::snprintf(b, sizeof b, "%.*f", dec, v);
            return std::string(b);
        };
        char c0[32], c1[32];
        std::snprintf(c0, sizeof c0, "%.6e", st.initial_cost);
        std::snprintf(c1, sizeof c1, "%.6e", st.final_cost);
        printf("%s: %s\n", H::ba_res_initial_cost.get(), c0);
        printf("%s: %s\n", H::ba_res_final_cost.get(), c1);
        printf("%s\n", format(H::ba_res_iterations,
                              {st.iterations, st.accepted}).c_str());
        if (st.cg_solves)
            printf("%s\n", format(H::ba_res_cg,
                                  {num(st.cg_iters_total / st.cg_solves, 1),
                                   st.cg_fallbacks}).c_str());
        printf("%s\n", format(H::ba_res_time,
                              {num(t_pre, 3), num(st.solve_seconds, 3),
                               num(t_pre + st.solve_seconds, 3)}).c_str());
        printf("%s\n", format(H::ba_res_vram, {num(st.vram_mb, 1)}).c_str());
    }
    return 0;
}
