// `spirula marker` -- detect AprilTags, align coordinate frame, scale and crop.
#include "app/Tools.h"

#include "data/DatasetParser.h"
#include "data/MarkerAlign.h"
#include "data/MarkerCropBox.h"
#include "data/MarkerDetector.h"
#include "data/MarkerScan.h"
#include "data/MarkerTriangulate.h"
#include "data/SparseEdit.h"
#include "i18n/catalog/Marker.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace M = spirula::i18n::msg::marker;

namespace {

void help_row(const char* flags, const std::string& desc, int col = 26) {
    std::string left = std::string("    ") + flags;
    if (spirula::i18n::display_width(left) >= col + 4) {
        std::fprintf(stderr, "%s\n", left.c_str());
        left.clear();
    }
    left = spirula::i18n::pad_to(left, col + 4);
    for (const std::string& line : spirula::i18n::wrap(desc, 86 - col - 4)) {
        std::fprintf(stderr, "%s%s\n", left.c_str(), line.c_str());
        left.assign((size_t)col + 4, ' ');
    }
}

void usage() {
    const std::string prog = app::program_name();
    std::fprintf(stderr, "%s marker -- %s\n\n", prog.c_str(), M::tagline.get());
    std::fprintf(stderr, "    %s marker detect <dataset> [options]\n", prog.c_str());
    std::fprintf(stderr, "    %s marker align <dataset> --origin <id> --x-axis <id> --xy-plane <id> [options]\n", prog.c_str());
    std::fprintf(stderr, "    %s marker crop <dataset> [options]\n", prog.c_str());
    std::fprintf(stderr, "    %s marker auto <dataset> --origin <id> --x-axis <id> --xy-plane <id> [options]\n", prog.c_str());
    std::fprintf(stderr, "\n");
    for (const std::string& l : spirula::i18n::wrap(M::usage_about.get(), 80))
        std::fprintf(stderr, "    %s\n", l.c_str());
    std::fprintf(stderr, "\nOptions:\n");
    help_row("--family <type>", M::family_help.get());
    help_row("--decimate <1..8>", M::marker_decimate_help.get());
    help_row("--threads <n>", M::threads_help.get());
    help_row("--origin <id>", "tag ID for origin (0, 0, 0)");
    help_row("--x-axis <id>", "tag ID defining the positive X axis");
    help_row("--xy-plane <id>", "tag ID defining the XY plane (+Y side)");
    help_row("--dist-x <m>", "physical distance in metres along origin->X (sets metric scale)");
    help_row("--pad <m>", "bounding box XY margin around markers in metres (default: 0.10)");
    help_row("--z-min <m>", "bounding box bottom relative to marker plane (default: -0.05)");
    help_row("--z-max <m>", "bounding box top height relative to marker plane (default: 1.00)");
    help_row("--roi-name <name>", "name of ROI JSON file (default: marker_crop.json)");
    help_row("--splat <in.ply>", "trained Gaussian splat file to align and transform");
    help_row("--out <out.ply>", "output destination for transformed splat file");
}

}  // namespace

