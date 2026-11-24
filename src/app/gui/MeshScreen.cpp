// The Mesh screen: what to mesh, the four choices that matter (color, formats,
// how many photos, where it goes), an Advanced header for the rest, and -- once
// it has run -- the splats and the surface side by side on one linked camera.

#include "app/gui/GuiApp.h"

#include "app/gui/GuiAppShared.h"
#include "app/gui/ProgressView.h"
#include "app/gui/Ui.h"
#include "checkpoint/SplatPly.h"
#include "data/Json.h"
#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/Edit.h"
#include "i18n/catalog/Gui.h"

#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;
namespace msg = spirula::i18n::msg::gui;
namespace emsg = spirula::i18n::msg::edit;
namespace dmsg = spirula::i18n::msg::dataset;
using spirula::i18n::Msg;

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
    // The output name follows the source until the user edits it. A file
    // becomes <name>_mesh, NEVER <name> -- meshing `splat.ply` to base
    // `splat` writes `splat.ply`, i.e. over the model being meshed.
    std::error_code ec;
    fs::path base(path);
    if (fs::is_regular_file(base, ec)) {
        const std::string stem = base.stem().string();
        base.replace_filename(stem + "_mesh");
    } else {
        base /= "mesh";
    }
    _mesh_job.output = base.string();
    // A run folder usually records its dataset; leave the field empty and let
    // the child read config.json, which is what the help text promises.
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
    close_mesh_preview();
    set_ground_model(_mesh_job);
    _mesh.start(_mesh_job);
}

void GuiApp::set_ground_model(MeshJob& job) const {
    job.ground_model.clear();
    job.ground_detector.clear();
#ifdef SS_BUILD_SAM
    if (job.ground_prompt.empty()) return;
    const MaskModelFiles m = selected_mask_model();
    job.ground_model = m.model;
    job.ground_detector = m.detector;
#endif
}

void GuiApp::close_mesh_preview() {
    if (!_mesh_preview_open) return;
    _compare.close();
    _mesh_preview_open = false;
}

void GuiApp::open_mesh_preview() {
    const std::string out = _mesh.output_path();
    if (out.empty()) return;
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
    remember(RecentKind::Model, out);
}

