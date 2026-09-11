#include "sam/Masking.h"

#include "nn/core/Parallel.h"

#include <algorithm>
#include <cstring>
#include <map>

namespace sam {

std::vector<std::string> split_phrases(const std::string& s) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= s.size()) {
        const size_t end = s.find(';', start);
        std::string p = s.substr(start, end == std::string::npos ? std::string::npos
                                                                 : end - start);
        const size_t b = p.find_first_not_of(" \t");
        const size_t e = p.find_last_not_of(" \t");
        if (b != std::string::npos) out.push_back(p.substr(b, e - b + 1));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return out;
}

nn::Image downscale_to_fit(const nn::Image& src, int max_size) {
    const int longest = std::max(src.width, src.height);
    if (max_size <= 0 || longest <= max_size || src.empty()) return src;

    // PIL's Image.resize((w, h)) rounds the target down, as mask.py's
    // int(size * scale) does; the sampling below is the plain box-free bilinear
    // the model's own preprocessing would have applied anyway.
    const double sc = (double)max_size / (double)longest;
    nn::Image out;
    out.width = std::max(1, (int)(src.width * sc));
    out.height = std::max(1, (int)(src.height * sc));
    out.channels = src.channels;
    out.data.resize((size_t)out.width * out.height * 3);

    const double sx = (double)src.width / out.width;
    const double sy = (double)src.height / out.height;
    // Row-parallel: at 4K in, this is tens of milliseconds of the per-frame
    // budget on one thread and a rounding error on many, and every caller runs
    // it once per frame with the GPU idle behind it.
    nn::parallel_for(out.height, [&](int64_t y_lo, int64_t y_hi) {
    for (int y = (int)y_lo; y < (int)y_hi; ++y) {
        double fy = (y + 0.5) * sy - 0.5;
        if (fy < 0.0) fy = 0.0;
        const int y0 = (int)fy;
        const int y1 = (y0 < src.height - 1) ? y0 + 1 : y0;
        const double wy = fy - y0;
        for (int x = 0; x < out.width; ++x) {
            double fx = (x + 0.5) * sx - 0.5;
            if (fx < 0.0) fx = 0.0;
            const int x0 = (int)fx;
            const int x1 = (x0 < src.width - 1) ? x0 + 1 : x0;
            const double wx = fx - x0;
            for (int c = 0; c < 3; ++c) {
                const double p00 = src.data[((size_t)y0 * src.width + x0) * 3 + c];
                const double p01 = src.data[((size_t)y0 * src.width + x1) * 3 + c];
                const double p10 = src.data[((size_t)y1 * src.width + x0) * 3 + c];
                const double p11 = src.data[((size_t)y1 * src.width + x1) * 3 + c];
                const double v = (1 - wy) * ((1 - wx) * p00 + wx * p01) +
                                 wy * ((1 - wx) * p10 + wx * p11);
                out.data[((size_t)y * out.width + x) * 3 + c] =
                    (uint8_t)std::min(255, std::max(0, (int)(v + 0.5)));
            }
        }
    }
    }, /*min_chunk=*/16);
    return out;
}

namespace {

// Nearest-neighbour, matching mask.py's Image.NEAREST upscale back to the
// source resolution. Anything smoother would put grey along every edge of a
// mask that is by definition binary.
void upscale_nearest(const std::vector<uint8_t>& src, int sw, int sh,
                     std::vector<uint8_t>& dst, int dw, int dh) {
    dst.resize((size_t)dw * dh);
    nn::parallel_for(dh, [&](int64_t y_lo, int64_t y_hi) {
        for (int y = (int)y_lo; y < (int)y_hi; ++y) {
            const int sy = std::min(sh - 1, (int)((int64_t)y * sh / dh));
            for (int x = 0; x < dw; ++x) {
                const int sx = std::min(sw - 1, (int)((int64_t)x * sw / dw));
                dst[(size_t)y * dw + x] = src[(size_t)sy * sw + sx];
            }
        }
    }, /*min_chunk=*/64);
}

}  // namespace

struct Masker::Impl {
    sam::Session session;
    // One tracker per positive phrase, plus -- when there are clicks -- one
    // more that only ever propagates what was seeded into it by hand.
    std::vector<std::unique_ptr<sam::Tracker>> trackers;
    std::unique_ptr<sam::Tracker> visual;
    std::vector<std::string> pos, neg;
    MaskOptions opts;
    int64_t frame = 0;
    // Seeds not yet applied, in frame order, and the instance each object was
    // given when its first seed landed.
    std::vector<SeedPrompt> pending;
    std::map<int, int> instance_of;

    static void append(sam::Result& all, const sam::Result& r) {
        all.detections.insert(all.detections.end(), r.detections.begin(),
                              r.detections.end());
    }