int spirula_marker_main(int argc, char** argv) {
    using namespace spirula;

    if (argc < 2) { usage(); return 1; }
    std::string action = argv[1];
    if (action == "--help" || action == "-h") { usage(); return 0; }

    if (action != "detect" && action != "align" && action != "crop" && action != "auto") {
        std::fprintf(stderr, "unknown action '%s'\n\n", action.c_str());
        usage();
        return 1;
    }

    std::string dataset_dir;
    MarkerFamily family = MarkerFamily::Auto;
    int origin_id = -1, x_axis_id = -1, xy_plane_id = -1;
    double dist_x = 0.0;
    double pad_xy = 0.10, z_min = -0.05, z_max = 1.00;
    std::string roi_name = "marker_crop.json";
    std::string splat_in, splat_out;
    double decimate = 1.0;
    int nthreads = 4;

    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s requires a value\n", a.c_str());
                std::exit(2);
            }
            return argv[++i];
        };

        if (a == "--family") {
            const std::string fam = next();
            if (!parse_marker_family(fam, family)) {
                std::fprintf(stderr, "unknown marker family '%s'\n", fam.c_str());
                return 2;
            }
        } else if (a == "--origin") { origin_id = std::atoi(next()); }
        else if (a == "--x-axis") { x_axis_id = std::atoi(next()); }
        else if (a == "--xy-plane") { xy_plane_id = std::atoi(next()); }
        else if (a == "--dist-x") { dist_x = std::atof(next()); }
        else if (a == "--pad" || a == "--crop-pad") { pad_xy = std::atof(next()); }
        else if (a == "--z-min" || a == "--crop-zmin") { z_min = std::atof(next()); }
        else if (a == "--z-max" || a == "--crop-zmax") { z_max = std::atof(next()); }
        else if (a == "--roi-name") { roi_name = next(); }
        else if (a == "--splat") { splat_in = next(); }
        else if (a == "--out") { splat_out = next(); }
        else if (a == "--decimate") { decimate = std::atof(next()); }
        else if (a == "--threads") { nthreads = std::atoi(next()); }
        else if (!a.empty() && a[0] == '-') {
            std::fprintf(stderr, "unknown option '%s'\n", a.c_str());
            usage();
            return 2;
        } else if (dataset_dir.empty()) {
            dataset_dir = a;
        }
    }

    if (dataset_dir.empty()) {
        std::fprintf(stderr, "error: dataset directory must be specified\n\n");
        usage();
        return 1;
    }

    const std::string resolved = resolve_sparse_dir(dataset_dir);
    if (resolved.empty() && !fs::exists(dataset_dir)) {
        std::fprintf(stderr, "%s\n", spirula::i18n::format(M::err_dataset_invalid, {dataset_dir}).c_str());
        return 1;
    }

    DatasetParserConfig dcfg;
    dcfg.require_image_files = false;
    const ParsedDataset ds = parse_dataset(dataset_dir, dcfg, "");

    std::printf("%s\n", spirula::i18n::format(M::detecting_markers, {(long long)ds.image_filenames.size()}).c_str());
    auto scan = scan_markers(ds, family, decimate, nthreads);
    std::vector<Marker3D>& markers = scan.markers;

    std::printf("%s\n", spirula::i18n::format(M::marker_result,
        {(long long)scan.images, (long long)scan.unreadable, (long long)scan.observations,
         (long long)markers.size(), marker_family_name(scan.family)}).c_str());

    std::printf("%s\n", spirula::i18n::format(M::triangulated_summary, {(long long)markers.size()}).c_str());

    std::printf("--------------------------------------------------------------------------------\n");
    std::printf("  ID  | Views | Reproj Err (px) | 3D Center (x, y, z)                | Normal   \n");
    std::printf("--------------------------------------------------------------------------------\n");
    for (const auto& m : markers) {
        std::printf("  %-4d|  %-5d|  %-15.2f| (%7.3f, %7.3f, %7.3f) | (%5.2f, %5.2f, %5.2f)\n",
            m.id, m.num_views, m.reproj_error_px,
            m.center[0], m.center[1], m.center[2],
            m.normal[0], m.normal[1], m.normal[2]);
    }
    std::printf("--------------------------------------------------------------------------------\n");

    const std::string mjson_path = dataset_dir + "/markers.json";
    if (markers.empty()) return 1;
    if (!save_markers_json(mjson_path, markers)) {
        std::fprintf(stderr, "%s\n", spirula::i18n::format(M::marker_write_failed,
            {mjson_path}).c_str());
        return 1;
    }

    if (action == "detect") return 0;

    Sim3 T;
    bool has_transform = false;

    if (action == "align" || action == "auto") {
        MarkerSystemConfig cfg;
        cfg.family = family;
        cfg.three_markers.origin_id = origin_id;
        cfg.three_markers.x_axis_id = x_axis_id;
        cfg.three_markers.xy_plane_id = xy_plane_id;
        cfg.three_markers.dist_x = dist_x;

        if (!cfg.has_three_markers()) {
            std::fprintf(stderr, "error: --origin, --x-axis and --xy-plane IDs must all be specified\n");
            return 1;
        }

        std::printf("%s\n", spirula::i18n::format(M::aligning_system,
            {(long long)origin_id, (long long)x_axis_id, (long long)xy_plane_id}).c_str());

        if (!solve_marker_system(markers, cfg, T)) {
            std::fprintf(stderr, "%s\n", M::err_solve_failed.get());
            return 1;
        }

        has_transform = true;
        char scale_buf[32], tx[32], ty[32], tz[32];
        std::snprintf(scale_buf, sizeof scale_buf, "%.6f", T.s);
        std::snprintf(tx, sizeof tx, "%.3f", T.t[0]);
        std::snprintf(ty, sizeof ty, "%.3f", T.t[1]);
        std::snprintf(tz, sizeof tz, "%.3f", T.t[2]);
        std::printf("%s\n", spirula::i18n::format(M::align_success, {scale_buf, tx, ty, tz}).c_str());

        std::string err;
        if (!apply_marker_alignment(dataset_dir, T, err)) {
            std::fprintf(stderr, "warning: could not write sparse alignment: %s\n", err.c_str());
        }

        if (!splat_in.empty()) {
            const std::string out_path = splat_out.empty() ? splat_in : splat_out;
            if (apply_marker_alignment_splat(splat_in, out_path, T, err)) {
                std::printf("%s\n", spirula::i18n::format(M::apply_splat_success, {out_path}).c_str());
            } else {
                std::fprintf(stderr, "error applying alignment to splat: %s\n", err.c_str());
            }
        }
    }

    if (action == "crop" || action == "auto") {
        std::vector<Marker3D> aligned_markers = markers;
        if (has_transform) {
            for (auto& m : aligned_markers) {
                double p[3];
                T.apply(m.center, p);
                m.center[0] = p[0]; m.center[1] = p[1]; m.center[2] = p[2];
                for (int k = 0; k < 4; k++) {
                    double c[3];
                    T.apply(m.corners[k], c);
                    m.corners[k][0] = c[0]; m.corners[k][1] = c[1]; m.corners[k][2] = c[2];
                }
            }
        }

        MarkerCropOptions opt;
        opt.pad_xy = pad_xy;
        opt.z_min = z_min;
        opt.z_max = z_max;

        const RoiDocument doc = compute_marker_crop_box(aligned_markers, opt);
        std::string crop_path, err;
        if (save_marker_crop_roi(dataset_dir, doc, roi_name, crop_path, err)) {
            std::printf("%s\n", spirula::i18n::format(M::crop_success, {crop_path}).c_str());
        } else {
            std::fprintf(stderr, "error saving crop box ROI: %s\n", err.c_str());
            return 1;
        }
    }

    return 0;
}

