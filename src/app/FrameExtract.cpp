// FrameExtract.cpp -- see app/FrameExtract.h.
//
// Which decoder runs, and what happens to a frame once it has chosen one
// (app/FrameDecode.h): the select/mask/write loop is the part worth having
// exactly once, whichever decoder and front end drive it.

#include "app/FrameExtract.h"
#include "app/CubeLut.h"

#include "app/FrameDecode.h"
#include "app/FrameMotion.h"
#include "app/WriterPool.h"
#include "app/gui/Subprocess.h"
#include "i18n/catalog/Data.h"
#include "i18n/catalog/Log.h"
#include "nn/core/Log.h"
#include "nn/Device.h"
#include "nn/io/Image.h"
#include "sam/Sam.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <functional>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace app {

namespace lmsg = spirula::i18n::msg::log;

void log_line(const FrameExtractSinks& sinks, const std::string& s) {
    if (sinks.log) sinks.log(s);
    else NN_LOG_ERROR("%s\n", s.c_str());
}

// What the plan came out as, for the log and for whatever is drawing it.
void report_plan(const FrameExtractSinks& sinks, const std::vector<int64_t>& plan,
                 int64_t frames, double fps) {
    if (plan.empty()) return;
    int64_t tightest = frames, widest = 0;
    for (size_t i = 1; i < plan.size(); i++) {
        const int64_t gap = plan[i] - plan[i - 1];
        tightest = std::min(tightest, gap);
        widest = std::max(widest, gap);
    }
    auto rate = [&](int64_t gap) {
        return std::round((gap > 0 ? fps / (double)gap : fps) * 100.0) / 100.0;
    };
    log_line(sinks,
             spirula::i18n::format(lmsg::motion_plan, {(long long)plan.size(),
                                                       rate(widest), rate(tightest)}));
    if (sinks.planned) sinks.planned(plan, frames);
}

namespace {

using app::WriteJob;
using app::WriterPool;

// ---------------------------------------------------------------------------
// Which decoder
// ---------------------------------------------------------------------------

struct Decoder {
    bool builtin = false;
    VideoFacts facts;                          // ffmpeg's probe, when not built-in
    std::vector<std::pair<int, int>> sizes;    // one per track
};

// The device must be frozen first: asking whether the built-in decoder works
// creates it.
bool pick_decoder(const FrameExtractJob& job, const FrameExtractSinks& sinks,
                  Decoder& d, std::string& error) {
    std::string why = "this build has no in-process video decoder"
                      " (-DSS_ENABLE_PATENTED=OFF)";
    if (job.decoder != FrameDecoder::Ffmpeg) {
#ifdef SS_HAVE_VIDEO
        why = video_decode_availability();
        if (why.empty()) {
            d.builtin = true;
            d.sizes = video_track_sizes(job.input, error);
            if (d.sizes.empty() && error.empty()) error = "no video track in " + job.input;
            return !d.sizes.empty();
        }
#endif
        if (job.decoder == FrameDecoder::Builtin) {
            error = why;
            return false;
        }
    }
    if (!gui::command_exists(job.ffmpeg_exe)) {
        error = job.decoder == FrameDecoder::Ffmpeg
                    ? spirula::i18n::format(lmsg::err_ffmpeg_not_found, {job.ffmpeg_exe})
                    : spirula::i18n::format(lmsg::err_no_video_decoder,
                                            {why, job.ffmpeg_exe});
        return false;
    }
#ifdef SS_HAVE_VIDEO
    if (job.decoder == FrameDecoder::Auto)
        log_line(sinks, spirula::i18n::format(lmsg::decode_fallback_ffmpeg, {why}));
#endif
    static const std::atomic<bool> never{false};
    ffmpeg_probe_video(job.ffmpeg_exe, job.input, d.facts,
                       sinks.cancel ? *sinks.cancel : never);
    d.sizes = d.facts.tracks;
    if (d.sizes.empty()) error = "no video track in " + job.input;
    return !d.sizes.empty();
}

bool measure_motion(const FrameExtractJob& job, const FrameExtractSinks& sinks,
                    const Decoder& d, int track, int tracks, MotionPlanInput& out,
                    FrameExtractStats& t, std::string& error) {
#ifdef SS_HAVE_VIDEO
    if (d.builtin)
        return measure_motion_vulkan(job, sinks, track, tracks, out, t, error);
#endif
    return measure_motion_ffmpeg(job, sinks, d.facts, track, tracks, out, t, error);
}

// The tracks' chosen instants, from whichever decoder `d` is.
using Decode = std::function<bool(const std::vector<int>& tracks, bool convert,
                                  const InstantSink& on_frame)>;

}  // namespace

