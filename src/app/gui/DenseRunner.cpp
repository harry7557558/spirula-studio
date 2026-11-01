#include "app/gui/DenseRunner.h"

#include "app/AppPaths.h"
#include "app/gui/Subprocess.h"
#include "core/Sha256.h"
#include "dense/ConfigFields.h"
#include "dense/Artifact.h"
#include "i18n/Locale.h"
#include "i18n/catalog/Dense.h"
#ifdef SS_HAVE_ROMA
#include "nn/Device.h"
#endif

#include <ctime>
#include <filesystem>
#include <fstream>

namespace gui {
namespace fs = std::filesystem;
namespace D = spirula::i18n::msg::dense;

std::string dense_availability() {
#ifndef SS_HAVE_ROMA
    return spirula::i18n::format(D::error, {"SS_BUILD_SAM=OFF"});
#else
    if (app::exe_path().empty()) return spirula::i18n::format(D::error, {"executable path unavailable"});
    return {};
#endif
}

bool dense_completed(const std::string& dataset) {
    return spirula::dense::artifact_complete(dataset);
}

bool run_dense_step(const DenseJob& job, const std::string& dataset, const std::string& images,
                    RunProgress& progress, const std::atomic<bool>& cancel, std::string& error) {
    if (!job.enable) return true;
    if (auto unavailable = dense_availability(); !unavailable.empty()) { error = unavailable; return false; }
    auto config = job.config;
    if (!images.empty()) config.image_dir = images;
#ifdef SS_HAVE_ROMA
    if (config.device.empty()) config.device = nn::configured_device_selector();
#endif
    try { config.validate_run(); }
    catch (const std::exception& e) { error = spirula::i18n::format(D::error, {e.what()}); return false; }
    const fs::path root = fs::path(dataset) / "dense";
    fs::create_directories(root);
    const fs::path settings = root / "job.json";
    std::ofstream file(settings); file << spirula::dense::config_json(config); file.flush();
    if (!file) { error = spirula::i18n::format(D::error, {settings.string()}); return false; }
    file.close();
    progress.enter(Stage::Dense, D::title.get());
    std::vector<std::string> argv{app::exe_path(), "--lang", spirula::i18n::code(spirula::i18n::current()),
                                  "dense", dataset, "--config", settings.string(),
                                  "--progress-dir", spirula::dense::progress_dir(dataset).string()};
    fs::path perf;
    if (job.log_performance) {
        char stamp[32];
        const std::time_t now = std::time(nullptr);
        std::strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", std::localtime(&now));
        perf = fs::absolute(root / "perf" / stamp);
        argv.insert(argv.end(), {"--perf-dir", perf.string()});
    }
    std::string child_error, phase;
    const int code = run_process(argv, "", [&](const std::string& line) {
        std::vector<std::string> fields;
        // Each phase counts its own items, so each gets a bar of its own.
        auto stage_label = [&](const std::string& label) {
            const bool known = label == D::prepare.get() || label == D::match.get() || label == D::refine.get() ||
                               label == D::fuse.get() || label == D::load_model.get() || label == D::outliers.get() ||
                               label == D::export_cloud.get();
            if (known && label != phase) {
                if (!phase.empty()) progress.restart(Stage::Dense);
                phase = label;
            }
            return known;
        };
        if (spirula::i18n::scan(D::error, line, fields)) {
            child_error = line;
            progress.note(line, false);
        } else if (spirula::i18n::scan(D::progress, line, fields) && fields.size() == 3 && stage_label(fields[0])) {
            progress.count(Stage::Dense, std::atoll(fields[1].c_str()), std::atoll(fields[2].c_str()));
            progress.detail(Stage::Dense, line);
        } else if (spirula::i18n::scan(D::progress_percent, line, fields) && fields.size() == 2 && stage_label(fields[0])) {
            progress.count(Stage::Dense, std::atoll(fields[1].c_str()), 100);
            progress.detail(Stage::Dense, line);
        } else if (spirula::i18n::scan(D::count, line, fields) && fields.size() == 2 && stage_label(fields[0])) {
            progress.count(Stage::Dense, std::atoll(fields[1].c_str()), 0);
            progress.detail(Stage::Dense, line);
        } else progress.note(line, !spirula::i18n::scan(D::completed, line, fields));
    }, cancel);
    if (!perf.empty()) progress.note(Stage::Dense, spirula::i18n::format(D::perf_saved, {perf.string()}), false);
    if (code != 0 || !dense_completed(dataset)) {
        error = !child_error.empty() && code != kCancelled ? child_error :
            spirula::i18n::format(D::error, {code == kCancelled ? "cancelled" : "child process did not complete"});
        return false;
    }
    try {
        if (!spirula::dense::artifact_checksum_valid(spirula::dense::artifact_files(dataset))) {
            error = spirula::i18n::format(D::error, {"cloud checksum mismatch"}); return false;
        }
    } catch (const std::exception& e) { error = spirula::i18n::format(D::error, {e.what()}); return false; }
    progress.mark(Stage::Dense, StageStatus::Done);
    return true;
}

}  // namespace gui
