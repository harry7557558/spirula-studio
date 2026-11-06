// PreviewFrames.cpp -- see PreviewFrames.h.

#include "app/gui/PreviewFrames.h"

#include "app/CubeLut.h"
#include "app/FrameMask.h"
#include "app/gui/DatasetPrep.h"
#include "app/gui/HeifPhoto.h"
#include "app/gui/Subprocess.h"
#include "i18n/catalog/Dataset.h"

#include "core/ColorSpace.h"
#include "core/ImageFile.h"
#ifdef SS_HAVE_VIDEO
#include "app/FrameExtract.h"
#include "video/Video.h"
#endif
#ifdef SS_BUILD_SAM
#include "sam/Sam.h"   // sam::freeze_device
#endif

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;
namespace dmsg = spirula::i18n::msg::dataset;

namespace gui {

namespace {

fs::path temp_still(const char* tag) {
    return fs::temp_directory_path() /
           (std::string("spirula-") + tag + "-" +
            std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()) +
            ".jpg");
}

colorspace::Exposure exposure_of(const PreviewSource& src) {
    colorspace::Exposure e;
    colorspace::parse_exposure(src.image_exposure, e);
    return e;
}

void convert_to_srgb(const PreviewSource& src, std::vector<uint8_t>& rgb) {
    colorspace::to_srgb_inplace(rgb.data(), rgb.size() / 3,
                                src.image_gamut,
                                src.image_is_linear.value_or(false));
    colorspace::expose_srgb8_inplace(
        rgb.data(), rgb.size(),
        colorspace::exposure_gain_srgb8(exposure_of(src), rgb.data(), rgb.size() / 3, 1));
}

// A packed frame's lens `folder`, cut out in place.
void crop_packed(const PreviewSource& src, int folder, int& w, int& h,
                 std::vector<uint8_t>& rgb) {
    if (src.look.packed_lenses < 2 || rgb.empty()) return;
    std::vector<uint8_t> lens;
    app::packed_lens_crop(rgb.data(), w, h, 3, src.look.packed_lenses, folder,
                          lens, w, h);
    rgb.swap(lens);
}

bool load_photo(const PreviewSource& src, const std::string& path,
                int& w, int& h, std::vector<uint8_t>& rgb, int folder,
                const std::atomic<bool>& cancel, std::string& error) {
    if (is_heif_path(path)) {
        if (!load_heif(path, src.builtin_decode, src.ffmpeg_exe, w, h, rgb, cancel,
                       error))
            return false;
        convert_to_srgb(src, rgb);
    } else if (imagefile::handles(path)) {
        imagefile::Info info;
        imagefile::Options opt;
        opt.exposure = exposure_of(src);
        if (!imagefile::decode_srgb8(path, opt, info, rgb, src.image_gamut,
                                     src.image_is_linear).empty())
            return false;
        w = info.width;
        h = info.height;
    } else {
        if (!app::load_rgb(path, w, h, rgb)) return false;
        convert_to_srgb(src, rgb);
    }
    // Cut before the turn: the lenses are laid out in the stored pixels.
    crop_packed(src, folder, w, h, rgb);
    if (!src.photos_as_stored)
        app::turn_pixels(app::photo_turn(path), 3, rgb, w, h);
    return true;
}

// One still through the external ffmpeg, processed the way the run's own
// ffmpeg path leaves it.
bool load_ffmpeg_frame(const PreviewSource& src, double at, int folder, int& w,
                       int& h, std::vector<uint8_t>& rgb,
                       const std::atomic<bool>& cancel) {
    std::vector<PreviewPixels> out;
    if (!load_ffmpeg_stills(src, at, {folder}, false, 0, out, cancel)) return false;
    w = out[0].w;
    h = out[0].h;
    rgb = std::move(out[0].rgb);
    return true;
}

}  // namespace

void preview_to_srgb(const PreviewSource& src, std::vector<uint8_t>& rgb) {
    convert_to_srgb(src, rgb);
}

