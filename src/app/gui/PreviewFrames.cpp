// PreviewFrames.cpp -- see PreviewFrames.h.

#include "app/gui/PreviewFrames.h"

#include "app/FrameMask.h"
#include "app/gui/DatasetPrep.h"
#include "app/gui/Subprocess.h"
#include "i18n/catalog/Dataset.h"

#ifdef SS_HAVE_VIDEO
#include "app/FrameExtract.h"
#include "video/Video.h"
#endif

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;
namespace dmsg = spirula::i18n::msg::dataset;

namespace gui {

namespace {

bool is_image_file(const fs::path& p) {
    std::string e = p.extension().string();
    for (auto& c : e) c = (char)std::tolower((unsigned char)c);
    return e == ".jpg" || e == ".jpeg" || e == ".png" || e == ".webp" ||
           e == ".tif" || e == ".tiff" || e == ".bmp";
}

fs::path temp_still(const char* tag) {
    return fs::temp_directory_path() /
           (std::string("spirula-") + tag + "-" +
            std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()) +
            ".jpg");
}

// A photo the way up the masker will see it, which is the way up it was meant
// to be shown -- see app::load_upright.
bool load_photo(const std::string& path, bool as_stored, int& w, int& h,
                std::vector<uint8_t>& rgb) {
    if (!app::load_rgb(path, w, h, rgb)) return false;
    if (!as_stored) app::turn_pixels(app::photo_turn(path), 3, rgb, w, h);
    return true;
}

// One still through the external ffmpeg, processed the way the run's own
// ffmpeg path leaves it: `-noautorotate` where extraction passes it, and the
// EAC canvas warped here because ffmpeg's own sampler insets every face.
bool load_ffmpeg_frame(const PreviewSource& src, double at, int folder, int& w,
                       int& h, std::vector<uint8_t>& rgb,
                       const std::atomic<bool>& cancel) {
    FfmpegStillOpts opts;
    opts.track = folder;
    opts.auto_rotate = src.look.auto_rotate;
    if (src.look.pano()) opts.eac = src.look.eac;
    const fs::path tmp = temp_still("preview");
    const bool ok =
        ffmpeg_extract_frame(src.ffmpeg_exe, src.input, at, tmp.string(), cancel,
                             opts);
    if (ok && !cancel.load()) app::load_rgb(tmp.string(), w, h, rgb);
    std::error_code rm;
    fs::remove(tmp, rm);
    if (rgb.empty()) return false;
    if (!src.look.pano()) return true;

    const size_t view =
        std::min((size_t)std::max(folder, 0), src.look.views.size() - 1);
    app::Pano360Remap map;
    app::pano360_remap(src.look.eac, src.look.views[view], map);
    std::vector<uint8_t> out((size_t)map.width * map.height * 3);
    app::pano360_apply(map, rgb.data(), w, h, 0, out.data());
    rgb.swap(out);
    w = map.width;
    h = map.height;
    return true;
}

}  // namespace

std::vector<std::string> preview_folders(const PreviewSource& src) {
    if (!src.is_video) return {std::string()};
    return app::frame_folders(src.look, src.tracks);
}

void collect_preview_frames(PreviewSource& src, int offers,
                            std::vector<PreviewFrame>& frames,
                            std::vector<std::string>& all_files,
                            const std::atomic<bool>& cancel) {
    frames.clear();
    all_files.clear();
    src.video_seconds = 0.0;
    offers = std::max(1, offers);
    std::error_code ec;

    if (src.is_video) {
        // Frames are decoded on demand; the list is just how many offers the
        // slider makes. Seeking is not supported by the decoder, so "frame N"
        // means "the Nth frame we sample while reading forward".
        long long total = 0;
#ifdef SS_HAVE_VIDEO
        if (src.builtin_decode) {
            video::VideoReader r;
            if (r.open(src.input)) {
                total = r.info().frame_count;
                // Free here, and the only thing a later fallback would be
                // missing: ffmpeg seeks by time, not by frame.
                if (r.info().fps > 0.0)
                    src.video_seconds = (double)total / r.info().fps;
            }
        }
#endif
        if (total <= 0) {
            // No in-process decoding on this machine, or it could not open the
            // file: ffmpeg is what will read this video, in the preview and in
            // the run, so ffmpeg is who to ask how long it is.
            VideoFacts facts;
            if (ffmpeg_probe_video(src.ffmpeg_exe, src.input, facts, cancel)) {
                src.video_seconds = facts.duration;
                total = facts.frames;
                if (src.tracks < (int)facts.tracks.size())
                    src.tracks = (int)facts.tracks.size();
            }
        }
        total = std::max(total, (long long)offers);
        src.video_frames = total;
        for (int i = 0; i < offers; i++) {
            PreviewFrame f;
            f.index = std::min(total - 1, (long long)i * (total / offers));
            f.position = (float)((double)f.index / (double)std::max(1LL, total - 1));
            frames.push_back(f);
        }
        return;
    }

    // follow_directory_symlink for the same reason DatasetPrep walks that way:
    // a prepared capture's images/ is often a link into the raw one, and the
    // default iterator quietly returns nothing for it.
    for (fs::recursive_directory_iterator it(
             src.input, fs::directory_options::skip_permission_denied |
                            fs::directory_options::follow_directory_symlink, ec), end;
         !ec && it != end; it.increment(ec))
        if (it->is_regular_file(ec) && is_image_file(it->path()))
            all_files.push_back(it->path().string());
    std::sort(all_files.begin(), all_files.end());

    // A spread through the capture is enough to judge a setting, and keeps the
    // slider meaningful on a 3000-photo folder. The index kept is the one in
    // the FULL list, because that is what a run counts.
    const size_t n = all_files.size();
    const size_t want = std::min<size_t>(n, (size_t)offers);
    for (size_t i = 0; i < want; i++) {
        PreviewFrame f;
        f.index = want > 1 ? (long long)(i * (n - 1) / (want - 1)) : 0;
        f.path = all_files[(size_t)f.index];
        f.position = n > 1 ? (float)((double)f.index / (double)(n - 1)) : 0.0f;
        frames.push_back(f);
    }
}

