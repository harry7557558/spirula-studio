#include "app/gui/DatasetPrep.h"
#include "app/gui/Subprocess.h"
#include "i18n/catalog/Data.h"
#include "nn/Device.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <string>

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

}  // namespace

int main() {
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
         "-i", "testsrc2=size=64x64:rate=30", "-frames:v", "4", "-c:v", "ffv1",
         video.string()},
        "", [&](const std::string& line) { output += line + "\n"; }, cancel);
    check(rc == 0, "create a four-frame software video: " + output);
    if (rc == 0) {
        extract(video, root / "blocked", true);
        extract(video, root / "complete", false);
    }
    fs::remove_all(root);
    return failures == 0 ? 0 : 1;
}
