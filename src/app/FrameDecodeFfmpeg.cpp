// FrameDecodeFfmpeg.cpp -- app/FrameDecode.h's ffmpeg decoder: one child per
// track, each writing pictures to a pipe, read in lockstep.

#include "app/FrameDecode.h"
#include "app/CubeLut.h"

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

double span_fps(const VideoFacts& f) { return f.fps > 1.0 ? f.fps : 30.0; }

// Everything before the output. Every decoded frame comes out exactly once, in
// presentation order: a frame's place in this stream, plus the clip's first
// frame, is the index it is named by, as it is for the built-in decoder.
std::vector<std::string> decode_args(const FrameExtractJob& o, const VideoFacts& f,
                                     int track, bool auto_rotate,
                                     const std::string& filters, const FrameSpan& span) {
    static const bool fps_mode = has_fps_mode(o.ffmpeg_exe);
    const int stream = track < (int)f.streams.size() ? f.streams[track] : -1;
    std::vector<std::string> a{o.ffmpeg_exe, "-nostdin", "-hide_banner",
                               "-loglevel", "error", "-hwaccel", "none"};
    if (!auto_rotate) a.push_back("-noautorotate");
    if (span.first > 0) {
        // Half a frame early, so rounding cannot skip the first one. Exact on
        // a constant-rate file; a variable-rate one is off by what it varies.
        char at[32];
        std::snprintf(at, sizeof at, "%.6f", ((double)span.first - 0.5) / span_fps(f));
        a.insert(a.end(), {"-ss", at});
    }
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

struct Rgb48Frame {
    std::vector<uint8_t> data;
    int width = 0, height = 0;
};

void size_rgb8(const Rgb48Frame& src, nn::Image& dst) {
    dst.width = src.width;
    dst.height = src.height;
    dst.channels = 3;
    dst.data.resize((size_t)src.width * src.height * 3);
}

void quantize_rgb48be(const Rgb48Frame& src, nn::Image& dst) {
    size_rgb8(src, dst);
    for (size_t i = 0; i < dst.data.size(); i++) {
        const unsigned v = (unsigned)src.data[i * 2] * 256 + src.data[i * 2 + 1];
        dst.data[i] = (uint8_t)((v + 128) / 257);
    }
}

// Match the native luma thumbnail's box sampling and rounding.
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
    const double fps = span_fps(facts);
    const int stride = std::max(
        1, std::min((int)std::lround(fps / 15.0), std::max(1, o.skip / 3)));
    const FrameSpan span = clip_frames(o, fps);
    const int64_t span_n = span.length(facts.frames);

    // Retain every source frame so motion plans keep their source indices.
    FfmpegFrames ff;
    if (!ff.open(decode_args(o, facts, track, false, "extractplanes=y,format=gray",
                             span),
                 FfmpegFrameFormat::Gray8, error))
        return false;

    std::vector<uint8_t> luma, gray((size_t)mo.width * mo.height);
    int lw = 0, lh = 0;
    int64_t index = span.first - 1;
    size_t reported = 0;
    while (ff.next(luma, lw, lh, error)) {
        if (sinks.cancel && sinks.cancel->load()) {
            error = "cancelled";
            return false;
        }
        ++index;
        if (span.past(index)) {
            ff.close();
            break;
        }
        if (index % stride != 0) continue;
        luma_box(luma.data(), lw, lh, mo.width, mo.height, gray.data());
        tracker.track(gray.data(), index);
        ++t.analyzed;
        if (sinks.measured)
            for (; reported < tracker.costs().size(); reported++)
                sinks.measured(tracker.ends()[reported], facts.frames,
                               tracker.costs()[reported]);
        if (sinks.scanning) sinks.scanning(index + 1 - span.first, span_n);
    }
    if (!error.empty()) return false;
    tracker.finish();
    out.cost = tracker.costs();
    out.step = tracker.steps();
    out.ends = tracker.ends();
    // A clip's end stops the decode short of the whole video's count.
    out.frames = span.end >= 0 ? std::max<int64_t>(span.end, facts.frames) : index + 1;
    if (span.first > 0 || span.end >= 0) out.span = span.length(out.frames);
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
    std::shared_ptr<const CubeLut> lut;
    if (!o.lut.empty()) {
        lut = cube_lut_cached(o.lut, error);
        if (!lut) return false;
    }
    // A fixed rate is dropped by ffmpeg before the colour conversion, and the
    // k-th picture out is the clock's k-th candidate. A plan is thousands of
    // windows, too many for a filter expression, so every frame comes over.
    const bool filtered = plan.empty();
    const FrameSpan span = clip_frames(o, span_fps(facts));
    std::string select;
    if (filtered) {
        // `n` restarts at the clip; the clock counts from the file's start.
        char b[128];
        const long long at = (long long)span.first;
        if (keep == 0)
            std::snprintf(b, sizeof b, "select='not(mod(n+%lld,%d))'", at, o.skip);
        else
            std::snprintf(b, sizeof b, "select='lt(mod(n+%lld,%d),%d)'", at + keep,
                          o.skip, keep);
        select = b;
    }
    const std::string filters =
        join_filters({select, convert ? convert_filters(o) : "",
                      lut ? "format=rgb48be" : "format=rgb24"});
    std::vector<std::unique_ptr<FfmpegFrames>> ff(n);
    for (size_t k = 0; k < n; k++) {
        ff[k] = std::make_unique<FfmpegFrames>();
        if (!ff[k]->open(decode_args(o, facts, tracks[k], convert && o.auto_rotate,
                                     filters, span),
                         lut ? FfmpegFrameFormat::Rgb48Be : FfmpegFrameFormat::Rgb8,
                         error))
            return false;
    }

    // Only the best instant of the window is held: the window's argmax, as the
    // built-in decoder takes it, without keeping `keep` pictures a track.
    SelectClock clock{o.keep, o.skip, plan, 0};
    clock.span = span;
    PickRule rule{o.blur_reject, o.blur_neighbour, o.similar_bits};
    const bool judge = rule.on() || (bool)sinks.judged;
    std::unique_ptr<EdgeMeter> meter;
    if (rule.max_blur > 0.0f) meter = std::make_unique<EdgeMeter>();
    std::vector<nn::Image> cur(lut ? 0 : n), best(n);
    std::vector<Rgb48Frame> wide_cur(lut ? n : 0), wide_best(lut ? n : 0);
    std::vector<std::vector<float>> score_scratch(n);
    auto grade = [&](size_t k) {
        size_rgb8(wide_best[k], best[k]);
        apply_cube_lut_rgb48be(*lut, wide_best[k].data.data(), best[k].data.data(),
                              (size_t)best[k].width * best[k].height);
    };
    std::vector<double> score(n), score_ms(n);
    std::vector<std::string> errs(n);
    double best_score = -1.0;
    int64_t best_index = -1, i = span.first - 1;
    bool held = false;
    int written = 0;

    while (o.max_frames <= 0 || written < o.max_frames) {
        if (sinks.cancel && sinks.cancel->load()) {
            error = "cancelled";
            return false;
        }
        int64_t next = i + 1;
        if (filtered)
            while (!span.past(next) && !clock.candidate(next)) ++next;
        if (span.past(next)) break;
        const bool cand = filtered || clock.candidate(next);
        const bool measure = cand && keep != 0;
        auto read = [&](size_t k) {
            auto read_picture = [&](auto& im) {
                if (!ff[k]->next(im.data, im.width, im.height, errs[k])) return false;
                if (measure) {
                    const double t0 = nn::now_ms();
                    score[k] = ff[k]->sample_bytes() == 2
                                   ? sharpness_score_rgb48be(im.data.data(), im.width,
                                                            im.height, score_scratch[k])
                                   : sharpness_score(im.data.data(), im.width, im.height,
                                                     score_scratch[k]);
                    score_ms[k] = nn::now_ms() - t0;
                }
                return true;
            };
            if (lut) return read_picture(wide_cur[k]);
            cur[k].channels = 3;
            return read_picture(cur[k]);
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
            if (lut) wide_best.swap(wide_cur);
            else best.swap(cur);
            best_score = s;
            best_index = i;
            held = true;
        }
        if (!clock.step(i)) continue;
        held = false;
        bool graded_first = false;
        if (judge) {
            FramePick p;
            p.index = best_index;
            const double t1 = nn::now_ms();
            // Reuse track 0's output for ungraded judgement, then overwrite it.
            if (lut) quantize_rgb48be(wide_best[0], best[0]);
            measure_pick(p, best[0].data.data(), best[0].width, best[0].height, meter.get());
            t.sharpness += nn::now_ms() - t1;
            rule.judge(p);
            if (sinks.judged) {
                if (lut) {
                    grade(0);
                    graded_first = true;
                }
                sinks.judged(p, best[0]);
            }
            if (p.verdict != PickVerdict::Kept) {
                ++(p.verdict == PickVerdict::Blurry ? t.blurry : t.similar);
                continue;
            }
        }
        if (lut)
            for (size_t k = 0; k < n; k++)
                if (k != 0 || !graded_first) grade(k);
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
