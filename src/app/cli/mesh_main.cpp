// mesh_main.cpp -- standalone mesh-extraction CLI.
//
//     spirula-mesh <checkpoint> [--data <dir>] [--format ply,glb]
//                 [--color none|vertex|texture] [--flag value ...]
//
// <checkpoint> is a run directory (containing config.json and a step-*.ckpt/
// folder), a *.ckpt directory, or a splat.ply file directly. The dataset
// (for camera-based occupancy + colors) defaults to config.json's `data`;
// pass --no-data to mesh from Gaussian densities only.
//
// It loads the raw
// (un-activated) Gaussians from the checkpoint PLY, parses the dataset with
// the same C++ parsers the CLI trainer uses, and calls meshing::generate_mesh
// (Meshing.h) for the heavy lifting.

#include "app/Tools.h"

#include "checkpoint/SplatPly.h"
#include "mesh/Meshing.h"
#include "core/Camera.h"
#include "data/DatasetParser.h"
#include "data/ImageProbe.h"
#include "data/Json.h"
#include "i18n/catalog/Cli.h"
#include "mesh/MeshLog.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace cmsg = spirula::i18n::msg::cli;
namespace mmsg = spirula::i18n::msg::mesh;
namespace mlog = meshing::mlog;
using spirula::i18n::format;