// `-noautorotate` where extraction passes it, and the EAC canvas warped here
// because ffmpeg's own sampler insets every face.
bool load_ffmpeg_stills(const PreviewSource& src, double seconds,
                        const std::vector<int>& folders, bool keyframe, int max_side,
                        std::vector<PreviewPixels>& out, const std::atomic<bool>& cancel) {
    out.clear();
    if (folders.empty()) return false;
    FfmpegStillOpts opts;
    opts.auto_rotate = src.look.auto_rotate;
    opts.keyframe = keyframe;
    opts.max_side = max_side;
    if (src.look.pano()) opts.eac = src.look.eac;
    // Lenses packed in one picture are all cut from track 0.
    std::vector<int> tracks;
    for (int f : folders) {
        const int t = src.look.pano() || src.look.packed_lenses >= 2 ? 0 : f;
        if (std::find(tracks.begin(), tracks.end(), t) == tracks.end()) tracks.push_back(t);
    }
    std::vector<std::string> paths;
    for (size_t i = 0; i < tracks.size(); i++)
        paths.push_back(temp_still(("preview" + std::to_string(i)).c_str()).string());
    const bool ok = ffmpeg_extract_frames(src.ffmpeg_exe, src.input, seconds, tracks, paths,
                                          cancel, opts);
    std::vector<PreviewPixels> read(tracks.size());
    for (size_t i = 0; ok && i < tracks.size() && !cancel.load(); i++) {
        PreviewPixels& p = read[i];
        if (!app::load_rgb(paths[i], p.w, p.h, p.rgb)) continue;
        // First, as the run's ffmpeg filter graph and the built-in decoder do.
        std::string lut_error;
        if (!src.look.lut.empty())
            if (auto lut = app::cube_lut_cached(src.look.lut, lut_error))
                app::apply_cube_lut(*lut, p.rgb.data(), p.rgb.size() / 3);
        convert_to_srgb(src, p.rgb);
    }
    std::error_code rm;
    for (const std::string& path : paths) fs::remove(path, rm);
    if (!ok || cancel.load()) return false;

    for (int f : folders) {
        const int t = src.look.pano() || src.look.packed_lenses >= 2 ? 0 : f;
        const size_t i = (size_t)(std::find(tracks.begin(), tracks.end(), t) - tracks.begin());
        PreviewPixels p = read[i];
        if (p.rgb.empty()) return false;
        if (!src.look.pano()) {
            crop_packed(src, f, p.w, p.h, p.rgb);
            out.push_back(std::move(p));
            continue;
        }
        const size_t view = std::min((size_t)std::max(f, 0), src.look.views.size() - 1);
        app::Pano360Remap map;
        app::pano360_remap(src.look.eac, src.look.views[view], map);
        PreviewPixels v;
        v.w = map.width;
        v.h = map.height;
        v.rgb.resize((size_t)map.width * map.height * 3);
        app::pano360_apply(map, p.rgb.data(), p.w, p.h, 0, v.rgb.data());
        out.push_back(std::move(v));
    }
    return true;
}

PreviewSource gated(const PreviewSource& src, DecodeGate& gate,
                    std::unique_lock<std::mutex>& hold) {
    PreviewSource s = src;
    if (s.builtin_decode && gate.allowed.load()) {
        hold = std::unique_lock<std::mutex>(gate.busy);
        s.builtin_decode = gate.allowed.load();
        if (!s.builtin_decode) hold.unlock();
    } else {
        s.builtin_decode = false;
    }
    return s;
}

std::vector<std::string> preview_folders(const PreviewSource& src) {
    if (!src.is_video && src.look.packed_lenses < 2) return {std::string()};
    return app::frame_folders(src.look, src.is_video ? src.tracks : 1);
}

