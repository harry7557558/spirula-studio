#include "marigold/Marigold.h"

#include "core/Env.h"
#include "marigold/Common.h"
#include "marigold/model/Dit.h"
#include "marigold/model/Install.h"
#include "marigold/model/Layout.h"
#include "marigold/model/Streamer.h"
#include "marigold/model/Vae.h"
#include "nn/Ops.h"
#include "nn/io/Resize.h"
#include "nn/io/StageDump.h"
#include "nn/vk/Context.h"
#include "nn/vk/Stream.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>

namespace marigold {
namespace {

using nn::DType;
using nn::Tensor;
using nn::arena_tensor;

// Left to the driver, the other models of the process and fragmentation.
constexpr uint64_t kHeadroom = 512ull << 20;
constexpr int kMaxBatch = 8;

// Bilinear with half-pixel centres, cv2.INTER_LINEAR's mapping: how Marigold
// v2 takes a prediction back to the input's size.
std::vector<float> resize_linear(const std::vector<float>& src, int sw, int sh, int c, int dw,
                                 int dh) {
    std::vector<float> out((size_t)dw * dh * c);
    parallel_for(dh, [&](int64_t y0, int64_t y1) {
        for (int64_t y = y0; y < y1; ++y) {
            const float fy = std::clamp(((float)y + 0.5f) * sh / dh - 0.5f, 0.0f, (float)(sh - 1));
            const int ya = (int)fy, yb = std::min(ya + 1, sh - 1);
            const float wy = fy - (float)ya;
            for (int x = 0; x < dw; ++x) {
                const float fx = std::clamp((x + 0.5f) * sw / dw - 0.5f, 0.0f, (float)(sw - 1));
                const int xa = (int)fx, xb = std::min(xa + 1, sw - 1);
                const float wx = fx - (float)xa;
                for (int k = 0; k < c; ++k) {
                    auto at = [&](int yy, int xx) { return src[((size_t)yy * sw + xx) * c + k]; };
                    out[((size_t)y * dw + x) * c + k] =
                        (1 - wy) * ((1 - wx) * at(ya, xa) + wx * at(ya, xb)) +
                        wy * ((1 - wx) * at(yb, xa) + wx * at(yb, xb));
                }
            }
        }
    });
    return out;
}

uint64_t vram_cap() {
    const char* v = spirula::env("MARIGOLD_VRAM_MB");
    return v ? (uint64_t)std::max(0.0, std::atof(v)) << 20 : 0;
}

}  // namespace

struct Predictor::Impl {
    std::unique_ptr<nn::SafetensorsFile> base, task[2];
    Vae encoder, decoder[2];
    Dit dit;
    Streamer streamer;
    vk::Arena arena{"marigold"};
    bool have[2] = {};
    int plan_w = 0, plan_h = 0, plan_batch = 0;
    Task last = Task::Normals;
    nn::StageDump dump{"MARIGOLD_DUMP"};

    // Weights, scratch and the arena, against what the device will give us.
    uint64_t budget() const {
        const vk::Context& ctx = vk::Context::get();
        const spirula::vkmemory::Budget b = ctx.memoryBudget();
        const uint64_t ours = streamer.deviceBytes() + arena.capacity();
        uint64_t avail = b.budget_bytes ? b.available_bytes + ours
                                        : (uint64_t)(ctx.info().vram_bytes * 0.85) -
                                              std::min<uint64_t>(vk::Allocator::get().totalBytes() - ours,
                                                                 ctx.info().vram_bytes);
        if (const uint64_t cap = vram_cap()) {
            const uint64_t fixed = encoder.deviceBytes() + decoder[0].deviceBytes() +
                                   decoder[1].deviceBytes() + dit.deviceBytes();
            avail = std::min(avail, cap > fixed ? cap - fixed : 0);
        }
        return avail > kHeadroom ? avail - kHeadroom : 0;
    }

    uint64_t activations(int pw, int ph, int batch) const {
        const int64_t tokens = (int64_t)(pw / kPatch) * (ph / kPatch);
        const uint64_t latents = (uint64_t)batch * 2 * (pw / 8) * (ph / 8) * kLatent * 4;
        return latents + std::max(Vae::arenaBytes(ph, pw), dit.arenaBytes(tokens, batch));
    }

    int capacity(int pw, int ph) const {
        const int64_t tokens = (int64_t)(pw / kPatch) * (ph / kPatch);
        const uint64_t room = std::max(Vae::arenaBytes(ph, pw), budget() / 4);
        int b = 1;
        while (b < kMaxBatch && dit.arenaBytes(tokens, b + 1) <= room) ++b;
        return b;
    }