bool scan_motion(const FrameExtractJob& job, const FrameExtractSinks& sinks,
                 MotionPlanInput& out, FrameExtractStats& stats,
                 std::string& error) {
    if (!sam::freeze_device(job.device, error)) return false;
    Decoder d;
    if (!pick_decoder(job, sinks, d, error)) return false;
    const int n = (int)d.sizes.size();
    stats.tracks = n;
    return measure_motion(job, sinks, d, job.track >= 0 ? job.track : 0, n, out,
                          stats, error);
}

namespace {

// ---------------------------------------------------------------------------
// What a chosen instant becomes
// ---------------------------------------------------------------------------

// One track into one folder, masked on the way when there is a masker.
bool extract_track(const FrameExtractJob& o, const FrameExtractSinks& sinks,
                   const Decode& decode, int track, const fs::path& image_dir,
                   const fs::path& mask_dir, sam::Masker* masker, WriterPool& pool,
                   FrameExtractStats& t) {
    fs::create_directories(image_dir);
    if (masker) fs::create_directories(mask_dir);
    const bool jpeg = o.quality >= 0 && o.quality <= 100;
    auto on_frame = [&](std::vector<nn::Image>& imgs, int64_t index, std::string&) {
        nn::Image& image = imgs[0];
        char stem[64];
        std::snprintf(stem, sizeof(stem), "%05lld", (long long)index);
        if (masker) {
            const double t0 = nn::now_ms();
            sam::Mask mask;
            sam::Result overlay;
            // The decoded index, not the written one: a click was drawn on a
            // frame of the video, and only one frame per sharpness window
            // survives to be written.
            if (masker->run(image, mask, o.write_overlay ? &overlay : nullptr, index)) {
                t.mask += nn::now_ms() - t0;
                WriteJob mj;
                mj.mask = std::move(mask);
                mj.path = (mask_dir / (std::string(stem) + ".png")).string();
                pool.submit(std::move(mj));
                if (o.write_overlay)
                    sam::save_overlay_png(
                        image, overlay,
                        (mask_dir / (std::string(stem) + "_overlay.png")).string());
            } else {
                log_line(sinks, "frame " + std::to_string(index) +
                                    ": masking failed: " + masker->lastError());
            }
        }
        WriteJob job;
        job.path = (image_dir / (std::string(stem) + (jpeg ? ".jpg" : ".png"))).string();
        if (sinks.preview && image.channels == 3)
            sinks.preview(image.data.data(), image.width, image.height, job.path);
        job.quality = o.quality;
        job.image = std::move(image);
        const double t0 = nn::now_ms();
        pool.submit(std::move(job));
        t.submit += nn::now_ms() - t0;
        ++t.written;
        return true;
    };
    return decode({track}, true, on_frame);
}

// A 360 file's tracks, laid out as one canvas and resampled into every view.
bool extract_pair(const FrameExtractJob& o, const FrameExtractSinks& sinks,
                  const Decode& decode, const Decoder& d, const fs::path& image_dir,
                  WriterPool& pool, FrameExtractStats& t, std::string& error) {
    const std::vector<int> want = pano360_needs_track1(o.eac)
                                      ? std::vector<int>{0, 1}
                                      : std::vector<int>{0};
    for (size_t k = 0; k < want.size(); k++) {
        if (k < d.sizes.size() && d.sizes[k].first == o.eac.track_w &&
            d.sizes[k].second == o.eac.track_h)
            continue;
        error = "track " + std::to_string(k) + " is not the size the 360 layout"
                " was detected at";
        return false;
    }
    std::vector<Pano360Remap> maps(o.views.size());
    for (size_t i = 0; i < o.views.size(); i++) {
        pano360_remap(o.eac, o.views[i], maps[i]);
        fs::create_directories(image_dir / o.views[i].dir);
    }
    std::vector<uint8_t> canvas((size_t)o.eac.canvasW() * o.eac.canvasH() * 3);
    const bool jpeg = o.quality >= 0 && o.quality <= 100;

    auto on_frame = [&](std::vector<nn::Image>& track, int64_t index, std::string& err) {
        for (size_t k = 0; k < track.size(); k++)
            if (track[k].width != o.eac.track_w || track[k].height != o.eac.track_h) {
                err = "track " + std::to_string(k) + " decoded at another size";
                return false;
            }
        double t0 = nn::now_ms();
        pano360_canvas(o.eac, track[0].data.data(),
                       track.size() > 1 ? track[1].data.data() : nullptr,
                       canvas.data());
        t.convert += nn::now_ms() - t0;
        char stem[64];
        std::snprintf(stem, sizeof(stem), "%05lld", (long long)index);
        for (size_t i = 0; i < o.views.size(); i++) {
            t0 = nn::now_ms();
            WriteJob job;
            job.image.width = maps[i].width;
            job.image.height = maps[i].height;
            job.image.channels = 3;
            job.image.data.resize((size_t)maps[i].width * maps[i].height * 3);
            pano360_apply(maps[i], canvas.data(), o.eac.canvasW(), o.eac.canvasH(), 0,
                          job.image.data.data());
            t.convert += nn::now_ms() - t0;
            job.path = (image_dir / o.views[i].dir /
                        (std::string(stem) + (jpeg ? ".jpg" : ".png"))).string();
            job.quality = o.quality;
            // The reel shows the first view; the others are the same instant
            // seen the other way, and six thumbnails a frame is not what the
            // slider is for.
            if (i == 0 && sinks.preview)
                sinks.preview(job.image.data.data(), job.image.width, job.image.height,
                              job.path);
            t0 = nn::now_ms();
            pool.submit(std::move(job));
            t.submit += nn::now_ms() - t0;
            // Per view, not per frame: the count is what the progress bar is
            // scaled against, and a plan writes six files here.
            ++t.written;
        }
        return true;
    };
    return decode(want, false, on_frame);
}

// Every track of a multi-lens file at the same instants, one folder per
// track: the rig the frames of one stem form is then a rig in fact.
bool extract_synced(const FrameExtractJob& o, const FrameExtractSinks& sinks,
                    const Decode& decode, const std::vector<int>& tracks,
                    const fs::path& base, WriterPool& pool, FrameExtractStats& t) {
    std::vector<fs::path> dirs;
    for (size_t k = 0; k < tracks.size(); k++) {
        dirs.push_back(base / ("cam" + std::to_string(k)));
        fs::create_directories(dirs.back());
        log_line(sinks, "track " + std::to_string(tracks[k]) + " -> " + dirs.back().string());
    }
    const bool jpeg = o.quality >= 0 && o.quality <= 100;
    auto on_frame = [&](std::vector<nn::Image>& imgs, int64_t index, std::string&) {
        char stem[64];
        std::snprintf(stem, sizeof(stem), "%05lld", (long long)index);
        for (size_t k = 0; k < imgs.size(); k++) {
            WriteJob job;
            job.path = (dirs[k] / (std::string(stem) + (jpeg ? ".jpg" : ".png"))).string();
            if (k == 0 && sinks.preview && imgs[k].channels == 3)
                sinks.preview(imgs[k].data.data(), imgs[k].width, imgs[k].height, job.path);
            job.quality = o.quality;
            job.image = std::move(imgs[k]);
            const double t0 = nn::now_ms();
            pool.submit(std::move(job));
            t.submit += nn::now_ms() - t0;
            ++t.written;
        }
        return true;
    };
    return decode(tracks, true, on_frame);
}

}  // namespace

