#include "roma/Roma.h"

#include "roma/model/Pipeline.h"
#include "roma/model/Descriptor.h"
#include "roma/model/Layers.h"
#include "nn/core/Error.h"
#include "nn/io/Resize.h"
#include "nn/io/StageDump.h"
#include "nn/vk/Context.h"
#include "nn/vk/Stream.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <unordered_map>
#include <utility>

namespace spirula::roma {
namespace {

void validate_rgb(const float* rgb, int w, int h) {
    for (size_t i = 0; i < (size_t)w * h * 3; ++i)
        NN_CHECK(std::isfinite(rgb[i]) && rgb[i] >= 0 && rgb[i] <= 1, "RoMa input RGB must be finite in [0,1]");
}

Tensor upload(Arena& arena, const float* rgb, int w, int h, int wo, int ho, Tensor out = {}) {
    auto resized = nn::resize_rgb_bicubic(rgb, w, h, wo, ho, nn::BicubicWeights::Float32);
    if (!out.valid()) out = map(arena, ho, wo, 3);
    nn::tensor_from_host(out, resized.data(), out.numel());
    return out;
}

uint64_t aligned(uint64_t bytes) { return (bytes + 255) & ~uint64_t(255); }
uint64_t headroom(uint64_t budget, uint64_t used) { return budget > used ? budget - used : 0; }

struct CachedImage {
    nn::vk::DevicePtr allocation = 0;
    Tensor low, high;
    DescriptorFeatures features;
    int width, height;
    uint64_t bytes, used = 0;
    bool ready = false;

    static uint64_t payloadBytes(const MatchOptions& options) {
        return aligned((uint64_t)options.low_width * options.low_height * 3 * sizeof(float)) +
            aligned((uint64_t)options.high_width * options.high_height * 3 * sizeof(float)) +
            2 * aligned((uint64_t)(options.low_width / 16) * (options.low_height / 16) * 1024 * sizeof(float));
    }

    CachedImage(int w, int h, const MatchOptions& options) : width(w), height(h), bytes(payloadBytes(options)) {
        allocation = nn::vk::device_alloc(bytes, "roma image features");
        uint64_t offset = 0;
        auto take = [&](int h, int w, int channels) {
            Tensor out(allocation + offset, DType::F32, h, w, channels);
            offset += aligned(out.bytes());
            return out;
        };
        low = take(options.low_height, options.low_width, 3);
        if (options.high_width) high = take(options.high_height, options.high_width, 3);
        for (auto& feature : features) feature = take(options.low_height / 16, options.low_width / 16, 1024);
    }

    ~CachedImage() { nn::vk::device_free(allocation); }

    bool matches(int w, int h, const MatchOptions& options) const {
        return ready && width == w && height == h && low.shape[0] == options.low_height &&
            low.shape[1] == options.low_width && high.valid() == (options.high_width > 0) &&
            (!high.valid() || (high.shape[0] == options.high_height && high.shape[1] == options.high_width));
    }
};

Prediction download(const PredictionMaps& maps, float saturation) {
    Prediction out;
    out.width = (int)maps.warp.shape[1]; out.height = (int)maps.warp.shape[0];
    const size_t n = (size_t)out.width * out.height;
    out.warp.resize(n * 2); out.overlap.resize(n); out.precision.resize(n * 3);
    std::vector<float> confidence(n * 4);
    nn::tensor_to_host(maps.warp, out.warp.data(), (int64_t)out.warp.size());
    nn::tensor_to_host(maps.confidence, confidence.data(), (int64_t)confidence.size());
    for (float v : out.warp) NN_CHECK(std::isfinite(v), "RoMa produced a nonfinite warp");
    for (size_t i = 0; i < n; ++i) {
        for (int c = 0; c < 4; ++c) NN_CHECK(std::isfinite(confidence[i * 4 + c]), "RoMa produced nonfinite confidence");
        const float logit = confidence[i * 4];
        out.overlap[i] = logit >= 0 ? 1 / (1 + std::exp(-logit)) : std::exp(logit) / (1 + std::exp(logit));
        if (saturation >= 0 && out.overlap[i] > saturation) out.overlap[i] = 1;
        for (int c = 0; c < 3; ++c) out.precision[i * 3 + c] = confidence[i * 4 + c + 1];
    }
    return out;
}

}  // namespace

struct Session::Impl {
    Weights weights;
    Arena arena{"roma"};
    std::unordered_map<uint64_t, std::unique_ptr<CachedImage>> images;
    FeatureCacheStatistics cache;
    std::optional<uint64_t> cache_budget;
    uint64_t clock = 0;

