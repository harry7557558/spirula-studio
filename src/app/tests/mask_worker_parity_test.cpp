#include "app/FrameLook.h"
#include "app/InferenceWorker.h"
#include "app/InferenceWorkerDevices.h"
#include "app/MaskWorker.h"
#include "core/Sha256.h"
#include "core/HardwareThreads.h"
#include "data/JsonWrite.h"
#include "external/stb_image.h"
#include "external/stb_image_write.h"
#include "nn/Device.h"
#include "nn/core/Parallel.h"
#include <algorithm>
#include <chrono>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;
void require(bool value, const char* why) { if (!value) throw std::runtime_error(why); }

int main(int argc, char** argv) {
    if (argc == 3 && std::string(argv[1]) == "--child") {
        const int rc = app::mask_worker_main(argv[2]);
        // Match Main::finish_tool: release Vulkan before static destruction.
        nn::shutdown();
        return rc;
    }
    if (argc == 2 && std::string(argv[1]) == "--manifest-check") {
        try {
            const std::string source = "folder\\quoted\"name\nwith\rcarriage\treturn.png";
            JsonWriter input(false), output(false);
            const std::vector<std::string> items{
                input.object().field("source", source).field("source_sha256", "hash")
                    .field("stage", "scratch").field("frame", "7")
                    .field("train", true).field("feature", false).end().str(),
                output.object().field("turn", 1).field("mirror", true)
                    .field("width", 96).field("height", 64).end().str()};
            std::stringstream encoded;
            for (const auto& item : items) {
                require(item.find_first_of("\r\n") == std::string::npos,
                    "Compact JSON contains a physical record delimiter");
                app::write_mask_manifest_record(encoded, item);
            }
            std::string decoded;
            require(app::read_mask_manifest_record(encoded, decoded) && decoded == items[0],
                "JsonWriter input record did not round-trip");
            const auto parsed_input = json_parse(decoded);
            require(parsed_input.find("source") && parsed_input.find("source")->as_string() == source,
                "Manifest string escapes changed");
            require(app::read_mask_manifest_record(encoded, decoded) && decoded == items[1],
                "JsonWriter output record did not round-trip");
            const auto parsed_output = json_parse(decoded);
            require(parsed_output.get_double("turn", -1) == 1 &&
                parsed_output.get_double("width", 0) == 96 &&
                parsed_output.get_double("height", 0) == 64 &&
                parsed_output.find("mirror") && parsed_output.find("mirror")->as_bool(),
                "Manifest metadata changed");
            require(!app::read_mask_manifest_record(encoded, decoded),
                "JsonWriter appended extra manifest records");
            JsonWriter pretty;
            require(pretty.object().field("value", 1).end().str() == "{\n    \"value\": 1\n}\n",
                "Default JSON file formatting changed");
            const std::string item = "{\"source\":\"" + std::string(1024, 'x') + "\"}";
            std::stringstream manifest;
            for (size_t i = 0; i < 2048; ++i) app::write_mask_manifest_record(manifest, item);
            require(manifest.str().size() > 1024 * 1024, "Regression manifest does not exceed frame bound");
            std::string record;
            for (size_t i = 0; i < 2048; ++i) {
                require(app::read_mask_manifest_record(manifest, record) && record == item,
                    "Large sequence manifest lost ordering or records");
            }
            require(!app::read_mask_manifest_record(manifest, record), "Manifest grew extra records");
            for (const auto& invalid : {std::string("partial"), std::string("\n"),
                    std::string(app::kMaskManifestRecordBytes + 1, 'x') + "\n"}) {
                std::istringstream input(invalid);
                bool rejected = false;
                try { app::read_mask_manifest_record(input, record); }
                catch (const std::exception&) { rejected = true; }
                require(rejected, "Malformed or unbounded manifest record accepted");
            }
            std::printf("compact JSON input/output and bounded large-sequence mask manifest checks passed\n");
            return 0;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "%s\n", e.what());
            return 1;
        }
    }
    if (argc != 3) {
        std::fprintf(stderr, "Usage: mask_worker_parity_test <local-model> <uuid:...[,uuid:...]>\n");
        return 2;
    }
    fs::path root;
    bool root_owned = false;
    try {
        const auto devices = app::expand_worker_devices(
            app::parse_worker_devices(argv[2]), app::workers_per_gpu());
        root = fs::temp_directory_path() / ("spirula-mask-parity-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        root_owned = fs::create_directory(root);
        require(root_owned, "Cannot create exclusive parity scratch");
        std::vector<std::string> sources;
        for (int image = 0; image < 4; ++image) {
            std::vector<unsigned char> pixels(96 * 64 * 3);
            for (size_t k = 0; k < pixels.size(); ++k) pixels[k] = (unsigned char)((k * 13 + image * 41) % 256);
            const auto path = root / (std::to_string(image) + ".png");
            require(stbi_write_png(path.string().c_str(), 96, 64, 3, pixels.data(), 96 * 3) != 0,
                "Cannot write synthetic image");
            sources.push_back(path.string());
        }
        for (bool temporal : {false, true}) {
            sam::MaskOptions options;
            options.model = fs::absolute(argv[1]).string();
            options.device = devices.front();
            options.video = temporal;
            options.text = "person";
            options.feature_text = sam::is_subject_model(options.model) ? "" : "sky";
            if (temporal && !sam::is_subject_model(options.model)) {
                sam::SeedPrompt seed;
                seed.frame = 0; seed.object = 2; seed.prompt.pos_points.push_back({48, 32});
                options.seeds.push_back(seed);
            }
            const auto recipe = app::mask_worker_recipe(options, "Rec.709", std::nullopt, "0");
            std::vector<sam::Mask> expected, expected_features;
            {
                sam::Masker canonical;
                std::string why;
                nn::set_parallel_threads(1);
                nn::configure_device(options.device);
                if (!canonical.init(options, why)) throw std::runtime_error(why);
                for (size_t k = 0; k < sources.size(); ++k) {
                    sfm::ExifTransform turn;
                    colorspace::Exposure exposure;
                    require(colorspace::parse_exposure("0", exposure), "Exposure parse failed");
                    const auto image = app::load_upright(sources[k], "Rec.709", std::nullopt, exposure, turn);
                    sam::Mask mask, feature;
                    if (!canonical.run(image, mask, nullptr, (int64_t)k,
                        options.feature_text.empty() ? nullptr : &feature))
                        throw std::runtime_error(canonical.lastError());
                    expected.push_back(std::move(mask));
                    expected_features.push_back(std::move(feature));
                }
            }
            nn::shutdown();
            std::vector<std::vector<size_t>> groups;
            if (temporal) groups.push_back({0, 1, 2, 3});
            else groups = {{0, 1}, {2, 3}};
            std::vector<app::WorkerTask> tasks;
            for (size_t group = 0; group < groups.size(); ++group) {
                const auto manifest = root / ((temporal ? "sequence-" : "independent-") +
                    std::to_string(group) + ".jsonl");
                std::ofstream records(manifest, std::ios::binary | std::ios::trunc);
                for (size_t k : groups[group]) {
                    JsonWriter item(false);
                    app::write_mask_manifest_record(records, item.object().field("source", sources[k])
                        .field("source_sha256", spirula::sha256_file(sources[k]))
                        .field("stage", (root / (temporal ? "sequence" : "independent") / std::to_string(k)).string())
                        .field("frame", std::to_string(k)).field("train", true)
                        .field("feature", !options.feature_text.empty()).end().str());
                }
                records.close();
                require(bool(records), "Cannot close parity input manifest");
                JsonWriter request;
                tasks.push_back({request.object().field("id", std::to_string(group)).field("recipe", recipe)
                    .field("manifest", manifest.string()).field("count", std::to_string(groups[group].size()))
                    .field("threads", (int)std::max(1u, spirula::hardware_threads() / (unsigned)devices.size()))
                    .end().str(), 2 * 96 * 64 * 128});
            }
            std::vector<std::vector<std::string>> commands;
            for (const auto& device : devices) commands.push_back({fs::absolute(argv[0]).string(), "--child", device});
            std::atomic<bool> cancel{false};
            std::string error;
            size_t checked = 0;
            const bool workers_ok = app::run_inference_workers(commands, tasks, 4 * 96 * 64 * 128, cancel,
                [&](size_t group, const std::string& response, std::string&) {
                    const auto reply = json_parse(response);
                    require(reply.find("id") && reply.find("id")->as_string() == std::to_string(group), "Wrong task identity");
                    require(reply.find("count") && reply.find("count")->as_string() ==
                        std::to_string(groups[group].size()), "Incomplete sequence family");
                    const auto* selected = reply.find("device");
                    require(selected && std::find(devices.begin(), devices.end(), selected->as_string()) != devices.end(),
                        "Mask model initialized on an unassigned device");
                    const auto manifest = root / ((temporal ? "sequence-" : "independent-") +
                        std::to_string(group) + ".jsonl.outputs");
                    std::ifstream metadata(manifest, std::ios::binary);
                    std::string record;
                    for (size_t k : groups[group]) {
                        require(app::read_mask_manifest_record(metadata, record), "Missing output metadata");
                        const auto value = json_parse(record);
                        require(value.get_double("width", 0) == 96 && value.get_double("height", 0) == 64,
                            "Invalid output metadata");
                        auto compare = [&](const char* name, const sam::Mask& reference) {
                            const auto path = root / (temporal ? "sequence" : "independent") / std::to_string(k) / name;
                            int width = 0, height = 0, channels = 0;
                            unsigned char* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, 1);
                            const bool same = pixels && width == reference.width && height == reference.height &&
                                std::equal(reference.data.begin(), reference.data.end(), pixels);
                            stbi_image_free(pixels);
                            require(same, "Worker masks differ from canonical same-device reference");
                            const auto canonical = path.string() + ".canonical.png";
                            require(sam::save_mask_png(reference, canonical), "Cannot encode canonical mask");
                            require(spirula::sha256_file(canonical) == spirula::sha256_file(path.string()),
                                "Worker PNG bytes differ from canonical encoding");
                        };
                        compare("train.png", expected[k]);
                        if (!options.feature_text.empty()) compare("feature.png", expected_features[k]);
                        ++checked;
                    }
                    require(!app::read_mask_manifest_record(metadata, record), "Extra output metadata");
                    return true;
                }, [](const std::string& line) { std::fprintf(stderr, "%s\n", line.c_str()); }, error);
            require(workers_ok, error.c_str());
            require(checked == sources.size(), "Required masks were not accepted");
        }
        fs::remove_all(root);
        std::printf("canonical independent/whole-sequence worker mask parity passed\n");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        if (root_owned)
            std::fprintf(stderr, "Parity scratch retained for diagnosis: %s\n", root.string().c_str());
        return 1;
    }
}