    void run(int pw, int ph, const std::vector<Image>& images, const std::vector<size_t>& idx,
             const PredictOptions& o, std::vector<Prediction>& out);

    void plan(int pw, int ph, int batch) {
        const bool larger = (int64_t)pw * ph > (int64_t)plan_w * plan_h;
        if (!larger && batch <= plan_batch) return;
        if (larger) {
            plan_w = pw;
            plan_h = ph;
        }
        plan_batch = std::max(plan_batch, batch);
        const uint64_t act = activations(plan_w, plan_h, plan_batch) + (64ull << 20);
        if (arena.capacity() < act) {
            arena.release();
            arena.reserve(act);
        }
        const uint64_t room = budget();
        NN_CHECK(room > arena.capacity(),
                 "Marigold v2 needs %.1f GB of GPU memory for a %dx%d image's activations and "
                 "the device has %.1f GB to give; lower --num-tokens",
                 arena.capacity() / 1e9, pw, ph, (room + kHeadroom) / 1e9);
        streamer.plan(room - arena.capacity());
    }
};

Predictor::Predictor() : impl_(new Impl) {}
Predictor::~Predictor() = default;

bool Predictor::loaded() const { return impl_->base != nullptr; }

void Predictor::load(bool want_depth, bool want_normal) {
    Impl& m = *impl_;
    NN_CHECK(base_installed() && (!want_depth || task_installed(Task::Depth)) &&
                 (!want_normal || task_installed(Task::Normals)),
             "Marigold v2 is not installed: `spirula geometry --install marigold-v2` builds it "
             "from Hugging Face's checkpoints (about 45 GB streamed, 11 GB kept)");
    if (!m.base) {
        m.base = std::make_unique<nn::SafetensorsFile>(base_path());
        m.encoder.load(*m.base, true, "marigold.encoder");
        m.dit.load(*m.base);
        m.streamer.addBase(*m.base);
    }
    for (Task t : {Task::Depth, Task::Normals}) {
        const int k = (int)t;
        if (m.have[k] || !(t == Task::Depth ? want_depth : want_normal)) continue;
        m.task[k] = std::make_unique<nn::SafetensorsFile>(task_path(t));
        m.decoder[k].load(*m.task[k], false, "marigold.decoder");
        m.dit.loadTask(t, *m.task[k]);
        m.streamer.addTask(t, *m.task[k]);
        m.have[k] = true;
    }
}

void Predictor::processingSize(int width, int height, int num_tokens, int* pw, int* ph) {
    const double s = std::min(1.0, std::sqrt((double)num_tokens * kPatch * kPatch /
                                             ((double)width * height)));
    *pw = std::max(kPatch, (int)std::lround(width * s / kPatch) * kPatch);
    *ph = std::max(kPatch, (int)std::lround(height * s / kPatch) * kPatch);
}

void Predictor::prepare(int width, int height, int num_tokens) {
    int pw, ph;
    processingSize(width, height, num_tokens, &pw, &ph);
    impl_->plan(pw, ph, 1);
}

int Predictor::batchCapacity(int width, int height, int num_tokens) {
    int pw, ph;
    processingSize(width, height, num_tokens, &pw, &ph);
    return impl_->capacity(pw, ph);
}

uint64_t Predictor::deviceBytes() const {
    const Impl& m = *impl_;
    return m.encoder.deviceBytes() + m.decoder[0].deviceBytes() + m.decoder[1].deviceBytes() +
           m.dit.deviceBytes() + m.streamer.deviceBytes() + m.arena.capacity();
}

std::vector<Prediction> Predictor::predict(const std::vector<Image>& images,
                                           const PredictOptions& o) {
    Impl& m = *impl_;
    NN_CHECK(m.base, "Marigold v2: predict before load");
    NN_CHECK((!o.want_depth || m.have[0]) && (!o.want_normal || m.have[1]),
             "Marigold v2: a task was asked for that was not loaded");
    // A pass is one processing size, not one input size: a split fisheye's
    // faces differ by a few pixels and land on the same multiple of 16.
    std::map<std::pair<int, int>, std::vector<size_t>> groups;
    for (size_t i = 0; i < images.size(); ++i) {
        int pw, ph;
        processingSize(images[i].width, images[i].height, o.num_tokens, &pw, &ph);
        groups[{pw, ph}].push_back(i);
    }
    std::vector<Prediction> out(images.size());
    for (const auto& [size, idx] : groups) m.run(size.first, size.second, images, idx, o, out);
    return out;
}

void Predictor::Impl::run(int pw, int ph, const std::vector<Image>& images,
                          const std::vector<size_t>& idx, const PredictOptions& o,
                          std::vector<Prediction>& out) {
    Impl& m = *this;
    const int cap = m.capacity(pw, ph);
    m.plan(pw, ph, std::min<int>(cap, (int)idx.size()));
    const int h = ph / 8, w = pw / 8;

    for (size_t b0 = 0; b0 < idx.size(); b0 += (size_t)cap) {
        const size_t n = std::min(idx.size() - b0, (size_t)cap);
        vk::ArenaScope scope(m.arena);
        std::vector<Tensor> mu(n), z(n);
        for (size_t b = 0; b < n; ++b) {
            mu[b] = arena_tensor(m.arena, DType::F32, h, w, kLatent);
            z[b] = arena_tensor(m.arena, DType::F32, h, w, kLatent);
        }
        for (size_t b = 0; b < n; ++b) {
            const Image& in = images[idx[b0 + b]];
            vk::ArenaScope enc(m.arena);
            const std::vector<float> sized =
                pw == in.width && ph == in.height
                    ? std::vector<float>(in.rgb, in.rgb + (size_t)in.width * in.height * 3)
                    : nn::resize_rgb_bicubic(in.rgb, in.width, in.height, pw, ph);
            Tensor x = arena_tensor(m.arena, DType::F32, ph, pw, 3);
            nn::tensor_from_host(x, sized.data(), x.numel());
            if (m.dump.on()) m.dump.tensor("rgb_in", x, {ph, pw, 3});
            nn::unary(x, x, nn::Act::None, 2.0f, -1.0f);
            m.encoder.encode(m.arena, x, mu[b]);
            if (m.dump.on()) m.dump.tensor("mu", mu[b], {h, w, kLatent});
            out[idx[b0 + b]].width = in.width;
            out[idx[b0 + b]].height = in.height;
        }
        // Alternating the order keeps one task's LoRA resident across batches.
        std::vector<Task> tasks;
        if (o.want_depth) tasks.push_back(Task::Depth);
        if (o.want_normal) tasks.push_back(Task::Normals);
        if (tasks.size() == 2 && m.last == Task::Normals) std::swap(tasks[0], tasks[1]);
        for (Task t : tasks) {
            m.last = t;
            m.streamer.beginPass(t);
            m.dit.forward(m.arena, m.streamer, t, mu, z, m.dump.on() ? &m.dump : nullptr);
            for (size_t b = 0; b < n; ++b) {
                if (m.dump.on()) m.dump.tensor("latent_out", z[b], {h, w, kLatent});
                vk::ArenaScope dec(m.arena);
                Tensor img = arena_tensor(m.arena, DType::F32, ph, pw, 3);
                m.decoder[(int)t].decode(m.arena, z[b], img);
                std::vector<float> d((size_t)img.numel());
                nn::tensor_to_host(img, d.data(), img.numel());
                for (float& v : d) v = std::clamp(v, -1.0f, 1.0f);
                if (m.dump.on()) m.dump.host("decoded", d.data(), {ph, pw, 3});
                Prediction& p = out[idx[b0 + b]];
                if (t == Task::Depth) {
                    std::vector<float> dl((size_t)pw * ph);
                    for (size_t i = 0; i < dl.size(); ++i)
                        dl[i] = (d[i * 3] + d[i * 3 + 1] + d[i * 3 + 2]) / 3.0f;
                    if (m.dump.on()) m.dump.host("depth_rel", dl.data(), {ph, pw});
                    p.depth_log = resize_linear(dl, pw, ph, 1, p.width, p.height);
                } else {
                    // Marigold's normals are OpenGL's axes (y up, z towards the
                    // viewer); the trainer reads OpenCV's.
                    std::vector<float> nrm = resize_linear(d, pw, ph, 3, p.width, p.height);
                    for (size_t i = 0; i * 3 < nrm.size(); ++i) {
                        float* v = &nrm[i * 3];
                        const float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
                        const float s = len > 1e-6f ? 1.0f / len : 0.0f;
                        v[0] *= s;
                        v[1] *= -s;
                        v[2] *= -s;
                    }
                    p.normal = std::move(nrm);
                }
            }
        }
    }
    NN_LOG_DEBUG("[marigold] %dx%d, %zu images: working memory %.2f GB of %.2f GB planned\n", pw,
                 ph, idx.size(), m.arena.highWater() / 1e9, m.arena.capacity() / 1e9);
}

}  // namespace marigold