    // Clicks arrive in pixels of the caller's image; the model sees the
    // downscaled one.
    static sam::VisualPrompt scaled_prompt(const sam::VisualPrompt& in, double fx,
                                           double fy) {
        sam::VisualPrompt vp = in;
        for (auto& p : vp.pos_points) { p.x = (float)(p.x * fx); p.y = (float)(p.y * fy); }
        for (auto& p : vp.neg_points) { p.x = (float)(p.x * fx); p.y = (float)(p.y * fy); }
        vp.box.x0 = (float)(vp.box.x0 * fx);
        vp.box.x1 = (float)(vp.box.x1 * fx);
        vp.box.y0 = (float)(vp.box.y0 * fy);
        vp.box.y1 = (float)(vp.box.y1 * fy);
        return vp;
    }

    // Only the overlay uses this; the pipeline's own mask_bounding_box lives
    // behind sam/pipeline/, which this file is deliberately not part of.
    static sam::Box bounding_box(const sam::Mask& m) {
        int x0 = m.width, y0 = m.height, x1 = -1, y1 = -1;
        for (int y = 0; y < m.height; ++y)
            for (int x = 0; x < m.width; ++x)
                if (m.data[(size_t)y * m.width + x] > 127) {
                    if (x < x0) x0 = x;
                    if (x > x1) x1 = x;
                    if (y < y0) y0 = y;
                    if (y > y1) y1 = y;
                }
        if (x1 < 0) return {};
        return {(float)x0, (float)y0, (float)x1, (float)y1};
    }
};

Masker::Masker() : impl_(new Impl()) {}
Masker::~Masker() = default;

const std::string& Masker::lastError() const { return impl_->session.lastError(); }
sam::Session& Masker::session() { return impl_->session; }

bool Masker::init(const MaskOptions& o, std::string& error) {
    // init() is also how a policy is *changed* -- the mask preview calls it on
    // every edit -- so everything a previous run accumulated goes first. The
    // trackers hold a reference to the session and must not outlive its model.
    impl_->trackers.clear();
    impl_->visual.reset();
    impl_->instance_of.clear();
    impl_->frame = 0;
    impl_->opts = o;
    impl_->pos = split_phrases(o.text);
    impl_->neg = split_phrases(o.neg_text);
    impl_->pending = o.seeds;
    // Applied in frame order regardless of the order they were given in, so a
    // correction drawn on frame 200 cannot land before the click on frame 3
    // that created the object it corrects.
    std::stable_sort(impl_->pending.begin(), impl_->pending.end(),
                     [](const SeedPrompt& a, const SeedPrompt& b) {
                         return a.frame < b.frame;
                     });
    if (impl_->pos.empty() && impl_->pending.empty()) {
        error = "nothing to segment: give --text, or --point to click on an object";
        return false;
    }

    sam::ModelParams mp;
    mp.model_path = o.model;
    mp.device_match = o.device;
    mp.validation = o.validate;
    mp.profile = o.profile;
    mp.img_size = o.img_size;
    if (!impl_->session.loadModel(mp)) {
        error = impl_->session.lastError();
        return false;
    }
    if (!impl_->pos.empty() && !impl_->session.supportsTextPrompts()) {
        error = "this checkpoint has no text encoder, so --text cannot be used; seed an "
                "instance with --point instead (SAM 2 checkpoints are visual-only)";
        return false;
    }

    if (o.video) {
        // One tracker per positive phrase, all reading the same backbone pass.
        auto make = [&](const std::string& phrase) {
            sam::VideoParams vp;
            vp.text_prompt = phrase;
            vp.score_threshold = o.threshold;
            vp.nms_threshold = o.nms;
            vp.detect_every = o.detect_every;
            vp.max_memory_frames = o.memory_frames;
            return std::make_unique<sam::Tracker>(impl_->session, vp);
        };
        for (const std::string& p : impl_->pos) impl_->trackers.push_back(make(p));
        // Clicked objects get their own tracker with no text, so the detector
        // never invents a second instance of something the user pointed at.
        if (!impl_->pending.empty()) impl_->visual = make("");
    }
    return true;
}

