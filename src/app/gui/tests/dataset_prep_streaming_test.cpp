#include "app/InferenceWorkerDevices.h"
#include "app/MaskWorker.h"
#include "app/gui/DatasetPrep.h"
#include "app/gui/Subprocess.h"
#include "i18n/catalog/Data.h"
#include "nn/Device.h"
#include "core/Sha256.h"
#include "external/stb_image_write.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) ++failures;
}

void extract(const fs::path& video, const fs::path& workspace, bool block_write) {
    gui::PrepJob job;
    job.workspace = workspace.string();
    job.resume = false;
    job.force_external_decode = true;
    job.video_fps = gui::kFpsEveryFrame;
    job.sharp_window = 1;
    job.max_frames = 4;
    gui::PrepInput input;
    input.path = video.string();
    input.is_video = true;
    input.lut = gui::kLutNone;
    job.inputs.push_back(input);
    if (block_write) fs::create_directories(workspace / "images/00000.jpg");

    gui::RunProgress progress;
    std::atomic<bool> cancel{false};
    gui::DatasetPrep prep(&progress, {}, cancel);
    gui::PrepResult out;
    std::string error;
    bool frames_done = false;
    const bool ok = prep.run(job, out, error, {},
        [&](gui::Stage stage, const gui::PrepJob&) {
            if (stage == gui::Stage::Frames) frames_done = true;
        });
    const int saved = gui::DatasetPrep::count_images((workspace / "images").string());
    if (block_write) {
        check(saved == 3, "one blocked write leaves three successfully saved frames");
        check(!ok, "a partial write failure fails preparation");
        check(!frames_done, "a partial extraction does not complete the frames step");
        check(error == spirula::i18n::format(spirula::i18n::msg::data::write_failures, {1}),
              "the error reports the failed write count: " + error);
    } else {
        check(ok && error.empty(), "successful extraction completes preparation: " + error);
        check(saved == 4 && out.n_images == 4, "all four frames reach disk");
        check(frames_done, "successful extraction completes the frames step");
    }
}

void one_packed_lens(const fs::path& video, const fs::path& workspace) {
    gui::PrepJob job;
    job.workspace = workspace.string();
    job.resume = false;
    job.force_external_decode = true;
    job.video_fps = gui::kFpsEveryFrame;
    job.sharp_window = 1;
    job.max_frames = 4;
    gui::PrepInput input;
    input.path = video.string();
    input.is_video = true;
    input.lut = gui::kLutNone;
    input.packed_lenses = 2;
    input.active_lens = 1;
    job.inputs.push_back(input);
    gui::RunProgress progress;
    std::atomic<bool> cancel{false};
    gui::DatasetPrep prep(&progress, {}, cancel);
    gui::PrepResult out;
    std::string error;
    check(prep.run(job, out, error), "one packed lens: " + error);
    check(!out.per_folder_cameras, "one packed lens is one camera");
    const fs::path images = workspace / "images";
    check(!fs::exists(images / "cam0") &&
              gui::DatasetPrep::count_images((images / "cam1").string()) == 4 &&
              gui::DatasetPrep::count_images(images.string()) == 4,
          "only the lens kept is written, and no whole frame is left");
}

void parallel_extract(const fs::path& video, const fs::path& root, bool blur = false) {
    gui::PrepJob job;
    job.resume = false;
    job.force_external_decode = true;
    job.video_fps = gui::kFpsEveryFrame;
    job.sharp_window = 1;
    job.max_frames = 4;
    gui::PrepInput input;
    input.path = video.string();
    input.is_video = true;
    input.lut = gui::kLutNone;
    input.packed_lenses = blur ? 0 : 2;
    if (blur) {
        job.video_fps = 5;
        job.skip_blurry = true;
        job.blur_px = 1000;
        job.blur_neighbour = 0;
        job.similar_bits = 0;
    }
    input.subdir = "a";
    job.inputs.push_back(input);
    std::atomic<bool> cancel{false};
    auto run = [&](const char* name) {
        job.workspace = (root / name).string();
        gui::RunProgress progress;
        gui::DatasetPrep prep(&progress, {}, cancel);
        gui::PrepResult output;
        std::string error;
        check(prep.run(job, output, error), std::string(name) + ": " + error);
        check(output.captures.size() == job.inputs.size(), "ordered capture count");
        for (size_t k = 0; k < output.captures.size(); ++k)
            check(output.captures[k].subdir == job.inputs[k].subdir, "capture order");
    };
    run("serial");
    input.subdir = "b";
    job.inputs.push_back(input);
    run("parallel");
    size_t compared = 0;
    for (const auto& file : fs::recursive_directory_iterator(root / "serial/images/a")) {
        if (!file.is_regular_file()) continue;
        const auto relative = fs::relative(file.path(), root / "serial/images/a");
        const auto hash = spirula::sha256_file(file.path().string());
        for (const char* camera : {"a", "b"})
            check(!hash.empty() && hash == spirula::sha256_file(
                (root / "parallel/images" / camera / relative).string()),
                "parallel packed JPEG parity: " + relative.string());
        ++compared;
    }
    check(blur ? compared > 0 : compared == 8, "selected frames reach both output folders");
    job.adaptive_fps = true;
    job.video_fps = 5;
    run("adaptive");
}

