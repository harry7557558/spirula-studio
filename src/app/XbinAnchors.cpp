#include "app/XbinAnchors.h"

#include "app/FfmpegVideo.h"
#include "app/LidarDataset.h"
#include "app/gui/Subprocess.h"
#include "data/XbinReader.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>

namespace app::lidar {
namespace {

namespace fs = std::filesystem;

constexpr int kColmapOpenCV = 4;
constexpr int kColmapOpenCVFisheye = 5;
constexpr const char* kDecodedPrefix = "decoded_";

// By the frame counter, which every camera shares: one instant has one name in
// each camera folder, which is what a rig groups on.
std::string frame_name(int64_t counter) {
    char name[32];
    std::snprintf(name, sizeof name, "%06lld.jpg", (long long)counter);
    return name;
}

XbinCameraPick pick_of(const std::vector<XbinCameraPick>& picks, size_t k) {
    return k < picks.size() ? picks[k] : XbinCameraPick{};
}

bool all_there(const fs::path& dir, const std::vector<int64_t>& counters) {
    std::error_code ec;
    for (int64_t n : counters)
        if (fs::file_size(dir / frame_name(n), ec) == 0 || ec) return false;
    return true;
}

void remove_decoded(const fs::path& dir) {
    std::error_code ec;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
        if (it->path().filename().string().rfind(kDecodedPrefix, 0) == 0) fs::remove(it->path(), ec);
}

// A folder filled at another rate is still a dataset input: what it keeps of
// that run would be imported beside this one's frames.
void remove_others(const fs::path& dir, const std::vector<int64_t>& counters) {
    std::set<std::string> keep;
    for (int64_t n : counters) keep.insert(frame_name(n));
    std::error_code ec;
    std::vector<fs::path> gone;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
        if (it->is_regular_file(ec) && !keep.count(it->path().filename().string()))
            gone.push_back(it->path());
    for (const fs::path& p : gone) fs::remove(p, ec);
}

// Every frame is intra-coded and carries its own parameter sets, so their
// concatenation is one Annex B stream that decodes frame for frame.
bool decode(const std::string& ffmpeg_exe, const fs::path& stream, const fs::path& dir,
            const std::vector<int64_t>& counters, const std::atomic<bool>* cancel,
            const std::function<void(int64_t)>& written) {
    static const bool fps_mode = ffmpeg_has_fps_mode(ffmpeg_exe);
    const std::string pattern = (dir / (std::string(kDecodedPrefix) + "%06d.jpg")).string();
    const std::vector<std::string> argv = {
        ffmpeg_exe, "-hide_banner", "-loglevel", "error", "-y", "-f", "h264", "-i", stream.string(),
        fps_mode ? "-fps_mode" : "-vsync", "passthrough", "-q:v", "2", "-start_number", "0", pattern};
    std::string last;
    std::atomic<bool> never{false};
    auto count = [&] {
        int64_t n = 0;
        std::error_code e;
        for (fs::directory_iterator it(dir, e), end; !e && it != end; it.increment(e))
            n += it->path().filename().string().rfind(kDecodedPrefix, 0) == 0;
        written(n);
    };
    const int code = gui::run_process(
        argv, "", [&](const std::string& l) { if (!l.empty()) last = l; },
        cancel ? *cancel : never, count);
    std::error_code ec;
    if (code == gui::kCancelled) {
        remove_decoded(dir);
        return false;
    }
    if (code != 0) {
        remove_decoded(dir);
        throw std::runtime_error("ffmpeg could not decode " + stream.string() +
                                 (last.empty() ? "" : ": " + last));
    }
    // One more than fed would shift every frame onto the next one's pose.
    char extra[32];
    std::snprintf(extra, sizeof extra, "%s%06zu.jpg", kDecodedPrefix, counters.size());
    if (fs::exists(dir / extra, ec)) {
        remove_decoded(dir);
        throw std::runtime_error("ffmpeg decoded more than the " + std::to_string(counters.size()) +
                                 " frames of " + stream.string());
    }
    for (size_t i = 0; i < counters.size(); i++) {
        char name[32];
        std::snprintf(name, sizeof name, "%s%06zu.jpg", kDecodedPrefix, i);
        if (!fs::exists(dir / name, ec)) {
            remove_decoded(dir);
            throw std::runtime_error("ffmpeg decoded " + std::to_string(i) + " of " +
                                     std::to_string(counters.size()) + " frames of " + stream.string());
        }
        fs::rename(dir / name, dir / frame_name(counters[i]));
    }
    return true;
}

// Camera `k`'s frames that `keep` takes, as <counter>.jpg in `dir` and nothing
// else there, with their times; false when `cancel` stopped it. Frames already
// there are reused.
bool decode_camera(spirula::xbin::Reader& reader, size_t k, const fs::path& dir,
                   const std::function<bool(int64_t)>& keep, const std::string& ffmpeg_exe,
                   const std::atomic<bool>* cancel, const std::function<void(int64_t)>& written,
                   std::vector<int64_t>& counters, std::vector<int64_t>& times) {
    std::error_code ec;
    fs::create_directories(dir, ec);
    // Beside the camera's folder, not in it: that folder is a dataset input.
    const fs::path stream = dir.parent_path() / (dir.filename().string() + ".h264");
    counters.clear();
    times.clear();
    std::set<int64_t> seen;
    {
        std::ofstream f(stream, std::ios::binary | std::ios::trunc);
        reader.read_frames(k, [&](const spirula::xbin::Frame& fr) {
            // Names come from the counter: a repeat would overwrite a frame.
            if (!seen.insert(fr.counter).second)
                throw std::runtime_error(reader.cameras()[k].name + ": frame counter " +
                                         std::to_string(fr.counter) +
                                         " repeats, a layout this reader does not know");
            if (keep(fr.time_us)) {
                f.write((const char*)fr.h264.data(), (std::streamsize)fr.h264.size());
                counters.push_back(fr.counter);
                times.push_back(fr.time_us);
            }
            return !(cancel && cancel->load());
        });
        if (!f) throw std::runtime_error("cannot write " + stream.string());
    }
    written(0);
    const bool decoded = (cancel && cancel->load()) ? false
                         : all_there(dir, counters) ? true
                                                    : decode(ffmpeg_exe, stream, dir, counters, cancel, written);
    fs::remove(stream, ec);
    if (decoded) remove_others(dir, counters);
    return decoded;
}

}  // namespace

std::vector<XbinCameraFolder> extract_xbin_anchors(const std::string& xbin_path,
                                                   const std::string& images_dir,
                                                   const std::string& subdir,
                                                   const std::string& anchors_path,
                                                   const std::vector<XbinCameraPick>& picks,
                                                   bool place_only,
                                                   const std::string& ffmpeg_exe,
                                                   const std::atomic<bool>* cancel,
                                                   const XbinProgress& progress) {
    spirula::xbin::Reader reader(xbin_path);
    std::vector<Anchor> anchors;
    std::error_code ec;
    if (fs::exists(anchors_path, ec)) anchors = read_anchors(anchors_path);
    const std::string prefix = subdir + "/";
    anchors.erase(std::remove_if(anchors.begin(), anchors.end(),
                                 [&](const Anchor& a) { return a.name.rfind(prefix, 0) == 0; }),
                  anchors.end());
    std::vector<XbinCameraFolder> out;
    const std::vector<spirula::xbin::Camera>& cams = reader.cameras();
    int64_t total = 0, before = 0;
    for (size_t k = 0; k < cams.size(); k++)
        if (const XbinCameraPick p = pick_of(picks, k); p.taken)
            total += frames_at_rate(cams[k].frames, cams[k].seconds, p.fps);
    auto report = [&](const std::string& camera, int64_t done) {
        if (progress) progress(camera, std::min(before + done, total), total);
    };
    for (size_t k = 0; k < cams.size(); k++) {
        const spirula::xbin::Camera& cam = cams[k];
        const XbinCameraPick pick = pick_of(picks, k);
        const std::string rel = subdir + "/" + cam.name;
        const fs::path dir = fs::path(images_dir) / rel;
        if (!pick.taken) {
            fs::remove_all(dir, ec);
            continue;
        }
        double R[9], c[3];
        FrameRate rate(pick.fps);
        // Before the trajectory starts there is no pose to give a frame.
        auto keep = [&](int64_t t) { return reader.camera_pose(k, t, R, c) && rate.keep(t); };
        std::vector<int64_t> counters, times;
        if (!decode_camera(reader, k, dir, keep, ffmpeg_exe, cancel,
                           [&](int64_t n) { report(cam.name, n); }, counters, times))
            return out;
        before += (int64_t)times.size();
        report(cam.name, 0);
        XbinCameraFolder folder;
        folder.rel = rel;
        folder.fisheye = cam.fisheye;
        folder.focal_factor = cam.params[0] / cam.width;
        folder.params.assign(cam.params, cam.params + 8);
        folder.width = cam.width;
        for (size_t i = 0; i < times.size(); i++) {
            reader.camera_pose(k, times[i], R, c);
            Anchor a;
            a.name = rel + "/" + frame_name(counters[i]);
            a.scan = xbin_path;
            a.rendered = place_only;
            for (int i = 0; i < 9; i++) a.R[i] = R[i];
            a.centre = {c[0], c[1], c[2]};
            a.camera_model = cam.fisheye ? kColmapOpenCVFisheye : kColmapOpenCV;
            a.width = cam.width;
            a.height = cam.height;
            a.params.assign(cam.params, cam.params + 8);
            anchors.push_back(a);
            folder.frames++;
        }
        out.push_back(folder);
    }
    write_anchors(anchors_path, anchors);
    return out;
}

void extract_xbin_samples(const std::string& xbin_path, const std::string& dir,
                          const std::vector<XbinCameraPick>& picks, const std::string& camera,
                          int per_camera, const std::string& ffmpeg_exe,
                          const std::atomic<bool>* cancel) {
    spirula::xbin::Reader reader(xbin_path);
    const std::vector<spirula::xbin::Camera>& cams = reader.cameras();
    for (size_t k = 0; k < cams.size(); k++) {
        if (cams[k].name != camera) continue;
        const XbinCameraPick pick = pick_of(picks, k);
        const int64_t kept = frames_at_rate(cams[k].frames, cams[k].seconds, pick.fps);
        const int64_t step = std::max<int64_t>(1, (kept + per_camera - 1) / std::max(1, per_camera));
        double R[9], c[3];
        FrameRate rate(pick.fps);
        int64_t seen = 0;
        auto keep = [&](int64_t t) {
            return reader.camera_pose(k, t, R, c) && rate.keep(t) && seen++ % step == 0;
        };
        std::vector<int64_t> counters, times;
        if (!decode_camera(reader, k, fs::path(dir) / cams[k].name, keep, ffmpeg_exe, cancel,
                           [](int64_t) {}, counters, times))
            return;
    }
}

}  // namespace app::lidar
