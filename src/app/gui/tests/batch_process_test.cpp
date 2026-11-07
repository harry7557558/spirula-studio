#include "app/gui/BatchProcess.h"
#include "app/gui/TrainPreset.h"
#include "app/AppPaths.h"
#include "core/SourcePath.h"
#include "i18n/catalog/Gui.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>

namespace fs = std::filesystem;

static int failures = 0;

#define SS_CHECK(condition) \
    do { \
        if (!(condition)) { \
            std::printf("FAIL %s:%d %s\n", SS_FILE, __LINE__, #condition); \
            failures++; \
        } \
    } while (0)

static bool close(float a, float b) { return std::abs(a - b) < 1e-6f; }

static void training_overrides(const fs::path& root) {
    gui::TrainPreset preset;
    train_apply_preset(preset.cfg, preset.base);
    preset.cfg.quality = "high";
    preset.cfg.floater_suppression = "strong";
    preset.cfg.sh_reg = 0.25f;
    preset.touched = {"quality", "floater_suppression", "sh_reg"};
    train_resolve_macros(preset.cfg, preset.touched);
    const std::string path = (root / "train.json").string();
    gui::save_preset(preset, path);

    gui::BatchRow row;
    row.runs.emplace_back();
    auto& run = row.runs.front();
    run.preset = {path, "saved"};
    TrainConfig cfg;
    std::string base, error;
    std::set<std::string> touched;
    auto build = [&] {
        return gui::batch_build_train_config(row, 0, root.string(), "external-images",
                                             "external-masks", true, cfg, base, error,
                                             &touched);
    };

    SS_CHECK(build());
    SS_CHECK(cfg.floater_suppression == "strong");
    SS_CHECK(cfg.num_iterations == 50000 && cfg.cap_max == 3000000);
    SS_CHECK(close(cfg.depth_distortion_reg, 0.03f));
    SS_CHECK(close(cfg.sh_reg, 0.25f));
    SS_CHECK(cfg.image_dir == "external-images" && cfg.mask_dir == "external-masks");
    SS_CHECK(cfg.flip_mask && cfg.disable_viewer);
    SS_CHECK(cfg.output_dir_prefix == (root / "outputs").string());

    run.iterations = "12";
    run.cap_max = "12345";
    run.sh_degree = "0";
    run.floater_suppression = "mild";
    row.partition = (root / "partition.json").string();
    row.partition_part = 2;
    SS_CHECK(build());
    SS_CHECK(cfg.num_iterations == 12 && cfg.cap_max == 12345 && cfg.sh_degree == 0);
    SS_CHECK(cfg.partition == row.partition && cfg.partition_part == 2);
    SS_CHECK(cfg.floater_suppression == "mild");
    SS_CHECK(close(cfg.depth_distortion_reg, 0.01f) && close(cfg.sh_reg, 0.25f));
    SS_CHECK(touched.count("cap_max") && touched.count("num_iterations") &&
             touched.count("sh_degree") && touched.count("floater_suppression"));

    run.floater_suppression = "off";
    SS_CHECK(build());
    TrainConfig stock;
    train_apply_preset(stock, base);
    SS_CHECK(close(cfg.depth_distortion_reg, stock.depth_distortion_reg));
    SS_CHECK(close(cfg.rgb_distortion_reg, stock.rgb_distortion_reg));
    SS_CHECK(close(cfg.max_screen_size, stock.max_screen_size));
    SS_CHECK(close(cfg.sh_reg, 0.25f));

    run.floater_suppression = "invalid";
    SS_CHECK(!build());
    SS_CHECK(error.find("floater_suppression") != std::string::npos);
    gui::BatchCapabilities caps;
    caps.device = true;
    bool bad_floater = false;
    for (const auto& issue : gui::batch_check_row(row, {row}, 0, caps))
        bad_floater = bad_floater || (issue.fatal &&
                       issue.raw.find("floater_suppression") != std::string::npos);
    SS_CHECK(bad_floater);
    run.floater_suppression.clear();
    for (const auto& bad : {"0", "1x", "1000000001"}) {
        run.iterations = bad;
        SS_CHECK(!build());
    }
    run.iterations = "12";
    run.sh_degree = "5";
    SS_CHECK(!build());
    run.sh_degree = "0";
    run.preset.path = (root / "missing.json").string();
    SS_CHECK(!build());
}

// Each run of one row trains in its own region; a region since deleted is
// caught before the queue starts rather than when that run loads.
static void region_per_run(const fs::path& root) {
    const fs::path ds = root / "roi-dataset";
    fs::create_directories(ds / "roi");
    std::ofstream(ds / "roi" / "castle.json") << "{}";
    gui::BatchRow row;
    row.dataset = ds.string();
    row.runs.resize(3);
    row.runs[1].roi_region = "roi/castle.json";
    row.runs[2].roi_region = "off";
    const char* want[] = {"", "roi/castle.json", "off"};
    for (int k = 0; k < 3; k++) {
        TrainConfig cfg;
        std::string base, error;
        SS_CHECK(gui::batch_build_train_config(row, k, ds.string(), {}, {}, false, cfg, base,
                                               error));
        SS_CHECK(cfg.roi_region == want[k]);
    }

    gui::BatchCapabilities caps;
    caps.device = true;
    auto duplicate = [&](const std::vector<gui::BatchRow>& all) {
        for (const auto& issue : gui::batch_check_row(all[0], all, 0, caps))
            if (issue.text == &spirula::i18n::msg::gui::chk_dataset_duplicate) return true;
        return false;
    };
    auto missing = [&](const char* name) {
        for (const auto& issue : gui::batch_check_row(row, {row}, 0, caps))
            if (issue.fatal && issue.raw.find(name) != std::string::npos) return true;
        return false;
    };
    gui::BatchRow other = row;
    SS_CHECK(duplicate({row, other}));
    other.runs[1].roi_region = "off";
    SS_CHECK(!duplicate({row, other}));

    SS_CHECK(!missing("castle.json"));
    row.runs[1].roi_region = "roi/gone.json";
    SS_CHECK(missing("gone.json"));
}

static void queue_roundtrip(const fs::path& root) {
    gui::BatchRow row;
    row.dataset = root.string();
    const char* regions[] = {"", "roi/castle.json", "off", "roi/castle.json"};
    int n = 0;
    for (const auto& value : {"", "off", "mild", "strong"}) {
        gui::BatchRun run;
        run.iterations = "123";
        run.cap_max = "456";
        run.sh_degree = "0";
        run.floater_suppression = value;
        run.roi_region = regions[n++];
        row.runs.push_back(run);
    }
    gui::save_batch_list({row});
    const auto loaded = gui::load_batch_list();
    SS_CHECK(loaded.size() == 1);
    if (loaded.size() != 1) return;
    SS_CHECK(loaded[0].runs.size() == row.runs.size());
    if (loaded[0].runs.size() != row.runs.size()) return;
    for (size_t i = 0; i < row.runs.size(); i++) {
        const auto& run = loaded[0].runs[i];
        SS_CHECK(run.floater_suppression == row.runs[i].floater_suppression);
        SS_CHECK(run.roi_region == row.runs[i].roi_region);
        SS_CHECK(run.iterations == "123" && run.cap_max == "456" && run.sh_degree == "0");
    }

    const fs::path file = fs::path(app::config_dir()) / "batch.json";
    std::ofstream(file) << R"({"rows":[{"runs":[{"preset":{"name":"3dgs"},"sh_degree":"0"}]}]})";
    const auto legacy = gui::load_batch_list();
    SS_CHECK(legacy.size() == 1);
    if (legacy.size() == 1 && !legacy[0].runs.empty()) {
        SS_CHECK(legacy[0].runs[0].floater_suppression.empty());
        SS_CHECK(legacy[0].runs[0].sh_degree == "0");
    } else {
        SS_CHECK(false);
    }
}

int main() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
                          ("spirula-batch-test-" + std::to_string(stamp));
    fs::create_directories(root);
#ifdef _WIN32
    _putenv_s("APPDATA", root.string().c_str());
#else
    setenv("XDG_CONFIG_HOME", root.string().c_str(), 1);
#endif
    training_overrides(root);
    region_per_run(root);
    queue_roundtrip(root);
    fs::remove_all(root);
    std::printf("batch_process_test: %d failures\n", failures);
    return failures ? 1 : 0;
}
