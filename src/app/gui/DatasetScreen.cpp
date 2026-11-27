// The New Dataset screen's layout. One screen for both reconstruction engines:
// the top half -- the photos or video, where the dataset goes, what to mask
// out -- is the same either way and is all a first-time user has to read. The
// engine-specific knobs sit under a collapsed "Advanced" header, and the engine
// selector appears only when there is a choice to make.

#include "app/gui/GuiApp.h"

#include "app/gui/DatasetPrep.h"
#include "app/gui/SfmInProcess.h"
#include "app/gui/Ui.h"
#include "dense/Artifact.h"
#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/Dense.h"
#include "i18n/catalog/Gui.h"

#include "imgui.h"

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;
namespace msg = spirula::i18n::msg::gui;
namespace dmsg = spirula::i18n::msg::dataset;

namespace gui {

// The form, and under it the button that acts on it. One column of the screen
// when a run has something to show beside it, the whole of it otherwise.
void GuiApp::draw_dataset_form(float height, bool running) {
    // The panel arrives at an output folder -- dropped, picked or typed -- and
    // takes the settings that folder's record says built it, once.
    if (!running && _workspace != _restored_ws && _source_probes_ready) {
        _restored_ws = _workspace;
        restore_from_record(/*announce=*/true);
    }
    if (_keep_built && _keep_built_for != _workspace) _keep_built = false;

    // The action band's height is measured from the last frame, because how
    // tall it is depends on what it is saying.
    ImGui::BeginChild("##dsform", ImVec2(0, height - _ds_action_h));

    // Step settings stay editable until the runner consumes them; inputs and engine stay fixed.
    ImGui::BeginDisabled(native_work_busy());
    draw_dataset_source();

    // The selector appears only when both engines are usable: a Vulkan user
    // with no COLMAP installed never learns that COLMAP exists, and a CUDA
    // user is never offered a back end this build does not have.
    if (builtin_sfm_available() && colmap_available()) {
        ImGui::Spacing();
        int eng = _engine == Engine::BuiltIn ? 0 : 1;
        ui::Text(dmsg::reconstruction);
        ImGui::SameLine();
        if (ui::RadioButton(dmsg::engine_builtin, eng == 0)) eng = 0;
        ui::help_on_hover(dmsg::engine_builtin_help);
        ImGui::SameLine();
        if (ui::RadioButton(dmsg::engine_colmap, eng == 1)) eng = 1;
        ui::help_on_hover(dmsg::engine_colmap_help);
        if ((eng == 1) != (_engine == Engine::Colmap)) {
            _engine = eng == 1 ? Engine::Colmap : Engine::BuiltIn;
            save_settings();
        }
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
    ui::SeparatorText(msg::section_preset);
    // A batch owns these settings while it runs -- each row's come from its
    // own preset -- so what the running row IS building goes here instead.
    if (_batch_active) {
        draw_batch_progress();
    } else {
        ImGui::BeginDisabled(running);
        draw_dataset_preset_picker();
        ImGui::EndDisabled();
    }

    ImGui::Spacing();
    ui::SeparatorText(dmsg::section_settings);
    // The GPU choice sits at the top of the settings it constrains: a preview,
    // a masking step and the reconstruction all run on it, so it is offered
    // before any of them can be started.
#if defined(SS_BUILD_SAM) || defined(SS_TOOL_SFM) || defined(SS_BACKEND_VULKAN)
    draw_device_picker();
#ifdef SS_BUILD_SAM
    ImGui::BeginDisabled(running);
    draw_additional_gpus();
    ImGui::EndDisabled();
#endif
    ImGui::Spacing();
#endif
    draw_dataset_basics();
    ImGui::Spacing();
    ImGui::BeginDisabled(dataset_locked(Stage::Masks));
    draw_masking_options(dataset_masking_panel());
    ImGui::EndDisabled();
    ImGui::Spacing();

    // A laser scan gives the run its depth and normals.
    ImGui::BeginDisabled(dataset_locked(Stage::Geometry) || !_lidar.empty());
    draw_geometry_options();
    ImGui::EndDisabled();
    ImGui::BeginDisabled(dataset_locked(Stage::Dense));
    draw_dense_options();
    ImGui::EndDisabled();
    ImGui::Spacing();

    if (!_batch_active) {
        ImGui::PushID("dataset_training");
        ImGui::BeginDisabled(running || native_work_busy());
        if (ui::Checkbox(dmsg::train_after_creation, &_dataset_training.enabled))
            _dataset_training.error.clear();
        if (_dataset_training.enabled) {
            ui::Text(msg::batch_stage_train);
            draw_batch_run(_dataset_training.run, -3, 0, false);
            if (!_dataset_training.error.empty())
                ui::TextColoredWrappedRaw(kErr, _dataset_training.error.c_str());
        }
        ImGui::EndDisabled();
        ImGui::PopID();
        ImGui::Spacing();
    }

    ImGui::BeginDisabled(dataset_locked(Stage::Features));
    if (effective_engine() == Engine::BuiltIn) draw_sfm_advanced();
    else                                       draw_colmap_options();
    ImGui::EndDisabled();
    // Tool locations are settings of the application, not of the run.
    draw_tool_locations();

    ImGui::EndChild();

    // Edits made above reach the running job at the step that reads them.
    update_dataset_job();

    // ---- run / status ----
    const float action_y0 = ImGui::GetCursorPosY();
    ImGui::Spacing();
    bool input_missing = (_sources.empty() && _lidar.empty()) || _workspace.empty();
    for (const PrepInput& s : _sources)
        input_missing = input_missing || s.path.empty();
    const bool ready = !running && !input_missing && _source_probes_ready;
    if (!running) {
        const bool need_mask_model = mask_model_missing();
        const bool need_feat_model = feature_model_missing();
        const bool need_geom_model = geometry_model_missing();
        const bool need_dense_model = dense_model_missing();
        const bool need_model = need_mask_model || need_feat_model || need_geom_model || need_dense_model;
        if (ready) draw_dataset_plan(dataset_plan());
        // The button names what pressing it does: a folder that already holds
        // a reconstruction is added to, not built.
        const bool adding = workspace_state().model && !_redo_model;
        ImGui::BeginDisabled(!ready || need_model || native_work_busy() ||
                             _batch_active);
        if (ui::Button(adding ? dmsg::update_dataset : dmsg::create_dataset,
                       ImVec2(px(kActionButtonW), px(kActionButtonH))))
            start_dataset_job();
        ImGui::EndDisabled();
        if (input_missing) {
            ImGui::SameLine();
            ui::TextDisabled(dmsg::pick_input_first);
        } else if (!_source_probes_ready) {
            ImGui::SameLine();
            ui::TextDisabled(dmsg::sensors_reading);
        } else if (need_model) {
            // The options above carry the same buttons, but they are a scroll
            // away by the time somebody is reaching for this one. One missing
            // checkpoint at a time; the next takes its place once this lands.
            FileDownload& dl = need_mask_model ? _download
                               : need_feat_model ? _feat_download.current()
                               : need_geom_model ? _geom_download.current() : _dense_download;
            draw_model_fetch(dl,
                             need_mask_model   ? dmsg::mask_model_first
                             : need_feat_model ? dmsg::feat_model_first
                             : need_geom_model ? dmsg::geom_model_first
                                               : spirula::i18n::msg::dense::model_first,
                             need_mask_model   ? dmsg::mask_get_model
                             : need_feat_model ? dmsg::feat_get_model
                             : need_geom_model ? dmsg::geom_get_model
                                               : spirula::i18n::msg::dense::get_model,
                             [&] {
                                 if (need_mask_model)      request_model_download(_model_id, _mask_detector_id);
                                 else if (need_feat_model) request_feature_download();
                                 else if (need_geom_model) request_geometry_download();
                                 else                      request_dense_download();
                             });
        }
        if (ready) {
            draw_dataset_rerun(workspace_state());
            draw_dataset_reset();
        }
    } else {
        if (_sfm.reviewing()) {
            if (ui::Button(dmsg::review_continue, ImVec2(px(kActionButtonW), px(kActionButtonH))))
                continue_frame_review();
            ui::help_on_hover(dmsg::review_hint);
            ImGui::SameLine();
        }
        if (ui::Button(dmsg::cancel, ImVec2(px(kActionButtonW), px(kActionButtonH)))) cancel_dataset_job();
    }

    // Both runners report through the same three states.
    struct {
        bool done, failed, cancelled;
        std::string dir, image_dir, mask_dir, err;
        bool mask_flipped;
    } st{};
    if (effective_engine() == Engine::BuiltIn) {
        st.done = _sfm.state() == SfmRunner::State::Done;
        st.failed = _sfm.state() == SfmRunner::State::Failed;
        st.cancelled = _sfm.state() == SfmRunner::State::Cancelled;
        st.dir = _sfm.dataset_dir();
        st.image_dir = _sfm.image_dir();
        st.mask_dir = _sfm.mask_dir();
        st.mask_flipped = _sfm.mask_flipped();
        st.err = _sfm.error();
    } else {
        st.done = _colmap.state() == ColmapRunner::State::Done;
        st.failed = _colmap.state() == ColmapRunner::State::Failed;
        st.cancelled = _colmap.state() == ColmapRunner::State::Cancelled;
        st.dir = _colmap.dataset_dir();
        st.image_dir = _colmap.image_dir();
        st.mask_dir = _colmap.mask_dir();
        st.mask_flipped = _colmap.mask_flipped();
        st.err = _colmap.error();
    }
    if (st.done) {
        if (effective_engine() == Engine::BuiltIn && _sfm.partial())
            ui::TextColoredWrapped(kWarn, dmsg::partial_reconstruction);
        if (effective_engine() == Engine::BuiltIn && _sfm.not_metric())
            ui::TextColoredWrapped(kWarn, dmsg::not_metric_reconstruction);
        ui::TextColoredWrapped(kOk, dmsg::done_at, {st.dir});
    } else if (st.failed) {
        ui::TextColoredWrapped(kErr, dmsg::failed, {st.err});
    } else if (st.cancelled) {
        ui::TextColored(kDim, dmsg::cancelled);
    }
    _ds_action_h = ImGui::GetCursorPosY() - action_y0;
}

// What the header links to: the dataset the run just made, or the one already
// in the output folder -- the same either way.
unsigned GuiApp::dataset_screen_links(DatasetFolders& f) {
    if (dataset_busy()) return 0;
    const bool builtin = effective_engine() == Engine::BuiltIn;
    const bool done = builtin ? _sfm.state() == SfmRunner::State::Done
                              : _colmap.state() == ColmapRunner::State::Done;
    bool model = true;
    std::error_code ec;
    if (done) {
        f = builtin ? DatasetFolders{_sfm.dataset_dir(), _sfm.image_dir(), _sfm.mask_dir(),
                                     _sfm.mask_flipped()}
                    : DatasetFolders{_colmap.dataset_dir(), _colmap.image_dir(),
                                     _colmap.mask_dir(), _colmap.mask_flipped()};
        const fs::path masks = fs::path(f.dir) / f.mask_dir;
        if (f.mask_dir.empty() || !fs::is_directory(masks, ec) || fs::is_empty(masks, ec))
            f.mask_dir.clear();
    } else {
        bool missing = (_sources.empty() && _lidar.empty()) || _workspace.empty();
        for (const PrepInput& s : _sources) missing = missing || s.path.empty();
        if (missing || !_source_probes_ready) return 0;
        const WorkspaceState& prior = workspace_state();
        f = workspace_folders(prior);
        model = prior.model;
    }
    unsigned links = f.mask_dir.empty() ? 0u : (unsigned)kLinkMasks;
    if (model) {
        links |= kLinkTrain | kLinkEdit | kLinkRegion | kLinkPartition;
        // Cleaning the seed cloud belongs here rather than after training: a
        // floater removed now is one the run never fits to.
        refresh_dense_versions(f.dir);
        if (!_dense_versions.empty()) links |= kLinkDense;
    }
    return links;
}

void GuiApp::draw_new_dataset() {
    const bool running = dataset_busy();

    const bool from_video = !_sources.empty() && _sources[0].is_video;
    DatasetFolders links_to;
    const unsigned links = dataset_screen_links(links_to);
    draw_screen_header(from_video ? &dmsg::title_from_video : &dmsg::title_from_photos,
                       [&] { draw_dataset_links(links_to, links); });
    ImGui::Spacing();

    // What the run says about itself, read before the layout is decided:
    // whether there is anything to show is what decides whether the screen is
    // one column or two.
    poll_sfm_progress();
    poll_dense_progress();
    // The memory of the steps that can run out of it -- mapping and dense --
    // sits in a strip above the log, where training keeps its status.
    const Stage step = dataset_steps()->current();
    const bool builtin = effective_engine() == Engine::BuiltIn;
    const Stage memory_step = !running ? Stage::Frames
                            : step == Stage::Dense && !_workspace.empty() ? Stage::Dense
                            : step == Stage::Mapping && builtin ? Stage::Mapping
                                                                : Stage::Frames;
    if (memory_step != _run_memory_step) _run_memory.reset();
    _run_memory_step = memory_step;
    if (memory_step == Stage::Dense) {
        _run_memory.poll_report(spirula::dense::progress_dir(_workspace).string());
    } else if (memory_step == Stage::Mapping) {
        const double work = std::max(0.0f, dataset_steps()->stage(Stage::Mapping).fraction);
        if (_sfm_job.subprocess)
            _run_memory.poll_report(_sfm.progress_dir(), work);
        else
            _run_memory.poll_self(work, [](spirula::RunReporter::Device& d) {
                return sfm_device_memory(d.ours, d.others, d.capacity);
            });
    }
    const bool memory_strip = memory_step != Stage::Frames;
    const float strip_h = memory_strip ? ImGui::GetTextLineHeightWithSpacing() + px(6.0f) : 0.0f;

    // The log spans the bottom whatever the body does above it: it is the one
    // panel that is read across everything, and it is what a run used to be
    // watched entirely through.
    const float log_h = log_height(ImGui::GetContentRegionAvail().y - strip_h);

    const bool wide = ImGui::GetContentRegionAvail().x >= px(1000.0f);
    update_source_preview_gpu();

    ImGui::BeginChild("##dsbody", ImVec2(0, body_height(log_h) - strip_h));
    if (wide) {
        const float w = std::clamp(_ds_panel_w * ui_scale(), px(320.0f),
                                   std::max(px(320.0f),
                                            ImGui::GetContentRegionAvail().x * 0.6f));
        const float col_h = ImGui::GetContentRegionAvail().y;
        ImGui::BeginChild("##dsleft", ImVec2(w, col_h));
        draw_dataset_form(col_h, running);
        ImGui::EndChild();
        float dragged = w;
        if (splitter_v("##dspanelsplit", &dragged, px(320.0f),
                       ImGui::GetWindowWidth() * 0.75f, col_h)) {
            _ds_panel_w = dragged / ui_scale();
            _layout_dirty = true;
        }
        ImGui::BeginGroup();
        if (running || preview_has_content()) draw_dataset_steps();
        if (!running && !source_preview_available() && !run_preview_available()) {
            _source_view.pause();
            ImGui::BeginChild("##dsEmptyPreview", ImVec2(0, col_h));
            ui::CenteredText(dmsg::preview_choose_source);
            ImGui::EndChild();
        } else draw_dataset_right(ImGui::GetContentRegionAvail().y);
        ImGui::EndGroup();
    } else {
        const float avail = ImGui::GetContentRegionAvail().y;
        // The step strip belongs beside the Cancel button when it has no
        // column of its own.
        draw_dataset_form(avail - _ds_preview_h_used, running);
        const float y0 = ImGui::GetCursorPosY();
        if (running) draw_dataset_steps();
        draw_dataset_right(0.0f);
        _ds_preview_h_used = ImGui::GetCursorPosY() - y0;
    }
    ImGui::EndChild();

    if (memory_strip) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + px(3.0f));
        const float x0 = ImGui::GetCursorPosX(), avail = ImGui::GetContentRegionAvail().x;
        _run_memory.draw(memory_step == Stage::Dense ? spirula::i18n::msg::dense::memory_help
                                                     : msg::sfm_memory_help,
                         x0, avail);
    }
    draw_log_panel(log_h);

