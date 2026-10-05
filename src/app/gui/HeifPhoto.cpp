// HeifPhoto.cpp -- see HeifPhoto.h.

#include "app/gui/HeifPhoto.h"

#include "app/FrameMask.h"
#include "app/gui/Subprocess.h"
#include "i18n/catalog/Log.h"
#ifdef SS_HAVE_VIDEO
#include "video/Video.h"
#endif

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <thread>

namespace fs = std::filesystem;
namespace lmsg = spirula::i18n::msg::log;

namespace gui {

namespace {

// "8.1.2-full_build" -> 801, "n7.1" -> 701. 0 for anything else, such as a
// build from git ("N-118000-g..."), which is newer than any release refused.
int version_number(const std::string& v) {
    size_t i = (!v.empty() && v[0] == 'n') ? 1 : 0;
    if (i >= v.size() || !std::isdigit((unsigned char)v[i])) return 0;
    const int major = std::atoi(v.c_str() + i);
    const size_t dot = v.find('.', i);
    const int minor = dot == std::string::npos ? 0 : std::atoi(v.c_str() + dot + 1);
    return major * 100 + minor;
}

// The word after "ffmpeg version", asked once per executable.
std::string ffmpeg_version(const std::string& exe, const std::atomic<bool>& cancel) {
    static std::mutex mu;
    static std::map<std::string, std::string> seen;
    {
        std::lock_guard<std::mutex> lk(mu);
        auto it = seen.find(exe);
        if (it != seen.end()) return it->second;
    }
    std::string first;
    const int rc = run_process({exe, "-version"}, "",
                               [&first](const std::string& line) {
                                   if (first.empty()) first = line;
                               },
                               cancel);
    static const std::string kHead = "ffmpeg version ";
    std::string v;
    if (first.rfind(kHead, 0) == 0) {
        v = first.substr(kHead.size());
        v = v.substr(0, v.find(' '));
    }
    // A cancelled or failed probe is asked again next time, not remembered.
    if (rc != 0) return v;
    std::lock_guard<std::mutex> lk(mu);
    seen[exe] = v;
    return v;
}

}  // namespace

bool is_heif_path(const std::string& path) {
    std::string e = fs::path(path).extension().string();
    for (char& c : e) c = (char)std::tolower((unsigned char)c);
    return e == ".heic" || e == ".heif" || e == ".hif";
}

bool decode_heif_builtin(const std::string& path, int& w, int& h,
                         std::vector<uint8_t>& rgb, std::vector<uint8_t>* exif,
                         std::string& error) {
#ifdef SS_HAVE_VIDEO
    nn::Image im;
    if (!video::decode_heif(path, im, exif, error)) return false;
    w = im.width;
    h = im.height;
    rgb = std::move(im.data);
    return true;
#else
    (void)path; (void)w; (void)h; (void)rgb; (void)exif;
    error = "built without the in-process decoder (-DSS_ENABLE_PATENTED=OFF)";
    return false;
#endif
}

bool ffmpeg_heif_to_jpeg(const std::string& ffmpeg_exe, const std::string& heif,
                         const std::string& jpeg, const std::atomic<bool>& cancel,
                         std::string& error) {
    if (!command_exists(ffmpeg_exe)) {
        error = spirula::i18n::format(lmsg::err_ffmpeg_missing, {ffmpeg_exe});
        return false;
    }
    // Tiled HEIF arrived in 7.0; before it the grid is not put together, and a
    // lone tile is a size nothing downstream can tell from a real photo's.
    const std::string version = ffmpeg_version(ffmpeg_exe, cancel);
    const int n = version_number(version);
    if (n > 0 && n < 700) {
        error = spirula::i18n::format(lmsg::err_ffmpeg_heif_too_old,
                                      {ffmpeg_exe, version});
        return false;
    }
    std::error_code ec;
    fs::remove(jpeg, ec);
    std::string last;
    // One at a time: ffmpeg 8.1 commits 2.6 GB for a 24 MP iPhone grid, and
    // two at once already failed with ENOMEM on a 16 GB machine.
    static std::mutex one_at_a_time;
    std::lock_guard<std::mutex> lk(one_at_a_time);
    // The muxer is named, not guessed from `jpeg`: callers write to a
    // temporary name ("a.jpg.part") that ffmpeg would refuse.
    const int rc = run_process(
        {ffmpeg_exe, "-nostdin", "-y", "-v", "error", "-i", heif, "-frames:v", "1",
         "-q:v", "2", "-f", "mjpeg", jpeg},
        "",
        [&last](const std::string& line) {
            if (!line.empty()) last = line;
        },
        cancel);
    if (rc == 0 && fs::exists(jpeg, ec) && fs::file_size(jpeg, ec) > 0) return true;
    fs::remove(jpeg, ec);
    if (rc == kCancelled) {
        error = lmsg::err_cancelled.get();
    } else {
        if (last.empty()) last = "ffmpeg exited with " + std::to_string(rc);
        error = spirula::i18n::format(lmsg::err_heif_convert_failed, {heif, last});
    }
    return false;
}

bool heif_size(const std::string& path, int& w, int& h) {
#ifdef SS_HAVE_VIDEO
    std::string error;
    return video::probe_heif(path, w, h, error);
#else
    (void)path; (void)w; (void)h;
    return false;
#endif
}

bool load_heif(const std::string& path, bool builtin, const std::string& ffmpeg_exe,
               int& w, int& h, std::vector<uint8_t>& rgb,
               const std::atomic<bool>& cancel, std::string& error) {
    if (builtin && decode_heif_builtin(path, w, h, rgb, nullptr, error)) return true;
    const fs::path tmp =
        fs::temp_directory_path() /
        ("spirula-heic-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
         "-" + std::to_string(std::hash<std::thread::id>()(std::this_thread::get_id())) +
         ".jpg");
    bool ok = ffmpeg_heif_to_jpeg(ffmpeg_exe, path, tmp.string(), cancel, error);
    if (ok && !app::load_rgb(tmp.string(), w, h, rgb)) {
        error = spirula::i18n::format(lmsg::err_heif_convert_failed,
                                      {path, "ffmpeg wrote no readable JPEG"});
        ok = false;
    }
    std::error_code ec;
    fs::remove(tmp, ec);
    return ok;
}

}  // namespace gui
