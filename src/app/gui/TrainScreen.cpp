// The Train screen: settings, controls, metrics and the status strip.

#include "app/gui/GuiApp.h"

#include "app/CrashLog.h"
#include "app/RunLog.h"
#include "app/gui/GuiAppShared.h"
#include "app/gui/ProgressView.h"
#include "app/gui/Ui.h"
#include "app/gui/VramForecastView.h"
#include "app/webviewer/RegionOverlay.h"
#include "core/ColorBasis.h"
#include "data/RegionMesh.h"
#include "data/ScenePartition.h"
#include "dense/Artifact.h"
#include "external/stb_image.h"
#include "i18n/TimeFormat.h"
#include "i18n/catalog/Gui.h"
#include "i18n/catalog/MaskEdit.h"
#include "i18n/catalog/Render.h"
#include "i18n/catalog/TrainFields.h"

#include "imgui.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;
namespace msg = spirula::i18n::msg::gui;
namespace fld = spirula::i18n::msg::field;
namespace runlog = spirula::runlog;
namespace rmsg = spirula::i18n::msg::render;
namespace mmsg = spirula::i18n::msg::maskedit;
using spirula::i18n::Msg;
using spirula::i18n::format_duration_coarse;

namespace gui {

namespace {

std::string format_gib(uint64_t bytes) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.2f", (double)bytes / (1024.0 * 1024.0 * 1024.0));
    return buf;
}

}  // namespace

std::string format_count(double n) {
    char buf[32];
    if (n >= 1e6) std::snprintf(buf, sizeof buf, "%.2fM", n / 1e6);
    else if (n >= 1e3) std::snprintf(buf, sizeof buf, "%.1fk", n / 1e3);
    else std::snprintf(buf, sizeof buf, "%d", (int)n);
    return buf;
}

bool parse_settings_equal(const TrainConfig& a, const TrainConfig& b) {
    bool eq = true;
#define SS_CMP(member) eq = eq && (a.member == b.member);
    SS_DATASET_PARSE_FIELDS(SS_CMP)
#undef SS_CMP
    return eq;
}

void GuiApp::launch_training(const TrainConfig& cfg, const std::string& preset) {
    if (cfg.data.empty() || native_work_busy()) return;
    stop_inference_users();
    close_splat();
#ifdef SS_BACKEND_VULKAN
    if (!freeze_native_device()) return;
#else
    if (!freeze_cuda_device()) return;
#endif
    app::set_crash_note("training " + cfg.data);
    close_mesh_preview();
    detach_session_views();
    const fs::path log_file =
        runlog::open(_train_log, runlog::dataset_dir(cfg.data), "train", runlog::file_stamp());
    set_ss_env("UNREG_LOG", "");
    write_run_settings(_train_log);
    _runner.start_training(cfg, preset, log_file.string());
}

void GuiApp::start_training() {
    launch_training(_cfg, _preset);
}

void GuiApp::draw_train() {
    bool preparing = _runner.phase() == TrainRunner::Phase::Preparing;
    ImGui::BeginDisabled(preparing);   // no clean interruption point
    if (ui::Button(msg::back_home)) request_go_home();
    ImGui::EndDisabled();
    if (training_busy()) ui::help_on_hover(msg::leaving_stops_training);
    ImGui::SameLine();
    if (_batch_active) {
        if (ui::Button(msg::batch_show_list)) _screen = Screen::Batch;
        ImGui::SameLine();
        const BatchRow* row = batch_running_row();
        ui::TextDisabledRaw(row ? row->dataset : std::string());
    } else {
        ui::TextDisabledRaw(_cfg.data);
        // Offered when the dataset's mask folder exists; probed at most once
        // a second, as workspace_state() does.
        const std::string key = _cfg.data + "\n" + _cfg.mask_dir;
        const double now = ImGui::GetTime();
        if (key != _train_masks_key || now - _train_masks_at > 1.0) {
            std::error_code ec;
            fs::path md(_cfg.mask_dir);
            if (md.is_relative()) md = fs::path(_cfg.data) / md;
            _train_masks_key = key;
            _train_masks_at = now;
            _train_has_masks = !_cfg.data.empty() && fs::is_directory(md, ec) && !fs::is_empty(md, ec);
        }
        if (_train_has_masks) {
            ImGui::SameLine();
            ImGui::BeginDisabled(training_busy() || native_work_busy());
            if (ui::Button(mmsg::correct_masks)) {
                fs::path id(_cfg.image_dir), md(_cfg.mask_dir);
                if (id.is_relative()) id = fs::path(_cfg.data) / id;
                if (md.is_relative()) md = fs::path(_cfg.data) / md;
                open_mask_editor(_cfg.data, id.string(), md.string(), _cfg.flip_mask);
            }
            ImGui::EndDisabled();
            ui::help_on_hover(mmsg::correct_masks_help);
        }
    }
    // Two ways to watch a run: the scene in 3D, or one training photograph
    // beside the render of the same camera. Right-aligned on the header row so
    // it costs the preview below no height.
    {
        const float w = px(160.0f);
        draw_screenshot_controls(w);
        ImGui::SameLine(std::max(0.0f, ImGui::GetContentRegionMax().x - w));
        ImGui::SetNextItemWidth(w);
        int mode = _preview_images ? 1 : 0;
        if (ui::ComboRaw("##previewmode", &mode,
                         {&msg::preview_mode_3d, &msg::preview_mode_images}))
            _preview_images = mode == 1;
        ui::help_on_hover(msg::preview_mode_help);
    }

    draw_device_issue_banner();

    const float body_avail = ImGui::GetContentRegionAvail().y;
    if (_show_settings) {
        // Never more than 45% of the window: the viewport is the point of the
        // screen, and a settings column sized for a 1600 px window swallows a
        // 1100 px one.
        const float w = std::clamp(_panel_w * ui_scale(), px(220.0f),
                                   std::max(px(220.0f),
                                            ImGui::GetContentRegionAvail().x * 0.45f));
        ImGui::BeginChild("##settings", ImVec2(w, 0), ImGuiChildFlags_Borders);
        draw_train_settings();
        ImGui::EndChild();
        float dragged = w;
        if (splitter_v("##panelsplit", &dragged, px(220.0f),
                       ImGui::GetWindowWidth() * 0.7f, body_avail)) {
            _panel_w = dragged / ui_scale();
            _layout_dirty = true;
        }
    }

    ImGui::BeginGroup();
    float status_h = ImGui::GetFrameHeightWithSpacing() +
                     ImGui::GetTextLineHeightWithSpacing() + px(10.0f);
    float spacing = ImGui::GetStyle().ItemSpacing.y;
    float log_h = log_height(body_avail - status_h - spacing);
    float vp_h = -(log_h + (log_h > 0 ? splitter_extent() : 0) + status_h + spacing);
    ImGui::BeginChild("##viewport", ImVec2(0, vp_h), ImGuiChildFlags_Borders);
    const bool stepping = _runner.phase() == TrainRunner::Phase::Training;
    // The step both previews pace their refresh by while nobody is steering.
    const int step = stepping ? _runner.latest_progress().step : -1;
    // The images view's textures are ~0.7 GB at 60 MP; hidden, training can have them.
    if (_preview_images) _images.draw(stepping, step);
    else                 { _images.destroy_gl(); _viewport.draw(stepping, step); }
    ImGui::EndChild();
    draw_status_strip();
    draw_log_panel(log_h);
    ImGui::EndGroup();
}

