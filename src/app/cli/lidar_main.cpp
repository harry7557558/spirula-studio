// spirula lidar -- a reconstruction aligned with a laser scan and given its
// geometry (app/LidarDataset.h). The dataset screen's Align step runs this as a
// child process and reads its progress lines back, so every line is a Msg.

#include "app/LidarDataset.h"
#include "app/Tools.h"
#include "data/PointCloudFile.h"
#include "i18n/Message.h"
#include "i18n/catalog/Data.h"
#include "i18n/catalog/Lidar.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <string>

namespace {

namespace L = spirula::i18n::msg::lidar;
using spirula::i18n::format;

constexpr long long kDefaultPoints = 500000;
constexpr int kDefaultTrackCap = 12;

void help_row(const char* flags, const std::string& text, int col = 22) {
    std::string left = std::string("    ") + flags;
    if (spirula::i18n::display_width(left) >= col + 4) {
        std::fprintf(stderr, "%s\n", left.c_str());
        left.clear();
    }
    left = spirula::i18n::pad_to(left, col + 4);
    for (const std::string& line : spirula::i18n::wrap(text, 86 - col - 4)) {
        std::fprintf(stderr, "%s%s\n", left.c_str(), line.c_str());
        left.assign((size_t)col + 4, ' ');
    }
}

void usage() {
    const std::string prog = app::program_name();
    std::fprintf(stderr, "%s -- %s\n\n", prog.c_str(), L::tagline.get());
    std::fprintf(stderr, "    %s <dataset folder> --cloud <scan> [options]\n", prog.c_str());
    std::fprintf(stderr, "    %s --extract <scan.e57> <dataset folder> [<subfolder>]\n",
                 prog.c_str());
    std::fprintf(stderr, "    %s --info <scan>\n\n", prog.c_str());
    for (const std::string& l : spirula::i18n::wrap(L::usage_about.get(), 80))
        std::fprintf(stderr, "    %s\n", l.c_str());
    std::fprintf(stderr, "\n");
    for (const std::string& l : spirula::i18n::wrap(L::usage_extract.get(), 80))
        std::fprintf(stderr, "    %s\n", l.c_str());
    std::fprintf(stderr, "\n%s\n", L::head_options.get());
    help_row("--cloud <file>", L::opt_cloud.get());
    help_row("--mode <m>", L::opt_mode.get());
    help_row("--points <n>|all", format(L::opt_points, {kDefaultPoints}));
    help_row("--track-cap <n>", format(L::opt_track_cap, {(long long)kDefaultTrackCap}));
    help_row("--no-depth", L::opt_no_depth.get());
    help_row("--no-gap-points", L::opt_no_gaps.get());
    help_row("--anchors <file>", L::opt_anchors.get());
    help_row("--image-dir <dir>", L::opt_image_dir.get());
    help_row("--flip-masks", L::opt_flip_masks.get());
    help_row("--overwrite", L::opt_overwrite.get());
    help_row("--scanner-poses", L::opt_scanner_poses.get());
    help_row("--render-anchors", L::opt_render.get());
    help_row("--info <file>", L::opt_info.get());
}

void say(const std::string& line) {
    std::printf("%s\n", line.c_str());
    std::fflush(stdout);
}

int info(const std::string& path) {
    spirula::cloud::Reader r(path);
    static const char* kFormat[] = {"E57", "LAS", "PLY"};
    say(format(L::info_cloud, {path, kFormat[(int)r.info().format], (long long)r.info().points,
                               r.info().has_color ? L::word_yes.get() : L::word_no.get(),
                               (long long)r.info().stations.size()}));
    return 0;
}

}  // namespace

