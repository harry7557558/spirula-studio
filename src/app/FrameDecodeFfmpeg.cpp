// FrameDecodeFfmpeg.cpp -- app/FrameDecode.h's ffmpeg decoder: one child per
// track, each writing pictures to a pipe, read in lockstep.

#include "app/FrameDecode.h"

#include "app/FrameSharpness.h"
#include "app/gui/Subprocess.h"
#include "nn/core/Log.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <future>
#include <memory>

namespace app {

namespace {

// -fps_mode arrived in ffmpeg 5.1 and -vsync, which it replaces, is deprecated
// from then on; a version this cannot read is taken to be a recent build.
bool has_fps_mode(const std::string& exe) {
    int major = -1, minor = 0;
    std::atomic<bool> never{false};
    gui::run_process({exe, "-hide_banner", "-version"}, "",
                     [&](const std::string& line) {
                         if (major < 0)
                             std::sscanf(line.c_str(), "ffmpeg version %d.%d",
                                         &major, &minor);
                     },
                     never);
    return major < 0 || major > 5 || (major == 5 && minor >= 1);
}

// Everything before the output. Every decoded frame comes out exactly once, in
// presentation order: a frame's place in this stream is the index it is named
// by, as it is for the built-in decoder.
std::vector<std::string> decode_args(const FrameExtractJob& o, const VideoFacts& f,
                                     int track, bool auto_rotate, bool hwaccel,
                                     const std::string& filters) {
    static const bool fps_mode = has_fps_mode(o.ffmpeg_exe);
    const int stream = track < (int)f.streams.size() ? f.streams[track] : -1;
    std::vector<std::string> a{o.ffmpeg_exe, "-nostdin", "-hide_banner",
                               "-loglevel", "error"};
    if (hwaccel) a.insert(a.end(), {"-hwaccel", "auto"});
    if (!auto_rotate) a.push_back("-noautorotate");
    a.insert(a.end(), {"-i", o.input, "-map",
                       stream >= 0 ? "0:" + std::to_string(stream)
                                   : "0:v:" + std::to_string(track)});
    if (!filters.empty()) a.insert(a.end(), {"-vf", filters});
    a.insert(a.end(), {fps_mode ? "-fps_mode" : "-vsync", "passthrough"});
    return a;
}

// What video::ConvertOpts does, as filters: the built-in decoder's size
// rounding, then the extra turn clockwise.
std::string convert_filters(const FrameExtractJob& o) {
    std::string f;
    if (o.scale != 1.0f) {
        char b[128];
        std::snprintf(b, sizeof b,
                      "scale='max(1,trunc(iw*%.9g+0.5))':'max(1,trunc(ih*%.9g+0.5))'"
                      ":flags=area",
                      o.scale, o.scale);
        f = b;
    }
    const int r = ((o.rotate % 360) + 360) % 360;
    const char* turn = r == 90 ? "transpose=clock" : r == 180 ? "hflip,vflip"
                                   : r == 270 ? "transpose=cclock" : "";
    if (*turn) f += (f.empty() ? "" : ",") + std::string(turn);
    return f;
}

std::string join_filters(std::initializer_list<std::string> parts) {
    std::string out;
    for (const std::string& p : parts)
        if (!p.empty()) out += (out.empty() ? "" : ",") + p;
    return out;
}

// video.slang's luma_thumbnail on the host, summed in the same order and
// type, so the plan is the built-in decoder's: both decode the Y plane
// bit-exactly, and a plan moves by frames when its grey input moves by one.
void luma_box(const uint8_t* y, int sw, int sh, int ow, int oh, uint8_t* out) {
    for (int oy = 0; oy < oh; oy++) {
        const int y0 = (int)((int64_t)oy * sh / oh);
        const int y1 = std::min(sh, std::max(y0 + 1, (int)((int64_t)(oy + 1) * sh / oh)));
        for (int ox = 0; ox < ow; ox++) {
            const int x0 = (int)((int64_t)ox * sw / ow);
            const int x1 =
                std::min(sw, std::max(x0 + 1, (int)((int64_t)(ox + 1) * sw / ow)));
            float sum = 0.0f;
            for (int yy = y0; yy < y1; yy++)
                for (int xx = x0; xx < x1; xx++) sum += (float)y[(size_t)yy * sw + xx];
            const float v = sum / float((x1 - x0) * (y1 - y0));
            out[(size_t)oy * ow + ox] = (uint8_t)std::min(255.0f, std::max(0.0f, v + 0.5f));
        }
    }
}

}  // namespace

bool measure_motion_ffmpeg(const FrameExtractJob& o, const FrameExtractSinks& sinks,
                           const VideoFacts& facts, int track, int tracks,
                           MotionPlanInput& out, FrameExtractStats& t,
                           std::string& error) {
    const double t_start = nn::now_ms();
    if (track >= (int)facts.tracks.size()) {
        error = "no video track " + std::to_string(track);
        return false;
    }
    const int w = facts.tracks[track].first, h = facts.tracks[track].second;

    // As the built-in pass measures: unturned source frames, the same view.
    MotionOptions mo;
    if (o.eac.valid()) mo.view = MotionView::Packed360;
    else if (tracks >= 2 && w == h)
        mo.view = MotionView::Fisheye;
    mo.eac = o.eac;
    mo.out_fov = mo.view == MotionView::Fisheye ? mo.circle_fov
                                                : motion_out_fov(o.views);
    motion_frame_size(mo.view, w, h, mo.width, mo.height);

    MotionTracker tracker(mo);
    const double fps = facts.fps > 1.0 ? facts.fps : 30.0;
    const int stride = std::max(
        1, std::min((int)std::lround(fps / 15.0), std::max(1, o.skip / 3)));

    // Every frame's Y plane comes over whole: dropping frames in ffmpeg would
    // lose the count the plan is budgeted against. Decoded in software: NVDEC's
    // download squeezed a full-range Y plane into studio range.
    FfmpegFrames ff;
    if (!ff.open(decode_args(o, facts, track, false, false, "extractplanes=y,format=gray"),
                 true, error))
        return false;

    std::vector<uint8_t> luma, gray((size_t)mo.width * mo.height);
    int lw = 0, lh = 0;
    int64_t index = -1;
    size_t reported = 0;
    while (ff.next(luma, lw, lh, error)) {
        if (sinks.cancel && sinks.cancel->load()) {
            error = "cancelled";
            return false;
        }
        ++index;
        if (index % stride != 0) continue;
        luma_box(luma.data(), lw, lh, mo.width, mo.height, gray.data());
        tracker.track(gray.data(), index);
        ++t.analyzed;
        if (sinks.measured)
            for (; reported < tracker.costs().size(); reported++)
                sinks.measured(tracker.ends()[reported], facts.frames,
                               tracker.costs()[reported]);
        if (sinks.scanning) sinks.scanning(index + 1, facts.frames);
    }
    if (!error.empty()) return false;
    tracker.finish();
    out.cost = tracker.costs();
    out.step = tracker.steps();
    out.ends = tracker.ends();
    out.frames = index + 1;
    out.skip = o.skip;
    out.window = std::max(o.keep, 1);
    out.max_frames = o.max_frames;
    out.fps = fps;
    out.view = mo.view;
    out.out_fov = mo.out_fov;
    t.plan = nn::now_ms() - t_start;
    return true;
}

bool decode_lockstep_ffmpeg(const FrameExtractJob& o, const FrameExtractSinks& sinks,
                            const VideoFacts& facts, const std::vector<int>& tracks,
                            bool convert, const std::vector<int64_t>& plan,
                            FrameExtractStats& t, std::string& error,
                            const InstantSink& on_frame) {
    const size_t n = tracks.size();
    const int keep = o.keep;
    // A fixed rate is dropped by ffmpeg before the colour conversion, and the
    // k-th picture out is the clock's k-th candidate. A plan is thousands of
    // windows, too many for a filter expression, so every frame comes over.
    const bool filtered = plan.empty();
    std::string select;
    if (filtered) {
        char b[96];
        if (keep == 0) std::snprintf(b, sizeof b, "select='not(mod(n,%d))'", o.skip);
        else
            std::snprintf(b, sizeof b, "select='lt(mod(n+%d,%d),%d)'", keep, o.skip,
                          keep);
        select = b;
    }
    const std::string filters =
        join_filters({select, convert ? convert_filters(o) : "", "format=rgb24"});
    std::vector<std::unique_ptr<FfmpegFrames>> ff(n);
    for (size_t k = 0; k < n; k++) {
        ff[k] = std::make_unique<FfmpegFrames>();
        if (!ff[k]->open(decode_args(o, facts, tracks[k], convert && o.auto_rotate,
                                     true, filters),
                         false, error))
            return false;
    }

    // Only the best instant of the window is held: the window's argmax, as the
    // built-in decoder takes it, without keeping `keep` pictures a track.
    SelectClock clock{o.keep, o.skip, plan, 0};
    std::vector<nn::Image> cur(n), best(n);
    std::vector<double> score(n), score_ms(n);
    std::vector<std::string> errs(n);
    double best_score = -1.0;
    int64_t best_index = -1, i = -1;
    bool held = false;
    int written = 0;

    while (o.max_frames <= 0 || written < o.max_frames) {
        if (sinks.cancel && sinks.cancel->load()) {
            error = "cancelled";
            return false;
        }
        int64_t next = i + 1;
        if (filtered)
            while (!clock.candidate(next)) ++next;
        const bool cand = filtered || clock.candidate(next);
        const bool measure = cand && keep != 0;
        auto read = [&](size_t k) {
            nn::Image& im = cur[k];
            if (!ff[k]->next(im.data, im.width, im.height, errs[k])) return false;
            im.channels = 3;
            if (measure) {
                const double t0 = nn::now_ms();
                score[k] = sharpness_score(im.data.data(), im.width, im.height);
                score_ms[k] = nn::now_ms() - t0;
            }
            return true;
        };
        const double t0 = nn::now_ms();
        bool got = true;
        if (n == 1) {
            got = read(0);
        } else {
            std::vector<std::future<bool>> pending;
            for (size_t k = 0; k < n; k++)
                pending.push_back(std::async(std::launch::async, read, k));
            for (auto& p : pending) got = p.get() && got;
        }
        const double scoring =
            measure ? *std::max_element(score_ms.begin(), score_ms.end()) : 0.0;
        t.decode += nn::now_ms() - t0 - scoring;
        t.sharpness += scoring;
        if (!got) {
            for (size_t k = 0; k < n; k++)
                if (!errs[k].empty()) {
                    error = errs[k];
                    return false;
                }
            break;   // end of stream
        }
        t.decoded += next - i;
        i = next;
        if (!cand) continue;
        if (measure) ++t.measured;

        double s = 0;
        for (size_t k = 0; k < n; k++) s += measure ? score[k] : 0.0;
        if (!held || s > best_score) {
            best.swap(cur);
            best_score = s;
            best_index = i;
            held = true;
        }
        if (!clock.step(i)) continue;
        std::string err;
        if (!on_frame(best, best_index, err)) {
            log_line(sinks, "frame " + std::to_string(best_index) + ": " + err);
        } else {
            ++written;
            if (sinks.progress) sinks.progress(t.written, t.decoded);
        }
        held = false;
    }
    return true;
}

}  // namespace app