// The session's region as a boundary mesh, the same one the editor draws. The
// engine renders in the dataset frame less its centre; the preview draws the
// points, which carry the relative scale as well.
void GuiApp::update_roi_overlay() {
    const TrainRunner::Phase ph = _runner.phase();
    spirula::TrainerSession* s = _runner.session();
    const bool settled = ph == TrainRunner::Phase::Ready || _runner.engine_ready();
    const void* key = settled && s && s->roi ? s->roi.get() : nullptr;
    if (!settled && key == nullptr && _roi_key != nullptr && s) return;
    if (key != _roi_key) {
        _roi_key = key;
        _viewport.set_region_overlay(nullptr, nullptr);
        if (_roi_job.valid()) _roi_job.wait();
        _roi_job = {};
        if (!key) return;
        std::shared_ptr<const spirula::Region> roi = s->roi;
        const double rs = s->cfg.relative_scale.value_or(1.0f);
        const std::array<double, 3> c = s->ds.center;
        const int64_t n = s->ds.points.num();
        std::vector<float> world((size_t)n * 3);
        for (int64_t i = 0; i < n; i++)
            for (int r = 0; r < 3; r++) world[(size_t)i * 3 + r] = (float)(s->ds.points.xyz[(size_t)i * 3 + r] / rs + c[r]);
        float rgb[3] = {1.0f, 0.55f, 0.1f};
        if (!s->cfg.partition.empty() && s->cfg.partition_part >= 0)
            spirula::part_color(s->cfg.partition_part, rgb);
        _roi_job = std::async(std::launch::async, [roi, world, rs, c, rgb]() {
            const spirula::Aabb box = spirula::region_mesh_box(
                *roi, spirula::robust_bounds(world.data(), (int64_t)world.size() / 3));
            const spirula::RegionMesh m = spirula::region_boundary_mesh(*roi, box, 128);
            const float to_engine[12] = {1, 0, 0, (float)-c[0], 0, 1, 0, (float)-c[1], 0, 0, 1, (float)-c[2]};
            const float to_preview[12] = {(float)rs, 0, 0, (float)(-rs * c[0]), 0, (float)rs, 0, (float)(-rs * c[1]),
                                          0, 0, (float)rs, (float)(-rs * c[2])};
            auto engine = std::make_shared<spirula::RegionOverlay>();
            auto preview = std::make_shared<spirula::RegionOverlay>();
            engine->add(m, rgb, to_engine);
            engine->region = roi;
            for (int r = 0; r < 3; r++) engine->shift[r] = c[r];
            preview->add(m, rgb, to_preview);
            auto inside = std::make_shared<std::vector<uint8_t>>(world.size() / 3);
            roi->contains_many(world.data(), (int64_t)inside->size(), inside->data());
            return RoiOverlays{engine, preview, inside};
        });
        return;
    }
    if (_roi_job.valid() && _roi_job.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        const RoiOverlays o = _roi_job.get();
        _viewport.set_region_overlay(o.engine, o.preview, o.points_inside);
    }
}

