// Meshing job lifecycle and screen layout.

#include "app/gui/GuiApp.h"

#include "app/gui/GuiAppShared.h"
#include "app/gui/ProgressView.h"
#include "app/gui/Ui.h"
#include "checkpoint/SplatPly.h"
#include "data/Json.h"
#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/Gui.h"

#include "imgui.h"

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;
namespace msg = spirula::i18n::msg::gui;
namespace dmsg = spirula::i18n::msg::dataset;

namespace gui {

bool mesh_format_carries(int format, int color) {
    meshing::MeshFormatSpec spec;
    spec.fmt = kMeshFormats[format];
    return meshing::check_export_support(spec, (meshing::MeshColorMode)color)
        .empty();
}

void GuiApp::set_mesh_source(const std::string& path) {
    if (path.empty()) return;
    _mesh_job.checkpoint = path;
    _mesh_job.output = default_mesh_output(path);
    _mesh_job.data_dir.clear();
    close_mesh_preview();
}

bool GuiApp::mesh_dataset_found() {
    if (!_mesh_job.use_data) return false;
    const std::string key = _mesh_job.checkpoint + '\n' + _mesh_job.data_dir;
    if (key == _mesh_data_probe_key) return _mesh_data_probe_found;
    _mesh_data_probe_key = key;
    _mesh_data_probe_found = false;
    std::error_code ec;
    if (!_mesh_job.data_dir.empty()) {
        _mesh_data_probe_found = fs::exists(_mesh_job.data_dir, ec);
    } else if (!_mesh_job.checkpoint.empty()) {
        try {
            auto [ply, run_dir] = spirula::find_splat_ply(_mesh_job.checkpoint);
            (void)ply;
            const fs::path cfg = fs::path(run_dir) / "config.json";
            if (fs::is_regular_file(cfg, ec)) {
                const JsonValue run_cfg = json_parse_file(cfg.string());
                const JsonValue* d = run_cfg.find("data");
                if (d && !d->is_null()) {
                    fs::path cand = d->as_string();
                    if (cand.is_relative()) cand = fs::path(run_dir) / cand;
                    _mesh_data_probe_found = fs::exists(cand, ec);
                }
            }
        } catch (const std::exception&) {
        }
    }
    return _mesh_data_probe_found;
}

void GuiApp::start_meshing() {
    if (_mesh_job.checkpoint.empty() || native_work_busy()) return;
    stop_inference_users();
    close_splat();
#ifdef SS_BACKEND_VULKAN
    if (!freeze_native_device()) return;
#else
    if (!freeze_cuda_device()) return;
#endif
    // Again at dispatch: a mesh preset applied since the freeze replaced the job.
    _mesh_job.device_uuid = _native_device_uuid;
#ifndef SS_BACKEND_VULKAN
    _mesh_job.cuda_device = _cuda_device_index;
#endif
    _mesh.start(_mesh_job);
}

void GuiApp::close_mesh_preview() {
    _mesh_monitor.clear();
    if (!_mesh_preview_open) return;
    _compare.close();
    _mesh_preview_open = false;
}

void GuiApp::open_mesh_preview() {
    const std::string out = _mesh.output_path();
    if (out.empty()) return;
    _mesh_monitor.clear();
    close_native_previews();
    close_splat();
#ifdef SS_BACKEND_VULKAN
    if (!freeze_native_device()) return;
#else
    if (!freeze_cuda_device()) return;
#endif
    // Whatever else had the engine (a file opened from the viewer screen) has
    // to let go before the splats take it.
    if (!training_busy()) {
        detach_session_views();
        _runner.note_engine_taken();
        _compare.add(_mesh_job.checkpoint, &msg::mesh_side_splats);
    }
    _compare.add(out, &msg::mesh_side_mesh);
    _mesh_preview_open = true;
    _viewer_base_mode = ViewMode::View;
    remember(RecentKind::Model, out);
    go_to(Screen::Viewer);
}

void GuiApp::draw_mesh() {
    // The run finished while this screen was up: show what it made, ONCE, on
    // the viewer. Keyed on the run id, so Back here stays here; not during a
    // batch, which moves on to its next task instead.
    if (_mesh.state() == MeshRunner::State::Done && !_batch_active &&
        _mesh.run_id() != _mesh_shown_run) {
        _mesh_shown_run = _mesh.run_id();
        open_mesh_preview();
        if (_screen != Screen::Mesh) return;
    }

    const bool running = _mesh.busy();
    draw_screen_header(&msg::mesh_title, [&] {
        // The last result, for coming back to it after Back.
        if (running || _mesh.state() != MeshRunner::State::Done || _mesh.output_path().empty())
            return;
        ImGui::SameLine(0.0f, px(18.0f));
        if (ui::Button(msg::mesh_view_result)) open_mesh_preview();
        ui::help_on_hover(msg::mesh_view_result_help);
    });

    _mesh.poll();
    // The run's memory sits in a strip above the log, as on the dataset screen.
    const bool memory_strip = running && _mesh.memory().visible();
    const float strip_h = memory_strip ? ImGui::GetTextLineHeightWithSpacing() + px(6.0f) : 0.0f;
    const float log_h = log_height(ImGui::GetContentRegionAvail().y - strip_h);

    ImGui::BeginChild("##meshbody", ImVec2(0, body_height(log_h) - strip_h));
    const bool wide = ImGui::GetContentRegionAvail().x >= px(920);
    const float height = ImGui::GetContentRegionAvail().y;
    const float max_width = std::max(px(320), ImGui::GetContentRegionAvail().x - px(320));
    float width = wide ? std::clamp(px(_mesh_panel_w), px(320), max_width) : 0;
    ImGui::BeginChild("##meshsettings", ImVec2(width, wide ? height : std::max(px(370), height * 0.55f)));
    // A batch owns the options while it runs; what it IS meshing goes here.
    if (_batch_active) {
        draw_batch_progress();
        ImGui::Spacing();
    }
    const float footer = px(kActionButtonH) + ImGui::GetTextLineHeightWithSpacing() * 2;
    ImGui::BeginChild("##meshfields", ImVec2(0, -footer));
    draw_mesh_options();
    ImGui::EndChild();

    ImGui::Spacing();
    if (running) {
        if (ui::Button(msg::mesh_cancel, ImVec2(px(kActionButtonW), px(kActionButtonH)))) {
            if (_batch_active) _batch_stop_after = _batch_stop_now = true;
            _mesh.cancel();
        }
    } else {
        ImGui::BeginDisabled(_mesh_job.checkpoint.empty() || _batch_active ||
                             mesh_job_writes_nothing(_mesh_job));
        if (ui::Button(msg::mesh_start, ImVec2(px(kActionButtonW), px(kActionButtonH)))) start_meshing();
        ImGui::EndDisabled();
        if (mesh_job_writes_nothing(_mesh_job)) ui::TextColoredWrapped(kErr, msg::mesh_no_output_warn);
        else if (!wide && _mesh_job.checkpoint.empty()) {
            ImGui::SameLine();
            ui::TextColored(kDim, msg::mesh_no_model);
        }
        if (_mesh.state() == MeshRunner::State::Failed) {
            ui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), msg::mesh_failed);
            ui::TextDisabledRaw(_mesh.error());
        } else if (_mesh.state() == MeshRunner::State::Cancelled) {
            ui::TextColored(kDim, dmsg::cancelled);
        }
    }
    ImGui::EndChild();
    if (wide && splitter_v("##meshpanelsplit", &width, px(320), max_width, height)) {
        _mesh_panel_w = width / ui_scale();
        _layout_dirty = true;
    }
    ImGui::BeginChild("##meshprogress", ImVec2(0, wide ? height : std::max(px(230), ImGui::GetContentRegionAvail().y)));
    if (running) {
        const std::string stage = _mesh.stage();
        timed_progress_bar(_mesh.clock(), stage.empty() ? msg::mesh_running.get() : stage.c_str());
    }
    _mesh_monitor.draw(_mesh_job.checkpoint, running ? _mesh.progress_dir() : "", running ? &_mesh.memory() : nullptr);
    ImGui::EndChild();
    ImGui::EndChild();
    if (memory_strip) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + px(3.0f));
        const float x0 = ImGui::GetCursorPosX(), avail = ImGui::GetContentRegionAvail().x;
        _mesh.memory().draw(msg::mesh_memory_help, x0, avail);
    }
    draw_log_panel(log_h);
}

}  // namespace gui
