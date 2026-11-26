#include "app/MaskFrames.h"

#include "app/AppPaths.h"
#include "app/FrameLook.h"
#include "app/FrameMask.h"
#include "app/InferenceWorker.h"
#include "app/InferenceWorkerDevices.h"
#include "app/MaskWorker.h"
#include "app/WriterPool.h"
#include "birefnet/BiRefNet.h"
#include "core/AtomicFile.h"
#include "core/BufferedFile.h"
#include "core/ColorSpace.h"
#include "core/HardwareThreads.h"
#include "core/Sha256.h"
#include "data/JsonWrite.h"
#include "external/stb_image.h"
#include "gdino/GroundingDino.h"
#include "i18n/catalog/Log.h"
#include "nn/core/Log.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace fs = std::filesystem;
namespace lmsg = spirula::i18n::msg::log;

namespace app {
namespace {

void remove_tree(const fs::path& p) {
    std::error_code ec;
    fs::remove_all(p, ec);
}

bool probe_size(const fs::path& file, int& width, int& height) {
    return image_size(file.string(), width, height) && width > 0 && height > 0;
}

double kept_share(const sam::Mask& m) {
    if (m.data.empty()) return 0;
    const size_t kept = (size_t)std::count_if(m.data.begin(), m.data.end(),
                                              [](uint8_t v) { return v > 127; });
    return (double)kept / (double)m.data.size();
}

// Reading a frame is ~10-20 ms of CPU that the model it feeds does not need
// the CPU for. One thread ahead of the masking loop hides all of it.
class ImagePrefetch {
public:
    ImagePrefetch(std::vector<fs::path> files, const std::atomic<bool>& cancel,
                  std::string gamut, std::optional<bool> is_linear,
                  colorspace::Exposure exposure)
        : _files(std::move(files)), _cancel(cancel), _gamut(std::move(gamut)),
          _is_linear(is_linear), _exposure(exposure), _worker([this] { run(); }) {}
    ~ImagePrefetch() {
        {
            std::lock_guard<std::mutex> lk(_mu);
            _stop = true;
        }
        _space.notify_all();
        _worker.join();
    }
    ImagePrefetch(const ImagePrefetch&) = delete;
    ImagePrefetch& operator=(const ImagePrefetch&) = delete;

    // The next file, the way up the model wants it, `turn` saying what to
    // undo on the mask. Empty when it could not be read or the reader stopped.
    // What the reader threw is rethrown here, the thread that can report it.
    nn::Image take(sfm::ExifTransform& turn) {
        std::unique_lock<std::mutex> lk(_mu);
        _ready.wait(lk, [this] { return !_queue.empty() || _done; });
        if (_queue.empty()) {
            if (_err) std::rethrow_exception(_err);
            return nn::Image();
        }
        Item item = std::move(_queue.front());
        _queue.pop_front();
        _space.notify_one();
        turn = item.turn;
        return std::move(item.img);
    }

private:
    // Two in hand plus the one being masked: a 4K frame is ~25 MB, and the
    // depth buys nothing beyond covering one decode.
    static constexpr size_t kDepth = 2;

    struct Item {
        nn::Image img;
        sfm::ExifTransform turn;
    };

    void run() {
        try {
            for (const fs::path& f : _files) {
                if (_cancel.load()) break;
                Item item;
                item.img = load_upright(f.string(), _gamut, _is_linear, _exposure, item.turn);
                std::unique_lock<std::mutex> lk(_mu);
                _space.wait(lk, [this] { return _queue.size() < kDepth || _stop; });
                if (_stop) break;
                _queue.push_back(std::move(item));
                _ready.notify_one();
            }
        } catch (...) {
            std::lock_guard<std::mutex> lk(_mu);
            _err = std::current_exception();
        }
        {
            std::lock_guard<std::mutex> lk(_mu);
            _done = true;
        }
        _ready.notify_all();
    }