void GuiApp::draw_train_settings() {
    TrainRunner::Phase ph = _runner.phase();
    bool busy = ph == TrainRunner::Phase::Loading ||
                ph == TrainRunner::Phase::Preparing ||
                ph == TrainRunner::Phase::Training ||
                ph == TrainRunner::Phase::Evaluating;

    // ---- dataset ----
    ui::SeparatorText(msg::section_dataset);
    switch (ph) {
        case TrainRunner::Phase::Loading:
            ui::TextColored(kDim, msg::parsing_dataset);
            break;
        case TrainRunner::Phase::LoadError:
            // Parser errors name files and formats; they stay English so that
            // searching for one finds the same text everyone else saw.
            ui::TextColoredWrappedRaw(kErr, _runner.error());
            break;
        default:
            if (auto* s = _runner.session()) {
                if (ph == TrainRunner::Phase::Ready ||
                    ph == TrainRunner::Phase::Training ||
                    ph == TrainRunner::Phase::Evaluating ||
                    ph == TrainRunner::Phase::Done) {
                    ui::Text(msg::dataset_summary,
                             {(long long)s->ds.num_cameras,
                              (long long)s->post.n_post,
                              format_count((double)s->ds.points.num())});
                }
            } else {
                ui::TextColored(kDim, msg::no_dataset_loaded);
            }
            break;
    }
    ImGui::BeginDisabled(busy && ph != TrainRunner::Phase::Loading);
    if (ui::Button(msg::change_dataset)) {
        open_pick(PickAction::OpenDataset, msg::menu_open_dataset.get(),
                  FileDialog::Mode::Folder);
    }
    ImGui::EndDisabled();
    if (!_batch_active) draw_roi_row(busy);

    // Vulkan builds share the native picker with every built-in workflow.
#ifdef SS_BACKEND_VULKAN
    draw_device_picker();
#else
    ui::SeparatorText(msg::section_device);
    {
        int n_dev = backend::device_count();
        int cur = backend::device_current();
        backend::DeviceInfo curd = backend::device_info(cur);
        ImGui::BeginDisabled(_cuda_device_locked || busy || n_dev == 0);
        ImGui::SetNextItemWidth(px(-8.0f));
        // Device names come from the driver; only the "none found" and
        // "unsupported" notes are ours.
        if (ui::BeginComboRaw("##device",
                              cur >= 0 ? curd.name : msg::no_device_found.get())) {
            for (int i = 0; i < n_dev; i++) {
                backend::DeviceInfo d = backend::device_info(i);
                char label[324];
                std::snprintf(label, sizeof(label), "%s (%s, %llu MB)%s##d%d",
                              d.name, d.type,
                              (unsigned long long)(d.vram_bytes >> 20),
                              d.usable ? "" : msg::device_unsupported.get(), i);
                ImGui::BeginDisabled(!d.usable);
                bool sel = i == cur;
                if (ui::SelectableRaw(label, sel)) backend::device_select(i);
                if (sel) ImGui::SetItemDefaultFocus();
                ImGui::EndDisabled();
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        if (_cuda_device_locked)
            ui::TextColoredWrapped(kDim, msg::device_cuda_locked);
    }
#endif

    // ---- preset + options ----
    // A batch owns the config while it runs (each row has its own preset), so
    // show what the row is training instead of the editor.
    if (_batch_active) {
        draw_batch_progress();
    } else {
        // Snapshot: any change to a dataset-parsing option below triggers an
        // automatic reload (deferred until the edited widget loses focus).
        TrainConfig parse_before = _cfg;
        ImGui::BeginDisabled(busy);
        ui::SeparatorText(msg::section_preset);
        // A preset reloads the dataset itself; against the old snapshot the
        // preset's own values would look like an edit and load it again.
        if (draw_preset_picker()) parse_before = _cfg;

        ui::SeparatorText(msg::section_basic_options);
        draw_basic_options();

        // Without a dense cloud the only choice is the dataset's own points,
        // so the section stays out of the way; Advanced still has the flag.
        if (dense_cloud_present() ||
            (!_cfg.seed_pointcloud.empty() && _cfg.seed_pointcloud != spirula::dense::kSparseSeed))
            draw_seed_cloud(ph);

        ImGui::Spacing();
        if (ui::CollapsingHeader(msg::section_all_options))
            draw_config_editor(_cfg, _defaults, _cfg_ui);
        if (!_cfg_ui.pick.field.empty()) {
            const PathPick p = std::exchange(_cfg_ui.pick, {});
            _pick_field = p.field;
            open_pick(PickAction::ConfigPath, p.title,
                      p.folder ? FileDialog::Mode::Folder : FileDialog::Mode::File,
                      p.extensions, p.start_dir);
        }
        ImGui::EndDisabled();

        // Macro options (quality, floater_suppression, ...) fill in their flags
        // except hand-edited ones, so the panels show what the run will use.
        // None is a dataset-parsing field, so the snapshot below stays valid.
        TrainConfig macro_base;   // not _defaults: a saved preset bakes in macros
        train_apply_preset(macro_base, _preset);
        train_resolve_macros(_cfg, _cfg_ui.touched, nullptr, &macro_base);

        if (_cfg.save_full_checkpoint != parse_before.save_full_checkpoint) {
            _keep_full_ckpt = _cfg.save_full_checkpoint;
            save_settings();
        }
        if (!parse_settings_equal(parse_before, _cfg)) _parse_dirty = true;
    }

    // ---- controls + metrics ----
    ui::SeparatorText(msg::section_training);
    draw_train_controls();
    draw_metrics();
}

// Every edit here records itself in _cfg_ui.touched, the same way the
// generated editor does: a flag the user set by hand is off limits to the
// macro options (see train_resolve_macros()).
void GuiApp::draw_train_mask_mode(float width) {
    int mi = 0;
    const ResolvedMaskMode m = resolve_train_mask_mode(_cfg);
    if (m == ResolvedMaskMode::Off) mi = 3;
    else if (m == ResolvedMaskMode::SegmentAndIgnore) mi = 2;
    else if (m == ResolvedMaskMode::Segment) mi = 1;
    else if (_cfg.mask_mode == "auto" && !_cfg.apply_loss_for_mask.has_value()) {
        const TrainRunner::Phase ph = _runner.phase();
        if (ph == TrainRunner::Phase::Ready || ph == TrainRunner::Phase::Training ||
            ph == TrainRunner::Phase::Evaluating ||
            ph == TrainRunner::Phase::Done)
            if (auto* s = _runner.session())
                if (s->cfg.apply_loss_for_mask.value_or(false)) mi = 1;
    }
    ImGui::SetNextItemWidth(width);
    if (ui::Combo(msg::opt_mask_mode, &mi,
                  {&msg::opt_mask_mode_exclude,
                   &msg::opt_mask_mode_cut_out,
                   &msg::opt_mask_mode_segment_and_ignore,
                   &msg::opt_mask_mode_off})) {
        static constexpr const char* kModes[4] = {
            "ignore", "segment", "segment_and_ignore", "off"};
        _cfg.mask_mode = kModes[mi];
        _cfg.load_masks = mi != 3;
        _cfg_ui.touched.insert("mask_mode");
        _cfg_ui.touched.insert("load_masks");
        if (mi != 3) {
            _cfg.apply_loss_for_mask = (mi == 1 || mi == 2);
            _cfg_ui.touched.insert("apply_loss_for_mask");
        }
    }
    ui::help_on_hover_disabled(msg::opt_mask_mode_help);
}

// Under a transparency draw, background_mode shows on segments only: say so
// when the dataset and the mask mode leave none. Decodes at most three masks.
void GuiApp::draw_background_warning() {
    if (_cfg.background_mode == "black" || _cfg.background_transparency_suppression == "none")
        return;
    const TrainRunner::Phase ph = _runner.phase();
    auto* s = _runner.session();
    if (!s || ph == TrainRunner::Phase::Loading || ph == TrainRunner::Phase::LoadError ||
        s->cfg.data != _cfg.data)
        return;
    std::vector<const std::string*> files;
    for (const std::string& f : s->ds.mask_filenames)
        if (!f.empty()) files.push_back(&f);
    const bool alpha = std::count(s->alpha_images.begin(), s->alpha_images.end(), 1) > 0;
    const std::string& bg = _cfg.background_mode;
    if (files.empty() && !alpha) {
        ui::TextColoredWrapped(kWarn, msg::bg_unused_no_masks, {bg});
        return;
    }
    // As TrainerCore resolves auto: cut out when the only masks are alpha.
    ResolvedMaskMode m = resolve_train_mask_mode(_cfg);
    if (_cfg.mask_mode == "auto" && _cfg.load_masks && !_cfg.apply_loss_for_mask.has_value())
        m = alpha && files.empty() ? ResolvedMaskMode::Segment : ResolvedMaskMode::Ignore;
    if (m == ResolvedMaskMode::Off || m == ResolvedMaskMode::Ignore) {
        const Msg& mode = m == ResolvedMaskMode::Off ? msg::opt_mask_mode_off
                                                     : msg::opt_mask_mode_exclude;
        ui::TextColoredWrapped(kWarn, msg::bg_unused_mask_mode, {mode.get(), bg});
        return;
    }
    const bool seg_ign = m == ResolvedMaskMode::SegmentAndIgnore;
    if (!alpha) {
        const std::string key = _cfg.data + '\n' + _cfg.mask_dir + '\n' +
            std::to_string((int)seg_ign) + std::to_string((int)_cfg.flip_mask) + '\n' +
            std::to_string(files.size());
        if (key != _bg_segment_probe_key) {
            _bg_segment_probe_key = key;
            _bg_segment_probe_found = false;
            const size_t num_picks = std::min(files.size(), (size_t)10);
            for (size_t k = 0; k < num_picks && !_bg_segment_probe_found; k++) {
                int w = 0, h = 0, ch = 0;
                size_t file_idx = std::min(files.size() * k / num_picks, files.size() - 1);
                stbi_uc* px = stbi_load(files[file_idx]->c_str(), &w, &h, &ch, 1);
                if (!px) { _bg_segment_probe_found = true; break; }  // unknown: say nothing
                for (size_t i = 0; i < (size_t)w * h && !_bg_segment_probe_found; i++) {
                    const uint8_t v = px[i];
                    _bg_segment_probe_found = seg_ign ? (v >= 128 && v <= 250)
                                                      : ((v != 0) == _cfg.flip_mask);
                }
                stbi_image_free(px);
            }
        }
        if (!_bg_segment_probe_found) {
            ui::TextColoredWrapped(kWarn, seg_ign ? msg::bg_unused_no_grey
                                                  : msg::bg_unused_no_masked, {bg});
            return;
        }
    }
    // As train_mask_preserves_background: unset is on for a skybox here.
    if (bg == "skybox" && !_cfg.preserve_background.value_or(true))
        ui::TextColoredWrapped(kWarn, msg::bg_skybox_untrained, {fld::preserve_background.get()});
}

// Masks, depths and normals, each offered only once the parse has found it.
// Ticking a map sets the preset's weight back (or a fallback where the preset
// has none); unticking zeroes it, which is also what keeps the maps unloaded.
void GuiApp::draw_dataset_aux(float width) {
    // Unmeasured: the Pearson term is per image and stays well under 1, so it
    // gets five times the per-pixel normal default.
    constexpr float kDatasetDepthWeight = 0.05f;
    const TrainRunner::Phase ph = _runner.phase();
    auto* s = _runner.session();
    if (!s || ph == TrainRunner::Phase::Loading || ph == TrainRunner::Phase::LoadError ||
        s->cfg.data != _cfg.data)
        return;
    ImGui::Spacing();
    if (!s->ds.mask_filenames.empty() || !s->alpha_images.empty())
        draw_train_mask_mode(width);
    if (!s->ds.depth_filenames.empty()) {
        bool on = _cfg.load_depths && _cfg.depth_supervision_weight > 0.0f;
        if (ui::Checkbox(msg::opt_dataset_depths, &on)) {
            _cfg.load_depths = _cfg.load_depths || on;
            _cfg.depth_supervision_weight = !on ? 0.0f
                : _defaults.depth_supervision_weight > 0.0f ? _defaults.depth_supervision_weight
                                                           : kDatasetDepthWeight;
            _cfg_ui.touched.insert("load_depths");
            _cfg_ui.touched.insert("depth_supervision_weight");
        }
        ui::help_on_hover_disabled(msg::opt_dataset_depths_help);
    }
    if (!s->ds.normal_filenames.empty()) {
        bool on = _cfg.load_normals && (_cfg.normal_supervision_weight > 0.0f ||
                                        _cfg.median_normal_supervision_weight > 0.0f);
        if (ui::Checkbox(msg::opt_dataset_normals, &on)) {
            _cfg.load_normals = _cfg.load_normals || on;
            _cfg.normal_supervision_weight = !on ? 0.0f
                : _defaults.normal_supervision_weight > 0.0f ? _defaults.normal_supervision_weight
                                                            : TrainConfig{}.normal_supervision_weight;
            _cfg.median_normal_supervision_weight = on ? _defaults.median_normal_supervision_weight : 0.0f;
            _cfg_ui.touched.insert("load_normals");
            _cfg_ui.touched.insert("normal_supervision_weight");
            _cfg_ui.touched.insert("median_normal_supervision_weight");
        }
        ui::help_on_hover_disabled(msg::opt_dataset_normals_help);
    }
}

void GuiApp::draw_basic_options() {
    // Wide enough for the longest value any of these dropdowns offers, and
    // scaled: at 200% the labels are twice the size and the box is not.
    const float w = px(170.0f);

    // A dropdown over a flag's choices, the macro options among them. The value
    // written is the token the flag takes; what the user picks is the word for it.
    auto macro_option = [&](const char* key, std::string& value,
                            const Msg& label, const Msg& help,
                            std::initializer_list<const char*> values) {
        ImGui::SetNextItemWidth(w);
        const bool open = ui::BeginComboRaw(ui::detail::label(label),
                                            gui::choice_display(key, value).c_str());
        if (!open) { ui::help_on_hover_disabled(help); return; }
        for (const char* v : values) {
            bool sel = value == v;
            if (ui::SelectableRaw(gui::choice_display(key, v), sel)) {
                value = v;
                _cfg_ui.touched.insert(key);
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    };

    // Output location first -- the thing every new user looks for.
    ImGui::SetNextItemWidth(w);
    if (ui::InputTextRaw("##outdir", &_cfg.output_dir_prefix))
        _cfg_ui.touched.insert("output_dir_prefix");
    ImGui::SameLine();
    if (ui::ButtonRaw("...##outdir")) {
        open_pick(PickAction::OutputPrefix, msg::pick_output_folder.get(),
                  FileDialog::Mode::Folder, {}, _cfg.output_dir_prefix);
    }
    ImGui::SameLine();
    ui::Text(msg::opt_output_folder);
    ui::help_on_hover_disabled(msg::opt_output_folder_help);
    ImGui::SetNextItemWidth(w);
    if (ui::InputTextWithHint(msg::opt_run_name, msg::opt_run_name_hint,
                              &_cfg.output_dir_name))
        _cfg_ui.touched.insert("output_dir_name");
    ui::help_on_hover_disabled(msg::opt_run_name_help);
    {
        std::string run = _cfg.output_dir_name.empty()
            ? fs::path(_cfg.data).stem().string() + "_<time>"
            : _cfg.output_dir_name;
        ui::TextColoredWrappedRaw(
            kDim, "-> " + (fs::path(_cfg.output_dir_prefix) / run).string());
    }
    ImGui::Spacing();

#if 0
    // Ahead of the two flags it sets, so the usual path is to pick a quality
    // and move on, and the numbers below are what that choice came to.
    macro_option("quality", _cfg.quality, fld::quality, fld::quality_help,
                 {"low", "medium", "high", "ultra"});
#endif

    ImGui::SetNextItemWidth(w);
    if (ui::InputInt(msg::opt_steps, &_cfg.num_iterations))
        _cfg_ui.touched.insert("num_iterations");
    ui::help_on_hover_disabled(msg::opt_steps_help);

    ImGui::SetNextItemWidth(w);
    if (ui::InputInt(msg::opt_max_splats, &_cfg.cap_max))
        _cfg_ui.touched.insert("cap_max");
    ui::help_on_hover_disabled(msg::opt_max_splats_help);

    {
        // Primitive names are the config's own values, written into
        // config.json and accepted by `--primitive`; they are not translated.
        static const char* prims[] = {"3dgs", "mip", "3dgut", "2dgs"};
        int pi = 0;
        for (int k = 0; k < 4; k++)
            if (_cfg.primitive == prims[k]) pi = k;
        ImGui::SetNextItemWidth(w);
        if (ui::ComboRaw(ui::detail::label(msg::opt_primitive), &pi, prims, 4)) {
            _cfg.primitive = prims[pi];
            _cfg_ui.touched.insert("primitive");
        }
        ui::help_on_hover_disabled(spirula::i18n::msg::field::primitive_help);
        // if (_cfg.primitive == "3dgut")
        //     ui::TextColoredWrapped(kWarn, msg::opt_primitive_3dgut_warn);
    }

    int ds_idx = _cfg.train_resolution_divisor == 2.0f ? 1
               : _cfg.train_resolution_divisor == 4.0f ? 2
               : _cfg.train_resolution_divisor == 8.0f ? 3 : 0;
    {
        // "1/2" is a fraction, not a word; only "native" is translated.
        const char* items[] = {msg::opt_resolution_native.get(), "1/2", "1/4", "1/8"};
        ImGui::SetNextItemWidth(w);
        if (ui::ComboRaw(ui::detail::label(msg::opt_resolution), &ds_idx, items, 4)) {
            const float vals[] = {0.0f, 2.0f, 4.0f, 8.0f};
            _cfg.train_resolution_divisor = vals[ds_idx];
            _cfg_ui.touched.insert("train_resolution_divisor");
        }
    }
    ui::help_on_hover_disabled(msg::opt_resolution_help);
#if 0
    ImGui::SetNextItemWidth(w);
    if (ui::InputInt(fld::max_resolution, &_cfg.max_resolution)) {
        _cfg.max_resolution = std::clamp(_cfg.max_resolution, 0, 32768);
        _cfg_ui.touched.insert("max_resolution");
    }
    ui::help_on_hover_disabled(fld::max_resolution_help);
#endif

#if 0
    if (ui::Checkbox(fld::progressive_resolution, &_cfg.progressive_resolution))
        _cfg_ui.touched.insert("progressive_resolution");
    ui::help_on_hover_disabled(fld::progressive_resolution_help);
    if (_cfg.progressive_resolution) {
        ImGui::Indent();
        // A manual schedule replaces the automatic stages these two shape.
        ImGui::BeginDisabled(!_cfg.progressive_resolution_schedule.empty());
        int start = _cfg.progressive_resolution_start == 2 ? 0 : _cfg.progressive_resolution_start == 8 ? 2 : 1;
        const char* starts[] = {"1/2", "1/4", "1/8"};
        ImGui::SetNextItemWidth(w);
        if (ui::ComboRaw(ui::detail::label(fld::progressive_resolution_start), &start, starts, 3)) {
            _cfg.progressive_resolution_start = 2 << start;
            _cfg_ui.touched.insert("progressive_resolution_start");
        }
        ui::help_on_hover_disabled(fld::progressive_resolution_start_help);
        ImGui::SetNextItemWidth(w);
        if (ui::SliderFloat(fld::progressive_resolution_full_at, &_cfg.progressive_resolution_full_at, 0.05f, 1.0f, "%.2f"))
            _cfg_ui.touched.insert("progressive_resolution_full_at");
        ui::help_on_hover_disabled(fld::progressive_resolution_full_at_help);
        ImGui::EndDisabled();
        ImGui::SetNextItemWidth(w);
        if (ui::InputText(fld::progressive_resolution_schedule, &_cfg.progressive_resolution_schedule))
            _cfg_ui.touched.insert("progressive_resolution_schedule");
        ui::help_on_hover_disabled(fld::progressive_resolution_schedule_help);

        if (ui::Checkbox(fld::progressive_splat_budget, &_cfg.progressive_splat_budget))
            _cfg_ui.touched.insert("progressive_splat_budget");
        ui::help_on_hover_disabled(fld::progressive_splat_budget_help);
        if (_cfg.progressive_splat_budget) draw_splat_budget(w);
        ImGui::Unindent();
    }
#endif

    ImGui::SetNextItemWidth(w);
    if (ui::SliderInt(msg::opt_sh_degree, &_cfg.sh_degree, 0, 4))
        _cfg_ui.touched.insert("sh_degree");
    ui::help_on_hover_disabled(msg::opt_sh_degree_help);

// #ifdef SS_BACKEND_VULKAN
#if 0
    // Shows the SH degree the lobes leave (train_resolve_macros), rather
    // than the one the run would quietly replace.
    ImGui::SetNextItemWidth(w);
    if (ui::SliderInt(fld::sb_lobes, &_cfg.sb_lobes, 0,
                      ColorBasis::kMaxSbLobes)) {
        _cfg_ui.touched.insert("sb_lobes");
        if (!_cfg_ui.touched.count("sh_degree"))
            _cfg.sh_degree = _cfg.sb_lobes > 0 ? 0 : _defaults.sh_degree;
    }
    ui::help_on_hover_disabled(fld::sb_lobes_help);
#endif

    if (ui::Checkbox(msg::opt_bilateral_grid, &_cfg.use_bilateral_grid))
        _cfg_ui.touched.insert("use_bilateral_grid");
    ui::help_on_hover_disabled(msg::opt_bilateral_grid_help);

    if (ui::Checkbox(msg::opt_ppisp, &_cfg.use_ppisp))
        _cfg_ui.touched.insert("use_ppisp");
    ui::help_on_hover_disabled(msg::opt_ppisp_help);

    macro_option("calib_correction", _cfg.calib_correction, fld::calib_correction,
                 fld::calib_correction_help, {"off", "basic", "accurate"});

    ImGui::Spacing();

    // The two "if the capture went wrong" dials, last because a first run
    // should leave them alone and come back to them only if the result has
    // the problem they name.
    macro_option("floater_suppression", _cfg.floater_suppression,
                 fld::floater_suppression, fld::floater_suppression_help,
                 {"off", "mild", "moderate", "strong"});
#if 0
    macro_option("distraction_robustness", _cfg.distraction_robustness,
                 fld::distraction_robustness, fld::distraction_robustness_help,
                 {"off", "mild", "strong"});
    if (_cfg.distraction_robustness != "off")
        ui::TextColoredWrapped(kWarn, msg::opt_distraction_warn);
#endif

    ImGui::Spacing();

    macro_option("background_mode", _cfg.background_mode, fld::background_mode,
                 fld::background_mode_help, {"black", "color", "skybox"});
    if (_cfg.background_mode == "color") {
        ImGui::Indent();
        ImGui::PushID("background_color");
        if (gui::draw_color_field(_cfg.background_color))
            _cfg_ui.touched.insert("background_color");
        ImGui::PopID();
        ui::help_on_hover_disabled(fld::background_color_help);
        ImGui::Unindent();
    }
    macro_option("background_transparency_suppression",
                 _cfg.background_transparency_suppression,
                 fld::background_transparency_suppression,
                 fld::background_transparency_suppression_help,
                 {"none", "noise", "pseudorandom", "random"});
    draw_background_warning();

    draw_dataset_aux(w);
}

void GuiApp::draw_train_controls() {
    TrainRunner::Phase ph = _runner.phase();
    switch (ph) {
        case TrainRunner::Phase::Idle:
        case TrainRunner::Phase::Ready:
        case TrainRunner::Phase::LoadError:
        case TrainRunner::Phase::Done:
        case TrainRunner::Phase::TrainError: {
            if (ph == TrainRunner::Phase::TrainError)
                ui::TextColoredWrappedRaw(kErr, _runner.error());
            if (ph == TrainRunner::Phase::Done) {
                const bool saved = _runner.saved_on_stop();
                ui::TextColored(saved ? kOk : kWarn,
                                saved ? msg::training_complete
                                      : msg::training_stopped_unsaved);
                if (auto* s = _runner.session()) {
                    ImGui::PushTextWrapPos();
                    ui::TextDisabled(saved ? msg::saved_to : msg::unsaved_note,
                                     {s->out_dir.string()});
                    ImGui::PopTextWrapPos();
                }
            }
            // A batch owns the runner between its tasks, so the queue's own
            // next row is what starts -- never a click here.
            bool can_start = !_batch_active && !native_work_busy() &&
                             (ph == TrainRunner::Phase::Ready ||
                              ph == TrainRunner::Phase::Done ||
                              ph == TrainRunner::Phase::TrainError);
            ImGui::BeginDisabled(!can_start);
            if (ui::Button(ph == TrainRunner::Phase::Done ? msg::train_again
                                                          : msg::start_training,
                           ImVec2(-8, 36)))
                start_training();
            ImGui::EndDisabled();
            // What a finished run is for: a picture of it, or a cleaned copy.
            if (ph == TrainRunner::Phase::Done && _runner.saved_on_stop() &&
                _runner.session() && !_batch_active) {
                const std::string out = _runner.session()->out_dir.string();
                const float half = (ImGui::GetContentRegionAvail().x - 8.0f -
                                    ImGui::GetStyle().ItemSpacing.x) * 0.5f;
                if (ui::Button(rmsg::train_render, ImVec2(half, 30))) {
                    _render_after_open = true;
                    request_open_splat(out);
                }
                ui::help_on_hover(rmsg::train_render_help);
                ImGui::SameLine();
                if (ui::Button(rmsg::train_edit, ImVec2(half, 30))) {
                    _edit_after_open = true;
                    request_open_splat(out);
                }
                ui::help_on_hover(rmsg::train_edit_help);
            }
            break;
        }
        case TrainRunner::Phase::Loading:
            ui::TextColored(kDim, msg::parsing_dataset);
            break;
        case TrainRunner::Phase::Preparing: {
            const std::string fetching = _runner.fetch_line();
            if (fetching.empty()) ui::TextColored(kDim, msg::preparing_engine);
            else ui::TextColoredWrappedRaw(kDim, fetching);
            break;
        }
        case TrainRunner::Phase::Evaluating: {
            ui::TextColored(kDim, msg::status_evaluating);
            const bool stopping = _runner.session() &&
                                  _runner.session()->stop_requested.load();
            ImGui::BeginDisabled(stopping);
            if (ui::Button(stopping ? msg::stopping : msg::stop, ImVec2(-8, 32))) {
                _pending = Pending::StopHere;
                _open_confirm = true;
            }
            ImGui::EndDisabled();
            ui::help_on_hover(msg::stop_help);
            break;
        }
        case TrainRunner::Phase::Training: {
            bool paused = _runner.paused();
            // While a batch runs, stopping is the batch panel's business --
            // "stop and save" here would end one job and silently start the
            // next, which is not what anybody pressing it means.
            float half = _batch_active
                             ? -8.0f
                             : (ImGui::GetContentRegionAvail().x - 12) * 0.5f;
            if (ui::Button(paused ? msg::resume : msg::pause, ImVec2(half, 32)))
                _runner.set_paused(!paused);
            if (_batch_active) break;
            ImGui::SameLine();
            bool stopping = _runner.session() &&
                            _runner.session()->stop_requested.load();
            ImGui::BeginDisabled(stopping);
            // Asks first: whether the run's last minutes reach the disk is
            // the one thing about stopping that cannot be undone afterwards.
            if (ui::Button(stopping ? msg::stopping : msg::stop,
                           ImVec2(half, 32))) {
                _pending = Pending::StopHere;
                _open_confirm = true;
            }
            ImGui::EndDisabled();
            ui::help_on_hover(msg::stop_help);
            break;
        }
    }
}

void GuiApp::draw_metrics() {
    static std::vector<TrainRunner::MetricPoint> pts;
    _runner.get_metrics(pts);
    if (pts.empty()) return;

    ui::SeparatorText(msg::section_metrics);
    int stride = std::max<size_t>(1, pts.size() / 240);
    static std::vector<float> steps, values;
    steps.clear();
    values.clear();
    auto value = [&](const TrainRunner::MetricPoint& p) {
        switch (_plot_metric) {
            case PlotMetric::Ssim: return p.ssim;
            case PlotMetric::Loss: return p.rgb_loss;
            case PlotMetric::LogLoss: return p.rgb_loss;
            default: return p.psnr;
        }
    };
    auto take = [&](size_t i) {
        steps.push_back((float)pts[i].step);
        values.push_back(value(pts[i]));
    };
    size_t last_i = 0;
    for (size_t i = 0; i < pts.size(); i += stride) { take(i); last_i = i; }
    // The stride can stop short of the newest point, which on a step axis
    // leaves the curve ending before the score printed beside it.
    if (last_i != pts.size() - 1) take(pts.size() - 1);
    const auto& last = pts.back();
    ui::PlotLinesRaw(steps.data(), values.data(), (int)values.size(),
                     ImVec2(-8, px(64.0f)), _plot_metric == PlotMetric::LogLoss);
    {
        char v[128];
        const char* label = _plot_metric == PlotMetric::Psnr ? "PSNR"
                          : _plot_metric == PlotMetric::Ssim ? "SSIM"
                          : msg::metric_loss.get();
        std::snprintf(v, sizeof v, "%s %.*f", label,
                      _plot_metric == PlotMetric::Psnr ? 2 : 4, value(last));
        const ImVec2 lo = ImGui::GetItemRectMin();
        const ImVec2 hi = ImGui::GetItemRectMax();
        const ImVec2 sz = ImGui::CalcTextSize(v);
        const float pad = ImGui::GetStyle().FramePadding.y;
        const ImVec2 at(std::max(lo.x, (lo.x + hi.x - sz.x) * 0.5f),
            _plot_metric == PlotMetric::Loss || _plot_metric == PlotMetric::LogLoss ?
                lo.y : std::max(lo.y, hi.y - sz.y - pad));
        ImGui::GetWindowDrawList()->AddText(
            at, ImGui::GetColorU32(ImGuiCol_Text), v);
    }
    if (ImGui::BeginPopupContextItem("##training_metric")) {
        if (ui::MenuItemRaw("PSNR", _plot_metric == PlotMetric::Psnr))
            _plot_metric = PlotMetric::Psnr;
        if (ui::MenuItemRaw("SSIM", _plot_metric == PlotMetric::Ssim))
            _plot_metric = PlotMetric::Ssim;
        if (ui::MenuItem(msg::metric_loss, nullptr, _plot_metric == PlotMetric::Loss))
            _plot_metric = PlotMetric::Loss;
        if (ui::MenuItem(msg::metric_loss_log, nullptr, _plot_metric == PlotMetric::LogLoss))
            _plot_metric = PlotMetric::LogLoss;
        ImGui::EndPopup();
    }
    char psnr[96];
    std::snprintf(psnr, sizeof psnr, "%.2f", last.psnr);
    char ssim[96];
    std::snprintf(ssim, sizeof ssim, "%.3f", last.ssim);
    char loss[96];
    std::snprintf(loss, sizeof loss, "%.4f", last.rgb_loss);
    ui::TextWrapped(msg::status_metrics,
                    {format_count(last.num_splats), psnr, ssim, loss});
}

// The bar is the trainer's own time forecast, so a run whose steps get dearer
// as the model grows does not crawl through its last half.
void GuiApp::tick_train_clock() {
    const TrainRunner::Phase ph = _runner.phase();
    const spirula::TrainerProgress p = _runner.latest_progress();
    const double now = progress_now();
    if (ph == TrainRunner::Phase::Training && p.total_steps > 0) {
        const double elapsed = _runner.elapsed_seconds();
        if (!_train_clock.started() || elapsed + 1.0 < _train_clock.elapsed(now))
            _train_clock.start(now - elapsed);
        _train_clock.work(now, (double)(p.step + 1) / (double)p.total_steps,
                          _runner.eta_seconds());
    } else if (ph == TrainRunner::Phase::Done && _train_clock.started()) {
        _train_clock.finish(now);
    }
}

void GuiApp::draw_status_strip() {
    // Strip content region, captured before any widget consumes it -- used to
    // right-align the VRAM readout on the second (text) row.
    const float x0 = ImGui::GetCursorPosX();
    const float avail = ImGui::GetContentRegionAvail().x;

    TrainRunner::Phase ph = _runner.phase();
    spirula::TrainerProgress p = _runner.latest_progress();
    const double now = progress_now();
    if (ph != TrainRunner::Phase::Evaluating) _eval_clock = ProgressClock{};
    tick_train_clock();
    if (ph == TrainRunner::Phase::Training && p.total_steps > 0) {
        ui::ProgressBar(_train_clock.fraction(), ImVec2(-8, 0), msg::status_step,
                        {p.step + 1, p.total_steps, (int)(_train_clock.fraction() * 100.0f)});
        const double avg = _runner.avg_step_latency();
        char ms[32];
        std::snprintf(ms, sizeof ms, "%.0f",
                      (avg >= 0.0 ? avg : p.step_latency) * 1000.0);
        ui::Text(_runner.paused() ? msg::status_rate_paused : msg::status_rate,
                 {ms, format_duration_coarse(_train_clock.elapsed(now)),
                  format_duration_coarse(_train_clock.remaining(now)),
                  format_count((double)p.num_splats)});
        draw_timing_link();
    } else if (ph == TrainRunner::Phase::Done && p.total_steps > 0) {
        ui::ProgressBar(1.0f, ImVec2(-8, 0), msg::status_done_steps,
                        {p.step + 1, format_duration_coarse(_runner.elapsed_seconds())});
        ui::TextDisabled(msg::status_explore);
        draw_timing_link();
    } else if (ph == TrainRunner::Phase::Preparing) {
        const std::string fetching = _runner.fetch_line();
        const float frac = _runner.fetch_progress();
        if (fetching.empty())
            ui::ProgressBar(-(float)ImGui::GetTime(), ImVec2(-8, 0), msg::status_preparing);
        else
            ui::ProgressBarRaw(frac >= 0.0f ? frac : -(float)ImGui::GetTime(), ImVec2(-8, 0),
                               fetching.c_str());
        ui::TextDisabledRaw(" ");
    } else if (ph == TrainRunner::Phase::Evaluating) {
        const spirula::EvalProgress p = _runner.latest_eval_progress();
        if (!_eval_clock.started()) _eval_clock.start(now);
        if (p.total > 0) {
            _eval_clock.work(now, std::clamp((double)p.completed / (double)p.total, 0.0, 1.0));
            ui::ProgressBar(_eval_clock.fraction(), ImVec2(-8, 0), msg::status_eval_views,
                            {p.completed, p.total, (int)(_eval_clock.fraction() * 100.0f)});
        } else {
            ui::ProgressBar(-(float)ImGui::GetTime(), ImVec2(-8, 0),
                            msg::status_evaluating);
        }
        progress_time_line(_eval_clock);
    } else if (ph == TrainRunner::Phase::Ready) {
        ui::ProgressBar(0.0f, ImVec2(-8, 0), msg::status_ready);
        ui::TextDisabled(msg::status_ready_hint);
    } else {
        ui::ProgressBar(0.0f, ImVec2(-8, 0), msg::status_idle);
        ui::TextDisabledRaw(" ");
    }

    draw_vram_readout(x0, avail);
}

SavingsContext GuiApp::savings_context(const spirula::TrainerSession& session) const {
    SavingsContext ctx;
    ctx.frames_split = session.post.any_warp;
    ctx.sh_degree = session.cfg.sh_degree;
    if (ctx.frames_split && session.post.n_post > 0)
        ctx.images_kept =
            (double)session.post.K_per_camera.size() / (double)session.post.n_post;
    ctx.preset_360 = _train_presets.file.empty() && _preset == "360-camera";
    ctx.preset_360_label = preset_label("360-camera");
    ctx.preset_general_label = preset_label("3dgs");
    return ctx;
}

void GuiApp::draw_timing_link() {
#ifdef SS_BACKEND_VULKAN
    spirula::TrainerSession* session = _runner.engine_ready() ? _runner.session() : nullptr;
    if (!session) return;
    _timing_view.follow(session, session->cur_step.load());
    ImGui::SameLine();
    // A link rather than a third bar: the row is already the one that gives
    // way first, and the breakdown needs words to read anyway.
    if (ui::TextLink(msg::timing_details)) ImGui::OpenPopup("##step_timing");
    const bool pinned = ImGui::IsPopupOpen("##step_timing");
    if (!pinned &&
        ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_NoSharedDelay) &&
        ImGui::BeginTooltip()) {
        step_timing_card(session->time_breakdown(false), session->time_breakdown(true),
                         session->cfg, savings_context(*session), _timing_view, false);
        ImGui::EndTooltip();
    }
    if (ImGui::BeginPopup("##step_timing")) {
        step_timing_card(session->time_breakdown(false), session->time_breakdown(true),
                         session->cfg, savings_context(*session), _timing_view, true);
        ImGui::EndPopup();
    }
#endif
}

void GuiApp::draw_vram_readout(float x0, float avail) {
    // Poll the backend at ~2 Hz; the driver queries are cheap but there is no
    // reason to hit them every frame.
    double now = ImGui::GetTime();
    if (_vram_polled_at < 0.0 || now - _vram_polled_at > 0.5) {
        _vram = backend::memory_usage();
        _vram_polled_at = now;
    }
    const backend::MemoryUsage& m = _vram;

    // Nothing queryable (no device / all queries failed): stay silent rather
    // than show a confusing "n/a".
    if (!m.has_process && !m.has_used && !m.has_total) return;

    auto part = [](bool has, uint64_t bytes) {
        return has ? format_gib(bytes) : std::string("?");
    };

    // A bar, because what matters here is a proportion: how close the device
    // is to full, and how much of that is this program rather than everything
    // else on the card. Three numbers in a row said neither without arithmetic.
    const bool sized = m.has_total && m.total_bytes > 0;
    const double total = sized ? (double)m.total_bytes : 0.0;
    const double used = m.has_used ? (double)m.used_bytes : (double)m.process_bytes;
    const double proc = m.has_process ? (double)m.process_bytes : 0.0;
    const float used_f = sized ? (float)std::min(used / total, 1.0) : 0.0f;
    const float proc_f = sized ? (float)std::min(proc / total, (double)used_f) : 0.0f;

    // Pressure is a property of the whole device, so the fill colour follows
    // system-wide use even though the bright segment is this process's share.
    ImVec4 color = kDim;
    if (sized && m.has_used)
        color = used_f >= 0.9f ? kErr : used_f >= 0.7f ? kWarn : kOk;

    // The same three numbers vram_help names, in that order: what this run
    // costs is the one a user is deciding on, and it is not recoverable from
    // the other two.
    const std::string label =
        sized ? part(m.has_process, m.process_bytes) + " / " +
                    part(m.has_used, m.used_bytes) + " / " +
                    format_gib(m.total_bytes) + " GiB"
              : "VRAM " + part(m.has_process, m.process_bytes) + " GiB";

    // A run in progress (or just finished) has a forecast: its risk goes in
    // front of the bar, and hovering either shows the projection behind it.
    spirula::TrainerSession* session = nullptr;
    {
        const TrainRunner::Phase ph = _runner.phase();
        if ((ph == TrainRunner::Phase::Training || ph == TrainRunner::Phase::Evaluating ||
             ph == TrainRunner::Phase::Done) &&
            _runner.engine_ready())
            session = _runner.session();
    }
    const spirula::VramForecast brief =
        session ? session->forecast().vram(false) : spirula::VramForecast{};
    const spirula::i18n::Msg* risk = session ? oom_risk_label(brief.risk) : nullptr;
    auto hover = [&] {
        if (!session) { ui::help_on_hover(msg::vram_help); return; }
        if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort |
                                  ImGuiHoveredFlags_NoSharedDelay) ||
            !ImGui::BeginTooltip())
            return;
        ImGui::PushTextWrapPos(px(480.0f));
        ui::TextDisabled(msg::vram_help);
        ImGui::PopTextWrapPos();
        ImGui::Separator();
        vram_forecast_card(session->forecast().vram(), session->cfg.num_iterations,
                           savings_context(*session));
        ImGui::EndTooltip();
    };

    const ImGuiStyle& st = ImGui::GetStyle();
    float text_w = ImGui::CalcTextSize(label.c_str()).x;
    if (risk) text_w += ImGui::CalcTextSize(risk->get()).x + st.ItemSpacing.x;
    ImGui::SameLine();
    // The bar is the first thing to give when the row is short -- the numbers
    // beside it say everything it does. Without this the readout ran off the
    // right edge of a narrow panel, or of any panel at a large interface size.
    float bar_w = sized ? px(120.0f) : 0.0f;
    float gap = sized ? st.ItemInnerSpacing.x : 0.0f;
    float target = x0 + avail - bar_w - gap - text_w - px(8.0f);
    if (target <= ImGui::GetCursorPosX()) {
        bar_w = gap = 0.0f;
        target = x0 + avail - text_w - px(8.0f);
    }
    if (target > ImGui::GetCursorPosX()) ImGui::SetCursorPosX(target);

    if (risk) {
        ui::TextColored(oom_risk_color(brief.risk), *risk);
        hover();
        ImGui::SameLine();
    }
    if (bar_w > 0.0f) {
        const float h = ImGui::GetTextLineHeight();
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float r = st.FrameRounding;
        dl->AddRectFilled(p, ImVec2(p.x + bar_w, p.y + h),
                          ImGui::GetColorU32(ImGuiCol_FrameBg), r);
        // Everything in use, dim; this process's share of it, solid on top.
        ImVec4 rest = color;
        rest.w = 0.35f;
        if (used_f > 0.0f)
            dl->AddRectFilled(p, ImVec2(p.x + bar_w * used_f, p.y + h),
                              ImGui::GetColorU32(rest), r);
        if (proc_f > 0.0f)
            dl->AddRectFilled(p, ImVec2(p.x + bar_w * proc_f, p.y + h),
                              ImGui::GetColorU32(color), r);
        dl->AddRect(p, ImVec2(p.x + bar_w, p.y + h),
                    ImGui::GetColorU32(ImGuiCol_Border), r);
        ui::InvisibleButtonRaw("##vram", ImVec2(bar_w, h));
        hover();
        ImGui::SameLine(0.0f, gap);
    }
    ui::TextColoredRaw(sized ? kDim : color, label);
    hover();
}

}  // namespace gui
