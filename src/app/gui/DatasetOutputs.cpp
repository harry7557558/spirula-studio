// A finished dataset's outputs: splat budget, open buttons, partition and ROI.

#include "app/gui/GuiApp.h"

#include "app/gui/DatasetPrep.h"
#include "app/gui/GuiAppShared.h"
#include "app/gui/MaskPrompt.h"
#include "app/gui/Ui.h"
#include "data/ResolutionSchedule.h"
#include "data/RoiDocument.h"
#include "data/SeedPointCount.h"
#include "dense/Artifact.h"
#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/Dense.h"
#include "i18n/catalog/Edit.h"
#include "i18n/catalog/Gui.h"
#include "i18n/catalog/MaskEdit.h"
#include "i18n/catalog/Partition.h"
#include "i18n/catalog/Roi.h"
#include "i18n/catalog/TrainFields.h"

#include "imgui.h"

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;
namespace i18n = spirula::i18n;
namespace msg = spirula::i18n::msg::gui;
namespace emsg = spirula::i18n::msg::edit;
namespace fld = spirula::i18n::msg::field;
namespace dmsg = spirula::i18n::msg::dataset;
namespace mmsg = spirula::i18n::msg::maskedit;

namespace gui {

GuiApp::DatasetFolders GuiApp::workspace_folders(const WorkspaceState& prior) const {
    DatasetFolders f;
    f.dir = _workspace;
    f.image_dir = planned_image_dir(_sources, _workspace, _photo_import);
    std::error_code ec;
    if (prior.masks) {
        // The run's own masks/, which every branch of DatasetPrep writes in
        // the app's convention -- only a bundled folder is ever left flipped.
        f.mask_dir = (fs::path(_workspace) / "masks").string();
    } else if (prior.input_masks && _sources.size() == 1 && !_sources[0].is_video &&
               _sources[0].packed_lenses == 0 && !_sources[0].heif) {
        // Masks that came with the photos pair with them where they lie.
        f.image_dir = fs::absolute(_sources[0].path, ec).string();
        f.mask_dir = fs::absolute(_sources[0].mask_dir, ec).string();
        f.mask_flipped = _flip_found_masks;
    }
    return f;
}

// -1 leaves the automatic budget waiting for a parsed dataset.
int64_t GuiApp::starting_points() {
    std::error_code ec;
    const std::string seed = _cfg.seed_pointcloud.empty() && dense_cloud_present()
        ? spirula::dense::default_cloud(_cfg.data).ply.string()
        : _cfg.seed_pointcloud == spirula::dense::kSparseSeed ? std::string() : _cfg.seed_pointcloud;
    if (!seed.empty()) {
        fs::path p = seed;
        if (p.is_relative()) p = fs::path(_cfg.data) / p;
        p = spirula::seed_count_file(p);
        const auto time = fs::last_write_time(p, ec);
        if (ec) return -1;
        if (p.string() != _seed_count_path || time != _seed_count_time) {
            _seed_count_path = p.string();
            _seed_count_time = time;
            _seed_count = spirula::seed_point_count(p);
        }
        return _seed_count;
    }
    const auto ph = _runner.phase();
    if (const auto* s = _runner.session();
        s && (ph == TrainRunner::Phase::Ready || ph == TrainRunner::Phase::Training ||
              ph == TrainRunner::Phase::Evaluating || ph == TrainRunner::Phase::Done))
        return s->ds.points.num();
    return -1;
}

void GuiApp::draw_splat_budget(float w) {
    std::string& text = _cfg.progressive_splat_budget_schedule;
    progressive::Setpoints stages;
    try {
        stages = _cfg.progressive_resolution_schedule.empty()
            ? progressive::automatic_setpoints(_cfg.progressive_resolution_start,
                                               _cfg.progressive_resolution_full_at, _cfg.num_iterations)
            : progressive::parse_setpoints(_cfg.progressive_resolution_schedule);
    } catch (const std::exception&) { return; }   // the resolution fields above show the error
    const int64_t seed = starting_points();
    const int64_t cap = std::max(1, _cfg.cap_max);

    int mode = text.empty() ? 0 : 1;
    ImGui::SetNextItemWidth(w);
    if (ui::Combo(fld::progressive_splat_budget_mode, &mode,
                  {&fld::progressive_splat_budget_auto, &fld::progressive_splat_budget_manual})) {
        // Manual starts from the automatic values, so switching changes nothing yet.
        text.clear();
        if (mode == 1)
            for (const auto& [step, splats] : progressive::automatic_budget(stages, std::max<int64_t>(seed, 0), cap))
                text += (text.empty() ? "" : ", ") + std::to_string(splats);
        _cfg_ui.touched.insert("progressive_splat_budget_schedule");
    }
    ui::help_on_hover(fld::progressive_splat_budget_help);

    if (mode == 1 && text.find(':') != std::string::npos) {
        // A step:splats schedule from the command line or an older config.
        ImGui::SetNextItemWidth(w);
        if (ui::InputText(fld::progressive_splat_budget_schedule, &text))
            _cfg_ui.touched.insert("progressive_splat_budget_schedule");
        ui::help_on_hover(fld::progressive_splat_budget_schedule_help);
    } else if (mode == 1) {
        std::vector<std::string> boxes(1);
        for (char c : text) {
            if (c == ',') boxes.emplace_back();
            else boxes.back() += c;
        }
        boxes.resize(stages.size());
        bool changed = false;
        for (size_t k = 0; k < boxes.size(); ++k) {
            const std::string label = stages[k].second > 1
                ? i18n::format(fld::progressive_splat_budget_stage, {(long long)stages[k].second})
                : fld::progressive_splat_budget_stage_full.get();
            std::string box = boxes[k];
            while (!box.empty() && box.front() == ' ') box.erase(box.begin());
            ImGui::SetNextItemWidth(w);
            if (ui::InputTextWithHintRaw((label + "##splat_budget" + std::to_string(k)).c_str(),
                                         fld::progressive_splat_budget_hint, &box)) {
                boxes[k] = box;
                changed = true;
            }
            ui::help_on_hover(fld::progressive_splat_budget_schedule_help);
        }
        if (changed) {
            std::string joined;
            for (size_t k = 0; k < boxes.size(); ++k) {
                std::string b = boxes[k];
                while (!b.empty() && b.front() == ' ') b.erase(b.begin());
                joined += (k ? ", " : "") + b;
            }
            // Never empty, or the mode would read as automatic.
            text = joined.find_first_not_of(" ,") == std::string::npos && boxes.size() == 1 ? " " : joined;
            _cfg_ui.touched.insert("progressive_splat_budget_schedule");
        }
    }

    // What the run will follow, before it starts.
    std::string plan;
    try {
        for (const auto& [step, splats] : progressive::resolve_budget(stages, text, std::max<int64_t>(seed, 0), cap))
            plan += (plan.empty() ? "" : "  >  ") + format_count((double)splats);
        plan = i18n::format(fld::progressive_splat_budget_plan, {plan});
    } catch (const std::exception& e) {
        plan = e.what();
    }
    ui::TextColoredWrappedRaw(kDim, plan);
    if (mode == 0) {
        if (seed >= 0)
            ui::TextColoredWrappedRaw(kDim, i18n::format(fld::progressive_splat_budget_seed,
                                                         {format_count((double)seed), format_count((double)cap)}));
        else
            ui::TextColoredWrappedRaw(kDim, fld::progressive_splat_budget_seed_unknown.get());
    }
}

void GuiApp::draw_dataset_open_buttons(const DatasetFolders& f, bool model) {
    if (model) {
        if (ui::Button(dmsg::open_in_trainer)) {
            if (training_busy()) {
                _pending = Pending::OpenDataset;
                _pending_path = f.dir;
                _open_confirm = true;
            } else {
                open_dataset(f.dir, f.image_dir, f.mask_dir, f.mask_flipped,
                             /*keep_log=*/true);
            }
        }
        // Cleaning the seed cloud belongs here rather than after training: a
        // floater removed now is one the run never fits to.
        ImGui::SameLine();
        if (ui::Button(emsg::sparse_edit)) {
            _edit_after_open = true;
            _sparse_edit_src = f;
            request_open_splat(f.dir);
        }
        ui::help_on_hover(emsg::sparse_edit_help);
        refresh_dense_versions(f.dir);
        if (!_dense_versions.empty()) {
            ImGui::SameLine();
            if (ui::Button(spirula::i18n::msg::dense::edit_cloud)) edit_dense_cloud(f);
            ui::help_on_hover(spirula::i18n::msg::dense::edit_cloud_help);
        }
        ImGui::SameLine();
        if (ui::Button(spirula::i18n::msg::partition::open_button)) open_partition_panel(f);
        ui::help_on_hover(spirula::i18n::msg::partition::open_button_help);
        ImGui::SameLine();
        if (ui::Button(spirula::i18n::msg::roi::editor_button)) open_roi_editor(f.dir);
        ui::help_on_hover(spirula::i18n::msg::roi::editor_button_help);
    }
    if (!f.mask_dir.empty()) {
        if (model) ImGui::SameLine();
        ImGui::BeginDisabled(dataset_busy() || native_work_busy());
        if (ui::Button(mmsg::correct_masks)) {
            // A run reports its folders as the config spells them: relative to
            // the dataset, and empty for the parser's default.
            TrainConfig stock;
            auto under = [&](const std::string& d, const std::string& fallback) {
                return (fs::path(f.dir) / (d.empty() ? fallback : d)).lexically_normal().string();
            };
            open_mask_editor(f.dir, under(f.image_dir, stock.image_dir),
                             under(f.mask_dir, stock.mask_dir), f.mask_flipped);
        }
        ImGui::EndDisabled();
        ui::help_on_hover(mmsg::correct_masks_help);
    }
}

void GuiApp::open_partition_panel(const DatasetFolders& f) {
    if (dataset_busy() || native_work_busy()) return;
    PartitionPanel::Hooks hooks;
    hooks.log = [this](const std::string& s) { log(s); };
    hooks.open_splat = [this](const std::string& path) { request_open_splat(path); };
    hooks.queue_batch = [this, f](const std::string& partition, int n) {
        open_partition_queue(f, partition, n);
        return 0;
    };
    hooks.open_batch = [this] {
        if (!native_work_busy()) _screen = Screen::Batch;
    };
    _partition_panel.open(f.dir, std::move(hooks));
}

void GuiApp::open_roi_editor(const std::string& dataset, const std::string& file) {
    // A reconstruction in flight is rewriting the model the editor would read.
    if (dataset_busy()) return;
    RoiEditor::Hooks hooks;
    hooks.log = [this](const std::string& s) { log(s); };
    // The training screen's preview shows the region the run would use, so a
    // save re-reads it -- unless a run owns the session.
    hooks.changed = [this](const std::string& ds) {
        _roi_files_for.clear();
        std::error_code ec;
        const TrainRunner::Phase ph = _runner.phase();
        if (!_cfg.data.empty() && fs::equivalent(ds, _cfg.data, ec) &&
            (ph == TrainRunner::Phase::Ready || ph == TrainRunner::Phase::LoadError))
            _parse_dirty = true;
    };
    _roi_editor.open(dataset, std::move(hooks), file);
}

// Which saved region the run trains in: the first by default, any other, or
// none. What it writes is --roi-region's own spelling, so a batch row and the
// command line read it the same way. Hidden until there is a region to choose.
void GuiApp::draw_roi_row(bool busy) {
    namespace roimsg = spirula::i18n::msg::roi;
    if (_cfg.data.empty()) return;
    if (_roi_files_for != _cfg.data) {
        _roi_files_for = _cfg.data;
        _roi_files = spirula::list_roi_files(_cfg.data);
    }
    const spirula::RoiChoice now = spirula::resolve_roi_setting(_cfg.roi_region, _cfg.data);
    // A region given by path still shows, so it can be turned off.
    if (_roi_files.empty() && now.path.empty()) return;
    auto stem = [](const std::string& p) { return fs::path(p).stem().string(); };
    const std::string shown = now.off ? std::string(roimsg::train_none.get()) : stem(now.path);
    ui::Text(roimsg::train_label);
    ui::help_on_hover(roimsg::train_help);
    ImGui::BeginDisabled(busy);
    const float edit_w = ImGui::CalcTextSize(roimsg::train_edit_region.get()).x +
                         2 * ImGui::GetStyle().FramePadding.x + ImGui::GetStyle().ItemSpacing.x;
    ImGui::SetNextItemWidth(std::max(px(120.0f), ImGui::GetContentRegionAvail().x - edit_w - px(8.0f)));
    std::string pick;
    bool picked = false;
    if (ui::BeginComboRaw("##roirow", shown.c_str())) {
        _roi_files = spirula::list_roi_files(_cfg.data);
        for (size_t i = 0; i < _roi_files.size(); i++) {
            ImGui::PushID((int)i);
            std::error_code ec;
            const bool on = !now.off && fs::equivalent(now.path, _roi_files[i], ec);
            if (ui::SelectableRaw(stem(_roi_files[i]), on) && !on) {
                pick = "roi/" + fs::path(_roi_files[i]).filename().generic_string();
                picked = true;
            }
            ImGui::PopID();
        }
        if (ui::Selectable(roimsg::train_none, now.off)) {
            pick = "off";
            picked = true;
        }
        ImGui::EndCombo();
    }
    if (picked && pick != _cfg.roi_region) {
        _cfg.roi_region = pick;
        _cfg_ui.touched.insert("roi_region");
        _parse_dirty = true;
    }
    ImGui::SameLine();
    if (ui::Button(roimsg::train_edit_region))
        open_roi_editor(_cfg.data, !now.off && !now.path.empty() ? now.path : std::string());
    ImGui::EndDisabled();
}

int GuiApp::add_batch_partition_rows(const DatasetFolders& f, const std::string& partition,
                                     int num_parts) {
    for (int k = 0; k < num_parts; k++) {
        BatchRow r;
        r.dataset = f.dir;
        r.image_dir = f.image_dir;
        r.mask_dir = f.mask_dir;
        r.mask_flipped = f.mask_flipped;
        r.partition = partition;
        r.partition_part = k;
        r.does(BatchStage::Train) = true;
        r.runs.push_back(_pq.run);
        _batch.push_back(std::move(r));
    }
    _batch_open_row = -1;
    batch_edited();
    return num_parts;
}

void GuiApp::open_mask_editor(const std::string& workspace, const std::string& image_dir,
                              const std::string& mask_dir, bool mask_flipped) {
    if (dataset_busy() || native_work_busy()) return;
    close_native_previews();
    _mask_editor.set_log([this](const std::string& s) { log(s); });
    // The editor's own checkpoint, over the app's one download.
    _mask_editor.set_model_picker([this] {
        draw_mask_model_picker(_mask_editor_model_id, nullptr, _download,
                               [this] { request_model_download(_mask_editor_model_id, ""); });
    });
    // SAM loads on the device every other inference user freezes, never nn's
    // default; a failed freeze refuses the prompt with the same sentence.
    _mask_editor.set_sam_device_gate([this](std::string& device, std::string& error) {
        if (!freeze_native_device()) {
            error = _native_device_error.empty() ? msg::no_device_found.get()
                                                 : _native_device_error;
            return false;
        }
        device = _native_device_uuid;
        return true;
    });
    std::string err;
    if (!_mask_editor.open(workspace, image_dir, mask_dir, mask_flipped, err)) log(err);
}

}  // namespace gui