namespace {

// ===========================================================================
// Camera loading via the CLI trainer's dataset parsers
// ===========================================================================

struct MeshCameras {
    std::vector<float>   positions;    // [C*3] camera centers, splat frame
    std::vector<float>   viewmats;     // [C*16] world->cam, engine convention
    std::vector<float>   intrins;      // [C*4]
    std::vector<float>   dist_coeffs;  // [C*10]
    std::vector<int32_t> widths, heights;
    std::string          model;
    int64_t num() const { return (int64_t)widths.size(); }
};

MeshCameras load_cameras(const JsonValue& run_cfg, const std::string& data_dir,
                         const std::string& data_format_override) {
    DatasetParserConfig pcfg;
    std::string data_format = data_format_override;
    // config.json is flat: one key per training flag, spelled as the flag is.
    auto dp_str = [&](const char* key, const std::string& def) {
        const JsonValue* v = run_cfg.find(key);
        return (v && !v->is_null()) ? v->as_string() : def;
    };
    pcfg.recon_dir  = dp_str("colmap_recon_dir", "");
    pcfg.image_dir  = dp_str("image_dir", "images");
    pcfg.mask_dir   = dp_str("mask_dir", "masks");
    pcfg.metashape_xml = dp_str("metashape_xml", "");
    pcfg.metashape_ply = dp_str("metashape_ply", "");
    pcfg.metashape_psx = dp_str("metashape_psx", "");
    pcfg.downscale_rounding_mode = dp_str("downscale_rounding_mode", "floor");
    pcfg.probe_image_size = probe_image_size;
    {
        const JsonValue* v = run_cfg.find("train_resolution_divisor");
        if (v && v->type == JsonValue::Type::Number)
            pcfg.train_resolution_divisor = (float)v->as_double(0.0);
        const JsonValue* fmt = run_cfg.find("data_format");
        if (data_format.empty() && fmt && !fmt->is_null()) data_format = fmt->as_string();
    }
    // Use every parsed frame (train + would-be eval): more cameras only make
    // the visual-hull occupancy and the bake better.
    pcfg.eval_mode = "all";
    pcfg.validation_fraction = 0.0f;

    ParsedDataset ds = parse_dataset(data_dir, pcfg, data_format);
    const int64_t C = ds.num_cameras;
    if (C == 0) throw std::runtime_error("dataset has no cameras: " + data_dir);

    // relative_scale: splats live in a frame scaled by this;
    // scale the c2w translations to match before inverting.
    float rel = 1.0f;
    if (const JsonValue* rs = run_cfg.find("relative_scale"))
        if (rs->type == JsonValue::Type::Number) rel = (float)rs->as_double(1.0);
    if (rel != 1.0f)
        for (int64_t i = 0; i < C; ++i)
            for (int r = 0; r < 3; ++r)
                ds.c2w[i*12 + r*4 + 3] *= rel;

    // Engine-convention world->cam (Y/Z flip + inverse) via the identity
    // (no-warp) post-split bake -- the same algebra the trainer uses.
    PostSplitCameras post = bake_post_split(ds, false, false);
    if (post.n_post != C)
        throw std::runtime_error("unexpected camera split in mesh CLI");

    MeshCameras out;
    out.viewmats = std::move(post.viewmats);
    out.intrins = std::move(post.intrins);
    out.dist_coeffs = std::move(post.dist_coeffs);
    out.widths = ds.widths;
    out.heights = ds.heights;
    out.positions.resize(C * 3);
    for (int64_t i = 0; i < C; ++i)
        for (int r = 0; r < 3; ++r)
            out.positions[i*3 + r] = ds.c2w[i*12 + r*4 + 3];

    int32_t m0 = ds.camera_models.empty() ? 0 : ds.camera_models[0];
    for (int32_t m : ds.camera_models)
        if (m != m0) {
            mlog::warn(mlog::Stage::Loading, cmsg::mesh_mixed_camera_models);
            break;
        }
    out.model = camera_model_to_string((CameraModelType)m0);
    return out;
}

// ===========================================================================
// Flags
// ===========================================================================

struct Options {
    std::string checkpoint;
    std::string data;
    bool no_data = false;
    std::string output;                 // base path (extension optional)
    std::string format = "ply";
    std::string color = "vertex";       // none | vertex | texture
    std::string data_format;            // "" = config/auto
    std::optional<float> iso;           // default depends on cameras
    meshing::MeshingConfig m;           // remaining knobs live here
};

// One row of `--help`: the flag with its value syntax on the left, the
// sentence for it on the right. The flag is an identifier and stays as it is;
// only the sentence is translated -- and a translated sentence runs longer
// than the English one more often than not, so each of its lines is laid out
// at the same column rather than assumed to fit beside the flag.
void help_row(const char* flags, const std::string& text) {
    constexpr int kCol = 26;
    std::string left = std::string("  ") + flags;
    if ((int)left.size() >= kCol) {
        std::printf("%s\n", left.c_str());
        left.clear();
    }
    left.resize(kCol, ' ');
    size_t at = 0;
    while (at <= text.size()) {
        const size_t nl = text.find('\n', at);
        const std::string line =
            text.substr(at, nl == std::string::npos ? nl : nl - at);
        std::printf("%s%s\n", left.c_str(), line.c_str());
        left.assign(kCol, ' ');
        if (nl == std::string::npos) break;
        at = nl + 1;
    }
}

void help_row(const char* flags, const spirula::i18n::Msg& m) {
    help_row(flags, std::string(m.get()));
}
void help_row(const char* flags, const spirula::i18n::Msg& m,
              std::initializer_list<spirula::i18n::Arg> args) {
    help_row(flags, spirula::i18n::format(m, args));
}

void print_help(const char* argv0) {
    Options d;
    std::printf("%s\n\n", format(mmsg::help_usage, {argv0}).c_str());
    std::printf("%s\n\n", mmsg::help_intro.get());

    help_row("--data <dir>", mmsg::help_data);
    help_row("--no-data", mmsg::help_no_data);
    help_row("--data-format <fmt>", mmsg::help_data_format);
    help_row("--output <path>", mmsg::help_output);
    help_row("--format <list>", mmsg::help_format, {d.format});
    help_row("--color <mode>", mmsg::help_color, {d.color});
    help_row("--texture-size <n>", mmsg::help_texture_size, {d.m.texture_size});
    help_row("--tex-gutter-px <n>", mmsg::help_tex_gutter, {d.m.tex_gutter_px});
    help_row("--chart-angle-deg <a>", mmsg::help_chart_angle,
             {(double)d.m.chart_angle_deg});
    std::printf("\n");
    help_row("--iso <v>", mmsg::help_iso);
    help_row("--merge-factor <v>", mmsg::help_merge_factor,
             {(double)d.m.merge_factor});
    help_row("--bisection-iters <n>", mmsg::help_bisection_iters,
             {d.m.bisection_iters});
    help_row("--max-cameras <n>", mmsg::help_max_cameras, {d.m.max_cameras});
    help_row("--max-grid-res <n>", mmsg::help_max_grid_res, {d.m.max_grid_res});
    help_row("--grid-cell-factor <v>", mmsg::help_grid_cell_factor,
             {(double)d.m.grid_cell_factor});
    help_row("--num-threads <n>", mmsg::help_num_threads, {d.m.num_threads});
    help_row("--carve-k <n>", mmsg::help_carve_k, {d.m.carve_k});
    help_row("--cull-unseen <0|1>", mmsg::help_cull_unseen,
             {(int)d.m.cull_unseen});
    help_row("--merge-max-flip-deg <a>", mmsg::help_merge_max_flip,
             {(double)d.m.merge_max_flip_deg});
    help_row("--floater-min-faces <n>", mmsg::help_floater_min_faces,
             {d.m.floater_min_faces});
    help_row("--fill-hole-ratio <v>", mmsg::help_fill_hole_ratio,
             {(double)d.m.fill_hole_ratio});
    help_row("--fill-hole-max-edges <n>", mmsg::help_fill_hole_max_edges,
             {d.m.fill_hole_max_edges});
    help_row("--degenerate-angle-deg <a>", mmsg::help_degenerate_angle,
             {(double)d.m.degenerate_angle_deg});
    help_row("--quality-iters <n>", mmsg::help_quality_iters,
             {d.m.quality_iters});
    help_row("--quiet", mmsg::help_quiet);
}

float parse_f(const std::string& key, const char* v) {
    try { size_t p; float x = std::stof(v, &p); if (p == std::strlen(v)) return x; }
    catch (...) {}
    throw std::runtime_error("--" + key + ": expected a number, got '" + v + "'");
}
int parse_i(const std::string& key, const char* v) {
    try { size_t p; int x = std::stoi(v, &p); if (p == std::strlen(v)) return x; }
    catch (...) {}
    throw std::runtime_error("--" + key + ": expected an integer, got '" + v + "'");
}

Options parse_args(int argc, char** argv) {
    Options o;
    // meshing preset defaults
    o.m.floater_min_faces = 10;
    o.m.fill_hole_max_edges = 20;
    o.m.degenerate_angle_deg = 15.0f;
    o.m.max_cameras = -1;

    int i = 1;
    if (i < argc && argv[i][0] != '-') o.checkpoint = argv[i++];
    for (; i < argc; ++i) {
        std::string key = argv[i];
        if (key == "--help" || key == "-h") { print_help(argv[0]); std::exit(0); }
        while (!key.empty() && key[0] == '-') key.erase(key.begin());
        for (auto& c : key) if (c == '-') c = '_';
        auto next = [&]() -> const char* {
            if (i + 1 >= argc)
                throw std::runtime_error("--" + key + ": missing value");
            return argv[++i];
        };
        if      (key == "data")            o.data = next();
        else if (key == "no_data")         o.no_data = true;
        else if (key == "data_format")     o.data_format = next();
        else if (key == "output")          o.output = next();
        else if (key == "format")          o.format = next();
        else if (key == "color")           o.color = next();
        else if (key == "texture_size")    o.m.texture_size = parse_i(key, next());
        else if (key == "tex_gutter_px")   o.m.tex_gutter_px = parse_i(key, next());
        else if (key == "chart_angle_deg") o.m.chart_angle_deg = parse_f(key, next());
        else if (key == "iso")             o.iso = parse_f(key, next());
        else if (key == "merge_factor")    o.m.merge_factor = parse_f(key, next());
        else if (key == "bisection_iters") o.m.bisection_iters = parse_i(key, next());
        else if (key == "max_cameras")     o.m.max_cameras = parse_i(key, next());
        else if (key == "max_grid_res")    o.m.max_grid_res = parse_i(key, next());
        else if (key == "grid_cell_factor") o.m.grid_cell_factor = parse_f(key, next());
        else if (key == "num_threads")     o.m.num_threads = parse_i(key, next());
        else if (key == "carve_k")         o.m.carve_k = parse_i(key, next());
        else if (key == "cull_unseen")     o.m.cull_unseen = parse_i(key, next()) != 0;
        else if (key == "no_cull_unseen")  o.m.cull_unseen = false;
        else if (key == "merge_max_flip_deg") o.m.merge_max_flip_deg = parse_f(key, next());
        else if (key == "floater_min_faces")  o.m.floater_min_faces = parse_i(key, next());
        else if (key == "fill_hole_ratio")    o.m.fill_hole_ratio = parse_f(key, next());
        else if (key == "fill_hole_max_edges") o.m.fill_hole_max_edges = parse_i(key, next());
        else if (key == "degenerate_angle_deg") o.m.degenerate_angle_deg = parse_f(key, next());
        else if (key == "quality_iters")   o.m.quality_iters = parse_i(key, next());
        else if (key == "quiet")           o.m.verbose = false;
        else throw std::runtime_error("unknown flag: --" + key + " (see --help)");
    }
    if (o.checkpoint.empty())
        throw std::runtime_error("missing <checkpoint> argument (see --help)");
    return o;
}

}  // namespace