int spirula_lidar_main(int argc, char** argv) {
    app::set_program_name(argc > 0 ? argv[0] : nullptr, "spirula lidar");
    app::lidar::DatasetOptions opt;
    opt.seed_points = kDefaultPoints;
    opt.track_cap = kDefaultTrackCap;
    const char* error_word = spirula::i18n::msg::data::word_error.get();
    auto bad = [&](const std::string& flag, const std::string& v) {
        std::fprintf(stderr, "%s\n", format(L::err_bad_value, {flag, v}).c_str());
        return 2;
    };
    try {
        for (int i = 1; i < argc; ++i) {
            const std::string a = argv[i];
            const bool has_value = i + 1 < argc;
            if (a == "--help" || a == "-h") { usage(); return 0; }
            if (a == "--info" && has_value) return info(argv[i + 1]);
            if (a == "--extract" && i + 2 < argc) {
                const std::string ds = argv[i + 2];
                const std::string sub = i + 3 < argc
                    ? argv[i + 3] : std::filesystem::path(argv[i + 1]).stem().string();
                const app::lidar::ExtractedPhotos x = app::lidar::extract_e57_anchors(
                    argv[i + 1], (std::filesystem::path(ds) / "images").string(), sub,
                    (std::filesystem::path(ds) / "lidar" / "anchors.json").string());
                say(format(L::extracted, {(long long)(x.pinhole + x.panorama), (long long)x.pinhole,
                                          (long long)x.panorama}));
                return 0;
            }
            if (a == "--render-anchors" && i + 2 < argc) {
                std::vector<std::string> clouds;
                std::string ds;
                for (int k = i + 1; k < argc; k++) {
                    const std::string v = argv[k];
                    if (spirula::cloud::is_cloud_path(v)) clouds.push_back(v);
                    else ds = v;
                }
                const int64_t n = app::lidar::render_anchor_views(
                    clouds, (std::filesystem::path(ds) / "images").string(), "scan_views",
                    (std::filesystem::path(ds) / "lidar" / "anchors.json").string(), say);
                say(format(L::rendered_views, {(long long)n}));
                return n > 0 ? 0 : 1;
            }
            if (a == "--cloud" && has_value) opt.clouds.push_back(argv[++i]);
            else if (a == "--anchors" && has_value) opt.anchors = argv[++i];
            else if (a == "--image-dir" && has_value) opt.image_dir = argv[++i];
            else if (a == "--no-depth") opt.depth_maps = false;
            else if (a == "--no-gap-points") opt.sfm_points_in_gaps = false;
            else if (a == "--flip-masks") opt.flip_masks = true;
            else if (a == "--overwrite") opt.overwrite = true;
            else if (a == "--scanner-poses") opt.scanner_poses_only = true;
            else if (a == "--mode" && has_value) {
                const std::string v = argv[++i];
                if (v == "auto") opt.mode = app::lidar::AlignMode::Auto;
                else if (v == "anchors") opt.mode = app::lidar::AlignMode::Anchors;
                else if (v == "keep") opt.mode = app::lidar::AlignMode::Keep;
                else if (v == "refine") opt.mode = app::lidar::AlignMode::Refine;
                else return bad(a, v);
            } else if (a == "--points" && has_value) {
                const char* v = argv[++i];
                if (std::strcmp(v, "all") == 0) { opt.all_points = true; continue; }
                char* end = nullptr;
                const long long n = std::strtoll(v, &end, 10);
                if (end == v || *end != '\0' || n < 0) return bad(a, v);
                opt.seed_points = n;
            } else if (a == "--track-cap" && has_value) {
                const char* v = argv[++i];
                char* end = nullptr;
                const long n = std::strtol(v, &end, 10);
                if (end == v || *end != '\0' || n < 1 || n > 64) return bad(a, v);
                opt.track_cap = (int)n;
            } else if (!a.empty() && a[0] == '-') {
                std::fprintf(stderr, "%s\n\n", format(L::err_unknown_option, {a}).c_str());
                usage();
                return 2;
            } else if (spirula::cloud::is_cloud_path(a) || spirula::cloud::is_laz_path(a)) {
                opt.clouds.push_back(a);
            } else if (opt.dataset.empty()) {
                opt.dataset = a;
            } else {
                usage();
                return 2;
            }
        }
        if (opt.dataset.empty() || opt.clouds.empty()) {
            std::fprintf(stderr, "%s\n", format(L::err_no_input, {app::program_name()}).c_str());
            return 2;
        }
        const app::lidar::DatasetResult r = app::lidar::write_lidar_dataset(opt, say);
        if (r.cancelled) return 1;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s %s\n", error_word, e.what());
        return 1;
    }
    return 0;
}