    void erase(decltype(images)::iterator it) {
        cache.bytes -= it->second->bytes;
        images.erase(it);
        ++cache.evictions;
    }

    bool evict(const uint64_t* protected_ids) {
        auto victim = images.end();
        for (auto it = images.begin(); it != images.end(); ++it) {
            if (protected_ids && (it->first == protected_ids[0] || it->first == protected_ids[1])) continue;
            if (victim == images.end() || it->second->used < victim->second->used) victim = it;
        }
        if (victim == images.end()) return false;
        erase(victim);
        return true;
    }

    bool trim(uint64_t limit, const uint64_t* protected_ids = nullptr) {
        while (cache.bytes > limit) if (!evict(protected_ids)) return false;
        return true;
    }

    CachedImage* image(uint64_t id, const float* rgb, int w, int h, const MatchOptions& options,
                       const uint64_t* protected_ids) {
        auto it = images.find(id);
        if (it != images.end()) {
            if (it->second->matches(w, h, options)) {
                ++cache.hits;
                it->second->used = ++clock;
                return it->second.get();
            }
            erase(it);
        }
        ++cache.misses;
        const uint64_t bytes = CachedImage::payloadBytes(options);
        if (bytes > cache.limit_bytes || !trim(cache.limit_bytes - bytes, protected_ids)) return nullptr;
        const auto max_allocations = nn::vk::Context::get().limits().maxMemoryAllocationCount;
        while (nn::vk::Allocator::get().allocationCount() >= max_allocations)
            if (!evict(protected_ids)) return nullptr;
        validate_rgb(rgb, w, h);
        auto entry = std::make_unique<CachedImage>(w, h, options);
        upload(arena, rgb, w, h, options.low_width, options.low_height, entry->low);
        if (options.high_width) upload(arena, rgb, w, h, options.high_width, options.high_height, entry->high);
        entry->used = ++clock;
        auto* result = entry.get();
        images.emplace(id, std::move(entry));
        cache.bytes += bytes;
        cache.peak_bytes = std::max(cache.peak_bytes, cache.bytes);
        return result;
    }
};

Session::Session() : impl_(new Impl) {}
Session::~Session() { unload(); delete impl_; }

InferencePrecision Session::resolvePrecision(InferencePrecision precision) {
    if (precision == InferencePrecision::Automatic)
        return nn::coop_matrix_enabled() ? InferencePrecision::Mixed : InferencePrecision::Float32;
    NN_CHECK(precision == InferencePrecision::Float32 || precision == InferencePrecision::Mixed, "unknown RoMa precision");
    return precision;
}

InferencePrecision Session::precision() const {
    return impl_->weights.mixedPrecision() ? InferencePrecision::Mixed : InferencePrecision::Float32;
}

void Session::load(const std::string& checkpoint, InferencePrecision precision,
                   const std::function<void(uint64_t, uint64_t)>& progress) {
    unload();
    impl_->weights.load(checkpoint, resolvePrecision(precision) == InferencePrecision::Mixed, progress);
}

void Session::unload() {
    if (!impl_->weights.bytes() && !impl_->arena.capacity() && impl_->images.empty()) return;
    nn::vk::Stream::get().sync();
    clearFeatureCache();
    impl_->cache = {};
    impl_->arena.release();
    impl_->weights.release();
}

bool Session::loaded() const { return impl_->weights.bytes() != 0; }
uint64_t Session::deviceBytes() const { return impl_->weights.bytes() + impl_->arena.capacity() + impl_->cache.bytes; }
uint64_t Session::peakScratchBytes() const { return impl_->arena.highWater(); }

void Session::setFeatureCacheBudget(std::optional<uint64_t> bytes) {
    impl_->cache_budget = bytes;
    if (bytes) {
        impl_->cache.limit_bytes = std::min(impl_->cache.limit_bytes, *bytes);
        impl_->trim(*bytes);
    }
}

void Session::clearFeatureCache() {
    impl_->images.clear();
    impl_->cache.bytes = 0;
}

FeatureCacheStatistics Session::featureCacheStatistics() const { return impl_->cache; }

uint64_t Session::plannedScratchBytes(const MatchOptions& options) {
    options.validate();
    const uint64_t low = (uint64_t)options.low_width * options.low_height;
    const uint64_t high = (uint64_t)options.high_width * options.high_height;
    // Measured peaks are 1295-1341 bytes a matching pixel (512, 640, and 1280 both ways);
    // an arena cannot grow mid-pass, so this keeps about 15% over the worst.
    return std::max(low, high) * 1536 + (128ull << 20);
}

PairPrediction Session::match(const float* a, int width_a, int height_a,
                               const float* b, int width_b, int height_b, const MatchOptions& options) {
    return matchImpl(a, width_a, height_a, b, width_b, height_b, options, nullptr);
}

PairPrediction Session::matchCached(uint64_t image_a, const float* a, int width_a, int height_a,
                                     uint64_t image_b, const float* b, int width_b, int height_b,
                                     const MatchOptions& options) {
    NN_CHECK(image_a != image_b, "RoMa cached pair must identify two distinct views");
    const uint64_t ids[] = {image_a, image_b};
    return matchImpl(a, width_a, height_a, b, width_b, height_b, options, ids);
}

PairPrediction Session::matchImpl(const float* a, int width_a, int height_a,
                                   const float* b, int width_b, int height_b, const MatchOptions& options,
                                   const uint64_t* image_ids) {
    NN_CHECK(loaded(), "RoMa match before load");
    options.validate();
    NN_CHECK(options.precision == InferencePrecision::Automatic || options.precision == precision(),
             "RoMa match precision differs from loaded weights; reload the session");
    for (const auto& input : {std::make_pair(a, std::make_pair(width_a, height_a)),
                              std::make_pair(b, std::make_pair(width_b, height_b))}) {
        const int w = input.second.first, h = input.second.second;
        NN_CHECK(input.first && w > 0 && h > 0 && (uint64_t)w * h <= std::vector<float>().max_size() / 3,
                 "RoMa input image is empty or exceeds host addressable storage");
    }
    const uint64_t planned = plannedScratchBytes(options);
    auto& context = nn::vk::Context::get();
    const uint64_t hardware_budget = context.info().vram_bytes * 4 / 5;
    const uint64_t budget = options.memory_budget_bytes ? options.memory_budget_bytes : hardware_budget;
    const uint64_t reserved = std::max<uint64_t>(impl_->arena.capacity(), (planned + (4ull << 20) - 1) & ~((4ull << 20) - 1));
    NN_CHECK(budget == 0 || impl_->weights.bytes() + reserved <= budget,
             "RoMa needs %llu device bytes for these settings; budget is %llu",
             (unsigned long long)(impl_->weights.bytes() + reserved), (unsigned long long)budget);
    impl_->cache.limit_bytes = budget ? budget - impl_->weights.bytes() - reserved : 0;
    const uint64_t scratch_growth = reserved - impl_->arena.capacity();
    const uint64_t other_allocations = nn::vk::Allocator::get().totalBytes() - impl_->cache.bytes;
    if (hardware_budget)
        impl_->cache.limit_bytes = std::min(impl_->cache.limit_bytes, headroom(hardware_budget, other_allocations + scratch_growth));
    const auto driver_budget = context.memoryBudget();
    if (driver_budget.budget_bytes)
        impl_->cache.limit_bytes = std::min(impl_->cache.limit_bytes,
            headroom(driver_budget.budget_bytes * 4 / 5, headroom(driver_budget.usage_bytes, impl_->cache.bytes) + scratch_growth));
    if (impl_->cache_budget) impl_->cache.limit_bytes = std::min(impl_->cache.limit_bytes, *impl_->cache_budget);
    impl_->trim(impl_->cache.limit_bytes);
    if (options.progress) options.progress("prepare");
    impl_->arena.reset();
    if (planned > impl_->arena.capacity()) impl_->arena.release();
    impl_->arena.reserve(planned);
    ArenaScope scope(impl_->arena);
    Arena& arena = impl_->arena;
    const bool high = options.high_width > 0;
    const int w = high ? options.high_width : options.low_width, h = high ? options.high_height : options.low_height;
    PredictionMaps ab{map(arena, h, w, 2), map(arena, h, w, 4)}, ba;
    if (options.bidirectional) ba = {map(arena, h, w, 2), map(arena, h, w, 4)};
    CachedImage* cached[] = {nullptr, nullptr};
    if (image_ids) {
        cached[0] = impl_->image(image_ids[0], a, width_a, height_a, options, image_ids);
        cached[1] = impl_->image(image_ids[1], b, width_b, height_b, options, image_ids);
    }
    if (!cached[0]) validate_rgb(a, width_a, height_a);
    if (!cached[1]) validate_rgb(b, width_b, height_b);
    const Tensor low_a = cached[0] ? cached[0]->low : upload(arena, a, width_a, height_a, options.low_width, options.low_height);
    const Tensor low_b = cached[1] ? cached[1]->low : upload(arena, b, width_b, height_b, options.low_width, options.low_height);
    Tensor high_a, high_b;
    if (high) {
        high_a = cached[0] ? cached[0]->high : upload(arena, a, width_a, height_a, options.high_width, options.high_height);
        high_b = cached[1] ? cached[1]->high : upload(arena, b, width_b, height_b, options.high_width, options.high_height);
    }
    nn::StageDump dump("ROMA_DUMP");
    auto dump_rgb = [&](const char* name, const Tensor& rgb) {
        if (rgb.valid()) dump.tensor(name, rgb, {rgb.shape[0], rgb.shape[1], 3});
    };
    dump_rgb("low_a", low_a); dump_rgb("low_b", low_b);
    dump_rgb("high_a", high_a); dump_rgb("high_b", high_b);
    if (options.progress) options.progress("descriptor");
    for (auto* entry : cached) {
        if (!entry || entry->ready) continue;
        descriptor(arena, impl_->weights, entry->low, entry->features);
        entry->ready = true;
    }
    forward(arena, impl_->weights, low_a, low_b, high_a, high_b, ab, ba,
              [&](const char* stage, const PredictionMaps& maps) {
                  const std::string prefix(stage);
                  dump.tensor((prefix + "_warp").c_str(), maps.warp, {maps.warp.shape[0], maps.warp.shape[1], 2});
                  dump.tensor((prefix + "_confidence").c_str(), maps.confidence,
                              {maps.confidence.shape[0], maps.confidence.shape[1], maps.confidence.shape[2]});
                  if (options.progress) options.progress(stage);
              }, cached[0] ? &cached[0]->features : nullptr, cached[1] ? &cached[1]->features : nullptr);
    PairPrediction out;
    out.forward = download(ab, options.overlap_saturation);
    if (options.bidirectional) out.backward = download(ba, options.overlap_saturation);
    return out;
}

}  // namespace spirula::roma