bool Masker::run(const nn::Image& image, sam::Mask& out, sam::Result* overlay_out,
                 int64_t frame_id) {
    Impl& s = *impl_;
    if (frame_id < 0) frame_id = s.frame;
    const nn::Image scaled = downscale_to_fit(image, s.opts.max_size);
    const size_t n = (size_t)scaled.width * scaled.height;
    const double fx = (double)scaled.width / std::max(1, image.width);
    const double fy = (double)scaled.height / std::max(1, image.height);
    std::vector<uint8_t> hit(n, 0);
    sam::Result all;

    if (!s.session.encodeImage(scaled)) return false;

    if (s.opts.video) {
        for (auto& t : s.trackers) {
            sam::Result r = t->trackEncoded();
            Impl::append(all, r);
        }
        if (s.visual) {
            // Propagate first: addInstance below attributes the instance to the
            // frame the tracker has just finished, and on an empty tracker this
            // is a no-op anyway.
            sam::Result r = s.visual->trackEncoded();
            Impl::append(all, r);

            // Everything drawn at or before this frame that has not been used
            // yet. "At or before" and not "on", because the frame a click was
            // drawn on may be one this run never sees -- the extractor keeps
            // the sharpest frame of each window and drops the rest.
            while (!s.pending.empty() && s.pending.front().frame <= frame_id) {
                const SeedPrompt seed = s.pending.front();
                s.pending.erase(s.pending.begin());
                const sam::VisualPrompt vp =
                    Impl::scaled_prompt(seed.prompt, fx, fy);
                // The propagation above ran before this instance existed, so
                // its mask for THIS frame comes back from the seeding call
                // itself rather than from that Result.
                sam::Result seeded;
                seeded.detections.emplace_back();
                sam::Mask& m = seeded.detections.back().mask;
                auto it = s.instance_of.find(seed.object);
                if (it == s.instance_of.end()) {
                    const int id = s.visual->addInstance(vp, &m);
                    if (id < 0) return false;
                    s.instance_of[seed.object] = id;
                } else if (!s.visual->refineInstance(it->second, vp.pos_points,
                                                     vp.neg_points, &m)) {
                    return false;
                }
                seeded.detections.back().instance_id = m.instance_id;
                seeded.detections.back().score = m.iou_score;
                seeded.detections.back().box = Impl::bounding_box(m);
                Impl::append(all, seeded);
            }
        }
    } else {
        for (const std::string& phrase : s.pos) {
            sam::ConceptPrompt cp;
            cp.text = phrase;
            cp.score_threshold = s.opts.threshold;
            cp.nms_threshold = s.opts.nms;
            sam::Result r = s.session.segmentConcept(cp);
            Impl::append(all, r);
        }
        // Without a memory bank a click means nothing on any frame but its own,
        // so only the seeds drawn on this one are used -- one segmentation per
        // object, unioned, never one prompt holding everybody's clicks.
        std::map<int, sam::VisualPrompt> per_object;
        for (const SeedPrompt& seed : s.opts.seeds) {
            if (seed.frame != frame_id) continue;
            sam::VisualPrompt& vp = per_object[seed.object];
            const sam::VisualPrompt sc = Impl::scaled_prompt(seed.prompt, fx, fy);
            vp.pos_points.insert(vp.pos_points.end(), sc.pos_points.begin(),
                                 sc.pos_points.end());
            vp.neg_points.insert(vp.neg_points.end(), sc.neg_points.begin(),
                                 sc.neg_points.end());
            if (sc.use_box) { vp.box = sc.box; vp.use_box = true; }
        }
        for (const auto& kv : per_object) {
            sam::Result r = s.session.segmentVisual(kv.second);
            Impl::append(all, r);
        }
    }

    // Negative phrases carve back out of what the positives found. The features
    // are already on the device, so this is a decoder pass only.
    sam::Result neg;
    for (const std::string& phrase : s.neg) {
        sam::ConceptPrompt cp;
        cp.text = phrase;
        cp.score_threshold = s.opts.threshold;
        cp.nms_threshold = s.opts.nms;
        Impl::append(neg, s.session.segmentConcept(cp));
    }

    // Every positive detection reached `all` as it was found, so the union and
    // the margin happen once here. The boxes it measures the margin from are
    // still in `scaled` pixels; the overlay below is what converts them.
    compose_hit(all, neg, s.opts.dilate_ratio, hit);

    std::vector<uint8_t> mask(n);
    {
        const uint8_t* src = hit.data();
        uint8_t* dst = mask.data();
        const bool keep = s.opts.keep_prompted;
        nn::parallel_for((int64_t)n, [src, dst, keep](int64_t lo, int64_t hi) {
            for (int64_t i = lo; i < hi; ++i)
                dst[i] = (uint8_t)(((src[i] != 0) == keep) ? 255 : 0);
        }, /*min_chunk=*/65536);
    }

    out.width = image.width;
    out.height = image.height;
    if (scaled.width == image.width && scaled.height == image.height)
        out.data = std::move(mask);
    else
        upscale_nearest(mask, scaled.width, scaled.height, out.data, image.width,
                        image.height);

    if (overlay_out) {
        // The overlay is drawn over the caller's image, so per-instance masks
        // have to come back to its resolution too.
        if (scaled.width != image.width || scaled.height != image.height) {
            for (auto& d : all.detections) {
                if (d.mask.data.size() != n) continue;
                std::vector<uint8_t> up;
                upscale_nearest(d.mask.data, scaled.width, scaled.height, up, image.width,
                                image.height);
                d.mask.data = std::move(up);
                d.mask.width = image.width;
                d.mask.height = image.height;
                const double fx = (double)image.width / scaled.width;
                const double fy = (double)image.height / scaled.height;
                d.box.x0 = (float)(d.box.x0 * fx);
                d.box.x1 = (float)(d.box.x1 * fx);
                d.box.y0 = (float)(d.box.y0 * fy);
                d.box.y1 = (float)(d.box.y1 * fy);
            }
        }
        *overlay_out = std::move(all);
    }
    s.frame = frame_id + 1;
    return true;
}

}  // namespace sam