// ---------------------------------------------------------------------------
// Entry points
// ---------------------------------------------------------------------------

bool extract_frames(const FrameExtractJob& job_in, const FrameExtractSinks& sinks,
                    FrameExtractStats& stats, std::string& error) {
    FrameExtractJob job = job_in;
    if (job.skip < 1) job.skip = 1;
    if (job.keep < 0) job.keep = (int)(0.5 * job.skip + 0.5);
    if (job.rotate % 90 != 0) {
        error = "rotation must be a multiple of 90 degrees";
        return false;
    }

    // Before the decoder probe, which creates the device and reports the video
    // queue family this job then decodes on. Everything after -- decoder,
    // Masker, Tracker -- inherits that identity.
    if (!sam::freeze_device(job.device.empty() ? job.mask.device : job.device,
                            error))
        return false;
    // Past this point the canonical UUID is what crosses into the decoder and
    // the masker; an ordinal only ever meant something in the resolution that
    // consumed it.
    job.device = nn::configured_device_selector();
    job.mask.device = job.device;

    Decoder dec;
    if (!pick_decoder(job, sinks, dec, error)) return false;
    std::shared_ptr<const CubeLut> lut;
    if (!job.lut.empty()) {
        lut = cube_lut_cached(job.lut, error);
        if (!lut) return false;
        log_line(sinks, spirula::i18n::format(lmsg::video_lut_applied,
                                              {fs::path(job.lut).filename().string()}));
    }

    const bool pano = job.eac.valid() && !job.views.empty();
    if (pano) {
        // The layout is measured in source pixels and the sphere is turned by
        // the plan's own yaw/pitch/roll, so neither of these can be honoured
        // quietly.
        if (job.scale != 1.0f || job.rotate != 0) {
            error = "--scale and --rotate do not apply to a 360 capture";
            return false;
        }
        // One masker cannot track six views at once: its memory bank is keyed
        // by frame, and six directions per frame are six videos to it.
        if (!job.mask.model.empty()) {
            error = "masking during extraction does not support a 360 capture;"
                    " mask the extracted frames instead";
            return false;
        }
    }
    if (job.sync_tracks && !job.mask.model.empty()) {
        error = "masking during extraction does not support synchronized tracks;"
                " mask the extracted frames instead";
        return false;
    }

    std::vector<int> tracks;
    if (job.track >= 0) {
        if (job.track >= (int)dec.sizes.size()) {
            error = "no video track " + std::to_string(job.track) + " in " + job.input;
            return false;
        }
        tracks.push_back(job.track);
    } else {
        for (int i = 0; i < (int)dec.sizes.size(); ++i) tracks.push_back(i);
    }
    stats.tracks = (int)tracks.size();

#ifdef SS_HAVE_VIDEO
    // ffmpeg turns the picture by the container's matrix itself.
    if (dec.builtin) {
        bool mixed = false;
        const sfm::ExifTransform turn =
            fold_auto_rotate(job.input, tracks, job, mixed);
        if (turn.turns_cw > 0) {
            log_line(sinks, spirula::i18n::format(
                                lmsg::video_autorotate,
                                {(long long)(90 * turn.turns_cw)}));
            if (mixed) log_line(sinks, lmsg::video_autorotate_mixed.get());
        }
        if (turn.mirror) log_line(sinks, lmsg::video_autorotate_mirror.get());
    }
#endif

    std::unique_ptr<sam::Masker> masker;
    if (!job.mask.model.empty()) {
        masker = std::make_unique<sam::Masker>();
        if (!masker->init(job.mask, error)) return false;
    }

    stats.encoder_threads =
        job.threads > 0 ? job.threads
                        : std::max(1, (int)std::thread::hardware_concurrency() - 1);
    WriterPool pool(stats.encoder_threads);

    const fs::path base(job.image_dir);
    const fs::path mask_base(job.mask_dir.empty() ? (base.parent_path() / "masks")
                                                  : fs::path(job.mask_dir));

    const double t_start = nn::now_ms();
    bool ok = true;
    const bool synced = !pano && job.sync_tracks && tracks.size() > 1;
    // One plan for the whole file, measured on its first track: the tracks of
    // a rig see the same motion, and the ones that do not are still one camera
    // moving through one scene.
    std::vector<int64_t> plan = job.plan;
    if (job.adaptive && plan.empty()) {
        MotionPlanInput mi;
        if (!measure_motion(job, sinks, dec, tracks[0], (int)tracks.size(), mi, stats,
                            error))
            return false;
        std::vector<std::vector<int64_t>> got =
            plan_by_motion({mi}, job.adaptive_range);
        if (!got.empty()) plan = std::move(got[0]);
        if (plan.empty()) {
            error = "the capture is too short to space frames by motion";
            return false;
        }
        report_plan(sinks, plan, mi.frames, mi.fps);
    }

    const Decode decode = [&](const std::vector<int>& tr, bool convert,
                              const InstantSink& on_frame_in) {
        // Only the kept instants reach the sink, so the LUT costs per frame
        // written, not per frame decoded.
        const InstantSink on_frame = !lut ? on_frame_in
            : InstantSink([&](std::vector<nn::Image>& imgs, int64_t index, std::string& err) {
                  for (nn::Image& im : imgs)
                      apply_cube_lut(*lut, im.data.data(), im.data.size() / 3);
                  return on_frame_in(imgs, index, err);
              });
#ifdef SS_HAVE_VIDEO
        if (dec.builtin)
            return decode_lockstep_vulkan(job, sinks, tr, convert, plan, stats, error,
                                          on_frame);
#endif
        return decode_lockstep_ffmpeg(job, sinks, dec.facts, tr, convert, plan, stats,
                                      error, on_frame);
    };
    if (pano) {
        stats.tracks = 2;
        ok = extract_pair(job, sinks, decode, dec, base, pool, stats, error);
    } else if (synced) {
        ok = extract_synced(job, sinks, decode, tracks, base, pool, stats);
    }
    for (size_t ti = 0; ti < tracks.size() && ok && !pano && !synced; ++ti) {
        // A multi-track file (an Insta360 .insv carries two fisheye streams)
        // becomes cam0/, cam1/, ... -- one camera per folder downstream.
        const bool multi = tracks.size() > 1;
        const fs::path image_dir = multi ? base / ("cam" + std::to_string(ti)) : base;
        const fs::path mask_dir =
            multi ? mask_base / ("cam" + std::to_string(ti)) : mask_base;
        if (multi)
            log_line(sinks, "track " + std::to_string(tracks[ti]) + " -> " +
                                image_dir.string());
        ok = extract_track(job, sinks, decode, tracks[ti], image_dir, mask_dir,
                           masker.get(), pool, stats);
    }

    const double t_drain = nn::now_ms();
    pool.finish();
    stats.drain = nn::now_ms() - t_drain;
    stats.total = nn::now_ms() - t_start;
    stats.encode_cpu = pool.busyMs();
    stats.write_failures = pool.failures();
    return ok;
}

