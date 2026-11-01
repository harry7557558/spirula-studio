#include "roma/model/Weights.h"

#include "core/HardwareThreads.h"
#include "nn/core/Error.h"
#include "nn/core/Half.h"

#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <exception>
#include <map>
#include <mutex>
#include <set>
#include <thread>

namespace spirula::roma {
namespace {

using Shape = std::vector<int64_t>;
using Schema = std::map<std::string, Shape>;

void affine(Schema& s, const std::string& p, int out, int in = 0, int k = 0, bool bias = true) {
    s[p + ".weight"] = k ? Shape{out, in, k, k} : in ? Shape{out, in} : Shape{out};
    if (bias) s[p + ".bias"] = {out};
}

void norm(Schema& s, const std::string& p, int channels) {
    affine(s, p, channels);
    s[p + ".running_mean"] = {channels};
    s[p + ".running_var"] = {channels};
    s[p + ".num_batches_tracked"] = {};
}

Schema schema() {
    Schema s;
    s["f.cls_token"] = {1, 1, 1024};
    s["f.storage_tokens"] = {1, 4, 1024};
    s["f.mask_token"] = {1, 1024};
    s["f.rope_embed.periods"] = {16};
    affine(s, "f.patch_embed.proj", 1024, 3, 16);
    affine(s, "f.norm", 1024);
    for (const auto& family : {std::string("f"), std::string("matcher.mv_vit")}) {
        const bool descriptor = family == "f";
        const int d = descriptor ? 1024 : 768;
        const int blocks = descriptor ? 24 : 12;
        for (int i = 0; i < blocks; ++i) {
            const std::string p = family + ".blocks." + std::to_string(i);
            affine(s, p + ".norm1", d); affine(s, p + ".norm2", d);
            affine(s, p + ".attn.qkv", 3 * d, d); affine(s, p + ".attn.proj", d, d);
            affine(s, p + ".mlp.fc1", 4 * d, d); affine(s, p + ".mlp.fc2", d, 4 * d);
            if (descriptor) {
                s[p + ".attn.qkv.bias_mask"] = {3 * d};
                s[p + ".ls1.gamma"] = {d}; s[p + ".ls2.gamma"] = {d};
            }
        }
    }
    affine(s, "matcher.mv_vit.projector", 768, 2048);
    affine(s, "matcher.mv_vit.output_projector", 1024, 768);
    affine(s, "matcher.mv_vit.norm", 768);
    s["matcher.mv_vit.rope_embed.periods"] = {16};
    s["matcher.omega"] = {512, 2}; s["matcher.scale"] = {}; s["matcher.temp"] = {};
    const std::string h = "matcher.head.";
    affine(s, h + "norm", 1024);
    const int channels[] = {256, 512, 1024, 1024};
    for (int i = 0; i < 4; ++i) {
        affine(s, h + "projects." + std::to_string(i), channels[i], 1024, 1);
        affine(s, h + "scratch.layer" + std::to_string(i + 1) + "_rn", 256, channels[i], 3, false);
        const std::string p = h + "scratch.refinenet" + std::to_string(i + 1);
        affine(s, p + ".out_conv", 256, 256, 1);
        for (int unit = i == 3 ? 2 : 1; unit <= 2; ++unit) {
            affine(s, p + ".resConfUnit" + std::to_string(unit) + ".conv1", 256, 256, 3);
            affine(s, p + ".resConfUnit" + std::to_string(unit) + ".conv2", 256, 256, 3);
        }
    }
    affine(s, h + "resize_layers.0", 256, 256, 4);
    affine(s, h + "resize_layers.1", 512, 512, 2);
    affine(s, h + "resize_layers.3", 1024, 1024, 3);
    affine(s, h + "scratch.output_conv1", 128, 256, 3);
    affine(s, h + "scratch.output_conv2.0", 32, 128, 3);
    affine(s, h + "scratch.output_conv2.2", 3, 32, 1);
    const int index[] = {0, 3, 7, 10, 14, 17, 20, 23};
    const int width[] = {64, 64, 128, 128, 256, 256, 256, 256};
    for (int i = 0; i < 8; ++i) {
        const std::string p = "refiner_features.layers.";
        affine(s, p + std::to_string(index[i]), width[i], i ? width[i - 1] : 3, 3);
        norm(s, p + std::to_string(index[i] + 1), width[i]);
    }
    const int patch[] = {4, 2, 1}, feat[] = {256, 128, 64}, proj[] = {192, 48, 12};
    const int disp[] = {79, 23, 8}, hidden[] = {512, 128, 32};
    for (int i = 0; i < 3; ++i) {
        const std::string p = "refiners." + std::to_string(patch[i]);
        affine(s, p + ".proj", proj[i], feat[i]);
        affine(s, p + ".disp_emb", disp[i], 2, 1);
        affine(s, p + ".warp_head", 2, hidden[i], 1);
        affine(s, p + ".confidence_head", 4, hidden[i], 1);
        for (int block = -1; block < 8; ++block) {
            const std::string b = p + (block < 0 ? ".block1" : ".hidden_blocks." + std::to_string(block));
            affine(s, b + ".conv_depthwise", hidden[i], 1, 5);
            norm(s, b + ".norm", hidden[i]);
            affine(s, b + ".conv_pointwise", hidden[i], hidden[i], 1);
        }
    }
    return s;
}

std::vector<float> transpose_patch(const nn::OnnxTensor& t) {
    const int64_t ci = t.shape[0], co = t.shape[1], taps = t.shape[2] * t.shape[3];
    std::vector<float> out(t.data.size());
    for (int64_t i = 0; i < ci; ++i)
        for (int64_t o = 0; o < co; ++o)
            for (int64_t k = 0; k < taps; ++k)
                out[(size_t)((o * taps + k) * ci + i)] = t.data[(size_t)((i * co + o) * taps + k)];
    return out;
}

}  // namespace

void Weights::validate(const nn::TorchCheckpoint& f) {
    const Schema expected = schema();
    NN_CHECK(f.names().size() == expected.size(), "'%s': RoMa v2 tensor count differs (%zu expected, %zu found)",
             f.path().c_str(), expected.size(), f.names().size());
    for (const auto& p : expected) {
        NN_CHECK(f.has(p.first) && f.entry(p.first).shape == p.second,
                 "'%s': RoMa v2 tensor '%s' is missing or has an incompatible shape", f.path().c_str(), p.first.c_str());
    }
}

namespace {

struct Prepared {
    std::string name;
    std::vector<int64_t> shape;
    std::vector<float> data;
    std::vector<uint16_t> half;
    uint64_t bytes() const { return data.size() * sizeof(float) + half.size() * sizeof(uint16_t); }
};

// One checkpoint tensor as staged, after any batch-normalization folding it feeds.
std::vector<Prepared> prepare(const nn::TorchCheckpoint& f, nn::TorchCheckpoint::Reader& reader, const std::string& path,
                              const std::string& name, bool mixed) {
    std::vector<Prepared> out;
    nn::OnnxTensor t = reader.read(name);
    for (float v : t.data) NN_CHECK(std::isfinite(v), "'%s': nonfinite weight '%s'", path.c_str(), name.c_str());
    if (name.size() >= 12 && name.compare(name.size() - 12, 12, ".running_var") == 0) {
        const std::string p = name.substr(0, name.size() - 12);
        const auto mean = reader.read(p + ".running_mean").data;
        const auto gamma = reader.read(p + ".weight").data;
        const auto beta = reader.read(p + ".bias").data;
        std::vector<float> scale(t.data.size()), shift(t.data.size());
        for (size_t i = 0; i < scale.size(); ++i) {
            NN_CHECK(t.data[i] >= 0, "'%s': negative batch normalization variance", path.c_str());
            scale[i] = gamma[i] / std::sqrt(t.data[i] + 1e-5f);
            shift[i] = beta[i] - mean[i] * scale[i];
        }
        out.push_back({p + ".scale", t.shape, std::move(scale), {}});
        out.push_back({p + ".shift", t.shape, std::move(shift), {}});
    }
    if (name.size() >= 5 && name.compare(name.size() - 5, 5, ".bias") == 0 && f.has(name + "_mask")) {
        const auto mask = reader.read(name + "_mask").data;
        for (size_t i = 0; i < mask.size(); ++i) t.data[i] *= mask[i];
    }
    const bool transpose = name == "matcher.head.resize_layers.0.weight" || name == "matcher.head.resize_layers.1.weight";
    if (transpose) {
        auto data = transpose_patch(t);
        t.shape = {t.shape[1] * t.shape[2] * t.shape[3], t.shape[0]};
        t.data = std::move(data);
    }
    const bool matrix = name.size() >= 7 && name.compare(name.size() - 7, 7, ".weight") == 0 &&
        (t.shape.size() == 2 || t.shape.size() == 4) && name.find("conv_depthwise") == std::string::npos;
    bool half = mixed && matrix;
    if (half) for (float v : t.data) if (std::abs(v) > 65504) { half = false; break; }
    Prepared tensor{name, std::move(t.shape), {}, {}};
    if (half) {
        tensor.half.resize(t.data.size());
        for (size_t i = 0; i < t.data.size(); ++i) tensor.half[i] = nn::float_to_half(t.data[i]);
    } else tensor.data = std::move(t.data);
    out.push_back(std::move(tensor));
    return out;
}

}  // namespace

void Weights::load(const std::string& path, bool mixed, const std::function<void(uint64_t, uint64_t)>& progress) {
    release();
    mixed_ = mixed;
    nn::TorchCheckpoint f(path);
    validate(f);
    descriptor_periods_ = f.read("f.rope_embed.periods").data;
    matcher_periods_ = f.read("matcher.mv_vit.rope_embed.periods").data;
    for (const auto* periods : {&descriptor_periods_, &matcher_periods_})
        for (float value : *periods)
            NN_CHECK(std::isfinite(value) && value > 0, "'%s': RoMa positional periods must be positive", path.c_str());
    matcher_omega_ = f.read("matcher.omega").data;
    matcher_scale_ = f.read("matcher.scale").data[0];
    matcher_temperature_ = f.read("matcher.temp").data[0];
    NN_CHECK(std::isfinite(matcher_temperature_) && matcher_temperature_ > 0,
             "'%s': RoMa matcher temperature must be positive", path.c_str());
    uint64_t staged = 0;
    try {
        // Workers decode, check and pack tensors; this thread stages them in checkpoint
        // order, so device placement does not depend on which worker finished first.
        const auto names = f.names();
        const size_t n = names.size();
        std::vector<std::vector<Prepared>> slots(n);
        std::vector<char> ready(n, 0);
        std::mutex mutex;
        std::condition_variable changed;
        size_t next = 0;
        uint64_t ahead = 0;
        bool stop = false;
        std::exception_ptr failure;
        constexpr uint64_t kAheadBytes = 256ull << 20;
        auto work = [&] {
            try {
                nn::TorchCheckpoint::Reader reader(f);
                for (;;) {
                    size_t i;
                    {
                        std::unique_lock<std::mutex> lock(mutex);
                        changed.wait(lock, [&] { return stop || ahead < kAheadBytes; });
                        if (stop || next == n) return;
                        i = next++;
                    }
                    auto items = prepare(f, reader, path, names[i], mixed);
                    std::lock_guard<std::mutex> lock(mutex);
                    for (const auto& item : items) ahead += item.bytes();
                    slots[i] = std::move(items); ready[i] = 1;
                    changed.notify_all();
                }
            } catch (...) {
                std::lock_guard<std::mutex> lock(mutex);
                if (!failure) failure = std::current_exception();
                stop = true;
                changed.notify_all();
            }
        };
        std::vector<std::thread> pool;
        struct Join {
            std::vector<std::thread>& pool; std::mutex& mutex; std::condition_variable& changed; bool& stop;
            ~Join() {
                { std::lock_guard<std::mutex> lock(mutex); stop = true; }
                changed.notify_all();
                for (auto& thread : pool) thread.join();
            }
        } join{pool, mutex, changed, stop};
        const unsigned threads = std::clamp(spirula::hardware_threads(), 1u, 16u);
        for (unsigned t = 0; t < threads; ++t) pool.emplace_back(work);
        for (size_t i = 0; i < n; ++i) {
            if (progress && i * 100 / n != (i + 1) * 100 / n)
                progress(i + 1, n);   // whole percents: a log line per tensor is noise
            std::vector<Prepared> items;
            {
                std::unique_lock<std::mutex> lock(mutex);
                changed.wait(lock, [&] { return ready[i] || failure; });
                if (!ready[i]) std::rethrow_exception(failure);
                items = std::move(slots[i]);
                for (const auto& item : items) ahead -= item.bytes();
            }
            changed.notify_all();
            staged += (items.back().data.size() + items.back().half.size()) * sizeof(float);
            for (auto& item : items) {
                if (!item.half.empty()) store_.stage_f16(item.name, std::move(item.shape), std::move(item.half));
                else store_.stage(item.name, std::move(item.shape), std::move(item.data), false);
            }
            if (staged >= (64ull << 20)) { store_.upload("roma-weights"); staged = 0; }
        }
        store_.stage("input.std", {3}, {0.229f, 0.224f, 0.225f}, false);
        store_.stage("input.mean", {3}, {0.485f, 0.456f, 0.406f}, false);
        store_.upload("roma-weights");
        if (progress) progress(names.size(), names.size());
    } catch (...) { release(); throw; }
}

}  // namespace spirula::roma