constexpr int photo_width = 128;
constexpr int photo_height = 96;

void require(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}

std::string frame_name(int frame) {
    return std::to_string(frame) + ".png";
}

fs::path mask_path(const gui::PrepJob& job, int frame, bool feature = false) {
    return fs::path(job.workspace) / (feature ? gui::kFeatureMaskDirName : "masks") /
           frame_name(frame);
}

gui::PrepJob mask_job(const fs::path& root, int frames, const fs::path& model, const char* device) {
    const auto photos = root / "photos";
    fs::create_directories(photos);
    std::vector<unsigned char> rgb(photo_width * photo_height * 3);
    for (int i = 0; i < frames; ++i) {
        for (int y = 0; y < photo_height; ++y) {
            for (int x = 0; x < photo_width; ++x) {
                const bool object = x >= 20 + 9 * i && x < 72 + 9 * i && y >= 23 && y < 74;
                auto* pixel = &rgb[(y * photo_width + x) * 3];
                pixel[0] = object ? 220 : 32;
                pixel[1] = object ? 88 : 139;
                pixel[2] = object ? 39 : 191;
            }
        }
        require(stbi_write_png((photos / frame_name(i)).string().c_str(),
            photo_width, photo_height, 3, rgb.data(), photo_width * 3) != 0,
            "write synthetic mask input");
    }
    gui::PrepJob job;
    job.workspace = (root / "workspace").string();
    job.photo_import = gui::PhotoImport::InPlace;
    job.inputs.emplace_back().path = photos.string();
    job.mask_enable = true;
    job.mask_model_path = model.string();
    job.mask_prompt = "red rectangle";
    job.mask_feature_prompt = "blue background";
    job.mask_max_image_size = photo_width;
    job.inference_devices = device;
    return job;
}

void seed_masks(const gui::PrepJob& job, int frames) {
    for (bool feature : {false, true}) {
        if (feature && job.mask_feature_prompt.empty()) continue;
        const std::vector<unsigned char> saved(photo_width * photo_height, feature ? 91 : 37);
        fs::create_directories(mask_path(job, 0, feature).parent_path());
        for (int i = 0; i < frames; ++i)
            require(stbi_write_png(mask_path(job, i, feature).string().c_str(),
                photo_width, photo_height, 1, saved.data(), photo_width) != 0,
                "write distinctive retained mask");
    }
}

std::string mask_hash(const gui::PrepJob& job, int frame, bool feature = false) {
    return spirula::sha256_file(mask_path(job, frame, feature).string());
}

gui::StageProgress run_masks(const gui::PrepJob& job, const std::string& label, const char* expected_error = nullptr) {
    gui::RunProgress progress;
    const std::atomic<bool> cancel{false};
    gui::DatasetPrep prep(&progress, {}, cancel);
    gui::PrepResult out;
    bool masks_done = false;
    std::string error;
    const bool ok = prep.run(job, out, error, {},
        [&](gui::Stage stage, const gui::PrepJob&) { masks_done |= stage == gui::Stage::Masks; });
    if (expected_error) {
        check(!ok && !masks_done && error.find(expected_error) != std::string::npos, label + ": " + error);
        return {};
    }
    if (!ok)
        for (const auto& line : progress.drain()) std::fprintf(stderr, "%s\n", line.text.c_str());
    require(ok && error.empty() && masks_done &&
            out.mask_dir == mask_path(job, 0).parent_path().string() &&
            (job.mask_feature_prompt.empty() ||
             out.feature_mask_dir == mask_path(job, 0, true).parent_path().string()),
            label + ": " + error);
    return progress.stage(gui::Stage::Masks);
}