bool load_preview_frame(const PreviewSource& src, const PreviewFrame& frame,
                        int folder, int& w, int& h, std::vector<uint8_t>& rgb,
                        std::string& error, const std::atomic<bool>& cancel) {
    w = h = 0;
    rgb.clear();
    if (!src.is_video) {
        if (frame.path.empty() ||
            !load_photo(frame.path, src.photos_as_stored, w, h, rgb)) {
            error = dmsg::preview_frame_unreadable.get();
            return false;
        }
        return true;
    }

#ifdef SS_HAVE_VIDEO
    if (src.builtin_decode) {
        nn::Image img;
        std::string err;
        if (app::extract_one_frame(src.input, src.look, frame.index, folder, img,
                                   &cancel, err)) {
            w = img.width;
            h = img.height;
            rgb = std::move(img.data);
        }
        if (cancel.load()) return false;
    }
#endif
    // The same fallback the dataset run takes when the driver cannot decode
    // (DatasetPrep::extract_video). A whole subprocess for one still, which is
    // why it is not the first choice.
    if (rgb.empty()) {
        if (!command_exists(src.ffmpeg_exe)) {
            error = spirula::i18n::format(dmsg::preview_needs_ffmpeg,
                                          {src.ffmpeg_exe});
            return false;
        }
        // ffmpeg seeks by time, and the very end of a file is past the last
        // frame often enough to be worth backing away from.
        const double at = std::min((double)frame.position * src.video_seconds,
                                   std::max(0.0, src.video_seconds - 0.1));
        load_ffmpeg_frame(src, at, folder, w, h, rgb, cancel);
        if (cancel.load()) return false;
    }
    if (rgb.empty()) {
        error = dmsg::preview_frame_unreadable.get();
        return false;
    }
    return true;
}

std::vector<PreviewFrame> spread_preview_frames(
    const PreviewSource& src, const std::vector<PreviewFrame>& offers,
    const std::vector<std::string>& files, int samples) {
    std::vector<PreviewFrame> out;
    samples = std::max(2, samples);
    if (!src.is_video) {
        const size_t n = files.size();
        const size_t want = std::min<size_t>(n, (size_t)samples);
        for (size_t i = 0; i < want; i++) {
            PreviewFrame f;
            f.index = want > 1 ? (long long)(i * (n - 1) / (want - 1)) : 0;
            f.path = files[(size_t)f.index];
            out.push_back(f);
        }
        return out;
    }
    if (!src.builtin_decode) return offers;
    // 720 is about half a minute, and the fit wants a moving camera rather
    // than a long one: reading further costs a decode per frame for nothing.
    const long long total = std::max(1LL, src.video_frames);
    const long long span = std::min(total, 720LL);
    const long long stride = std::max(1LL, span / samples);
    for (long long i = 0; i < span; i += stride) {
        PreviewFrame f;
        f.index = i;
        f.position = (float)((double)i / (double)std::max(1LL, total - 1));
        out.push_back(f);
    }
    return out;
}

void scan_preview_frames(const PreviewSource& src,
                         const std::vector<PreviewFrame>& frames, int folder,
                         const std::function<void(const uint8_t*, int, int)>& on_frame,
                         const std::atomic<bool>& cancel) {
#ifdef SS_HAVE_VIDEO
    if (src.is_video && src.builtin_decode && !frames.empty()) {
        std::vector<int64_t> indices;
        for (const PreviewFrame& f : frames) indices.push_back(f.index);
        std::string err;
        bool any = false;
        app::extract_frames_at(src.input, src.look, indices, folder,
                               [&](nn::Image& img, int64_t) {
                                   any = true;
                                   on_frame(img.data.data(), img.width,
                                            img.height);
                               },
                               &cancel, err);
        if (any || cancel.load()) return;
    }
#endif
    for (const PreviewFrame& f : frames) {
        if (cancel.load()) return;
        int w = 0, h = 0;
        std::vector<uint8_t> rgb;
        std::string err;
        if (load_preview_frame(src, f, folder, w, h, rgb, err, cancel))
            on_frame(rgb.data(), w, h);
    }
}

}  // namespace gui
