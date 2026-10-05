// FrameDecodeVulkan.cpp -- app/FrameDecode.h's in-process decoder, and the
// entry points of app/FrameExtract.h that exist only where it does.

#include "app/FrameDecode.h"

#include "nn/core/Log.h"
#include "nn/Device.h"
#include "sam/Sam.h"
#include "video/Demuxer.h"
#include "video/VideoPipeline.h"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

namespace app {

// ---------------------------------------------------------------------------
// Adaptive selection
// ---------------------------------------------------------------------------

// Pass one of an adaptive run: ONE track, reduced on the GPU so a frame costs
// a few tens of kilobytes over the bus. Decoding twice is cheaper than holding
// a video's worth of pictures until the plan is known.
bool measure_motion_vulkan(const FrameExtractJob& o, const FrameExtractSinks& sinks,
                           int track, int tracks, MotionPlanInput& out,
                           FrameExtractStats& t, std::string& error) {
    const double t_start = nn::now_ms();
    video::VideoPipeline pipe;
    if (!pipe.open(o.input, track, 1, error)) return false;
    const video::TrackInfo& info = pipe.track();

    MotionOptions mo;
    // A capture that sees the whole sphere: the 360 packing, or the two square
    // tracks every dual-fisheye camera writes.
    if (o.eac.valid()) mo.view = MotionView::Packed360;
    else if (tracks >= 2 && info.width == info.height)
        mo.view = MotionView::Fisheye;
    mo.eac = o.eac;
    mo.out_fov = mo.view == MotionView::Fisheye ? mo.circle_fov
                                           : motion_out_fov(o.views);
    motion_frame_size(mo.view, info.width, info.height, mo.width, mo.height);

    MotionTracker tracker(mo);
    const double fps = info.fps > 1.0 ? info.fps : 30.0;
    // 15 samples a second is enough to follow a walk, but never coarser than a
    // third of the spacing being planned: a gap can only land on a sample.
    const int stride = std::max(
        1, std::min((int)std::lround(fps / 15.0), std::max(1, o.skip / 3)));

    // Tracking is CPU and decoding is GPU, so they run at the same time: the
    // decoder fills a short queue and one consumer thread owns the tracker.
    struct Sample {
        std::vector<uint8_t> gray;
        int64_t index = 0;
    };
    std::deque<Sample> queue;
    std::mutex mu;
    std::condition_variable room, work;
    bool feeding = true;
    size_t reported = 0;
    std::thread consumer([&] {
        for (;;) {
            Sample s;
            {
                std::unique_lock<std::mutex> lk(mu);
                work.wait(lk, [&] { return !queue.empty() || !feeding; });
                if (queue.empty()) return;
                s = std::move(queue.front());
                queue.pop_front();
            }
            room.notify_one();
            tracker.track(s.gray.data(), s.index);
            // Read on the thread that appended them, so the vectors need no
            // lock of their own; finish() may still revise what is already out.
            if (sinks.measured)
                for (; reported < tracker.costs().size(); reported++)
                    sinks.measured(tracker.ends()[reported], info.frame_count,
                                   tracker.costs()[reported]);
        }
    });
    auto stop = [&]() {
        {
            std::lock_guard<std::mutex> lk(mu);
            feeding = false;
        }
        work.notify_all();
        consumer.join();
    };

    std::vector<uint8_t> gray;
    int64_t last = -1;
    for (;;) {
        if (sinks.cancel && sinks.cancel->load()) {
            error = "cancelled";
            stop();
            return false;
        }
        video::FrameHandle h;
        if (!pipe.next(h, error)) {
            if (!error.empty()) {
                stop();
                return false;
            }
            break;
        }
        if (h.index % stride == 0) {
            if (!pipe.toGray(h, mo.width, mo.height, gray, error)) {
                pipe.release(h);
                stop();
                return false;
            }
            std::unique_lock<std::mutex> lk(mu);
            room.wait(lk, [&] { return queue.size() < 3; });
            queue.push_back({gray, h.index});
            lk.unlock();
            work.notify_one();
            ++t.analyzed;
            if (sinks.scanning)
                sinks.scanning(h.index + 1, info.frame_count);
        }
        last = h.index;
        pipe.release(h);
    }
    stop();
    tracker.finish();
    out.cost = tracker.costs();
    out.step = tracker.steps();
    out.ends = tracker.ends();
    out.frames = info.frame_count > 0 ? info.frame_count : last + 1;
    out.skip = o.skip;
    out.window = std::max(o.keep, 1);
    out.max_frames = o.max_frames;
    out.fps = fps;
    out.view = mo.view;
    out.out_fov = mo.out_fov;
    t.plan = nn::now_ms() - t_start;
    return true;
}

// ---------------------------------------------------------------------------
// Extraction
// ---------------------------------------------------------------------------

// Several tracks decoded in lockstep under one sharpness window; the score is
// summed over the tracks.
bool decode_lockstep_vulkan(const FrameExtractJob& o, const FrameExtractSinks& sinks,
                            const std::vector<int>& tracks, bool convert,
                            const std::vector<int64_t>& plan, FrameExtractStats& t,
                            std::string& error, const InstantSink& on_frame) {
    const size_t n = tracks.size();
    video::ConvertOpts conv;
    if (convert) {
        conv.scale = o.scale;
        conv.rotate = o.rotate;
    }
    std::vector<std::unique_ptr<video::VideoPipeline>> pipe;
    for (size_t k = 0; k < n; k++) {
        pipe.push_back(std::make_unique<video::VideoPipeline>());
        if (!pipe[k]->open(o.input, tracks[k], std::max(o.keep, 1), error)) return false;
    }

    struct Buffered {
        std::vector<video::FrameHandle> h;
        int64_t index = 0;
    };
    std::deque<Buffered> window;
    const int keep = o.keep;
    SelectClock clock{o.keep, o.skip, plan, 0};
    int written = 0;
    bool measured_pending = false;

    auto release = [&](Buffered& b) {
        for (size_t k = 0; k < n; k++) pipe[k]->release(b.h[k]);
    };
    auto drain = [&]() {
        for (auto& b : window) release(b);
        window.clear();
    };

    auto flush_window = [&](bool write) {
        if (window.empty()) return;
        if (write) {
            if (measured_pending) {
                const double t0 = nn::now_ms();
                for (size_t k = 0; k < n; k++) pipe[k]->flushSharpness();
                t.sharpness += nn::now_ms() - t0;
                measured_pending = false;
            }
            size_t best = 0;
            if (keep != 0) {
                float best_score = -1.0f;
                for (size_t i = 0; i < window.size(); ++i) {
                    // Each track sees its share of the scene; the sum is the
                    // whole capture's.
                    float s = 0;
                    for (size_t k = 0; k < n; k++) s += pipe[k]->sharpness(window[i].h[k]);
                    if (s > best_score) {
                        best_score = s;
                        best = i;
                    }
                }
            }
            const Buffered& chosen = window[best];

            double t0 = nn::now_ms();
            std::vector<nn::Image> imgs(n);
            std::string err;
            bool ok = true;
            for (size_t k = 0; k < n && ok; k++)
                ok = pipe[k]->toImage(chosen.h[k], conv, imgs[k], err);
            t.convert += nn::now_ms() - t0;
            if (ok) ok = on_frame(imgs, chosen.index, err);
            if (!ok) {
                log_line(sinks, "frame " + std::to_string(chosen.index) + ": " + err);
            } else {
                ++written;
                if (sinks.progress) sinks.progress(t.written, t.decoded);
            }
        }
        drain();
    };

    while (o.max_frames <= 0 || written < o.max_frames) {
        if (sinks.cancel && sinks.cancel->load()) {
            error = "cancelled";
            drain();
            return false;
        }
        Buffered b;
        b.h.resize(n);
        const double t0 = nn::now_ms();
        bool got = true;
        for (size_t k = 0; k < n && got; k++) got = pipe[k]->next(b.h[k], error);
        t.decode += nn::now_ms() - t0;
        if (!got) {
            // One track ending first leaves the others' pictures held; release
            // them so a future caller can reopen without a full pool.
            for (size_t k = 0; k < n; k++)
                if (b.h[k].valid()) pipe[k]->release(b.h[k]);
            drain();
            return error.empty();   // end of stream
        }
        ++t.decoded;

        b.index = b.h[0].index;
        if (!clock.candidate(b.index)) {
            release(b);
            continue;
        }
        if (keep != 0) {
            const double t1 = nn::now_ms();
            for (size_t k = 0; k < n; k++) pipe[k]->queueSharpness(b.h[k]);
            t.sharpness += nn::now_ms() - t1;
            measured_pending = true;
            ++t.measured;
        }
        window.push_back(std::move(b));
        if ((int)window.size() > std::max(keep, 1)) {
            release(window.front());
            window.pop_front();
        }
        if (clock.step(b.index)) flush_window(true);
    }
    flush_window(false);
    return true;
}

// ---------------------------------------------------------------------------
// Entry points
// ---------------------------------------------------------------------------

std::string video_decode_availability() {
    return video::VideoPipeline::availability();
}

int video_track_count(const std::string& path, std::string& error) {
    // The demuxer alone: creating a video session per track just to count them
    // would be several hundred megabytes of DPB for nothing.
    auto demux = video::open_demuxer(path, error);
    if (!demux) return 0;
    return (int)demux->tracks().size();
}

std::vector<std::pair<int, int>> video_track_sizes(const std::string& path,
                                                   std::string& error) {
    std::vector<std::pair<int, int>> out;
    auto demux = video::open_demuxer(path, error);
    if (!demux) return out;
    for (const video::TrackInfo& t : demux->tracks())
        out.emplace_back(t.width, t.height);
    return out;
}

std::vector<sfm::ExifTransform> video_track_turns(const std::string& path,
                                                  std::string& error) {
    std::vector<sfm::ExifTransform> out;
    auto demux = video::open_demuxer(path, error);
    if (!demux) return out;
    for (const video::TrackInfo& t : demux->tracks())
        out.push_back({((t.rotate % 360) + 360) % 360 / 90, t.mirror});
    return out;
}

sfm::ExifTransform fold_auto_rotate(const std::string& path,
                                    const std::vector<int>& tracks,
                                    FrameLook& look, bool& mixed) {
    mixed = false;
    if (!look.auto_rotate || look.pano()) return {};
    std::string probe_error;
    const sfm::ExifTransform t =
        merge_turns(video_track_turns(path, probe_error), tracks, mixed);
    look.rotate = (look.rotate + 90 * t.turns_cw) % 360;
    return t;
}

bool extract_frames_at(const std::string& input, const FrameLook& look_in,
                       const std::vector<int64_t>& indices, int folder,
                       const FrameAtSink& on_frame,
                       const std::atomic<bool>* cancel, std::string& error,
                       const std::string& device) {
    // A preview decodes on the run's device, so a click drawn here names a
    // pixel the run will read.
    if (!sam::freeze_device(device, error)) return false;

    error = video_decode_availability();
    if (!error.empty()) return false;

    const int n = video_track_count(input, error);
    if (n <= 0) {
        if (error.empty()) error = "no video track in " + input;
        return false;
    }
    std::vector<int64_t> want;
    for (int64_t i : indices) want.push_back(std::max<int64_t>(i, 0));
    std::sort(want.begin(), want.end());
    want.erase(std::unique(want.begin(), want.end()), want.end());
    if (want.empty()) return true;

    std::vector<int> tracks;
    for (int i = 0; i < n; i++) tracks.push_back(i);
    FrameLook look = look_in;
    bool mixed = false;
    fold_auto_rotate(input, tracks, look, mixed);
    if (look.rotate % 90 != 0) {
        error = "rotation must be a multiple of 90 degrees";
        return false;
    }
    const bool pano = look.pano();
    if (pano && n < 2) {
        error = "a 360 capture needs two video tracks";
        return false;
    }

    video::ConvertOpts conv;
    if (!pano) {
        conv.scale = look.scale;
        conv.rotate = look.rotate;
    }
    const size_t np = pano && pano360_needs_track1(look.eac) ? 2 : 1;
    std::vector<std::unique_ptr<video::VideoPipeline>> pipe(np);
    for (size_t k = 0; k < np; k++) {
        pipe[k] = std::make_unique<video::VideoPipeline>();
        const int t = pano                    ? (int)k
                      : look.packed_lenses >= 2 ? 0
                                                : std::min(std::max(folder, 0), n - 1);
        // Two: the frame being looked at, and the one after it, so the last
        // picture decoded is still held when the stream ends.
        if (!pipe[k]->open(input, t, 2, error)) return false;
    }

    Pano360Remap map;
    std::vector<uint8_t> canvas;
    if (pano) {
        const size_t view =
            std::min((size_t)std::max(folder, 0), look.views.size() - 1);
        pano360_remap(look.eac, look.views[view], map);
        canvas.resize((size_t)look.eac.canvasW() * look.eac.canvasH() * 3);
    }

    // Straight to the keyframe that covers the first index wanted. Without it
    // the last frame of a fifteen-minute capture is a fifteen-minute decode,
    // and twice that for the two tracks of a 360 file.
    int64_t floor_index = 0;
    for (size_t k = 0; k < np; k++) {
        int64_t landed = 0;
        std::string seek_error;
        if (pipe[k]->seek(want.front(), landed, seek_error))
            floor_index = std::max(floor_index, landed);
    }

    std::vector<video::FrameHandle> held(np);
    auto release = [&](std::vector<video::FrameHandle>& h) {
        for (size_t k = 0; k < np; k++)
            if (h[k].valid()) pipe[k]->release(h[k]);
    };
    auto emit = [&](int64_t index) {
        std::vector<nn::Image> img(np);
        for (size_t k = 0; k < np; k++)
            if (!pipe[k]->toImage(held[k], conv, img[k], error)) return false;
        if (!pano) {
            if (look.packed_lenses >= 2) {
                std::vector<uint8_t> lens;
                packed_lens_crop(img[0].data.data(), img[0].width, img[0].height,
                                 img[0].channels, look.packed_lenses, folder, lens,
                                 img[0].width);
                img[0].data.swap(lens);
            }
            on_frame(img[0], index);
            return true;
        }
        for (size_t k = 0; k < np; k++)
            if (img[k].width != look.eac.track_w ||
                img[k].height != look.eac.track_h) {
                error = "track " + std::to_string(k) + " is not the size the"
                        " 360 layout was detected at";
                return false;
            }
        pano360_canvas(look.eac, img[0].data.data(),
                       np > 1 ? img[1].data.data() : nullptr, canvas.data());
        nn::Image out;
        out.width = map.width;
        out.height = map.height;
        out.channels = 3;
        out.data.resize((size_t)map.width * map.height * 3);
        pano360_apply(map, canvas.data(), look.eac.canvasW(),
                      look.eac.canvasH(), 0, out.data.data());
        on_frame(out, index);
        return true;
    };

    size_t next = 0;
    bool ended = false;
    while (next < want.size() && !ended) {
        if (cancel && cancel->load()) {
            release(held);
            error = "cancelled";
            return false;
        }
        // Every track brought up to `floor_index` -- and no further, or a
        // track that landed on a later keyframe than another could never be
        // caught up with. A frame already there is left where it is.
        for (size_t k = 0; k < np && !ended; k++) {
            while (!held[k].valid() || held[k].index < floor_index) {
                video::FrameHandle h;
                if (!pipe[k]->next(h, error)) {
                    ended = true;
                    break;
                }
                if (held[k].valid()) pipe[k]->release(held[k]);
                held[k] = h;
            }
        }
        if (ended) break;
        // Presentation order from the decoder, which is what extraction names
        // its files after -- not a count of how many have gone by.
        int64_t index = held[0].index;
        bool aligned = true;
        for (size_t k = 1; k < np; k++) {
            index = std::max(index, held[k].index);
            aligned = aligned && held[k].index == held[0].index;
        }
        floor_index = index;
        if (!aligned) continue;   // a track is behind; pull it up next round
        ++floor_index;
        while (next < want.size() && want[next] < index) ++next;
        if (next < want.size() && want[next] == index) {
            if (!emit(index)) {
                release(held);
                return false;
            }
            ++next;
        }
    }
    bool ok = error.empty();
    if (ok && next < want.size() && held[0].valid()) ok = emit(want[next]);
    release(held);
    return ok;
}

bool extract_one_frame(const std::string& input, const FrameLook& look,
                       int64_t index, int folder, nn::Image& out,
                       const std::atomic<bool>* cancel, std::string& error,
                       const std::string& device) {
    bool got = false;
    const bool ok = extract_frames_at(
        input, look, {index}, folder,
        [&](nn::Image& img, int64_t) {
            out = std::move(img);
            got = true;
        },
        cancel, error, device);
    if (ok && !got && error.empty()) error = "no frame could be decoded";
    return ok && got;
}

}  // namespace app