void click_first(gui::PrepJob& job) {
    auto& click = job.mask_clicks.emplace_back();
    click.x = 29;
    click.y = 48;
}

void complete_only(const fs::path& root) {
    auto job = mask_job(root, 3, root / "unavailable-model.ggml", "must-not-enumerate-devices");
    seed_masks(job, 3);
    for (bool temporal : {false, true}) {
        if (temporal) click_first(job);
        const auto train = mask_hash(job, 0), feature = mask_hash(job, 0, true);
        run_masks(job, temporal ? "complete click resume without a device or model"
                                : "complete image resume without a device or model");
        check(!fs::exists(fs::path(job.workspace) / ".spirula-mask-attempts"), "no mask worker scratch");
        for (int i = 0; i < 3; ++i)
            check(!train.empty() && !feature.empty() &&
                mask_hash(job, i) == train && mask_hash(job, i, true) == feature,
                "complete training and feature masks remain unchanged: " + frame_name(i));
    }
    const std::vector<unsigned char> rgb(photo_width * photo_height * 3, 128);
    const auto alias = fs::path(job.inputs.front().path) / "0.jpg";
    require(stbi_write_jpg(alias.string().c_str(), photo_width, photo_height, 3, rgb.data(), 90) != 0,
        "write a second image with the same mask destination");
    run_masks(job, "retained output has two owners", "Multiple mask images own the same output path");
}

void check_mask_scratch_empty(const gui::PrepJob& job, const char* label) {
    const auto staging = fs::path(job.workspace) / ".spirula-mask-attempts";
    check(!fs::exists(staging) || (fs::is_directory(staging) && fs::is_empty(staging)), label);
}

void mask_resume(const fs::path& root, const fs::path& model, const char* device) {
    require(fs::is_regular_file(model), "pass a cached text-capable SAM checkpoint (for example sam3-q4_0.ggml)");
    require(std::string(device).find(',') == std::string::npos && app::workers_per_gpu() == 1,
        "mask-resume requires one device UUID and SS_WORKERS_PER_GPU=1");
    auto job = mask_job(root / "independent", 6, fs::absolute(model), device);
    auto fresh = job;
    fresh.workspace = (root / "independent/reference").string();
    fresh.resume = false;
    run_masks(fresh, "fresh independent reference");
    seed_masks(job, 6);
    const auto train = mask_hash(job, 0), feature = mask_hash(job, 0, true);
    fs::remove(mask_path(job, 0));
    fs::remove(mask_path(job, 1, true));
    fs::remove(mask_path(job, 2));
    fs::remove(mask_path(job, 2, true));
    fs::resize_file(mask_path(job, 3), 0);
    const auto truncated = mask_path(job, 3, true);
    fs::resize_file(truncated, fs::file_size(truncated) - 4);
    const unsigned char tiny_pixel = 91;
    require(stbi_write_png(mask_path(job, 4).string().c_str(), 1, 1, 1, &tiny_pixel, 1) != 0,
        "write wrong-size PNG");
    {
        // stb writes IHDR then IDAT. Break zlib without disturbing dimensions or IEND.
        std::fstream png(mask_path(job, 4, true), std::ios::binary | std::ios::in | std::ios::out);
        png.seekg(37);
        char chunk[4];
        require(bool(png.read(chunk, 4)) && std::string(chunk, 4) == "IDAT", "locate IDAT fixture payload");
        png.seekp(41);
        png.put('\0');
        png.close();
        require(bool(png), "write undecodable IDAT");
    }
    const auto independent = run_masks(job, "partial independent resume");
    check(independent.done == 6 && independent.total == 6, "count each independent frame once");
    check_mask_scratch_empty(job, "independent resume leaves no staged masks");
    for (int i = 0; i < 6; ++i) {
        for (bool is_feature : {false, true}) {
            const bool retained = i == 5 || i == (is_feature ? 0 : 1);
            const auto expected = retained ? (is_feature ? feature : train) : mask_hash(fresh, i, is_feature);
            check(!expected.empty() && mask_hash(job, i, is_feature) == expected,
                std::string(retained ? "retained mask bytes preserved: " : "missing or invalid mask repaired: ") +
                mask_path(job, i, is_feature).lexically_relative(job.workspace).generic_string());
        }
    }
    seed_masks(job, 6);
    job.resume = true;
    job.redo_masks = true;
    run_masks(job, "redo rewrites valid retained masks");
    for (int i = 0; i < 6; ++i) {
        for (bool is_feature : {false, true}) {
            const auto expected = mask_hash(fresh, i, is_feature);
            check(!expected.empty() && expected != (is_feature ? feature : train) &&
                mask_hash(job, i, is_feature) == expected,
                "redo replaces retained bytes: " +
                mask_path(job, i, is_feature).lexically_relative(job.workspace).generic_string());
        }
    }

    job = mask_job(root / "temporal", 3, fs::absolute(model), device);
    job.mask_prompt.clear();
    click_first(job);
    fresh = job;
    fresh.workspace = (root / "temporal/reference").string();
    fresh.resume = false;
    run_masks(fresh, "fresh whole-sequence click reference");
    seed_masks(job, 3);
    const auto retained_train = mask_hash(job, 0), retained_feature = mask_hash(job, 0, true);
    fs::remove(mask_path(job, 1, true));
    fs::remove(mask_path(job, 2));
    const auto temporal = run_masks(job, "resume a click sequence with partial mask families");
    check(temporal.done == 3 && temporal.total == 3, "count each replayed frame once");
    check_mask_scratch_empty(job, "temporal resume leaves no staged masks");
    for (int i = 0; i < 3; ++i) {
        for (bool is_feature : {false, true}) {
            const bool retained = i != (is_feature ? 1 : 2);
            const auto expected = retained ? (is_feature ? retained_feature : retained_train)
                                           : mask_hash(fresh, i, is_feature);
            check(!expected.empty() && mask_hash(job, i, is_feature) == expected,
                std::string(retained ? "retained temporal mask bytes preserved: "
                                     : "pending temporal mask matches whole-sequence reference: ") +
                mask_path(job, i, is_feature).lexically_relative(job.workspace).generic_string());
        }
    }
}

