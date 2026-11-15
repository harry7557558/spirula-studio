#include "app/MaskWorker.h"
#include "app/InferenceWorker.h"
#include "app/FrameLook.h"
#include "app/WriterPool.h"
#include "core/ImageFile.h"
#include "core/Sha256.h"
#include "data/JsonWrite.h"
#include "external/stb_image_write.h"
#include "nn/Device.h"
#include "sfm/core/Exif.h"
#include <cstdio>
#include "nn/core/Parallel.h"
#include <filesystem>
#include <fstream>
#include <future>
#include <stdexcept>

namespace app {
namespace {
const JsonValue& required(const JsonValue& value, const char* key) {
    const auto* result = value.find(key);
    if (!result) throw std::runtime_error(std::string("Missing mask worker field: ") + key);
    return *result;
}
}

bool read_mask_manifest_record(std::istream& input, std::string& record) {
    record.clear();
    for (;;) {
        const auto next = input.get();
        if (next == std::char_traits<char>::eof()) {
            if (!input.eof() || !record.empty())
                throw std::runtime_error("Truncated mask manifest record");
            return false;
        }
        if (next == '\n') {
            if (record.empty()) throw std::runtime_error("Empty mask manifest record");
            return true;
        }
        if (record.size() == kMaskManifestRecordBytes)
            throw std::runtime_error("Mask manifest record exceeds byte bound");
        record.push_back((char)next);
    }
}

void write_mask_manifest_record(std::ostream& output, const std::string& record) {
    if (record.empty() || record.size() > kMaskManifestRecordBytes ||
        record.find('\n') != std::string::npos)
        throw std::runtime_error("Invalid mask manifest record");
    output.write(record.data(), (std::streamsize)record.size()).put('\n');
    if (!output) throw std::runtime_error("Could not write mask manifest record");
}

std::string mask_worker_recipe(const sam::MaskOptions& o, const std::string& gamut,
                               std::optional<bool> linear, const std::string& exposure) {
    JsonWriter w;
    const std::string model_hash = spirula::sha256_file(o.model);
    const std::string detector_hash = o.detector.empty() ? "" : spirula::sha256_file(o.detector);
    if (model_hash.empty() || (!o.detector.empty() && detector_hash.empty()))
        throw std::runtime_error("Mask worker requires resolved local model files");
    w.object().field("model", o.model).field("detector", o.detector)
        .field("model_sha256", model_hash).field("detector_sha256", detector_hash)
        .field("detector_threshold", o.detector_threshold).field("text", o.text)
        .field("neg_text", o.neg_text).field("feature_text", o.feature_text)
        .field("video", o.video).field("keep_prompted", o.keep_prompted)
        .field("dilate_ratio", o.dilate_ratio).field("threshold", o.threshold)
        .field("nms", o.nms).field("detect_every", o.detect_every)
        .field("memory_frames", o.memory_frames).field("max_size", o.max_size)
        .field("gamut", gamut).field("exposure", exposure)
        .field("linear", linear ? (*linear ? 1 : 0) : -1).key("seeds").array();
    for (const auto& seed : o.seeds) {
        w.object().field("object", seed.object).field("frame", std::to_string(seed.frame));
        auto points = [&](const char* key, const auto& values) {
            w.key(key).array();
            for (const auto& p : values) w.array().value(p.x).value(p.y).end();
            w.end();
        };
        points("positive", seed.prompt.pos_points);
        points("negative", seed.prompt.neg_points);
        // DatasetPrep click seeds never contain boxes or multimask prompts.
        if (seed.prompt.use_box || seed.prompt.multimask) throw std::runtime_error("Unsupported mask worker seed recipe");
        w.end();
    }
    return w.end().end().str();
}

int mask_worker_main(const std::string& device) {
    if (!worker_begin()) return 1;
    nn::set_parallel_threads(1);
    try {
        nn::configure_device(device);
        sam::Masker masker;
        std::string immutable_recipe, request;
        bool temporal_done = false;
        while (worker_read_frame(request)) {
            const auto task = json_parse(request);
            const auto* threads = task.find("threads");
            nn::set_parallel_threads(threads ? std::max(1, (int)threads->as_int()) : 1);
            const std::string recipe = required(task, "recipe").as_string();
            const auto config = json_parse(recipe);
            const bool temporal = required(config, "video").as_bool();
            if (immutable_recipe.empty()) {
                immutable_recipe = recipe;
                sam::MaskOptions o;
                o.device = device;
                o.model = required(config, "model").as_string();
                o.detector = required(config, "detector").as_string();
                o.detector_threshold = (float)required(config, "detector_threshold").as_double();
                o.text = required(config, "text").as_string();
                o.neg_text = required(config, "neg_text").as_string();
                o.feature_text = required(config, "feature_text").as_string();
                o.video = temporal;
                o.keep_prompted = required(config, "keep_prompted").as_bool();
                o.dilate_ratio = (float)required(config, "dilate_ratio").as_double();
                o.threshold = (float)required(config, "threshold").as_double();
                o.nms = (float)required(config, "nms").as_double();
                o.detect_every = (int)required(config, "detect_every").as_int();
                o.memory_frames = (int)required(config, "memory_frames").as_int();
                o.max_size = (int)required(config, "max_size").as_int();
                for (const auto& value : required(config, "seeds").arr) {
                    sam::SeedPrompt seed;
                    seed.object = (int)required(value, "object").as_int();
                    seed.frame = std::stoll(required(value, "frame").as_string());
                    auto points = [&](const char* key, auto& dst) {
                        for (const auto& p : required(value, key).arr) {
                            if (p.arr.size() != 2) throw std::runtime_error("Invalid seed point");
                            dst.push_back({(float)p.arr[0].as_double(), (float)p.arr[1].as_double()});
                        }
                    };
                    points("positive", seed.prompt.pos_points);
                    points("negative", seed.prompt.neg_points);
                    o.seeds.push_back(std::move(seed));
                }
                if (spirula::sha256_file(o.model) != required(config, "model_sha256").as_string() ||
                    (!o.detector.empty() && spirula::sha256_file(o.detector) !=
                        required(config, "detector_sha256").as_string()))
                    throw std::runtime_error("Mask model identity changed before initialization");
                std::string error;
                if (!masker.init(o, error)) throw std::runtime_error(error);
            } else if (immutable_recipe != recipe || (temporal && temporal_done)) {
                throw std::runtime_error("Mask worker recipe/sequence changed in flight");
            }
            colorspace::Exposure exposure;
            if (!colorspace::parse_exposure(required(config, "exposure").as_string(), exposure))
                throw std::runtime_error("Invalid exposure recipe");
            const int linear_value = (int)required(config, "linear").as_int();
            const std::optional<bool> linear = linear_value < 0 ? std::nullopt : std::optional<bool>(linear_value != 0);
            const std::string manifest = required(task, "manifest").as_string();
            const uint64_t count = std::stoull(required(task, "count").as_string());
            if (!count) throw std::runtime_error("Empty mask task");
            std::ifstream inputs(manifest, std::ios::binary);
            std::ofstream outputs(manifest + ".outputs", std::ios::binary | std::ios::trunc);
            if (!inputs || !outputs) throw std::runtime_error("Cannot open mask task manifests");
            struct Prepared {
                JsonValue item;
                nn::Image image;
                sfm::ExifTransform turn;
            };
            auto prepare = [&] {
                std::string record;
                if (!read_mask_manifest_record(inputs, record))
                    throw std::runtime_error("Incomplete mask input manifest");
                Prepared p;
                p.item = json_parse(record);
                if (spirula::sha256_file(required(p.item, "source").as_string()) !=
                    required(p.item, "source_sha256").as_string())
                    throw std::runtime_error("Mask source identity changed before inference");
                p.image = load_upright(required(p.item, "source").as_string(),
                    required(config, "gamut").as_string(), linear, exposure, p.turn);
                if (p.image.empty()) throw std::runtime_error("Unreadable required mask input");
                return p;
            };
            app::WriterPool writers((int)nn::parallel_threads());
            auto ahead = std::async(std::launch::async, prepare);
            for (uint64_t index = 0; index < count; ++index) {
                auto prepared = ahead.get();
                if (index + 1 < count) ahead = std::async(std::launch::async, prepare);
                const auto& item = prepared.item;
                const auto& image = prepared.image;
                const auto& turn = prepared.turn;
                sam::Mask mask, feature;
                const bool want_feature = required(item, "feature").as_bool();
                if (!masker.run(image, mask, nullptr, std::stoll(required(item, "frame").as_string()), want_feature ? &feature : nullptr))
                    throw std::runtime_error(masker.lastError());
                const std::string root = required(item, "stage").as_string();
                writers.wait();
                std::filesystem::create_directories(root);
                auto save = [&](sam::Mask&& m, const char* name, bool required_output) {
                    if (!required_output) return;
                    if (m.width != image.width || m.height != image.height ||
                        m.data.size() != (size_t)m.width * m.height)
                        throw std::runtime_error("Invalid required mask dimensions");
                    app::WriteJob job;
                    job.path = (std::filesystem::path(root) / name).string();
                    job.mask = std::move(m);
                    writers.submit(std::move(job));
                };
                save(std::move(mask), "train.png", required(item, "train").as_bool());
                save(std::move(feature), "feature.png", want_feature);
                JsonWriter metadata(false);
                write_mask_manifest_record(outputs, metadata.object()
                    .field("turn", turn.turns_cw).field("mirror", turn.mirror)
                    .field("width", image.width).field("height", image.height).end().str());
            }
            writers.finish();
            if (writers.failures()) throw std::runtime_error("Could not stage required mask output");
            std::string record;
            if (read_mask_manifest_record(inputs, record))
                throw std::runtime_error("Unexpected extra mask input record");
            outputs.close();
            if (!outputs) throw std::runtime_error("Could not close mask output manifest");
            JsonWriter reply;
            reply.object().field("id", required(task, "id").as_string())
                .field("count", std::to_string(count))
                .field("device", nn::current_device_selector()).end();
            temporal_done = temporal;
            if (!worker_write_frame(reply.str())) return 1;
        }
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Mask worker: %s\n", e.what());
        return 1;
    }
}
}  // namespace app
