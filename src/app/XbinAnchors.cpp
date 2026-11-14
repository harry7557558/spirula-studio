#include "app/XbinAnchors.h"

#include "app/FfmpegVideo.h"
#include "app/LidarDataset.h"
#include "app/gui/Subprocess.h"
#include "data/XbinReader.h"
#include "i18n/catalog/Log.h"

#ifdef SS_HAVE_VIDEO
#include "app/FrameExtract.h"
#include "video/Mp4Writer.h"
#endif

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>

namespace app::lidar {
namespace {

namespace fs = std::filesystem;
namespace lmsg = spirula::i18n::msg::log;

constexpr int kColmapOpenCV = 4;
constexpr int kColmapOpenCVFisheye = 5;

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

// Removed on every way out, a throw too.
struct RemoveOnExit {
    fs::path p;
    ~RemoveOnExit() { std::error_code e; fs::remove_all(p, e); }
};

#ifdef SS_HAVE_VIDEO

// The SPS and PPS NAL units of an Annex B access unit.
std::vector<uint8_t> parameter_sets(const std::vector<uint8_t>& au) {
    std::vector<uint8_t> out;
    size_t at = au.size();
    auto take = [&](size_t end) {
        while (end > at && au[end - 1] == 0) end--;   // a 4-byte start code's first zero
        const int type = at < end ? au[at] & 0x1f : 0;
        if (type != 7 && type != 8) return;
        out.insert(out.end(), {0, 0, 1});
        out.insert(out.end(), au.begin() + (std::ptrdiff_t)at, au.begin() + (std::ptrdiff_t)end);
    };
    for (size_t i = 0; i + 3 <= au.size(); i++)
        if (au[i] == 0 && au[i + 1] == 0 && au[i + 2] == 1) {
            take(i);
            at = i + 3;
            i += 2;
        }
    take(au.size());
    return out;
}

// An MP4, which the in-process decoder and ffmpeg both read as they read a video.
class Stream {
public:
    static constexpr const char* kExtension = ".mp4";
    Stream(const fs::path& path, const spirula::xbin::Camera& cam) : _path(path), _cam(cam) {}

    void add(const std::vector<uint8_t>& au) {
        std::vector<uint8_t> sets = parameter_sets(au);
        if (!_open) {
            std::string error;
            // Frames are matched back by their order; the rate only spaces them.
            if (!_mp4.open(_path.string(), video::Codec::H264, _cam.width, _cam.height, 30.0, false,
                           error))
                throw std::runtime_error(error);
            _mp4.set_parameter_sets(sets);
            _sets = std::move(sets);
            _open = true;
        } else if (sets != _sets) {
            // The file holds the first frame's, for every frame.
            throw std::runtime_error(_cam.name + ": the SPS or PPS changes between frames, a "
                                                 "layout this reader does not know");
        }
        if (!_mp4.write_sample(au.data(), au.size(), true))
            throw std::runtime_error("cannot write " + _path.string());
    }
    void finish() {
        std::string error;
        if (_open && !_mp4.close(error)) throw std::runtime_error(error);
    }

private:
    fs::path _path;
    const spirula::xbin::Camera& _cam;
    video::Mp4Writer _mp4;
    std::vector<uint8_t> _sets;
    bool _open = false;
};

// Through the same decoder choice as a video's frames, named by their place in
// the stream.
bool decode(const XbinDecoder& how, const fs::path& stream, const fs::path& dir,
            const std::vector<int64_t>& counters, const std::atomic<bool>* cancel,
            const std::function<void(int64_t)>& written) {
    // Emptied first: frames a crashed run left there would pass for this one's.
    const RemoveOnExit out{dir.parent_path() / (dir.filename().string() + ".decoded")};
    std::error_code ec;
    fs::remove_all(out.p, ec);
    FrameExtractJob job;
    job.input = stream.string();
    job.image_dir = out.p.string();
    job.device = how.device;
    job.decoder = how.external ? FrameDecoder::Ffmpeg : FrameDecoder::Auto;
    job.ffmpeg_exe = how.ffmpeg_exe;
    job.keep = 0;   // with `skip` 1, every frame
    FrameExtractSinks sinks;
    sinks.cancel = cancel;
    sinks.progress = [&](int64_t n, int64_t) { written(n); };
    FrameExtractStats stats;
    std::string error;
    if (!extract_frames(job, sinks, stats, error)) {
        if (cancel && cancel->load()) return false;
        throw std::runtime_error(error);
    }
    auto name = [&](size_t i) {
        char s[32];
        std::snprintf(s, sizeof s, "%05zu.jpg", i);
        return out.p / s;
    };
    // One more than fed would shift every frame onto the next one's pose.
    if (fs::exists(name(counters.size()), ec))
        throw std::runtime_error("decoded more than the " + std::to_string(counters.size()) +
                                 " frames of " + stream.string());
    for (size_t i = 0; i < counters.size(); i++)
        if (!fs::exists(name(i), ec))
            throw std::runtime_error("decoded " + std::to_string(i) + " of " +
                                     std::to_string(counters.size()) + " frames of " +
                                     stream.string());
    for (size_t i = 0; i < counters.size(); i++) fs::rename(name(i), dir / frame_name(counters[i]));
    return true;
}

#else

// Every frame is intra-coded and carries its own parameter sets, so their
// concatenation is one Annex B stream that decodes frame for frame.
class Stream {
public:
    static constexpr const char* kExtension = ".h264";
    Stream(const fs::path& path, const spirula::xbin::Camera&)
        : _path(path), _f(path, std::ios::binary | std::ios::trunc) {}