std::string format_extract_stats(const FrameExtractStats& s,
                                 const std::string& out_dir, bool masked) {
    namespace dmsg = spirula::i18n::msg::data;
    using spirula::i18n::format;
    using spirula::i18n::display_width;
    using spirula::i18n::pad_to;

    auto ms = [](double v) {
        char b[32];
        std::snprintf(b, sizeof b, "%.0f", v);
        return std::string(b);
    };
    auto rate = [](double per_ms_count, double ms_total) {
        char b[32];
        std::snprintf(b, sizeof b, "%.1f",
                      per_ms_count > 0 ? 1000.0 * per_ms_count /
                                             std::max(ms_total, 1e-6)
                                       : 0.0);
        return std::string(b);
    };

    // A label column and a value column. The labels are the translated ones,
    // so the column is as wide as the longest of THEM -- measured in terminal
    // columns, since a CJK label is half the characters and twice the width.
    struct Row { std::string label, value; bool gap_before; };
    std::vector<Row> rows;
    rows.push_back({dmsg::xs_decoded.get(),
                    std::to_string((long long)s.decoded), false});
    rows.push_back({dmsg::xs_measured.get(),
                    std::to_string((long long)s.measured), false});
    rows.push_back({dmsg::xs_written.get(),
                    format(dmsg::xs_written_to,
                           {(long long)s.written, out_dir}), false});
    if (s.analyzed > 0)
        rows.push_back({dmsg::xs_analyzed.get(),
                        std::to_string((long long)s.analyzed), false});
    rows.push_back({dmsg::xs_decode.get(),
                    format(dmsg::xs_ms_fps,
                           {ms(s.decode), rate((double)s.decoded, s.decode)}),
                    true});
    if (s.plan > 0)
        rows.push_back({dmsg::xs_motion.get(),
                        format(dmsg::xs_ms, {ms(s.plan)}), false});
    rows.push_back({dmsg::xs_sharpness.get(),
                    format(dmsg::xs_ms, {ms(s.sharpness)}), false});
    rows.push_back({dmsg::xs_convert.get(),
                    format(dmsg::xs_ms, {ms(s.convert)}), false});
    if (masked)
        rows.push_back({dmsg::xs_segmentation.get(),
                        format(dmsg::xs_ms, {ms(s.mask)}), false});
    rows.push_back({format(dmsg::xs_encode, {s.encoder_threads}),
                    format(dmsg::xs_ms_cpu, {ms(s.encode_cpu)}), false});
    rows.push_back({dmsg::xs_stalled.get(),
                    format(dmsg::xs_ms, {ms(s.submit)}), false});
    rows.push_back({dmsg::xs_drain.get(),
                    format(dmsg::xs_ms, {ms(s.drain)}), false});
    rows.push_back({dmsg::xs_total.get(),
                    format(dmsg::xs_ms_written_rate,
                           {ms(s.total), rate((double)s.written, s.total)}),
                    false});

    int width = 0;
    for (const Row& r : rows) width = std::max(width, display_width(r.label));

    std::string out = "\n";
    for (const Row& r : rows) {
        if (r.gap_before) out += "\n";
        out += pad_to(r.label, width + 2);
        out += r.value;
        out += "\n";
    }
    return out;
}

}  // namespace app