    std::vector<fs::path> _files;
    const std::atomic<bool>& _cancel;
    std::string _gamut;
    std::optional<bool> _is_linear;
    colorspace::Exposure _exposure;
    std::deque<Item> _queue;
    std::mutex _mu;
    std::condition_variable _ready, _space;
    bool _stop = false;
    // The reader is gone. Without it a take() the reader cancelled out from
    // under waits on a queue nothing will ever fill again.
    bool _done = false;
    std::exception_ptr _err;
    std::thread _worker;
};

// The trainer's mask from the model's, in the model's frame: all-keep when
// there is none, the caller's compose, then the feature mask as the segment.
bool finish_mask(const MaskFramesSinks& sinks, size_t index, const sfm::ExifTransform& turn,
                 int width, int height, sam::Mask& mask, const sam::Mask& sky,
                 std::string& why) {
    if (mask.data.empty()) {
        mask.width = width;
        mask.height = height;
        mask.data.assign((size_t)width * height, kMaskKeep);
    }
    if (sinks.compose && !sinks.compose(index, turn, mask, why)) return false;
    fold_segment(mask.data, mask.width, mask.height, sky.data, sky.width, sky.height);
    return true;
}

bool run_in_process(const MaskFramesJob& job, const MaskFramesSinks& sinks,
                    const std::atomic<bool>& cancel, std::string& error,
                    MaskFramesStats& stats) {
    sam::Masker masker;
    if (!masker.init(job.mask, error)) return false;

    // Encoding a 1080p mask costs about a third of what the model costs to
    // produce it, and none of it needs the GPU.
    WriterPool writers;
    colorspace::Exposure exposure;
    colorspace::parse_exposure(job.exposure, exposure);
    std::vector<fs::path> files;
    files.reserve(job.frames.size());
    for (const MaskFrame& f : job.frames) files.push_back(f.source);
    ImagePrefetch reader(std::move(files), cancel, job.gamut, job.linear, exposure);
    for (size_t k = 0; k < job.frames.size(); ++k) {
        const MaskFrame& f = job.frames[k];
        if (cancel.load()) { error = lmsg::err_cancelled.get(); return false; }
        double t0 = nn::now_ms();
        sfm::ExifTransform turn;
        nn::Image img = reader.take(turn);
        stats.load_ms += nn::now_ms() - t0;
        if (img.empty()) {
            if (sinks.warn)
                sinks.warn(spirula::i18n::format(lmsg::warn_unreadable_skipped,
                                                 {f.source.string()}));
            continue;
        }
        t0 = nn::now_ms();
        sam::Mask mask, sky;
        sam::Result overlay;
        if (!masker.run(img, mask, sinks.overlay ? &overlay : nullptr, f.frame,
                        job.features ? &sky : nullptr)) {
            error = spirula::i18n::format(lmsg::err_masking_failed_on,
                                          {f.source.filename().string(), masker.lastError()});
            return false;
        }
        stats.model_ms += nn::now_ms() - t0;
        t0 = nn::now_ms();
        if (sinks.overlay) sinks.overlay(k, img, overlay);
        if (!job.train) mask = sam::Mask{};
        // Still the way up the model saw it, which is the frame the stencil's
        // shapes were drawn in.
        if (!finish_mask(sinks, k, turn, img.width, img.height, mask, sky, error)) return false;
        // And back into the frame the file stores, which is the one the
        // trainer reads it against (docs/datasets.md, "EXIF orientation").
        const sfm::ExifTransform back = inverse_turn(turn);
        turn_pixels(back, 1, mask.data, mask.width, mask.height);
        turn_pixels(back, img.channels, img.data, img.width, img.height);
        if (sinks.done) {
            MaskFrameDone d;
            d.index = k;
            d.kept = kept_share(mask);
            d.image = &img;
            d.mask = &mask;
            sinks.done(d);
        }
        if (f.write) {
            // Unlinked first: a mask gathered beside the images may be a hard
            // link to the user's own file.
            std::error_code ec;
            fs::remove(f.output, ec);
            WriteJob wj;
            wj.mask = std::move(mask);
            wj.path = f.output.string();
            writers.submit(std::move(wj));
        }
        stats.write_ms += nn::now_ms() - t0;
    }
    // The last few masks are still in the queue, and whoever called this goes
    // straight on to read them.
    writers.finish();
    if (sinks.report) sinks.report(masker);
    if (writers.failures() > 0) {
        error = std::to_string(writers.failures()) + " mask(s) could not be written";
        return false;
    }
    return true;
}

// Model ids to the files they stand for: the recipe pins the weights' bytes.
sam::MaskOptions resolved_models(sam::MaskOptions o) {
    if (birefnet::find_model_source(o.model)) o.model = birefnet::resolve_model(o.model);
    if (!o.detector.empty() && gdino::find_model_source(o.detector))
        o.detector = gdino::resolve_model(o.detector).weights;
    return o;
}

bool run_workers(const MaskFramesJob& job, const MaskFramesSinks& sinks,
                 const std::atomic<bool>& cancel, std::string& error) {
    const auto& frames = job.frames;
    const bool write_train = job.train, write_features = job.features;
    const uint64_t budget = default_worker_memory_bytes();
    fs::path scratch, local_scratch;
    bool scratch_owned = false, local_scratch_owned = false;
    try {
        const sam::MaskOptions mo = resolved_models(job.mask);
        const auto& devices = job.workers;
        fs::create_directories(job.scratch);
        const fs::path local_base = fs::temp_directory_path() / "spirula-mask-attempts";
        fs::create_directories(local_base);
        const auto nonce = (long long)std::chrono::steady_clock::now().time_since_epoch().count();
        for (unsigned attempt = 0;; ++attempt) {
            const auto name = std::to_string(nonce) + "-" + std::to_string(attempt);
            scratch = job.scratch / name;
            if (!fs::create_directory(scratch)) continue;
            scratch_owned = true;
            local_scratch = local_base / name;
            if (fs::create_directory(local_scratch)) {
                local_scratch_owned = true;
                break;
            }
            remove_tree(scratch);
            scratch_owned = false;
        }
        const std::string recipe = mask_worker_recipe(mo, job.gamut, job.linear, job.exposure);
        std::vector<WorkerTask> tasks;
        std::vector<std::vector<size_t>> groups;
        std::vector<std::string> source_hashes;
        source_hashes.reserve(frames.size());
        for (const MaskFrame& f : frames) {
            auto hash = spirula::sha256_file(f.source.string());
            if (hash.empty()) throw std::runtime_error("Cannot freeze mask source identity");
            source_hashes.push_back(std::move(hash));
        }
        if (mo.video) {
            groups.emplace_back();
            for (size_t k = 0; k < frames.size(); ++k) groups.back().push_back(k);
        } else {
            for (size_t k = 0; k < frames.size(); k += 8) {
                groups.emplace_back();
                for (size_t i = k; i < std::min(k + 8, frames.size()); ++i)
                    groups.back().push_back(i);
            }
        }
        {
            spirula::BufferedOutputFile records;
            for (size_t group = 0; group < groups.size(); ++group) {
                const auto manifest = local_scratch / ("task-" + std::to_string(group) + ".jsonl");
                records.open(manifest, std::ios::binary | std::ios::trunc);
                if (!records) throw std::runtime_error("Cannot create mask input manifest");
                uint64_t bytes = 0;
                for (size_t k : groups[group]) {
                    if (cancel.load()) throw std::runtime_error("Mask preparation canceled");
                    int width = 0, height = 0;
                    const auto& source = frames[k].source;
                    if (!probe_size(source, width, height))
                        throw std::runtime_error("Cannot size required mask input");
                    // Two frames cover decode/inference overlap and pending mask writes.
                    const uint64_t side = (uint64_t)std::max(1024, mo.max_size);
                    const uint64_t pixels = std::max(uint64_t(width) * height, side * side);
                    if (pixels > budget / 256)
                        throw std::runtime_error("Mask image exceeds shared host-memory budget");
                    bytes = std::max(bytes, pixels * 256);
                    JsonWriter item(false);
                    // Feature prompts participate in inference even for a retained destination.
                    write_mask_manifest_record(records, item.object()
                        .field("source", source.string())
                        .field("source_sha256", source_hashes[k])
                        .field("stage", (local_scratch / std::to_string(k)).string())
                        .field("frame", std::to_string(frames[k].frame))
                        .field("train", frames[k].write && write_train)
                        .field("feature", write_features).end().str());
                }
                records.close();
                if (!records) throw std::runtime_error("Could not close mask input manifest");
                JsonWriter request;
                tasks.push_back({request.object().field("id", std::to_string(group))
                    .field("recipe", recipe).field("manifest", manifest.string())
                    .field("count", std::to_string(groups[group].size()))
                    .field("threads", (int)std::max(1u, spirula::hardware_threads() /
                        (unsigned)std::max<size_t>(1, std::min(devices.size(), groups.size()))))
                    .end().str(), bytes});
            }
        }
        std::vector<std::vector<std::string>> commands;
        for (const auto& device : devices)
            commands.push_back({exe_path(), "sam", "--mask-worker", device});
        const uint64_t resident_per_worker = 2 * (fs::file_size(mo.model) +
            (mo.detector.empty() ? 0 : fs::file_size(mo.detector))) + 16 * 1024 * 1024;
        const uint64_t admitted = std::min(commands.size(), tasks.size());
        if (admitted && resident_per_worker > budget / admitted)
            throw std::runtime_error("Mask model residency exceeds shared host-memory budget");
        const uint64_t task_budget = budget - resident_per_worker * admitted;
        std::vector<double> kept(frames.size(), 0.0);
        const bool ok = run_inference_workers(commands, tasks, task_budget, cancel,
            [&](size_t group, const std::string&, std::string& why) {
                if (cancel.load()) { why = "Mask publication canceled"; return false; }
                for (size_t k : groups[group]) {
                    const auto family = scratch / std::to_string(k);
                    if (frames[k].write) {
                        if (cancel.load()) throw std::runtime_error("Mask publication canceled");
                        spirula::replace_file(family / "train-final.png", frames[k].output);
                    }
                    remove_tree(local_scratch / std::to_string(k));
                    remove_tree(family);
                    if (sinks.done) {
                        MaskFrameDone d;
                        d.index = k;
                        d.kept = kept[k];
                        sinks.done(d);
                    }
                }
                return true;
            }, [&](const std::string& line) { if (sinks.log) sinks.log(line); }, error,
            [&](size_t group, const std::string& response, std::string& why) {
                const auto reply = json_parse(response);
                const auto* identity = reply.find("id");
                const auto* count = reply.find("count");
                if (!identity || identity->as_string() != std::to_string(group) ||
                    !count || count->as_string() != std::to_string(groups[group].size())) {
                    why = "Mask worker returned the wrong output family";
                    return false;
                }
                spirula::BufferedInputFile outputs(local_scratch / ("task-" + std::to_string(group) + ".jsonl.outputs"),
                    std::ios::binary);
                if (!outputs) { why = "Missing mask output manifest"; return false; }
                std::string record;
                for (size_t index = 0; index < groups[group].size(); ++index) {
                    if (cancel.load()) { why = "Mask publication canceled"; return false; }
                    if (!read_mask_manifest_record(outputs, record)) {
                        why = "Incomplete mask output manifest";
                        return false;
                    }
                    const size_t k = groups[group][index];
                    const auto value = json_parse(record);
                    const int width = (int)value.get_double("width", 0);
                    const int height = (int)value.get_double("height", 0);
                    const auto* mirror = value.find("mirror");
                    sfm::ExifTransform turn;
                    turn.turns_cw = (int)value.get_double("turn", -1);
                    turn.mirror = mirror && mirror->as_bool();
                    if (width <= 0 || height <= 0 || turn.turns_cw < 0 || turn.turns_cw > 3 || !mirror) {
                        why = "Invalid mask worker image/orientation metadata";
                        return false;
                    }
                    int expected_width = 0, expected_height = 0;
                    const auto& source = frames[k].source;
                    const auto expected_turn = photo_turn(source.string());
                    if (!probe_size(source, expected_width, expected_height)) {
                        why = "Required mask source became unreadable";
                        return false;
                    }
                    if (expected_turn.turns_cw & 1) std::swap(expected_width, expected_height);
                    if (width != expected_width || height != expected_height ||
                        turn.turns_cw != expected_turn.turns_cw || turn.mirror != expected_turn.mirror) {
                        why = "Mask output does not match its source orientation/dimensions";
                        return false;
                    }
                    if (spirula::sha256_file(source.string()) != source_hashes[k]) {
                        why = "Mask source changed during inference";
                        return false;
                    }
                    auto load = [&](const char* name, bool required, sam::Mask& mask) {
                        if (!required) return true;
                        int w = 0, h = 0, channels = 0;
                        unsigned char* data = stbi_load((local_scratch / std::to_string(k) / name).string().c_str(),
                            &w, &h, &channels, 1);
                        if (!data || w != width || h != height) {
                            stbi_image_free(data);
                            why = "Missing or invalid required staged mask";
                            return false;
                        }
                        mask.width = w; mask.height = h;
                        mask.data.assign(data, data + size_t(w) * h);
                        stbi_image_free(data);
                        return true;
                    };
                    sam::Mask mask, feature;
                    const bool wanted = frames[k].write;
                    if (!load("train.png", wanted && write_train, mask) ||
                        !load("feature.png", wanted && write_features, feature)) return false;
                    if (wanted && !finish_mask(sinks, k, turn, width, height, mask, feature, why))
                        return false;
                    turn_pixels(inverse_turn(turn), 1, mask.data, mask.width, mask.height);
                    kept[k] = kept_share(mask);
                    const auto family = scratch / std::to_string(k);
                    fs::create_directory(family);
                    if (wanted && !sam::save_mask_png(mask, (family / "train-final.png").string())) {
                        why = "Could not close complete postprocessed mask family";
                        return false;
                    }
                }
                if (cancel.load()) { why = "Mask publication canceled"; return false; }
                if (read_mask_manifest_record(outputs, record)) {
                    why = "Unexpected extra mask output record";
                    return false;
                }
                return true;
            }, [&](size_t group) {
                if (sinks.settled)
                    for (size_t k : groups[group]) sinks.settled(k);
            });
        remove_tree(scratch);
        remove_tree(local_scratch);
        return ok;
    } catch (const std::exception& e) {
        if (scratch_owned) remove_tree(scratch);
        if (local_scratch_owned) remove_tree(local_scratch);
        error = e.what();
        return false;
    }
}

}  // namespace

bool mask_has_target(const sam::MaskOptions& o) {
    return sam::is_subject_model(o.model) || !o.seeds.empty() ||
           !sam::split_phrases(o.text).empty();
}

bool mask_has_features(const sam::MaskOptions& o) {
    return !sam::is_subject_model(o.model) && !sam::split_phrases(o.feature_text).empty();
}

bool mask_frames(const MaskFramesJob& job, const MaskFramesSinks& sinks, std::string& error,
                 MaskFramesStats* stats) {
    if (job.frames.empty()) return true;
    static const std::atomic<bool> never{false};
    const std::atomic<bool>& cancel = sinks.cancel ? *sinks.cancel : never;
    MaskFramesStats local;
    try {
        fs::path made;
        for (const MaskFrame& f : job.frames) {
            if (!f.write || f.output.parent_path() == made) continue;
            made = f.output.parent_path();
            if (!made.empty()) fs::create_directories(made);
        }
        return job.workers.empty()
                   ? run_in_process(job, sinks, cancel, error, stats ? *stats : local)
                   : run_workers(job, sinks, cancel, error);
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

}  // namespace app