void GuiApp::draw_mesh_options() {
    ui::SeparatorText(msg::section_preset);
    draw_mesh_preset_picker();
    ImGui::Spacing();

    // A path row is [field][...][label]. The field takes what is left after
    // the button and the label, measured rather than guessed -- a fixed
    // reserve pushes the label off the edge in the longer languages.
    const float style_x = ImGui::GetStyle().ItemSpacing.x;
    auto field_width = [&](const Msg& label) {
        return -(60.0f + 2.0f * style_x + ImGui::CalcTextSize(label.get()).x);
    };

    // ---- what to mesh ----
    ui::SeparatorText(msg::mesh_source);
    ImGui::SetNextItemWidth(px(-140.0f));
    if (ui::InputTextRaw("##meshsrc", &_mesh_job.checkpoint)) {
        // Typed by hand: keep the derived output in step until it is edited.
    }
    ImGui::SameLine();
    if (ui::ButtonRaw("...##meshsrcdir", ImVec2(px(kBrowseButtonW), 0))) {
        open_pick(PickAction::MeshSource, msg::mesh_pick_model.get(),
                  FileDialog::Mode::Folder);
    }
    ImGui::SameLine();
    if (ui::ButtonRaw(".ply##meshsrcfile", ImVec2(px(kBrowseButtonW), 0))) {
        open_pick(PickAction::MeshSource, msg::mesh_pick_model.get(),
                  FileDialog::Mode::File, {".ply"});
    }
    ui::help_on_hover(msg::mesh_source_help);
    ui::TextDisabled(msg::mesh_drop_hint);

    // ---- the photos ----
    ui::Checkbox(msg::mesh_use_photos, &_mesh_job.use_data);
    ui::help_on_hover(msg::mesh_use_photos_help);
    // Meshing without cameras is a real choice, and a much worse mesh, so it
    // is said out loud on the screen rather than left to the child's warning
    // in the log panel -- which scrolls past, and only after the run starts.
    if (!_mesh_job.use_data)
        ui::TextColoredWrapped(kWarn, msg::mesh_no_photos_warn);
    if (_mesh_job.use_data) {
        ImGui::Indent();
        ImGui::SetNextItemWidth(field_width(msg::mesh_photos_dir));
        ui::InputTextWithHintRaw("##meshdata", msg::mesh_photos_dir_help,
                                 &_mesh_job.data_dir);
        ImGui::SameLine();
        if (ui::ButtonRaw("...##meshdatapick", ImVec2(px(kBrowseButtonW), 0))) {
            open_pick(PickAction::MeshPhotos, msg::mesh_pick_photos.get(),
                      FileDialog::Mode::Folder);
        }
        ImGui::SameLine();
        ui::TextDisabled(msg::mesh_photos_dir);
        // Ticked, but nothing will be found (a loose splat.ply, a moved
        // dataset, a typo): same outcome as unticking, so same warning. Silent
        // until a model is picked; the Create button says what is missing.
        if (!_mesh_job.checkpoint.empty() && !mesh_dataset_found())
            ui::TextColoredWrapped(kWarn, msg::mesh_photos_missing_warn);

        ImGui::SetNextItemWidth(px(kCompactFieldW));
        ui::InputInt(msg::mesh_max_cameras, &_mesh_job.max_cameras);
        _mesh_job.max_cameras = std::max(0, _mesh_job.max_cameras);
        ui::help_on_hover(msg::mesh_max_cameras_help);
        int method = _mesh_job.tsdf ? 1 : 0;
        ImGui::SetNextItemWidth(px(kFieldW));
        if (ui::Combo(msg::mesh_method, &method, {&msg::mesh_method_opacity, &msg::mesh_method_tsdf}))
            _mesh_job.tsdf = method == 1;
        ui::help_on_hover(msg::mesh_method_help);
        ImGui::SetNextItemWidth(field_width(msg::mesh_ground_masks));
        ui::InputTextRaw("##meshground", &_mesh_job.ground_masks);
        ImGui::SameLine();
        if (ui::ButtonRaw("...##meshgroundpick", ImVec2(px(kBrowseButtonW), 0))) {
            open_pick(PickAction::MeshGround, msg::mesh_pick_ground_masks.get(),
                      FileDialog::Mode::Folder);
        }
        ImGui::SameLine();
        ui::TextDisabled(msg::mesh_ground_masks);
        ui::help_on_hover(msg::mesh_ground_masks_help);
#ifdef SS_BUILD_SAM
        ImGui::SetNextItemWidth(field_width(msg::mesh_ground_prompt));
        ui::InputTextRaw("##meshgroundprompt", &_mesh_job.ground_prompt);
        ImGui::SameLine();
        ui::TextDisabled(msg::mesh_ground_prompt);
        ui::help_on_hover(msg::mesh_ground_prompt_help);
        if (!_mesh_job.ground_prompt.empty() && selected_mask_model().empty())
            ui::TextColored(kDim, msg::mesh_ground_model_missing);
#endif
        ImGui::Unindent();
    }

    // ---- color ----
    // A set rather than a choice: one extraction, written in each of them, so
    // a textured GLB and a plain PLY cost one run between them, not two.
    ui::SeparatorText(msg::mesh_color);
    const Msg* color_names[kNumMeshColorModes] = {
        &msg::mesh_color_none, &msg::mesh_color_vertex, &msg::mesh_color_texture};
    for (int i = 0; i < kNumMeshColorModes; i++) {
        if (i) ImGui::SameLine();
        ImGui::PushID(i);
        ui::Checkbox(*color_names[i], &_mesh_job.colors[i]);
        ImGui::PopID();
    }
    ui::help_on_hover(msg::mesh_color_help);
    if (_mesh_job.wants_color(2)) {
        ImGui::Indent();
        static const int kTexSizes[] = {0, 1024, 2048, 4096, 8192};
        int idx = 0;
        for (int i = 0; i < 5; i++)
            if (kTexSizes[i] == _mesh_job.texture_size) idx = i;
        // Mixed list: one translated entry and four resolutions, which are
        // numbers in every language -- hence ComboRaw with a separate label.
        char items[5][64];
        const char* ptrs[5];
        for (int i = 0; i < 5; i++) {
            if (i == 0) std::snprintf(items[i], sizeof items[i], "%s",
                                      msg::mesh_texture_size_auto.get());
            else std::snprintf(items[i], sizeof items[i], "%d", kTexSizes[i]);
            ptrs[i] = items[i];
        }
        ImGui::SetNextItemWidth(px(160.0f));
        if (ui::ComboRaw("##texsize", &idx, ptrs, 5))
            _mesh_job.texture_size = kTexSizes[idx];
        ImGui::SameLine();
        ui::TextDisabled(msg::mesh_texture_size);
        ImGui::Unindent();
    }

    // ---- formats ----
    // The child refuses a format that cannot carry the color asked of it, so
    // one no ticked color travels in is greyed out rather than failing the run.
    ui::SeparatorText(msg::mesh_formats);
    for (int i = 0; i < kNumMeshFormats; i++) {
        if (i) ImGui::SameLine();
        bool ok = false;
        for (int c = 0; c < kNumMeshColorModes; c++)
            ok = ok || (_mesh_job.colors[c] && mesh_format_carries(i, c));
        if (!ok) _mesh_job.formats[i] = false;
        ImGui::BeginDisabled(!ok);
        // A file extension is a file extension in every language.
        ImGui::PushID(i);
        ui::CheckboxRaw(kMeshFormats[i], &_mesh_job.formats[i]);
        ImGui::PopID();
        ImGui::EndDisabled();
    }
    // Nothing left that a requested color and a requested format can both
    // carry. Said here rather than fixed silently: which of the two to give up
    // is the user's choice, and the Create button stays disabled until it is.
    if (mesh_job_writes_nothing(_mesh_job))
        ui::TextColoredWrapped(kErr, msg::mesh_no_output_warn);
    ImGui::SetNextItemWidth(field_width(msg::mesh_output));
    ui::InputTextRaw("##meshout", &_mesh_job.output);
    ImGui::SameLine();
    if (ui::ButtonRaw("...##meshoutpick", ImVec2(px(kBrowseButtonW), 0))) {
        open_pick(PickAction::MeshOutput, msg::mesh_pick_output.get(),
                  FileDialog::Mode::Folder);
    }
    ImGui::SameLine();
    ui::TextDisabled(msg::mesh_output);
    ui::help_on_hover(msg::mesh_output_help);

    // ---- advanced ----
    if (ui::CollapsingHeader(msg::mesh_advanced)) {
        ImGui::SetNextItemWidth(px(kFieldW));
        ui::SliderFloat(msg::mesh_detail, &_mesh_job.merge_factor, 0.25f, 4.0f,
                        "%.2f");
        ui::help_on_hover(msg::mesh_detail_help);
        ImGui::SetNextItemWidth(px(kFieldW));
        ui::InputInt(msg::mesh_drop_specks, &_mesh_job.floater_min_faces);
        _mesh_job.floater_min_faces = std::max(0, _mesh_job.floater_min_faces);
        if (_mesh_job.use_data)
            ui::Checkbox(msg::mesh_cull_unseen, &_mesh_job.cull_unseen);
        ImGui::SetNextItemWidth(
            -(style_x + ImGui::CalcTextSize(msg::mesh_extra_args.get()).x));
        ui::InputTextRaw("##meshextra", &_mesh_job.extra_args);
        ImGui::SameLine();
        ui::TextDisabled(msg::mesh_extra_args);
    }
}

