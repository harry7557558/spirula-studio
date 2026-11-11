#include "app/gui/LidarStep.h"

#include "app/AppPaths.h"
#include "app/LidarDataset.h"
#include "app/gui/Subprocess.h"
#include "data/PointCloudFile.h"
#include "i18n/Locale.h"
#include "i18n/catalog/Lidar.h"
#include "i18n/catalog/Log.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <stdexcept>

namespace fs = std::filesystem;
namespace lmsg = spirula::i18n::msg::log;
namespace ldmsg = spirula::i18n::msg::lidar;
using spirula::i18n::format;

namespace gui {
namespace {

constexpr const char* kScanPrefix = "scan_";
constexpr const char* kViewsDir = "scan_views";
// The rendered views' lens: 1024 px square, 90 degrees across.
constexpr const char* kViewsFocal = "512";

std::string trim_right(const std::string& s) {
    size_t n = s.size();
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) n--;
    return s.substr(0, n);
}

std::string folder_name(const std::string& cloud) {
    std::string s = kScanPrefix;
    for (char c : fs::path(cloud).stem().string())
        s += (std::isalnum((unsigned char)c) || c == '-') ? c : '_';
    return s;
}

}  // namespace

std::vector<std::string> scan_photo_subdirs(const std::vector<std::string>& clouds) {
    std::vector<std::string> out;
    std::set<std::string> used;
    for (const std::string& c : clouds) {
        std::string sub = folder_name(c);
        for (int k = 2; used.count(sub); k++) sub = folder_name(c) + "_" + std::to_string(k);
        used.insert(sub);
        out.push_back(sub);
    }
    return out;
}

