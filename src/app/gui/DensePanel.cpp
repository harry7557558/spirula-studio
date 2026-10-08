#include "app/gui/GuiApp.h"

#include "app/gui/SfmProgress.h"
#include "app/gui/Ui.h"
#include "dense/Artifact.h"
#include "dense/ConfigFields.h"
#include "i18n/catalog/Dense.h"
#include "i18n/catalog/Dataset.h"
#include "core/HostMemory.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace gui {

namespace {

const ImVec4 kOk(0.35f, 0.85f, 0.45f, 1.0f);
const ImVec4 kErr(1.0f, 0.35f, 0.35f, 1.0f);

const spirula::i18n::Msg& quality_label(const spirula::dense::DenseConfig& config) {
    namespace D = spirula::i18n::msg::dense;
    if (config.matches_preset("fast")) return D::quality_fast;
    if (config.matches_preset("balanced")) return D::quality_balanced;
    if (config.matches_preset("high")) return D::quality_high;
    return D::quality_custom;
}

// The preset a hand edit leaves behind: the one it now matches, otherwise custom.
void follow_preset(spirula::dense::DenseConfig& config) {
    config.preset = "custom";
    for (const char* name : spirula::dense::kDensePresets)
        if (config.matches_preset(name)) config.preset = name;
}

}  // namespace

bool GuiApp::dense_model_missing() const {
    return _dense.enable && _dense.config.checkpoint == "romav2.0.1" && !model_is_cached(dense_model_entry());
}

void GuiApp::request_dense_download() {
    if (std::find(_accepted_licenses.begin(), _accepted_licenses.end(), "roma") == _accepted_licenses.end()) {
        _license_prompt = "roma"; _license_model_id = "romav2.0.1"; _license_detector_id.clear(); _license_tick = false;
    } else _dense_download.start(dense_model_entry());
}