    void add(const std::vector<uint8_t>& au) {
        _f.write((const char*)au.data(), (std::streamsize)au.size());
    }
    void finish() {
        if (!_f) throw std::runtime_error("cannot write " + _path.string());
        _f.close();
    }

private:
    fs::path _path;
    std::ofstream _f;
};

constexpr const char* kDecodedPrefix = "decoded_";

void remove_decoded(const fs::path& dir) {
    std::error_code ec;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
        if (it->path().filename().string().rfind(kDecodedPrefix, 0) == 0) fs::remove(it->path(), ec);
}

bool decode(const XbinDecoder& how, const fs::path& stream, const fs::path& dir,
            const std::vector<int64_t>& counters, const std::atomic<bool>* cancel,
            const std::function<void(int64_t)>& written) {
    const std::string& ffmpeg_exe = how.ffmpeg_exe;
    if (!gui::command_exists(ffmpeg_exe))
        throw std::runtime_error(spirula::i18n::format(lmsg::err_ffmpeg_not_found, {ffmpeg_exe}));
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
    if (code == gui::kSpawnFailed)
        throw std::runtime_error(
            spirula::i18n::format(lmsg::err_ffmpeg_unrunnable, {"cannot run " + ffmpeg_exe}));
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

#endif

// Camera `k`'s frames that `keep` takes, as <counter>.jpg in `dir` and nothing
// else there, with their times; false when `cancel` stopped it. Frames already
// there are reused.
bool decode_camera(spirula::xbin::Reader& reader, size_t k, const fs::path& dir,
                   const std::function<bool(int64_t)>& keep, const XbinDecoder& how,
                   const std::atomic<bool>* cancel, const std::function<void(int64_t)>& written,
                   std::vector<int64_t>& counters, std::vector<int64_t>& times) {
    std::error_code ec;
    fs::create_directories(dir, ec);
    // Beside the camera's folder, not in it: that folder is a dataset input. As
    // big as the camera's whole stream.
    const RemoveOnExit stream{dir.parent_path() / (dir.filename().string() + Stream::kExtension)};
    counters.clear();
    times.clear();
    std::set<int64_t> seen;
    {
        Stream f(stream.p, reader.cameras()[k]);
        reader.read_frames(k, [&](const spirula::xbin::Frame& fr) {
            // Names come from the counter: a repeat would overwrite a frame.
            if (!seen.insert(fr.counter).second)
                throw std::runtime_error(reader.cameras()[k].name + ": frame counter " +
                                         std::to_string(fr.counter) +
                                         " repeats, a layout this reader does not know");
            if (keep(fr.time_us)) {
                f.add(fr.h264);
                counters.push_back(fr.counter);
                times.push_back(fr.time_us);
            }
            return !(cancel && cancel->load());
        });
        f.finish();
    }
    written(0);
    const bool decoded = (cancel && cancel->load()) ? false
                         : all_there(dir, counters) ? true
                                                    : decode(how, stream.p, dir, counters, cancel, written);
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
                                                   const XbinDecoder& decoder,
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
        if (!decode_camera(reader, k, dir, keep, decoder, cancel,
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
                          int per_camera, const XbinDecoder& decoder,
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
        if (!decode_camera(reader, k, fs::path(dir) / cams[k].name, keep, decoder, cancel,
                           [](int64_t) {}, counters, times))
            return;
    }
}

}  // namespace app::lidar
