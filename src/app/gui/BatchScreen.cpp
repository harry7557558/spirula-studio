// The Batch screen: rows, runs, the plan preview, progress and the finish
// command. The list stays readable while a batch is in flight, which is what
// makes an unattended run something you come back to rather than watch.

#include "app/gui/GuiApp.h"

#include "app/gui/GuiAppShared.h"
#include "app/gui/ProgressView.h"
#include "app/gui/Subprocess.h"
#include "app/gui/Ui.h"
#include "data/RoiDocument.h"
#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/Edit.h"
#include "i18n/catalog/Gui.h"
#include "i18n/catalog/Partition.h"
#include "i18n/catalog/Roi.h"
#include "i18n/catalog/Train.h"
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
using spirula::i18n::Msg;

namespace gui {

namespace {

// The one word a row reports: the worst thing that happened to its tasks.
BatchStatus row_status(const std::vector<BatchTask>& tasks, int row) {
    bool any = false, all_done = true, failed = false, running = false;
    bool stopped = false, skipped = false;
    for (const BatchTask& t : tasks) {
        if (t.row != row) continue;
        any = true;
        running = running || t.status == BatchStatus::Running;
        failed = failed || t.status == BatchStatus::Failed;
        stopped = stopped || t.status == BatchStatus::Stopped;
        skipped = skipped || t.status == BatchStatus::Skipped;
        all_done = all_done && t.status == BatchStatus::Done;
    }
    if (!any) return BatchStatus::Pending;
    if (running) return BatchStatus::Running;
    if (failed) return BatchStatus::Failed;
    if (stopped) return BatchStatus::Stopped;
    if (all_done) return BatchStatus::Done;
    if (skipped) return BatchStatus::Skipped;
    return BatchStatus::Pending;
}

void draw_status_word(BatchStatus st) {
    switch (st) {
        case BatchStatus::Running: ui::TextColored(kWarn, msg::batch_status_running); break;
        case BatchStatus::Done:    ui::TextColored(kOk, msg::batch_status_done); break;
        case BatchStatus::Failed:  ui::TextColored(kErr, msg::batch_status_failed); break;
        case BatchStatus::Skipped: ui::TextColored(kDim, msg::batch_status_skipped); break;
        case BatchStatus::Stopped: ui::TextColored(kDim, msg::batch_status_stopped); break;
        default:                   ui::TextColored(kDim, msg::batch_status_pending); break;
    }
}

}  // namespace


const Msg& GuiApp::batch_stage_name(BatchStage s) const {
    switch (s) {
        case BatchStage::Dataset: return msg::batch_stage_dataset;
        case BatchStage::Mesh:    return msg::batch_stage_mesh;
        case BatchStage::Merge:   return spirula::i18n::msg::partition::stage_merge;
        default:                  return msg::batch_stage_train;
    }
}

// What a collapsed row is about: the thing it works on, which is the only
// part of it worth reading at a glance.
std::string GuiApp::batch_row_summary(const BatchRow& row) const {
    if (row.does(BatchStage::Dataset) && !row.sources.empty()) {
        std::string s = row.sources[0];
        if (row.sources.size() > 1)
            s += " (+" + std::to_string(row.sources.size() - 1) + ")";
        return s;
    }
    if (row.does(BatchStage::Merge) && !row.partition.empty()) return row.partition;
    if (!row.dataset.empty()) {
        // The regions the runs name, each once; a run on the default adds none.
        std::string s = row.dataset;
        for (const BatchRun& run : row.runs) {
            if (run.roi_region.empty()) continue;
            const std::string tag =
                " [" + (run.roi_region == "off"
                            ? std::string(spirula::i18n::msg::roi::train_none.get())
                            : fs::path(run.roi_region).stem().string()) + "]";
            if (s.find(tag) == std::string::npos) s += tag;
        }
        return s;
    }
    if (!row.model.empty()) return row.model;
    return {};
}


void GuiApp::draw_batch() {
    // Keeps each kind's saved presets warm for the rows' pickers and their
    // tooltips (rate-limited inside, so this is a no-op most frames).
    refresh_presets();
    refresh_dataset_presets();
    refresh_mesh_presets();
    check_batch_if_stale();

    draw_screen_header(&msg::batch_title);

    ImGui::PushTextWrapPos();
    ui::TextDisabled(msg::batch_intro);
    ImGui::PopTextWrapPos();

    const float log_h = log_height(ImGui::GetContentRegionAvail().y);
    ImGui::BeginChild("##batchlist", ImVec2(0, body_height(log_h)));

    if (_batch_active) {
        draw_batch_progress();
        ImGui::Spacing();
    }
    draw_batch_rows();

    ImGui::Spacing();
    ImGui::BeginDisabled(_batch_active);
    if (ui::Button(msg::batch_add_create)) {
        _pick_row = -1;
        open_pick(PickAction::BatchSourceVideo, msg::batch_pick_source.get(),
                  FileDialog::Mode::File,
                  std::vector<std::string>(kVideoExtensions,
                                           kVideoExtensions + kNumVideoExtensions),
                  "", /*multi=*/true);
    }
    ui::help_on_hover(msg::batch_add_create_help);
    ImGui::SameLine();
    if (ui::Button(msg::batch_add_photos)) {
        _pick_row = -1;
        open_pick(PickAction::BatchSourceImages, msg::batch_pick_photos.get(),
                  FileDialog::Mode::Folder);
    }
    ImGui::SameLine();
    if (ui::Button(msg::batch_add_row)) {
        _pick_row = -1;   // append
        open_pick(PickAction::BatchDataset, msg::batch_pick_dataset.get(),
                  FileDialog::Mode::Folder);
    }
    ImGui::SameLine();
    if (ui::Button(msg::batch_add_mesh)) {
        _pick_row = -1;
        open_pick(PickAction::BatchModel, msg::batch_pick_model.get(),
                  FileDialog::Mode::Folder);
    }
    ImGui::SameLine();
    // The datasets already opened in the trainer, which is where a queue is
    // usually assembled from -- they are the ones already known to parse.
    if (ui::Button(msg::batch_add_recent)) ImGui::OpenPopup("##batchrecent");
    if (ImGui::BeginPopup("##batchrecent")) {
        const std::vector<std::string> datasets = _recent.paths(RecentKind::Dataset);
        if (datasets.empty()) {
            ui::TextDisabled(msg::batch_no_recent);
        } else {
            for (size_t i = 0; i < datasets.size(); i++) {
                ImGui::PushID((int)i);
                // A path is a path in every language.
                if (ui::SelectableRaw(datasets[i])) add_batch_row(datasets[i]);
                ImGui::PopID();
            }
        }
        ImGui::EndPopup();
    }

    ImGui::Spacing();
    // A batch's own dataset or meshing task is not something else.
    const bool busy_elsewhere =
        !_batch_active && (dataset_busy() || _mesh.busy());
    ImGui::BeginDisabled(_batch.empty());
    if (ui::Button(msg::batch_clear)) _batch_confirm = BatchConfirm::ClearList;
    ImGui::SameLine();
    {
        int done = 0;
        for (int i = 0; i < (int)_batch.size(); i++) done += batch_row_done(i);
        ImGui::BeginDisabled(done == 0);
        if (ui::Button(msg::batch_clear_done)) _batch_confirm = BatchConfirm::ClearDone;
        ui::help_on_hover(msg::batch_clear_done_help);
        ImGui::EndDisabled();
        int unchecked = 0;
        for (const BatchRow& r : _batch) unchecked += !r.enabled;
        ImGui::SameLine();
        ImGui::BeginDisabled(unchecked == 0);
        if (ui::Button(msg::batch_clear_unchecked)) _batch_confirm = BatchConfirm::ClearUnchecked;
        ui::help_on_hover(msg::batch_clear_unchecked_help);
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(busy_elsewhere);
    if (ui::Button(msg::batch_start, ImVec2(px(160.0f), 0)))
        request_start_batch(/*skip_invalid=*/false);
    // The way past a row that cannot be fixed right now. Only offered once a
    // check has actually found one, so it is never the first thing tried.
    int bad = 0, live = 0;
    if (_batch_checked)
        for (const BatchRow& r : _batch) {
            if (!r.enabled) continue;
            live++;
            bad += batch_has_error(r.issues) ? 1 : 0;
        }
    if (bad > 0 && bad < live) {
        ImGui::SameLine();
        if (ui::Button(msg::batch_start_skip))
            request_start_batch(/*skip_invalid=*/true);
    }
    ImGui::EndDisabled();
    if (busy_elsewhere) {
        ImGui::SameLine();
        ui::TextColored(kDim, msg::batch_busy_elsewhere);
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    if (!_batch_msg.empty())
        ui::TextColoredWrappedRaw(_batch_msg_err ? kErr : kOk, _batch_msg);
    if (_batch_save.failed())
        ui::TextColoredWrapped(kErr, emsg::save_failed, {_batch_save.error()});

    draw_batch_issues();
    draw_batch_command();
    draw_batch_plan();
    draw_batch_confirm_modal();
    ImGui::EndChild();

    draw_log_panel(log_h);
}

bool GuiApp::batch_row_done(int index) const {
    // The flag outlives the task list, which a row edit or a restart clears.
    if (index >= 0 && index < (int)_batch.size() && _batch[(size_t)index].done) return true;
    bool any = false;
    for (const BatchTask& t : _batch_tasks) {
        if (t.row != index) continue;
        if (t.status != BatchStatus::Done) return false;
        any = true;
    }
    return any;
}

void GuiApp::draw_batch_confirm_modal() {
    if (_batch_confirm != BatchConfirm::None && !_batch_confirm_shown) {
        ui::OpenPopup(msg::batch_confirm_title);
        _batch_confirm_shown = true;
    }
    if (!_batch_confirm_shown) return;
    if (!ui::BeginPopupModal(msg::batch_confirm_title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        _batch_confirm_shown = false;
        _batch_confirm = BatchConfirm::None;
        return;
    }
    const BatchConfirm what = _batch_confirm;
    const bool all = what == BatchConfirm::ClearList;
    const bool unchecked = what == BatchConfirm::ClearUnchecked;
    ImGui::PushTextWrapPos(px(420.0f));
    ui::Text(all ? msg::batch_clear_confirm
             : unchecked ? msg::batch_clear_unchecked_confirm : msg::batch_clear_done_confirm);
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    if (ui::Button(all ? msg::batch_clear : unchecked ? msg::batch_clear_unchecked : msg::batch_clear_done,
                   ImVec2(px(kModalButtonW), 0))) {
        if (all) {
            _batch.clear();
        } else {
            std::vector<BatchRow> kept;
            for (int i = 0; i < (int)_batch.size(); i++)
                if (unchecked ? _batch[(size_t)i].enabled : !batch_row_done(i))
                    kept.push_back(_batch[(size_t)i]);
            _batch.swap(kept);
        }
        _batch_tasks.clear();
        _batch_open_row = -1;
        _batch_msg.clear();
        batch_edited();
        _batch_confirm_shown = false;
        _batch_confirm = BatchConfirm::None;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ui::Button(msg::cancel, ImVec2(px(kModalButtonW), 0))) {
        _batch_confirm_shown = false;
        _batch_confirm = BatchConfirm::None;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void GuiApp::draw_batch_row_merge(BatchRow& row, int index) {
    namespace pmsg = spirula::i18n::msg::partition;
    ui::SeparatorText(pmsg::stage_merge);
    ui::Text(pmsg::lbl_file);
    ImGui::SetNextItemWidth(px(-8.0f));
    if (ui::InputTextRaw("##mergepart", &row.partition)) batch_edited();
    ui::TextDisabledWrapped(pmsg::merge_row_help);
    ui::Text(pmsg::lbl_runs);
    const float bw = ImGui::GetFrameHeight() + 6.0f;
    ImGui::SetNextItemWidth(-bw);
    if (ui::InputTextWithHintRaw("##mergeout", msg::batch_output_hint, &row.output_dir))
        batch_edited();
    ImGui::SameLine(0, px(2.0f));
    if (ui::ButtonRaw("...##mergeout")) {
        _pick_row = index;
        open_pick(PickAction::BatchOutput, msg::batch_pick_output.get(),
                  FileDialog::Mode::Folder, {}, row.output_dir);
    }
}

// ---- queueing a partition's parts ----

void GuiApp::open_partition_queue(const DatasetFolders& f, const std::string& partition,
                                  int num_parts) {
    _pq = PartitionQueue{};
    _pq.folders = f;
    _pq.partition = partition;
    _pq.num_parts = num_parts;
    _pq.run = batch_run_on_screen(f.dir);
    _pq.open = true;
}

int GuiApp::queue_partition_rows(bool clear_pending) {
    if (clear_pending) {
        std::vector<BatchRow> kept;
        for (int i = 0; i < (int)_batch.size(); i++)
            if (!_batch[(size_t)i].enabled || batch_row_done(i)) kept.push_back(_batch[(size_t)i]);
        _batch.swap(kept);
        _batch_tasks.clear();
    }
    const int n = add_batch_partition_rows(_pq.folders, _pq.partition, _pq.num_parts);
    if (_pq.merge) {
        BatchRow m;
        m.dataset = _pq.folders.dir;
        m.partition = _pq.partition;
        m.partition_part = -1;
        for (int i = 0; i < kNumBatchStages; i++) m.stages[i] = false;
        m.does(BatchStage::Merge) = true;
        _batch.push_back(std::move(m));
        batch_edited();
    }
    log(i18n::format(spirula::i18n::msg::partition::batch_added, {n}));
    return n;
}

void GuiApp::draw_partition_queue_modal() {
    namespace pmsg = spirula::i18n::msg::partition;
    if (_pq.open) {
        ui::OpenPopup(pmsg::pq_title);
        _pq.open = false;
        _pq.shown = true;
    }
    if (!_pq.shown) return;
    if (!ui::BeginPopupModal(pmsg::pq_title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        _pq.shown = false;
        return;
    }
    constexpr float kQueueW = 520.0f;   // the text and the run row beneath it
    ImGui::PushTextWrapPos(px(kQueueW));
    ui::Text(pmsg::pq_runs, {_pq.num_parts, fs::path(_pq.folders.dir).filename().string()});
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    if (!_pq.ask_clear) {
        ImGui::SetNextItemWidth(px(kQueueW));
        draw_batch_run(_pq.run, -2, 0, false);
        ui::Checkbox(pmsg::pq_merge, &_pq.merge);
        ui::help_on_hover(pmsg::pq_merge_help);
        ImGui::Spacing();
        int pending = 0;
        for (int i = 0; i < (int)_batch.size(); i++)
            pending += _batch[(size_t)i].enabled && !batch_row_done(i);
        ImGui::BeginDisabled(_batch_active);
        if (ui::Button(pmsg::pq_queue, ImVec2(px(kModalButtonW), 0))) {
            if (pending > 0) {
                _pq.ask_clear = true;
            } else {
                queue_partition_rows(false);
                _pq.shown = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
        if (_batch_active) {
            ImGui::SameLine();
            ui::TextColored(kDim, msg::batch_busy_elsewhere);
        }
        ImGui::SameLine();
        if (ui::Button(msg::cancel, ImVec2(px(kModalButtonW), 0))) {
            _pq.shown = false;
            ImGui::CloseCurrentPopup();
        }
    } else {
        int pending = 0;
        for (int i = 0; i < (int)_batch.size(); i++)
            pending += _batch[(size_t)i].enabled && !batch_row_done(i);
        ImGui::PushTextWrapPos(px(kQueueW));
        ui::Text(pmsg::pq_pending, {pending});
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
        if (ui::Button(pmsg::pq_clear, ImVec2(px(kModalButtonW), 0))) {
            queue_partition_rows(true);
            _pq.shown = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ui::Button(pmsg::pq_keep, ImVec2(px(kModalButtonW), 0))) {
            queue_partition_rows(false);
            _pq.shown = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ui::Button(msg::cancel, ImVec2(px(kModalButtonW), 0))) {
            _pq.shown = false;
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::EndPopup();
}


void GuiApp::draw_batch_rows() {
    if (_batch.empty()) {
        ui::TextDisabled(msg::batch_empty);
        ui::TextDisabled(msg::batch_drop_hint);
        return;
    }
    int remove = -1, move = 0, move_from = -1;
    for (int i = 0; i < (int)_batch.size(); i++) {
        int one_move = 0;
        draw_batch_row(_batch[(size_t)i], i, remove, one_move);
        if (one_move != 0) { move = one_move; move_from = i; }
    }
    if (remove >= 0) {
        _batch.erase(_batch.begin() + remove);
        if (_batch_open_row == remove) _batch_open_row = -1;
        else if (_batch_open_row > remove) _batch_open_row--;
        // The last run's record is indexed by row; the rows have moved.
        _batch_tasks.clear();
        batch_edited();
    } else if (move != 0 && move_from >= 0) {
        const int to = move_from + move;
        if (to >= 0 && to < (int)_batch.size()) {
            std::swap(_batch[(size_t)move_from], _batch[(size_t)to]);
            if (_batch_open_row == move_from) _batch_open_row = to;
            else if (_batch_open_row == to) _batch_open_row = move_from;
            _batch_tasks.clear();
            batch_edited();
        }
    }
}


void GuiApp::draw_batch_row(BatchRow& row, int index, int& remove, int& move) {
    ImGui::PushID(index);
    const bool open = _batch_open_row == index;

    // ---- the header line ----
    if (ui::ButtonRaw(open ? "-##expand" : "+##expand",
                      ImVec2(ImGui::GetFrameHeight(), 0)))
        _batch_open_row = open ? -1 : index;
    ui::help_on_hover(msg::batch_row_expand_help);
    ImGui::SameLine();
    ImGui::BeginDisabled(_batch_active);
    if (ui::CheckboxRaw("##en", &row.enabled)) {
        if (row.enabled) row.done = false;
        batch_edited();
    }
    ui::help_on_hover(msg::batch_row_enabled_help);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ui::TextDisabledRaw(std::to_string(index + 1) + ".");

    // The stages as chips, each its own widget: a row of labels, never a
    // sentence stitched together out of them.
    for (int st = 0; st < kNumBatchStages; st++) {
        if (!row.stages[st]) continue;
        ImGui::SameLine();
        ui::TextColored(row.enabled ? kOk : kDim,
                        batch_stage_name((BatchStage)st));
    }
    if (row.does(BatchStage::Train) && batch_num_runs(row) > 1) {
        ImGui::SameLine();
        ui::TextDisabled(msg::batch_runs_count, {(long long)batch_num_runs(row)});
    }
    if (!row.partition.empty() && row.partition_part >= 0) {
        ImGui::SameLine();
        ui::TextColored(row.enabled ? kOk : kDim, spirula::i18n::msg::partition::chip_part,
                        {row.partition_part});
        ui::help_on_hover_raw(row.partition.c_str());
    }
    if (const std::string what = batch_row_summary(row); !what.empty()) {
        ImGui::SameLine();
        ui::TextDisabledRaw(what);
    }

    // ---- the right-hand controls ----
    const float bw = ImGui::GetFrameHeight();
    const float style_x = ImGui::GetStyle().ItemSpacing.x;
    ImGui::SameLine(std::max(ImGui::GetContentRegionAvail().x - 3 * bw -
                                 2 * style_x + ImGui::GetCursorPosX(),
                             ImGui::GetCursorPosX() + style_x));
    ImGui::BeginDisabled(_batch_active);
    if (ui::ButtonRaw("^##up", ImVec2(bw, 0))) move = -1;
    ui::help_on_hover(msg::batch_move_up);
    ImGui::SameLine(0, style_x);
    if (ui::ButtonRaw("v##down", ImVec2(bw, 0))) move = 1;
    ui::help_on_hover(msg::batch_move_down);
    ImGui::SameLine(0, style_x);
    if (ui::ButtonRaw("x##rm", ImVec2(bw, 0))) remove = index;
    ui::help_on_hover(dmsg::remove);
    ImGui::EndDisabled();

    // ---- status, and why ----
    // A row with no task in this run is not waiting for anything.
    bool in_run = false;
    for (const BatchTask& t : _batch_tasks) in_run = in_run || t.row == index;
    const BatchStatus st = !in_run && row.done ? BatchStatus::Done : row_status(_batch_tasks, index);
    if (st != BatchStatus::Pending || (_batch_active && in_run)) {
        ImGui::Indent(ImGui::GetFrameHeight() * 2.0f);
        draw_status_word(st);
        // The newest thing this row has to say: what went wrong if anything
        // did, and otherwise the last thing it produced.
        std::string detail;
        for (const BatchTask& t : _batch_tasks) {
            if (t.row != index) continue;
            if (!t.message.empty()) { detail = t.message; break; }
            if (!t.result.empty()) detail = t.result;
        }
        if (!detail.empty()) {
            ImGui::SameLine();
            ui::TextDisabledRaw(detail);
        }
        ImGui::Unindent(ImGui::GetFrameHeight() * 2.0f);
    }

    if (open) {
        ImGui::Indent();
        ImGui::BeginDisabled(_batch_active);
        for (int i = 0; i < kNumBatchStages; i++) {
            if (i) ImGui::SameLine();
            ImGui::PushID(i);
            if (ui::Checkbox(batch_stage_name((BatchStage)i), &row.stages[i]))
                batch_edited();
            ImGui::PopID();
        }
        if (row.does(BatchStage::Dataset) || row.does(BatchStage::Train))
            draw_batch_row_common_dataset(row, index);
        if (row.does(BatchStage::Dataset)) draw_batch_row_dataset(row, index);
        if (row.does(BatchStage::Train)) draw_batch_row_train(row, index);
        if (row.does(BatchStage::Mesh)) draw_batch_row_mesh(row, index);
        if (row.does(BatchStage::Merge)) draw_batch_row_merge(row, index);
        ImGui::EndDisabled();
        ImGui::Unindent();
    }
    ImGui::Separator();
    ImGui::PopID();
}


void GuiApp::draw_batch_row_dataset(BatchRow& row, int index) {
    ui::SeparatorText(msg::batch_stage_dataset);
    ui::Text(msg::batch_row_inputs);
    int drop = -1;
    const float bw = ImGui::GetFrameHeight() + 6.0f;
    for (size_t i = 0; i < row.sources.size(); i++) {
        ImGui::PushID((int)i);
        ImGui::SetNextItemWidth(-bw);
        if (ui::InputTextRaw("##src", &row.sources[i])) batch_edited();
        ImGui::SameLine(0, px(2.0f));
        if (ui::ButtonRaw("x##rmsrc")) drop = (int)i;
        ImGui::PopID();
    }
    if (drop >= 0) {
        row.sources.erase(row.sources.begin() + drop);
        batch_edited();
    }
    if (ui::Button(msg::batch_add_video)) {
        _pick_row = index;
        open_pick(PickAction::BatchSourceVideo, msg::batch_pick_source.get(),
                  FileDialog::Mode::File,
                  std::vector<std::string>(kVideoExtensions,
                                           kVideoExtensions + kNumVideoExtensions),
                  "", /*multi=*/true);
    }
    ImGui::SameLine();
    if (ui::Button(msg::batch_add_photos)) {
        _pick_row = index;
        open_pick(PickAction::BatchSourceImages, msg::batch_pick_photos.get(),
                  FileDialog::Mode::Folder);
    }

    ui::Text(msg::batch_preset_dataset);
    ImGui::SetNextItemWidth(px(-8.0f));
    const PresetChoice picked =
        preset_combo("##dspreset", _ds_presets, row.dataset_preset.path,
                     row.dataset_preset.name, dataset_builtin_rows(), true);
    if (!picked.moved) return;
    if (picked.from_file) {
        _pick_row = index;
        open_pick(PickAction::BatchDatasetPresetFile, msg::preset_pick_file.get(),
                  FileDialog::Mode::File, {".json"});
    } else if (picked.index >= 0 && picked.index < (int)_ds_presets.items.size()) {
        row.dataset_preset.path = _ds_presets.items[(size_t)picked.index].path;
        row.dataset_preset.name = _ds_presets.items[(size_t)picked.index].name;
        batch_edited();
    } else {
        row.dataset_preset = BatchPreset{"", picked.builtin};
        batch_edited();
    }
}


void GuiApp::draw_batch_row_train(BatchRow& row, int index) {
    ui::SeparatorText(msg::batch_stage_train);
    ui::Text(msg::batch_runs);
    const float bw = ImGui::GetFrameHeight() + 6.0f;
    // One region column for all the runs, so they line up. A region given by
    // path still shows with none saved, so it can be turned off.
    const std::string ds = !row.dataset.empty() ? row.dataset : batch_dataset_workspace(row);
    const std::vector<std::string> roi_files =
        ds.empty() ? std::vector<std::string>() : spirula::list_roi_files(ds);
    bool roi_column = !roi_files.empty();
    for (const BatchRun& run : row.runs) roi_column = roi_column || !run.roi_region.empty();
    int drop = -1;
    for (size_t i = 0; i < row.runs.size(); i++) {
        ImGui::PushID((int)i);
        if (draw_batch_run(row.runs[i], index, (int)i, row.does(BatchStage::Mesh),
                           roi_column ? ds : std::string(), roi_files))
            batch_edited();
        ImGui::SameLine();
        if (ui::ButtonRaw("x##rmrun", ImVec2(bw, 0))) drop = (int)i;
        ui::help_on_hover(dmsg::remove);
        ImGui::PopID();
    }
    if (drop >= 0) {
        row.runs.erase(row.runs.begin() + drop);
        batch_edited();
    }
    if (row.runs.empty()) ui::TextDisabled(msg::batch_runs_default);
    if (ui::Button(msg::batch_add_run)) {
        row.runs.push_back(batch_run_on_screen(row.dataset));
        batch_edited();
    }
    ui::help_on_hover(msg::batch_add_run_help);

    ui::Text(msg::batch_col_output);
    ImGui::SetNextItemWidth(-bw);
    if (ui::InputTextWithHintRaw("##out", msg::batch_output_hint, &row.output_dir))
        batch_edited();
    ui::help_on_hover(msg::batch_output_help);
    ImGui::SameLine(0, px(2.0f));
    if (ui::ButtonRaw("...##out")) {
        _pick_row = index;
        open_pick(PickAction::BatchOutput, msg::batch_pick_output.get(),
                  FileDialog::Mode::Folder, {}, row.output_dir);
    }
}


bool GuiApp::draw_batch_run(BatchRun& run, int row, int slot, bool meshing,
                            const std::string& roi_dataset,
                            const std::vector<std::string>& roi_files) {
    bool moved = false;
    const float num_w = px(84.0f);
    const float sp = ImGui::GetStyle().ItemSpacing.x;
    const float bw = ImGui::GetFrameHeight() + 6.0f;
    const bool inline_floater = row >= 0;
    const char* values[] = {"", "off", "mild", "moderate", "strong"};
    const Msg* choices[] = {&msg::floater_per_preset, &msg::floater_off, &msg::floater_mild,
                            &msg::floater_moderate, &msg::floater_strong};
    const int num_choices = IM_ARRAYSIZE(values);
    int selected = 0;
    float floater_w = 0.0f;
    for (int k = 0; k < num_choices; k++) {
        if (run.floater_suppression == values[k]) selected = k;
        floater_w = std::max(floater_w, ImGui::CalcTextSize(choices[k]->get()).x);
    }
    floater_w += ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.x * 2.0f;
    // Capped: the list itself opens as wide as its longest entry.
    float roi_w = 0.0f;
    if (!roi_dataset.empty()) {
        roi_w = ImGui::CalcTextSize(spirula::i18n::msg::roi::train_none.get()).x;
        for (const std::string& f : roi_files)
            roi_w = std::max(roi_w, ImGui::CalcTextSize(fs::path(f).stem().string().c_str()).x);
        roi_w = std::min(roi_w + ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.x * 2.0f,
                         px(200.0f));
    }
    const float boxes = 3.0f * (num_w + sp) +
                        (meshing ? ImGui::GetFrameHeight() + sp : 0.0f) +
                        (inline_floater ? floater_w + sp + bw + sp : 0.0f) +
                        (roi_w > 0.0f ? roi_w + sp : 0.0f);
    ImGui::SetNextItemWidth(std::max(px(120.0f),
                                     ImGui::GetContentRegionAvail().x - boxes));
    moved |= draw_batch_train_preset(run.preset, "##trpreset", row, slot);

    // Empty means inherit; SH degree 0 is an explicit override.
    const Msg* labels[] = {&msg::batch_col_splats, &msg::batch_col_sh,
                           &msg::batch_col_steps};
    std::string* fields[] = {&run.cap_max, &run.sh_degree, &run.iterations};
    for (int k = 0; k < 3; k++) {
        ImGui::SameLine(0, sp);
        ImGui::PushID(k);
        ImGui::SetNextItemWidth(num_w);
        if (ui::InputTextWithHintRaw("##ovr", *labels[k], fields[k],
                                     ImGuiInputTextFlags_CharsDecimal))
            moved = true;
        ui::help_on_hover(msg::batch_override_help);
        ImGui::PopID();
    }
    if (inline_floater) ImGui::SameLine(0, sp);
    ImGui::SetNextItemWidth(floater_w);
    if (ui::BeginComboRaw("##floater_suppression", choices[selected]->get())) {
        for (int k = 0; k < num_choices; k++) {
            if (ui::Selectable(*choices[k], selected == k)) {
                run.floater_suppression = values[k];
                moved = true;
            }
            if (selected == k) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ui::help_on_hover(fld::floater_suppression_help);
    if (roi_w > 0.0f) {
        ImGui::SameLine(0, sp);
        moved |= draw_batch_run_roi(run, roi_dataset, roi_files, roi_w);
    }
    if (meshing) {
        ImGui::SameLine(0, sp);
        if (ui::CheckboxRaw("##runmesh", &run.mesh)) moved = true;
        ui::help_on_hover(msg::batch_run_mesh_help);
    }
    return moved;
}


// The region a run trains in. Editing one is a right-click on its entry in
// the list, which keeps a button off every run line.
bool GuiApp::draw_batch_run_roi(BatchRun& run, const std::string& dataset,
                                const std::vector<std::string>& files, float width) {
    namespace roimsg = spirula::i18n::msg::roi;
    const spirula::RoiChoice now = spirula::resolve_roi_setting(run.roi_region, dataset);
    auto stem = [](const std::string& p) { return fs::path(p).stem().string(); };
    const std::string shown = now.off || now.path.empty() ? std::string(roimsg::train_none.get())
                                                          : stem(now.path);
    std::string pick;
    bool edit = false;
    std::string edit_file;
    ImGui::SetNextItemWidth(width);
    if (ui::BeginComboRaw("##runroi", shown.c_str())) {
        for (size_t k = 0; k <= files.size(); k++) {
            const bool none = k == files.size();
            std::error_code ec;
            const bool on = none ? now.off : !now.off && fs::equivalent(now.path, files[k], ec);
            ImGui::PushID((int)k);
            const bool clicked =
                none ? ui::Selectable(roimsg::train_none, on) : ui::SelectableRaw(stem(files[k]), on);
            if (clicked && !on)
                pick = none ? "off" : "roi/" + fs::path(files[k]).filename().generic_string();
            if (ImGui::BeginPopupContextItem("##roimenu")) {
                if (ui::MenuItem(roimsg::train_edit_region)) {
                    edit = true;
                    edit_file = none ? std::string() : files[k];
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
        if (edit) ImGui::CloseCurrentPopup();
        ImGui::EndCombo();
    }
    ui::help_on_hover(roimsg::batch_run_help);
    if (edit) open_roi_editor(dataset, edit_file);
    if (pick.empty() || pick == run.roi_region) return false;
    run.roi_region = pick;
    return true;
}


void GuiApp::draw_batch_row_mesh(BatchRow& row, int index) {
    ui::SeparatorText(msg::batch_stage_mesh);
    ui::Text(msg::batch_mesh_model_label);
    const float bw = ImGui::GetFrameHeight() + 6.0f;
    ImGui::SetNextItemWidth(-bw);
    if (ui::InputTextWithHintRaw("##meshmodel",
                                 row.does(BatchStage::Train)
                                     ? msg::batch_mesh_model_hint
                                     : msg::batch_mesh_model_pick,
                                 &row.model))
        batch_edited();
    ImGui::SameLine(0, px(2.0f));
    if (ui::ButtonRaw("...##meshmodel")) {
        _pick_row = index;
        open_pick(PickAction::BatchModel, msg::batch_pick_model.get(),
                  FileDialog::Mode::Folder, {}, row.model);
    }

    ui::Text(msg::batch_preset_mesh);
    ImGui::SetNextItemWidth(px(-8.0f));
    const PresetChoice picked = preset_combo("##meshpreset", _mesh_presets,
                                             row.mesh.preset.path, {}, {}, true);
    if (picked.moved) {
        if (picked.from_file) {
            _pick_row = index;
            open_pick(PickAction::BatchMeshPresetFile, msg::preset_pick_file.get(),
                      FileDialog::Mode::File, {".json"});
        } else if (picked.index >= 0 &&
                   picked.index < (int)_mesh_presets.items.size()) {
            row.mesh.preset.path = _mesh_presets.items[(size_t)picked.index].path;
            row.mesh.preset.name = _mesh_presets.items[(size_t)picked.index].name;
            batch_edited();
        } else {
            row.mesh.preset = BatchPreset{"", ""};
            batch_edited();
        }
    }
    if (draw_batch_mesh_outputs(row.mesh)) batch_edited();
}


// What the row writes, over what its preset says. Nothing ticked in a set is
// "whatever the preset says" rather than "write nothing", which is why the
// two sets are ticked rather than selected.
bool GuiApp::draw_batch_mesh_outputs(BatchMeshOptions& mesh) {
    bool moved = false;
    const Msg* color_names[kNumMeshColorModes] = {
        &msg::mesh_color_none, &msg::mesh_color_vertex, &msg::mesh_color_texture};

    ui::Text(msg::mesh_color);
    ui::help_on_hover(msg::batch_mesh_override_help);
    for (int i = 0; i < kNumMeshColorModes; i++) {
        ImGui::SameLine();
        ImGui::PushID(i);
        bool on = (mesh.colors & (1 << i)) != 0;
        if (ui::Checkbox(*color_names[i], &on)) {
            mesh.colors = on ? (mesh.colors | (1 << i)) : (mesh.colors & ~(1 << i));
            moved = true;
        }
        ImGui::PopID();
    }

    ui::Text(msg::mesh_formats);
    ui::help_on_hover(msg::batch_mesh_override_help);
    for (int i = 0; i < kNumMeshFormats; i++) {
        ImGui::SameLine();
        ImGui::PushID(i);
        bool on = (mesh.formats & (1 << i)) != 0;
        // A format no ticked colour can travel in is greyed out; with no
        // colour ticked at all the preset decides, so all of them are offered.
        bool ok = mesh.colors == 0;
        for (int c = 0; c < kNumMeshColorModes && !ok; c++)
            ok = (mesh.colors & (1 << c)) && mesh_format_carries(i, c);
        ImGui::BeginDisabled(!ok);
        if (ui::CheckboxRaw(kMeshFormats[i], &on)) {
            mesh.formats = on ? (mesh.formats | (1 << i)) : (mesh.formats & ~(1 << i));
            moved = true;
        }
        ImGui::EndDisabled();
        ImGui::PopID();
    }
    return moved;
}


// The dataset folder: written by the Dataset stage, read by the Train stage,
// so one field serves both and the two cannot point at different places.
void GuiApp::draw_batch_row_common_dataset(BatchRow& row, int index) {
    ui::Text(msg::batch_dataset_label);
    const float bw = ImGui::GetFrameHeight() + 6.0f;
    ImGui::SetNextItemWidth(-bw);
    if (ui::InputTextWithHintRaw("##ds",
                                 row.does(BatchStage::Dataset)
                                     ? msg::batch_dataset_auto_hint
                                     : msg::batch_dataset_hint,
                                 &row.dataset))
        batch_edited();
    ImGui::SameLine(0, px(2.0f));
    if (ui::ButtonRaw("...##ds")) {
        _pick_row = index;
        open_pick(PickAction::BatchDataset, msg::batch_pick_dataset.get(),
                  FileDialog::Mode::Folder, {}, row.dataset);
    }
    if (row.does(BatchStage::Dataset) && row.dataset.empty()) {
        const std::string ws = batch_dataset_workspace(row);
        if (!ws.empty()) ui::TextDisabledRaw(ws);
    }
}


bool GuiApp::draw_batch_train_preset(BatchPreset& p, const char* id, int row,
                                     int slot) {
    bool moved = false;
    const std::string preview = p.path.empty() ? preset_label(p.name) : p.name;
    const bool open = ui::BeginComboRaw(id, preview.c_str());
    std::string desc;
    if (p.path.empty()) {
        desc = preset_help(p.name);
    } else {
        for (const TrainPreset& t : _train_presets.items)
            if (t.path == p.path) { desc = t.description; break; }
    }
    preset_hover(desc, p.path);
    if (!open) return moved;
    refresh_presets();

    ui::SeparatorText(msg::preset_builtin_group);
    for (const auto& b : kTrainPresets) {
        const bool sel = p.path.empty() && p.name == b.name;
        if (ui::SelectableRaw(preset_label(b.name), sel)) {
            p.path.clear();
            p.name = b.name;
            moved = true;
        }
        preset_hover(preset_help(b.name), {});
        if (sel) ImGui::SetItemDefaultFocus();
    }

    ui::SeparatorText(msg::preset_user_group);
    if (_train_presets.items.empty()) ui::TextDisabled(msg::preset_none_saved);
    for (const auto& t : _train_presets.items) {
        ImGui::PushID(t.path.c_str());
        const bool sel = p.path == t.path;
        if (ui::SelectableRaw(t.name, sel)) {
            p.path = t.path;
            p.name = t.name;
            moved = true;
        }
        preset_hover(t);
        if (sel) ImGui::SetItemDefaultFocus();
        ImGui::PopID();
    }

    ImGui::Separator();
    if (ui::Selectable(msg::batch_preset_from_file)) {
        _pick_row = row;
        _pick_slot = slot;
        open_pick(PickAction::BatchPresetFile, msg::preset_pick_file.get(),
                  FileDialog::Mode::File, {".json"});
    }
    ImGui::EndCombo();
    return moved;
}


void GuiApp::draw_batch_issues() {
    for (int i = 0; i < (int)_batch.size(); i++) {
        if (!_batch[(size_t)i].enabled) continue;
        for (const BatchIssue& issue : _batch[(size_t)i].issues) {
            ui::TextColoredWrappedRaw(
                issue.fatal ? kErr : kWarn,
                i18n::format(msg::batch_issue_row,
                             {(long long)(i + 1), batch_issue_line(issue)}));
        }
    }
}


// A box that grows with what is in it. A real command is a curl invocation
// with a JSON body, which nobody writes on one line; the empty state is still
// one line, because that is what the screen looks like for everyone else.
static ImVec2 batch_cmd_box_size(const std::string& text, float room) {
    const ImGuiStyle& st = ImGui::GetStyle();
    const float least = px(560.0f);
    float widest = 0.0f;
    int lines = 0;
    for (size_t at = 0; at <= text.size();) {
        const size_t nl = text.find('\n', at);
        const std::string line =
            text.substr(at, nl == std::string::npos ? nl : nl - at);
        widest = std::max(widest, ImGui::CalcTextSize(line.c_str()).x);
        lines++;
        if (nl == std::string::npos) break;
        at = nl + 1;
    }
    return ImVec2(std::clamp(widest + st.FramePadding.x * 4.0f, least,
                             std::max(least, room)),
                  ImGui::GetTextLineHeight() * (float)std::clamp(lines, 1, 12) +
                      st.FramePadding.y * 2.0f);
}

// The command a finished queue runs. What {message} means is on the screen
// rather than behind a hover, because nobody guesses a placeholder.
void GuiApp::draw_batch_command() {
    ui::SeparatorText(msg::batch_cmd_title);
    // Split rather than empty: a field holding only spaces names no program,
    // and a Test button that does nothing at all is worse than a greyed one.
    const bool can_test =
        !split_args(_batch_cmd).empty() && !_finish_cmd_run.busy();
    const float button_w =
        ImGui::CalcTextSize(msg::batch_cmd_test.get()).x +
        ImGui::GetStyle().FramePadding.x * 2.0f;
    const float room = ImGui::GetContentRegionAvail().x - button_w -
                       ImGui::GetStyle().ItemSpacing.x - px(8.0f);
    // The content is a command line: English wherever the interface is.
    ui::InputTextMultilineRaw("##batchcmd", &_batch_cmd,
                              batch_cmd_box_size(_batch_cmd, room));
    if (ImGui::IsItemDeactivatedAfterEdit()) save_settings();
    if (_batch_cmd.empty())
        ui::hint_over_last_item_raw("python notify_me.py --message \"{message}\"");
    ImGui::SameLine();
    ImGui::BeginDisabled(!can_test);
    if (ui::Button(msg::batch_cmd_test))
        run_batch_command(msg::batch_cmd_test_message.get());
    ui::help_on_hover_disabled(msg::batch_cmd_test_help);
    ImGui::EndDisabled();
    ui::TextDisabledWrapped(msg::batch_cmd_help);
}


// What a start would actually run, in order. The whole point of the screen is
// that most of it does not exist yet, so seeing the list beforehand is the
// only way to tell a five-hour queue from a mistake.
void GuiApp::draw_batch_plan() {
    if (_batch.empty()) return;
    if (!ui::CollapsingHeader(msg::batch_plan_title)) return;
    const std::vector<BatchTask> plan =
        _batch_active ? _batch_tasks : batch_plan(_batch);
    if (plan.empty()) {
        ui::TextDisabled(msg::batch_plan_empty);
        return;
    }
    for (size_t i = 0; i < plan.size(); i++) {
        const BatchTask& t = plan[i];
        if (t.row < 0 || t.row >= (int)_batch.size()) continue;
        const BatchRow& row = _batch[(size_t)t.row];
        const long long n = (long long)i + 1;
        std::string line;
        switch (t.stage) {
            case BatchStage::Dataset:
                line = i18n::format(msg::batch_plan_dataset,
                                    {n, batch_dataset_workspace(row)});
                break;
            case BatchStage::Train: {
                const BatchPreset p = batch_run_of(row, t.variant).preset;
                const std::string name =
                    p.path.empty() ? preset_label(p.name) : p.name;
                line = row.does(BatchStage::Dataset)
                           ? i18n::format(msg::batch_plan_train_new, {n, name})
                           : i18n::format(msg::batch_plan_train,
                                          {n, row.dataset, name});
                break;
            }
            case BatchStage::Mesh: {
                if (!row.model.empty()) {
                    line = i18n::format(msg::batch_plan_mesh, {n, row.model});
                } else if (batch_num_runs(row) > 1) {
                    // Which run, once a row meshes some of them and not others.
                    const BatchPreset p = batch_run_of(row, t.variant).preset;
                    line = i18n::format(msg::batch_plan_mesh_run,
                                        {n, p.path.empty() ? preset_label(p.name)
                                                           : p.name});
                } else {
                    line = i18n::format(msg::batch_plan_mesh_new, {n});
                }
                break;
            }
            case BatchStage::Merge:
                line = i18n::format(spirula::i18n::msg::partition::plan_merge,
                                    {n, fs::path(row.partition).parent_path().filename().string()});
                break;
        }
        ImVec4 color = kDim;
        if (_batch_active) {
            if (t.status == BatchStatus::Done) color = kOk;
            else if (t.status == BatchStatus::Failed) color = kErr;
            else if (t.status == BatchStatus::Running) color = kWarn;
        }
        ui::TextColoredWrappedRaw(color, line);
    }
}


float GuiApp::batch_task_fraction(double& remaining) {
    remaining = -1.0;
    if (!_batch_active || _batch_current < 0 ||
        _batch_current >= (int)_batch_tasks.size())
        return -1.0f;
    switch (_batch_tasks[(size_t)_batch_current].stage) {
        case BatchStage::Dataset: {
            // The steps a dataset run goes through, with the running one's own
            // fraction inside its slot. Both engines report through that list.
            RunProgress* p = dataset_steps();
            const int at = std::clamp((int)p->current(), 0, kNumStages - 1);
            const StageProgress sp = p->stage((Stage)at);
            const float inner = sp.fraction >= 0.0f ? sp.fraction : 0.5f;
            return std::min(1.0f, ((float)at + inner) / (float)kNumStages);
        }
        case BatchStage::Train: {
            const TrainRunner::Phase phase = _runner.phase();
            if (phase != TrainRunner::Phase::Training) return -1.0f;
            const double elapsed = tick_train_clock(phase, _runner.latest_progress());
            if (!_train_clock.started()) return -1.0f;
            remaining = _train_clock.remaining(elapsed);
            return _train_clock.fraction();
        }
        case BatchStage::Merge: return 0.5f;
        case BatchStage::Mesh:
            _mesh.poll();
            remaining = _mesh.clock().remaining(progress_now());
            return _mesh.clock().started() ? _mesh.clock().fraction() : -1.0f;
    }
    return -1.0f;
}

double GuiApp::batch_task_elapsed() const {
    if (!_batch_active || _batch_current < 0 ||
        _batch_current >= (int)_batch_tasks.size())
        return 0.0;
    return std::max(0.0, ImGui::GetTime() -
                             _batch_tasks[(size_t)_batch_current].started_at);
}

const BatchRow* GuiApp::batch_running_row() const {
    if (!_batch_active || _batch_current < 0 ||
        _batch_current >= (int)_batch_tasks.size())
        return nullptr;
    const int row = _batch_tasks[(size_t)_batch_current].row;
    if (row < 0 || row >= (int)_batch.size()) return nullptr;
    return &_batch[(size_t)row];
}

void GuiApp::draw_batch_stop_buttons() {
    ImGui::BeginDisabled(_batch_stop_after);
    if (ui::Button(msg::batch_stop_after, ImVec2(-8, 0)))
        _batch_stop_after = true;
    ui::help_on_hover(msg::batch_stop_after_help);
    ImGui::EndDisabled();
    if (ui::Button(msg::batch_stop_now, ImVec2(-8, 0))) stop_batch_now();
    ui::help_on_hover(msg::batch_stop_now_help);
}

void GuiApp::stop_batch_now() {
    _batch_stop_after = true;
    _batch_stop_now = true;
    if (_batch_current >= 0 && _batch_current < (int)_batch_tasks.size()) {
        switch (_batch_tasks[(size_t)_batch_current].stage) {
            case BatchStage::Dataset: cancel_dataset_job(); break;
            case BatchStage::Mesh:    _mesh.cancel(); break;
            case BatchStage::Merge:   break;
            default:                  _runner.request_stop(); break;
        }
    }
}

// What is running, on the batch screen and on the work screen that owns the
// running task: two bars, what each has left, and the ways to stop. The work
// screen shows the job; this says where in the QUEUE it is.
void GuiApp::draw_batch_progress() {
    ui::SeparatorText(msg::batch_title);
    double runner_remaining;
    const float frac = batch_task_fraction(runner_remaining);
    const double elapsed = batch_task_elapsed();
    const BatchProgress p =
        batch_progress(_batch_tasks, _batch_current, frac, elapsed, runner_remaining);
    const double now = progress_now();
    if (!_batch_queue_clock.started()) _batch_queue_clock.start(now);
    _batch_queue_clock.work(now, p.total > 0 ? (double)p.done / (double)p.total : 0.0,
                            p.remaining);
    if (_batch_clock_task != _batch_current) {
        _batch_task_clock.start(now - elapsed);
        _batch_clock_task = _batch_current;
    }
    if (frac >= 0.0f || p.task_remaining >= 0.0)
        _batch_task_clock.work(now, std::max(0.0f, frac), p.task_remaining);

    ui::Text(msg::batch_running_banner,
             {(long long)(_batch_current + 1), (long long)_batch_tasks.size()});
    timed_progress_bar(_batch_queue_clock, nullptr, -8);

    if (_batch_current >= 0 && _batch_current < (int)_batch_tasks.size()) {
        const BatchTask& t = _batch_tasks[(size_t)_batch_current];
        ImGui::PushTextWrapPos();
        ui::TextDisabled(batch_stage_name(t.stage));
        if (t.row >= 0 && t.row < (int)_batch.size()) {
            const std::string what = batch_row_summary(_batch[(size_t)t.row]);
            if (!what.empty()) ui::TextDisabledRaw(what);
        }
        ImGui::PopTextWrapPos();
        timed_progress_bar(_batch_task_clock, nullptr, -8);
    }
    if (_batch_stop_after) ui::TextColored(kWarn, msg::batch_stopping);

    // One button, whichever way round the two screens are: the list from a
    // work screen, the running step from the list.
    if (_screen == Screen::Batch) {
        if (ui::Button(msg::batch_show_training, ImVec2(-8, 0))) {
            const BatchStage st =
                _batch_current >= 0 && _batch_current < (int)_batch_tasks.size()
                    ? _batch_tasks[(size_t)_batch_current].stage
                    : BatchStage::Train;
            switch (st) {
                case BatchStage::Dataset: go_to(Screen::NewDataset); break;
                case BatchStage::Mesh:    go_to(Screen::Mesh); break;
                case BatchStage::Merge:   break;
                default:                  go_to(Screen::Train); break;
            }
        }
    } else if (ui::Button(msg::batch_show_list, ImVec2(-8, 0))) {
        go_to(Screen::Batch);
    }
    draw_batch_stop_buttons();
}

}  // namespace gui