int spirula_mesh_main(int argc, char** argv) {
    try {
        Options o = parse_args(argc, argv);

        // ---- resolve color mode + formats, and validate BEFORE any work ----
        meshing::MeshColorMode mode;
        if      (o.color == "none")    mode = meshing::MeshColorMode::None;
        else if (o.color == "vertex")  mode = meshing::MeshColorMode::Vertex;
        else if (o.color == "texture") mode = meshing::MeshColorMode::Texture;
        else throw std::runtime_error("--color: expected none/vertex/texture, got '"
                                      + o.color + "'");
        o.m.color_mode = mode;
        std::vector<meshing::MeshFormatSpec> specs =
            meshing::parse_mesh_formats(o.format);
        if (specs.empty())
            throw std::runtime_error("--format: no valid format given");
        o.m.formats.clear();
        for (const auto& spec : specs) {
            std::string err = meshing::check_export_support(spec, mode);
            if (!err.empty()) throw std::runtime_error(err);
            o.m.formats.push_back(spec.token());
        }

        // ---- checkpoint ----
        auto [splat_ply_s, run_dir_s] = spirula::find_splat_ply(o.checkpoint);
        const fs::path splat_ply(splat_ply_s), run_dir(run_dir_s);
        mlog::out_raw(mlog::Stage::Loading, splat_ply.string());

        // ---- output base, and the one check that must happen FIRST ----
        // The output must not be one of the inputs: meshing a `foo.ply` with
        // `--output foo` would replace the model being meshed, and that model
        // is usually the only copy of itself. Checked before a single byte is
        // read, so the refusal costs nothing and can never come after the
        // pipeline has already earned the right to destroy it.
        std::string out_base = o.output;
        if (out_base.empty())
            out_base = (splat_ply.parent_path() / "mesh").string();
        {
            std::string err = meshing::check_mesh_outputs_safe(
                specs, mode, out_base, {splat_ply.string()});
            if (!err.empty()) throw std::runtime_error(err);
        }
        // Meshing reads geometry and DC colour only; skipping f_rest
        // skips most of the file.
        spirula::SplatCloud splats =
            spirula::read_splat_ply(splat_ply.string(), /*want_sh=*/false);
        mlog::out(mlog::Stage::Loading, mmsg::gaussians_loaded,
                  {(long long)splats.num});

        JsonValue run_cfg;
        fs::path cfg_path = run_dir / "config.json";
        if (fs::is_regular_file(cfg_path))
            run_cfg = json_parse_file(cfg_path.string());

        // ---- cameras ----
        MeshCameras cams;
        if (!o.no_data) {
            std::string data_dir = o.data;
            if (data_dir.empty()) {
                const JsonValue* d = run_cfg.find("data");
                if (d && !d->is_null()) {
                    fs::path cand = d->as_string();
                    if (cand.is_relative()) cand = run_dir / cand;
                    if (fs::exists(cand)) data_dir = cand.string();
                }
            }
            if (!data_dir.empty() && fs::exists(data_dir)) {
                mlog::out(mlog::Stage::Loading, mmsg::reading_cameras,
                          {data_dir});
                cams = load_cameras(run_cfg, data_dir, o.data_format);
                mlog::out(mlog::Stage::Loading, mmsg::cameras_loaded,
                          {(long long)cams.num(), cams.widths[0],
                           cams.heights[0], cams.model});
            }
        }
        const bool using_cameras = cams.num() > 0;
        if (!using_cameras)
            mlog::warn(mlog::Stage::Loading, cmsg::mesh_no_cameras);

        o.m.iso = o.iso.value_or(using_cameras ? 0.5f : 0.2f);


        meshing::CameraParams cp;
        if (using_cameras) {
            cp.viewmats = cams.viewmats.data();
            cp.intrins = cams.intrins.data();
            cp.dist_coeffs = cams.dist_coeffs.data();
            cp.widths = cams.widths.data();
            cp.heights = cams.heights.data();
            cp.camera_model = cams.model;
        }
        bool ok = meshing::generate_mesh(
            splats.means.data(), splats.quats.data(), splats.scales.data(),
            splats.opacities.data(), splats.features_dc.data(), (int)splats.num,
            using_cameras ? cams.positions.data() : nullptr, (int)cams.num(),
            cp, o.m, out_base);
        if (!ok) return 1;
    } catch (const std::exception& e) {
        mlog::fail_raw(e.what());
        return 1;
    }
    return 0;
}