void GuiApp::draw_mesh() {
    // The run finished while this screen was up: show what it made, ONCE.
    // Keyed on the run id, not "Done && not open", so a closed preview stays
    // closed; here, not in frame(), so only this screen takes the engine.
    if (_mesh.state() == MeshRunner::State::Done && !_mesh_preview_open &&
        _mesh.run_id() != _mesh_shown_run) {
        _mesh_shown_run = _mesh.run_id();
        open_mesh_preview();
    }

    if (ui::Button(msg::back_home)) request_go_home();
    ImGui::SameLine();
    ui::Text(msg::mesh_title);

    const bool running = _mesh.busy();
    _mesh.poll();
    // The run's memory sits in a strip above the log, as on the dataset screen.
    const bool memory_strip = running && _mesh.memory().visible();
    const float strip_h = memory_strip ? ImGui::GetTextLineHeightWithSpacing() + px(6.0f) : 0.0f;
    const float log_h = log_height(ImGui::GetContentRegionAvail().y - strip_h);

    ImGui::BeginChild("##meshbody", ImVec2(0, body_height(log_h) - strip_h));

    if (_mesh_preview_open) {
        // ---- results ----
        if (_mesh.num_verts() > 0)
            ui::Text(msg::mesh_done, {(long long)_mesh.num_verts(),
                                      (long long)_mesh.num_faces()});
        ui::TextDisabledRaw(_mesh.output_path());
        if (ui::Button(msg::mesh_open_in_viewer)) {
            // Hand both panels back first: the viewer screen reuses _compare,
            // which is holding this preview's models.
            const std::string out = _mesh.output_path();
            close_mesh_preview();
            request_open_splat(out);
        }
        ImGui::SameLine();
        // The start button and this button share a message ID.
        ImGui::PushID("again");
        if (ui::Button(msg::mesh_start)) close_mesh_preview();
        ImGui::PopID();
        // Which of them is on screen. One run writes several formats of one
        // surface, and four panes of the same mesh is not a comparison.
        ui::TextDisabled(emsg::mesh_shown);
        if (!training_busy() && !_mesh_job.checkpoint.empty()) {
            ImGui::SameLine();
            bool on = _compare.index_of(_mesh_job.checkpoint) >= 0;
            if (ui::Checkbox(emsg::mesh_side_splats_pane, &on))
                _compare.set_shown(_mesh_job.checkpoint, on,
                                   &msg::mesh_side_splats);
        }
        for (const std::string& out : _mesh.output_paths()) {
            ImGui::SameLine();
            ImGui::PushID(out.c_str());
            bool on = _compare.index_of(out) >= 0;
            if (ui::CheckboxRaw(fs::path(out).filename().string().c_str(), &on))
                _compare.set_shown(out, on, &msg::mesh_side_mesh);
            ImGui::PopID();
        }
        _compare.set_recents(recent_models(_recent));
        _compare.draw_toolbar();
        draw_compare_panes();
    } else {
        // A batch owns the options while it runs; what it IS meshing goes here.
        if (_batch_active) {
            draw_batch_progress();
            ImGui::Spacing();
        }
        ImGui::BeginDisabled(running);
        draw_mesh_options();
        ImGui::EndDisabled();

        ImGui::Spacing();
        if (running) {
            const std::string st = _mesh.stage();
            timed_progress_bar(_mesh.clock(), st.empty() ? msg::mesh_running.get() : st.c_str());
            if (ui::Button(msg::mesh_cancel)) {
                if (_batch_active) _batch_stop_after = _batch_stop_now = true;
                _mesh.cancel();
            }
        } else {
            ImGui::BeginDisabled(_mesh_job.checkpoint.empty() || _batch_active ||
                                 mesh_job_writes_nothing(_mesh_job));
            if (ui::Button(msg::mesh_start, ImVec2(px(kActionButtonW), px(kActionButtonH)))) start_meshing();
            ImGui::EndDisabled();
            if (_mesh_job.checkpoint.empty()) {
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
    }
    ImGui::EndChild();
    if (memory_strip) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + px(3.0f));
        const float x0 = ImGui::GetCursorPosX(), avail = ImGui::GetContentRegionAvail().x;
        _mesh.memory().draw(msg::mesh_memory_help, x0, avail);
    }
    draw_log_panel(log_h);
}

}  // namespace gui
