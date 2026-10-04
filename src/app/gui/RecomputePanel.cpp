// RecomputePanel.cpp -- see RecomputePanel.h.

#include "app/gui/RecomputePanel.h"

#include "app/AppPaths.h"
#include "app/gui/Layout.h"
#include "app/gui/RecomputeFiles.h"
#include "app/gui/SfmRunner.h"
#include "app/gui/Subprocess.h"
#include "app/gui/Ui.h"
#include "data/DatasetParser.h"
#include "i18n/Locale.h"
#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/Log.h"
#include "i18n/catalog/Recompute.h"

#include "imgui.h"

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;
namespace dmsg = spirula::i18n::msg::dataset;
namespace lmsg = spirula::i18n::msg::log;
namespace rcmsg = spirula::i18n::msg::recompute;
using spirula::i18n::format;

namespace gui {

namespace {

const ImVec4 kDim(0.6f, 0.6f, 0.6f, 1.0f);

// The run's workspace inside the dataset, removed when it ends.
constexpr const char* kWorkDir = "densification";

fs::path in_dataset(const std::string& dataset, const std::string& dir) {
    const fs::path p(dir);
    return p.is_absolute() ? p : fs::path(dataset) / p;
}

// As the log and the explanation name it: under the dataset when it is there.
std::string shown_path(const std::string& dataset, const fs::path& p) {
    std::error_code ec;
    fs::path rel = fs::relative(p, dataset, ec);
    if (ec || rel.empty() || *rel.begin() == "..") rel = p;
    rel.make_preferred();
    return rel.string();
}

bool has_files(const fs::path& dir) {
    std::error_code ec;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
        if (it->is_regular_file(ec) || it->is_directory(ec)) return true;
    return false;
}

// sfm::Stage as the bar names it; the --poses run has no mapping of its own.
const spirula::i18n::Msg& stage_label(uint32_t stage) {
    switch (stage) {
        case 0: return lmsg::stage_finding_features;
        case 1: return lmsg::stage_matching_images;
        case 5: return rcmsg::stage_writing;
        case 6: return lmsg::stage_reading_features;
        case 7: return lmsg::stage_selecting_pairs;
        default: return rcmsg::stage_points;
    }
}

}  // namespace

RecomputePanel::~RecomputePanel() {
    _cancel = true;
    if (_worker.joinable()) _worker.join();
}

void RecomputePanel::cancel() { _cancel = true; }

void RecomputePanel::log(const std::string& s, bool detail) {
    std::lock_guard<std::mutex> lk(_mu);
    _log.emplace_back(s, detail);
}

std::vector<std::pair<std::string, bool>> RecomputePanel::drain_log() {
    std::lock_guard<std::mutex> lk(_mu);
    return std::exchange(_log, {});
}

bool RecomputePanel::take_changed() { return _changed.exchange(false); }

void RecomputePanel::draw(const Source& src, bool busy,
                          const std::function<bool(std::string&)>& start) {
    if (src.dataset.empty()) return;
    const bool running = _running.load();
    // Asked again on opening, so a model exported since is the one kept.
    auto probe = [&] {
        _model_for = src.dataset;
        _model = find_colmap_poses(src.dataset);
        std::error_code ec;
        if (!_model.empty() && !(fs::exists(fs::path(_model) / "cameras.bin", ec) &&
                                 fs::exists(fs::path(_model) / "images.bin", ec)))
            _model.clear();
        _restorable = !_model.empty() && holds_recompute(_model);
    };
    if (_model_for != src.dataset) {
        _open = false;
        probe();
    }

    ImGui::BeginDisabled(busy || _model.empty() || running);
    if (ui::Button(rcmsg::button)) {
        _open = !_open;
        if (_open) probe();
    }
    ImGui::EndDisabled();
    if (_model.empty()) ui::help_on_hover_disabled(rcmsg::no_model);
    else ui::help_on_hover_disabled(rcmsg::button_help);
    if ((!_open && !running) || _model.empty()) return;

    const fs::path out = fs::path(src.dataset) / kWorkDir;
    ImGui::Indent(px(12.0f));
    ui::TextColoredWrapped(kDim, rcmsg::explain, {shown_path(src.dataset, _model)});

    ImGui::BeginDisabled(running || busy);
    ImGui::SetNextItemWidth(px(200.0f));
    ui::Combo(dmsg::quality, &_quality,
              {&dmsg::quality_fast, &dmsg::quality_balanced, &dmsg::quality_high_recommended,
               &dmsg::quality_maximum});
    ui::help_on_hover(dmsg::quality_help_builtin);
    ImGui::SetNextItemWidth(px(200.0f));
    ui::Combo(dmsg::features, &_features,
              {&dmsg::features_sift, &dmsg::features_aliked_n16, &dmsg::features_aliked_n32,
               &dmsg::features_loma_b128, &dmsg::features_loma_b});
    ui::help_on_hover(dmsg::features_help);
    ImGui::SetNextItemWidth(px(200.0f));
    ui::InputInt(dmsg::max_features_auto, &_max_features);
    ui::help_on_hover(dmsg::max_features_auto_help);
    ImGui::SetNextItemWidth(px(200.0f));
    ui::InputInt(dmsg::max_image_size_auto, &_max_image_size);
    ui::help_on_hover(dmsg::max_image_size_auto_help);
    _max_features = std::max(0, _max_features);
    _max_image_size = std::max(0, _max_image_size);
    const fs::path masks = in_dataset(src.dataset, src.mask_dir);
    const bool have_masks = !src.mask_dir.empty() && has_files(masks);
    if (have_masks) ui::Checkbox(rcmsg::use_masks, &_use_masks);
    ImGui::EndDisabled();

    if (running) {
        {
            RunStatus st;
            if (read_status(_progress_dir, _status_mtime, st)) _status = st;
        }
        const float f = _status.fraction();
        const float cancel_w = ImGui::CalcTextSize(dmsg::cancel.get()).x +
                               2 * ImGui::GetStyle().FramePadding.x +
                               ImGui::GetStyle().ItemSpacing.x;
        ui::ProgressBar(std::clamp(f, 0.0f, 1.0f), ImVec2(-cancel_w - px(8.0f), 0),
                        stage_label(_status.stage));
        ImGui::SameLine();
        if (ui::Button(dmsg::cancel)) _cancel = true;
    } else {
        ImGui::BeginDisabled(busy);
        std::string device;
        if (ui::Button(rcmsg::run) && start(device)) {
            const std::string image_dir = in_dataset(src.dataset, src.image_dir).string();
            std::vector<std::string> argv = {
                // This executable, in this language (see SfmRunner::run).
                app::exe_path(), "--lang", spirula::i18n::code(spirula::i18n::current()),
                "sfm", "auto", image_dir, "-o", out.string(),
                "--poses", _model,
                "--progress-dir", (out / ".progress").string(),
                "--quality", sfm_pick(kSfmQuality, _quality, 2),
                "--features", sfm_pick(kSfmFeatures, _features),
                "--matcher", sfm_matcher_for(_features, 1),
            };
            if (_max_features > 0) {
                argv.push_back(_features == 0   ? "--max-features"
                               : _features >= 3 ? "--loma-max-features"
                                                : "--aliked-max-features");
                argv.push_back(std::to_string(_max_features));
            }
            if (_max_image_size > 0) {
                argv.push_back("--max-image-size");
                argv.push_back(std::to_string(_max_image_size));
            }
            if (have_masks && _use_masks) {
                argv.push_back("--masks");
                argv.push_back(masks.string());
                if (src.mask_flipped) argv.push_back("--flip-mask");
            }
            if (!device.empty()) {
                argv.push_back("--device");
                argv.push_back(device);
            }
            if (_worker.joinable()) _worker.join();
            _progress_dir = (out / ".progress").string();
            _status = RunStatus{};
            _status_mtime = 0;
            _cancel = false;
            _running = true;
            _worker = std::thread(&RecomputePanel::run, this, std::move(argv), out.string(),
                                  _model);
        }
        if (_restorable) {
            ImGui::SameLine();
            if (ui::Button(rcmsg::restore)) {
                const std::string err = restore_original(_model);
                fs::path shown(_model);
                shown.make_preferred();
                log(err.empty() ? format(rcmsg::restored, {shown.string()})
                                : format(rcmsg::install_failed, {shown.string(), err}));
                _restorable = false;
                _changed = true;
            }
            ui::help_on_hover(rcmsg::restore_help);
        }
        ImGui::EndDisabled();
    }
    ImGui::Unindent(px(12.0f));
}

void RecomputePanel::run(std::vector<std::string> argv, std::string out_dir,
                         std::string model) {
    std::string cmd;
    for (const auto& a : argv) cmd += (cmd.empty() ? "$ " : " ") + a;
    log(cmd);
    const int rc = run_process(argv, "", [this](const std::string& l) {
        log(l, !sfm_child_line_is_notable(l));
    }, _cancel);

    const fs::path made = fs::path(out_dir) / "sparse" / "0";
    fs::path shown(model);
    shown.make_preferred();
    std::error_code ec;
    // 3 and 4 are `sfm auto`'s "partial" and "not metric": a model all the same.
    const bool ok = (rc == 0 || rc == 3 || rc == 4) && !_cancel.load() &&
                    fs::exists(made / "points3D.bin", ec);
    if (rc == kSpawnFailed) {
        log(format(rcmsg::spawn_failed, {argv[0]}));
    } else if (rc == kCancelled || _cancel.load()) {
        log(rcmsg::cancelled.get());
    } else if (!ok) {
        log(format(rcmsg::failed, {(long long)rc}));
    } else if (const std::string err = install_recomputed(model, made); !err.empty()) {
        log(format(rcmsg::install_failed, {shown.string(), err}));
    } else {
        RunStatus st;
        int64_t mtime = 0;
        read_status((fs::path(out_dir) / ".progress").string(), mtime, st);
        log(format(rcmsg::done, {(long long)st.points, (long long)st.registered, shown.string()}));
        _restorable = true;
        _changed = true;
    }
    remove_dir_tree(out_dir);
    _running = false;
}

}  // namespace gui