void collect_preview_frames(PreviewSource& src, int offers,
                            std::vector<PreviewFrame>& frames,
                            std::vector<std::string>& all_files,
                            const std::atomic<bool>& cancel) {
    frames.clear();
    all_files.clear();
    src.video_seconds = 0.0;
    offers = std::max(0, offers);
    std::error_code ec;

    if (src.is_video) {
        // Frames are decoded on demand; the list is just how many offers the
        // slider makes.
        long long total = 0;
        double fps = 0.0;
#ifdef SS_HAVE_VIDEO
        if (!cancel.load() && src.builtin_decode) {
            video::VideoProbe probe;
            std::string error;
            if (video::probe_video(src.input, probe, error)) {
                total = probe.frame_count;
                fps = probe.fps;
                if (probe.fps > 0.0)
                    src.video_seconds = (double)total / probe.fps;
                if (src.tracks < probe.tracks) src.tracks = probe.tracks;
            }
        }
#endif
        if (cancel.load()) return;
        if (total <= 0) {
            // No in-process decoding on this machine, or it could not open the
            // file: ffmpeg is what will read this video, in the preview and in
            // the run, so ffmpeg is who to ask how long it is.
            VideoFacts facts;
            if (ffmpeg_probe_video(src.ffmpeg_exe, src.input, facts, cancel)) {
                src.video_seconds = facts.duration;
                total = facts.frames;
                fps = facts.fps;
                if (src.tracks < (int)facts.tracks.size())
                    src.tracks = (int)facts.tracks.size();
            }
        }
        total = std::max(total, (long long)offers);
        src.video_frames = total;
        const app::FrameSpan clip = app::clip_frames(src.clip_start, src.clip_end, fps);
        const long long first = std::min<long long>(clip.first, total - 1);
        const long long end = clip.end >= 0 ? std::min<long long>(clip.end, total) : total;
        const long long span = std::max(1LL, end - first);
        src.clip_first = first;
        src.clip_frames = span;
        const int want = offers > 0 ? offers : (int)std::min(span, 100000LL);
        for (int i = 0; i < want; i++) {
            PreviewFrame f;
            const long long at = want > 1 ? (long long)i * (span - 1) / (want - 1) : 0;
            f.index = first + at;
            f.position = (float)((double)at / (double)std::max(1LL, span - 1));
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
         !cancel.load() && !ec && it != end; it.increment(ec))
        if (it->is_regular_file(ec) && is_image_file(it->path()))
            all_files.push_back(it->path().string());
    if (cancel.load()) return;
    std::sort(all_files.begin(), all_files.end());

    const size_t n = all_files.size();
    const size_t want = offers > 0 ? std::min<size_t>(n, (size_t)offers) : n;
    for (size_t i = 0; i < want; i++) {
        PreviewFrame f;
        f.index = (offers > 0 && want > 1) ? (long long)(i * (n - 1) / (want - 1)) : (long long)i;
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
    if (src.is_video && !src.device_error.empty()) {
        error = src.device_error;
        return false;
    }
    if (!src.is_video) {
#ifdef SS_BUILD_SAM
        if (src.builtin_decode && is_heif_path(frame.path)) {
            std::string select_error;
            if (!sam::freeze_device(src.device, select_error)) {
                error = select_error;
                return false;
            }
        }
#endif
        std::string why;
        if (frame.path.empty() ||
            !load_photo(src, frame.path, w, h, rgb, folder, cancel, why)) {
            error = why.empty() ? dmsg::preview_frame_unreadable.get() : why;
            return false;
        }
        return true;
    }

#ifdef SS_BUILD_SAM
    if (src.builtin_decode) {
        std::string select_error;
        if (!sam::freeze_device(src.device, select_error)) {
            error = select_error;
            return false;
        }
    }
#endif
#ifdef SS_HAVE_VIDEO
    if (src.builtin_decode) {
        nn::Image img;
        std::string err;
        if (app::extract_one_frame(src.input, src.look, frame.index, folder, img,
                                   &cancel, err, src.device)) {
            w = img.width;
            h = img.height;
            rgb = std::move(img.data);
        }
        if (cancel.load()) return false;
    }
#endif
    if (!rgb.empty()) convert_to_srgb(src, rgb);
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
        const double end = src.clip_end > src.clip_start
                               ? std::min(src.clip_end, src.video_seconds)
                               : src.video_seconds;
        const double from = std::min(src.clip_start, end);
        const double at = std::min(from + (double)frame.position * (end - from),
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
    const long long first = std::clamp(src.clip_first, 0LL, total - 1);
    const long long clip = src.clip_frames > 0 ? src.clip_frames : total - first;
    const long long span = std::min(clip, 720LL);
    const long long stride = std::max(1LL, span / samples);
    for (long long i = 0; i < span; i += stride) {
        PreviewFrame f;
        f.index = first + i;
        f.position = (float)((double)i / (double)std::max(1LL, clip - 1));
        out.push_back(f);
    }
    return out;
}

void scan_preview_frames(const PreviewSource& src,
                         const std::vector<PreviewFrame>& frames, int folder,
                         const std::function<void(const uint8_t*, int, int)>& on_frame,
                         const std::atomic<bool>& cancel) {
    if (src.is_video && !src.device_error.empty()) return;
    // Border fitting reads stored pixels; color overrides apply to models and display.
    PreviewSource stored = src;
    stored.image_gamut.clear();
    stored.image_is_linear.reset();
    stored.image_exposure.clear();
#ifdef SS_BUILD_SAM
    if (src.is_video && src.builtin_decode) {
        std::string select_error;
        if (!sam::freeze_device(src.device, select_error)) return;
    }
#endif
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
                               &cancel, err, src.device);
        if (any || cancel.load()) return;
    }
#endif
    for (const PreviewFrame& f : frames) {
        if (cancel.load()) return;
        int w = 0, h = 0;
        std::vector<uint8_t> rgb;
        std::string err;
        if (load_preview_frame(stored, f, folder, w, h, rgb, err, cancel))
            on_frame(rgb.data(), w, h);
    }
}

}  // namespace gui