// While the dense step runs, and once after it for the fused cloud's final snapshot.
void GuiApp::poll_dense_progress() {
    const bool running = dataset_busy() && dataset_steps()->current() == Stage::Dense;
    if (running) _dense_live = true;
    if ((!running && !_dense_live) || _workspace.empty()) return;
    if (!_show_preview) {
        if (_dense_model_read.valid() && _dense_model_read.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
            _dense_model_read.get();
        return;
    }
    const double now = ImGui::GetTime();
    if (_dense_model_read.valid()) {
        if (_dense_model_read.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
        auto read = _dense_model_read.get();
        const auto upload_start = std::chrono::steady_clock::now();
        _dense_preview_error = std::move(read.error);
        if (read.updated) {
            try {
                float up[3] = {0, 0, 1};
                if (!read.model.ds.gauge_oriented && snapshot_up(read.model, up)) _model_view.set_nav_up(up);
                _live_model = std::move(read.model);
                _model_view.attach_preview_data(_live_model.ds, _live_model.post, "dense-live", 1.0f, true);
                _model_attached = true;
                _dense_model_mtime = read.stamp;
            } catch (const std::exception& e) {
                _dense_preview_error = e.what();
                _model_view.detach(); _model_view.destroy_gl();
                _live_model = LiveModel{}; _model_attached = false;
            }
        }
        // Loading and upload together consume at most about 5% of refresh time.
        const double upload_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - upload_start).count();
        _dense_refresh_seconds = std::max(0.5, (read.seconds + upload_seconds) * 20.0);
        _dense_polled_at = ImGui::GetTime();
        if (!running && !read.during_run && _dense_preview_error.empty()) _dense_live = false;
        return;
    }
    if (!_dense_preview_error.empty()) return;
    if (_dense_polled_at > 0.0 && now - _dense_polled_at < _dense_refresh_seconds) return;
    _dense_polled_at = now;
    const auto dir = spirula::dense::progress_dir(_workspace).string();
    const auto stamp = _dense_model_mtime;
    _dense_model_read = std::async(std::launch::async, [dir, stamp, running] {
        const auto start = std::chrono::steady_clock::now();
        DenseLiveRead read; read.stamp = stamp; read.during_run = running;
        try {
            const uint64_t available = spirula::availableRamBytes();
            read.updated = read_live_model(dir, read.stamp, read.model, available - available / 5);
        } catch (const std::exception& e) { read.error = e.what(); }
        read.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        return read;
    });
}

void GuiApp::draw_dense_options() {
    namespace D = spirula::i18n::msg::dense;
    namespace dmsg = spirula::i18n::msg::dataset;
    const auto unavailable = dense_availability();
    if (!unavailable.empty()) {
        ui::TextDisabledWrapped(D::unavailable);
        ui::help_on_hover_raw(unavailable.c_str());
        return;
    }
    ui::Checkbox(D::enable, &_dense.enable);
    ui::help_on_hover_disabled(D::enable_help);
    if (!_dense.enable) return;

    ImGui::Indent();
    auto& config = _dense.config;
    ImGui::SetNextItemWidth(px(260.0f));
    if (ui::BeginCombo(D::preset, quality_label(config).get())) {
        const std::pair<const char*, const spirula::i18n::Msg*> choices[] = {
            {"fast", &D::quality_fast}, {"balanced", &D::quality_balanced}, {"high", &D::quality_high}};
        for (const auto& [name, label] : choices)
            if (ui::Selectable(*label, config.matches_preset(name))) {
                config.apply_preset(name);
                _dense_config_error.clear();
            }
        ImGui::EndCombo();
    }
    ui::help_on_hover_disabled(D::preset_help);

    // The model, the way the geometry step shows its own.
    if (_dense_download.state() == FileDownload::State::Running) {
        ui::ProgressBarRaw(std::max(0.f, _dense_download.progress()), ImVec2(px(260.0f), 0),
                           _dense_download.status().c_str());
        ImGui::SameLine();
        ImGui::PushID("densedl");
        if (ui::Button(dmsg::stop)) _dense_download.cancel();
        ImGui::PopID();
        ui::help_on_hover_disabled(D::download_cancel_help);
    } else if (dense_model_missing()) {
        if (ui::Button(D::get_model)) request_dense_download();
        ui::help_on_hover_disabled(D::download_help);
        ImGui::SameLine();
        ui::TextDisabledRaw(human_bytes(dense_model_entry().bytes));
        if (_dense_download.state() == FileDownload::State::Failed)
            ui::TextColoredWrappedRaw(kErr, _dense_download.status());
    } else if (config.checkpoint == dense_model_entry().id) {
        ui::TextColored(kOk, D::model_ready);
    }
    if (!_workspace.empty() && dense_completed(_workspace) && !dataset_busy()) {
        if (ui::Button(D::preview)) request_open_splat(spirula::dense::artifact_files(_workspace).cloud.string());
        ui::help_on_hover_disabled(D::preview_help);
    }

    const bool advanced = ui::CollapsingHeader(D::advanced);
    ui::help_on_hover_disabled(D::advanced_help);
    if (advanced) draw_dense_advanced();
    ImGui::Unindent();
}

void GuiApp::draw_dense_advanced() {
    namespace D = spirula::i18n::msg::dense;
    auto& config = _dense.config;
    const float number = px(120.0f), pair = px(150.0f), choice = px(240.0f);
    bool preset_edit = false, edited = false;
    // A matching grid snaps to the multiple the matcher needs once its field is left.
    auto snap = [](int value, int step, int lowest) { return std::max(lowest, (value + step / 2) / step * step); };

    int low[2] = {config.match.low_width, config.match.low_height};
    ImGui::SetNextItemWidth(pair);
    if (ui::InputInt2(D::low_resolution, low)) {
        config.match.low_width = low[0]; config.match.low_height = low[1]; preset_edit = true;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        config.match.low_width = snap(config.match.low_width, 16, 16);
        config.match.low_height = snap(config.match.low_height, 16, 16);
        preset_edit = true;
    }
    ui::help_on_hover_disabled(D::low_resolution_help);
    int high[2] = {config.match.high_width, config.match.high_height};
    ImGui::SetNextItemWidth(pair);
    if (ui::InputInt2(D::high_resolution, high)) {
        config.match.high_width = high[0]; config.match.high_height = high[1]; preset_edit = true;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        if (config.match.high_width <= 0 || config.match.high_height <= 0) config.match.high_width = config.match.high_height = 0;
        else {
            config.match.high_width = snap(config.match.high_width, 4, 4);
            config.match.high_height = snap(config.match.high_height, 4, 4);
        }
        preset_edit = true;
    }
    ui::help_on_hover_disabled(D::high_resolution_help);
    preset_edit |= ui::Checkbox(D::bidirectional, &config.match.bidirectional);
    if (config.effective_cycle_check() || !config.cycle_check) ui::help_on_hover_disabled(D::bidirectional_help);
    else {
        const std::string help = std::string(D::bidirectional_help.get()) + "\n\n" + D::cycle_dependency.get();
        ui::help_on_hover_raw(help.c_str(), ImGuiHoveredFlags_AllowWhenDisabled);
    }
    int precision = (int)config.match.precision;
    ImGui::SetNextItemWidth(choice);
    if (ui::Combo(D::precision, &precision, {&D::precision_auto, &D::precision_float32, &D::precision_mixed})) {
        config.match.precision = (spirula::roma::InferencePrecision)precision; edited = true;
    }
    ui::help_on_hover_disabled(D::precision_help);

    ImGui::Spacing();
    ImGui::SetNextItemWidth(number);
    if (ui::InputInt(D::neighbors, &config.pairs.neighbors)) {
        config.pairs.neighbors = std::clamp(config.pairs.neighbors, 1, 4096); preset_edit = true;
    }
    ui::help_on_hover_disabled(D::neighbors_help);
    ImGui::SetNextItemWidth(number);
    if (ui::InputInt(D::reference_coverage, &config.pairs.reference_coverage)) {
        config.pairs.reference_coverage = std::clamp(config.pairs.reference_coverage, 0, 255); preset_edit = true;
    }
    ui::help_on_hover_disabled(D::reference_coverage_help);
    ImGui::SetNextItemWidth(number);
    if (ui::InputInt(D::support, &config.geometry.min_source_images)) {
        config.geometry.min_source_images = std::clamp(config.geometry.min_source_images, 2, 64); edited = true;
    }
    ui::help_on_hover_disabled(D::support_help);
    ImGui::SetNextItemWidth(number);
    if (ui::InputInt(D::stride, &config.stride)) { config.stride = std::clamp(config.stride, 1, 64); preset_edit = true; }
    ui::help_on_hover_disabled(D::stride_help);

    ImGui::Spacing();
    auto bounded = [&](const spirula::i18n::Msg& label, const spirula::i18n::Msg& help, double& value,
                       double lowest, double highest, const char* format) {
        ImGui::SetNextItemWidth(number);
        if (ui::InputDouble(label, &value, format)) {
            value = std::isfinite(value) ? std::clamp(value, lowest, highest) : lowest;
            edited = true;
        }
        ui::help_on_hover_disabled(help);
    };
    bounded(D::min_confidence, D::min_confidence_help, config.min_overlap, 0.0, 1.0, "%.2f");
    bounded(D::max_reprojection, D::max_reprojection_help, config.geometry.max_reprojection_error, 0.01, 100.0, "%.2f");
    bounded(D::min_angle, D::min_angle_help, config.geometry.min_angle_degrees, 0.0, 45.0, "%.2f");
    edited |= ui::Checkbox(D::remove_outliers, &config.remove_outliers);
    ui::help_on_hover_disabled(D::remove_outliers_help);

    ImGui::Spacing();
    edited |= ui::Checkbox(D::split_views, &config.split_views);
    ui::help_on_hover_disabled(D::split_views_help);
    edited |= ui::Checkbox(D::sparse_face_pairs, &config.sparse_face_pairs);
    ui::help_on_hover_disabled(D::sparse_face_pairs_help);
    edited |= ui::Checkbox(D::use_masks, &config.use_masks);
    ui::help_on_hover_disabled(D::use_masks_help);
    int matching_space = config.matching_space == "source" ? 1 : 0;
    ImGui::SetNextItemWidth(choice);
    if (ui::Combo(D::matching_space, &matching_space, {&D::rectified_space, &D::source_space})) {
        config.matching_space = matching_space ? "source" : "rectified"; edited = true;
    }
    ui::help_on_hover_disabled(D::matching_space_help);
    if (config.matching_space == "source") {
        ImGui::Indent();
        bounded(D::reference_fraction, D::reference_fraction_help, config.reference_fraction, 0.01, 1.0, "%.2f");
        auto count = [&](const spirula::i18n::Msg& label, const spirula::i18n::Msg& help, uint64_t& setting) {
            double value = (double)setting;
            ImGui::SetNextItemWidth(number);
            if (ui::InputDouble(label, &value, "%.0f")) {
                setting = std::isfinite(value) && value > 0 ? (uint64_t)std::min(value, 1e15) : 0;
                edited = true;
            }
            ui::help_on_hover_disabled(help);
        };
        count(D::samples_per_reference, D::samples_per_reference_help, config.samples_per_reference);
        count(D::sampling_seed, D::sampling_seed_help, config.sampling_seed);
        bounded(D::source_reprojection, D::source_reprojection_help, config.source_reprojection_error, 0.01, 100.0, "%.2f");
        if (ui::Button(D::source_workflow)) { config.apply_source_workflow(); edited = true; }
        ui::help_on_hover_disabled(D::source_workflow_help);
        ImGui::Unindent();
    }

    ImGui::Spacing();
    // Shown in GB, stored in bytes; whole MiB keep a typed value from drifting.
    float cache_gb = (float)((double)config.image_cache_bytes / (1024.0 * 1024.0 * 1024.0));
    ImGui::SetNextItemWidth(number);
    if (ui::InputFloat(D::image_cache, &cache_gb, 0.0f, 0.0f, "%.1f")) {
        const double mib = std::round(std::clamp((double)cache_gb, 0.0, 65536.0) * 1024.0);
        config.image_cache_bytes = (uint64_t)mib << 20; edited = true;
    }
    ui::help_on_hover_disabled(D::image_cache_help);
    ui::Text(D::checkpoint);
    ui::help_on_hover_disabled(D::checkpoint_help);
    ImGui::SetNextItemWidth(-FLT_MIN);
    edited |= ui::InputTextRaw("##dense-checkpoint", &config.checkpoint);
    ui::help_on_hover_disabled(D::checkpoint_help);
    ui::Checkbox(D::use_training, &_dense.use_for_training);
    ui::help_on_hover_disabled(D::use_training_help);
    if (_dense.use_for_training) {
        ImGui::Indent();
        draw_train_mask_mode(px(220.0f));
        ImGui::Unindent();
    }
    ui::Checkbox(D::log_performance, &_dense.log_performance);
    ui::help_on_hover_disabled(D::log_performance_help);

    if (preset_edit) follow_preset(config);
    if (preset_edit || edited) {
        try { config.validate(); _dense_config_error.clear(); }
        catch (const std::exception& e) { _dense_config_error = spirula::i18n::format(D::error, {e.what()}); }
    }
    if (!_dense_config_error.empty()) ui::TextColoredWrappedRaw(kErr, _dense_config_error);
}

}  // namespace gui