    // What it saved is listed once it has closed.
    if (_segment_was_open && !_segment.is_open()) {
        _stencil_sets_stale = true;
        refresh_frame_shapes(_sources, _frame_shapes);
    }
    _segment_was_open = _segment.is_open();
    pump_scan_photos();
    if (_segment.is_open()) {
        // It edits one input's stencil, or one scan's, and shows its frames,
        // so it cannot outlive the list it was opened against.
        if (_mask_preview_scan < 0 && _mask_preview_input >= (int)_sources.size())
            _mask_preview_input = 0;
        app::FrameStencil* stencil =
            _mask_preview_scan >= 0
                ? (_mask_preview_scan < (int)_lidar.size() ? &_lidar[(size_t)_mask_preview_scan].stencil
                                                           : nullptr)
                : (_sources.empty() ? nullptr : &_sources[(size_t)_mask_preview_input].stencil);
        if (!stencil) {
            _segment.close();
        } else {
            _segment.set_workspace(_workspace);
            // Rows that read "same as above" of what it edits follow the edit.
            const StencilTable t = stencil_table();
            const int edited = panel_owner(t);
            settle_follow(t.owners, t.rows);
            const std::vector<app::CameraStencil> before = row_stencils(t.owners, t.rows);
            _segment.draw(_mask, *stencil);
            if (edited >= 0) follow_edit(t.owners, t.rows, before, (size_t)edited);
            if (_segment.take_shapes_edited()) refresh_frame_shapes(_sources, _frame_shapes);
            if (_segment.take_browse_request())
                open_pick(PickAction::StencilFile, dmsg::stencil_pick_file.get(),
                          FileDialog::Mode::File, {".svg"});
        }
    }
    if (_geometry_panel.is_open()) _geometry_panel.draw(_geometry);
    draw_clear_project_modal();
    draw_drop_intermediate_modal();
    draw_rebuild_confirm_modal();
    draw_stencil_set_dialogs();
    // The model on disk was mapped from the matches before the edit.
    if (_matrix_editor.is_open() && _matrix_editor.draw(dataset_busy())) {
        _matches_mtime = 0;
        _pairs_view.reload();
        _redo_model = true;
        _sfm.keep_intermediates();
    }
}

}  // namespace gui