int mask_test(bool complete, const char* model, const char* device) {
    const auto nonce = (long long)std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("spirula-mask-resume-test-" + std::to_string(nonce));
    if (!fs::create_directory(root)) {
        check(false, "create exclusive mask resume test workspace");
        return 1;
    }
    try {
        if (complete) complete_only(root);
        else mask_resume(root, model, device);
    } catch (const std::exception& e) {
        check(false, e.what());
    }
    fs::remove_all(root);
    return failures == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 4 && std::string(argv[1]) == "sam" && std::string(argv[2]) == "--mask-worker") {
        const int rc = app::mask_worker_main(argv[3]);
        nn::shutdown();
        return rc;
    }
    if (argc == 2 && std::string(argv[1]) == "--complete-only") return mask_test(true, nullptr, nullptr);
    if (argc == 4 && std::string(argv[1]) == "--mask-resume") return mask_test(false, argv[2], argv[3]);
    if (argc != 1) {
        std::fprintf(stderr, "Usage: dataset_prep_streaming_test [--complete-only | --mask-resume <SAM3-model> <uuid:...>]\n");
        return 2;
    }
    if (!gui::command_exists("ffmpeg")) {
        std::puts("SKIP: ffmpeg is unavailable");
        return 0;
    }
    const auto devices = nn::list_devices();
    if (std::none_of(devices.begin(), devices.end(), [](const auto& d) { return d.usable; })) {
        std::puts("SKIP: no usable Vulkan device");
        return 0;
    }
    const fs::path root = fs::temp_directory_path() / "spirula_dataset_prep_streaming_test";
    fs::remove_all(root);
    fs::create_directories(root);
    const fs::path video = root / "frames.mkv";
    const std::atomic<bool> cancel{false};
    std::string output;
    const int rc = gui::run_process(
        {"ffmpeg", "-nostdin", "-hide_banner", "-loglevel", "error", "-f", "lavfi",
         "-i", "testsrc2=size=256x256:rate=30", "-frames:v", "60", "-c:v", "ffv1",
         video.string()},
        "", [&](const std::string& line) { output += line + "\n"; }, cancel);
    check(rc == 0, "create a 60-frame software video: " + output);
    if (rc == 0) {
        extract(video, root / "blocked", true);
        extract(video, root / "complete", false);
        one_packed_lens(video, root / "one_lens");
        parallel_extract(video, root);
        parallel_extract(video, root / "blur", true);
    }
    fs::remove_all(root);
    return failures == 0 ? 0 : 1;
}