bool add_scan_photo_inputs(const LidarJob& job, const std::string& workspace,
                           const std::map<std::string, app::FrameStencil>& stencils,
                           std::vector<PrepInput>& inputs, LidarPrep& out, std::string& error) {
    const fs::path root = fs::path(workspace) / "lidar";
    const fs::path anchors = root / "anchors.json";
    std::error_code ec;
    // Written afresh every run, so a scan taken off the list leaves nothing.
    fs::remove(anchors, ec);
    if (!job.enabled() || job.in_frame || !job.scan_photos) return true;
    try {
        const std::vector<std::string> subs = scan_photo_subdirs(job.clouds);
        for (size_t i = 0; i < job.clouds.size(); i++) {
            const std::string& c = job.clouds[i];
            spirula::cloud::Reader r(c);
            if (!r.e57() || r.e57()->images().empty()) continue;
            const std::string& sub = subs[i];
            const app::lidar::ExtractedPhotos x = app::lidar::extract_e57_anchors(
                c, (root / "photos").string(), sub, anchors.string());
            const auto drawn = stencils.find(c);
            auto add = [&](const std::string& rel, bool pinhole) {
                PrepInput in;
                in.path = (root / "photos" / rel).string();
                in.subdir = rel;
                in.camera_model = pinhole ? "pinhole" : "equirectangular";
                in.focal_factor = pinhole ? (float)x.focal_factor : 0.0f;
                // A walk's panoramas are taken in order; a tripod's faces are not.
                in.sequential = !pinhole;
                in.scan = c;
                // Faces and panoramas, when a scan has both, are its two
                // camera folders, each with its own areas.
                if (drawn != stencils.end() && !x.split) {
                    in.stencil = drawn->second;
                } else if (drawn != stencils.end()) {
                    static_cast<app::CameraStencil&>(in.stencil) =
                        drawn->second.for_camera(pinhole ? "pinhole" : "panorama");
                }
                inputs.push_back(in);
            };
            if (x.pinhole) add(x.split ? sub + "/pinhole" : sub, true);
            if (x.panorama) add(x.split ? sub + "/panorama" : sub, false);
            out.photos += x.pinhole + x.panorama;
            if (x.pinhole + x.panorama > 0) out.photographed.push_back(c);
        }
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
    return true;
}

bool extract_scan_photos(const std::vector<std::string>& clouds, size_t scan,
                         const std::string& workspace, std::string& dir, std::string& error) {
    if (scan >= clouds.size() || workspace.empty()) return false;
    const fs::path photos = fs::path(workspace) / "lidar" / "photos";
    const std::string sub = scan_photo_subdirs(clouds)[scan];
    const fs::path anchors = fs::path(workspace) / "lidar" / ("preview-" + sub + ".json");
    std::error_code ec;
    try {
        app::lidar::extract_e57_anchors(clouds[scan], photos.string(), sub, anchors.string());
    } catch (const std::exception& e) {
        error = e.what();
        fs::remove(anchors, ec);
        return false;
    }
    fs::remove(anchors, ec);
    dir = (photos / sub).string();
    return true;
}

bool render_scan_views(const LidarJob& job, const std::string& workspace,
                       const std::string& image_dir, RunProgress& prog,
                       const std::atomic<bool>& cancel, LidarPrep& out, std::string& error) {
    std::error_code ec;
    const fs::path rel = fs::weakly_canonical(image_dir, ec)
                             .lexically_relative(fs::weakly_canonical(workspace, ec));
    if (rel.empty() || *rel.begin() == "..") return true;
    // Views of an earlier run would otherwise be reconstructed as photographs.
    fs::remove_all(fs::path(image_dir) / kViewsDir, ec);
    if (!job.enabled() || job.in_frame) return true;
    auto log = [&](const std::string& s) { prog.note(s, false); };
    try {
        const bool shared = app::lidar::scans_share_frame(job.clouds);
        std::vector<std::string> clouds;
        for (const std::string& c : job.clouds)
            if (std::find(out.photographed.begin(), out.photographed.end(), c) ==
                out.photographed.end())
                clouds.push_back(c);
        if (clouds.empty() || (shared && out.photos > 0)) return true;
        const fs::path anchors = fs::path(workspace) / "lidar" / "anchors.json";
        out.views = app::lidar::render_anchor_views(clouds, !shared, image_dir, kViewsDir,
                                                    anchors.string(), log, &cancel);
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
    if (cancel.load()) return false;
    log(format(ldmsg::rendered_views, {(long long)out.views}));
    if (out.views > 0)
        out.sfm_args.insert(out.sfm_args.end(),
                            {"--camera-model", std::string(kViewsDir) + "=pinhole",
                             "--focal", std::string(kViewsDir) + "=" + kViewsFocal});
    return true;
}

bool run_lidar_step(const LidarJob& job, const std::string& dataset,
                    const std::string& image_dir, RunProgress& prog, FilmReel* reel,
                    const std::atomic<bool>& cancel, std::string& error) {
    if (app::exe_path().empty()) {
        error = lmsg::err_no_exe_path.get();
        return false;
    }
    prog.enter(Stage::Align, ldmsg::stage_align.get());
    std::vector<std::string> argv = {
        app::exe_path(), "--lang", spirula::i18n::code(spirula::i18n::current()),
        "lidar", dataset, "--image-dir", image_dir.empty() ? std::string("images") : image_dir,
    };
    for (const std::string& c : job.clouds) {
        argv.push_back("--cloud");
        argv.push_back(c);
    }
    if (!job.mask_dir.empty()) argv.insert(argv.end(), {"--mask-dir", job.mask_dir});
    if (job.in_frame) argv.insert(argv.end(), {"--mode", "keep"});
    if (job.flip_masks) argv.push_back("--flip-masks");
    if (job.scanner_poses_only) argv.push_back("--scanner-poses");
    if (job.overwrite) argv.push_back("--overwrite");
    std::string cmd;
    for (const std::string& a : argv) cmd += (cmd.empty() ? "$ " : " ") + a;
    prog.note(cmd, true);

    const fs::path root(dataset);
    OutputWatch watch(reel, root / "normals", image_dir, root / "depths", /*fresh_only=*/true);
    const int rc = run_process(argv, "", [&](const std::string& raw) {
        const std::string line = trim_right(raw);
        if (line.empty()) return;
        std::vector<std::string> got;
        if (spirula::i18n::scan(ldmsg::progress_maps, line, got) && got.size() >= 2) {
            prog.count(Stage::Align, std::atoll(got[0].c_str()), std::atoll(got[1].c_str()));
            prog.detail(Stage::Align, line);
            watch.poll();
            return;
        }
        prog.note(line, false);
        watch.poll();
    }, cancel);
    watch.poll(/*flush=*/true);
    if (rc == kCancelled) {
        error = lmsg::err_cancelled.get();
        return false;
    }
    if (rc == kSpawnFailed) {
        error = format(ldmsg::err_spawn, {argv[0]});
        return false;
    }
    if (rc != 0) {
        error = ldmsg::err_failed.get();
        return false;
    }
    prog.mark(Stage::Align, StageStatus::Done);
    return true;
}

}  // namespace gui
