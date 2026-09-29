// PartitionPanel.cpp -- see PartitionPanel.h.

#include "app/gui/PartitionPanel.h"

#include "app/webviewer/RegionOverlay.h"

#include "app/gui/Layout.h"
#include "app/gui/Ui.h"
#include "checkpoint/SplatMerge.h"
#include "checkpoint/SplatPly.h"
#include "i18n/catalog/Gui.h"
#include "i18n/catalog/Partition.h"

#include "imgui.h"
#include "imgui_stdlib.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;
namespace gmsg = spirula::i18n::msg::gui;
namespace pmsg = spirula::i18n::msg::partition;

using spirula::i18n::format;

namespace gui {

namespace {

const spirula::CovisibilitySource kSources[] = {
    spirula::CovisibilitySource::Auto, spirula::CovisibilitySource::Tracks,
    spirula::CovisibilitySource::Projection, spirula::CovisibilitySource::Proximity};
const spirula::i18n::Msg* kSourceNames[] = {&pmsg::src_auto, &pmsg::src_tracks,
                                            &pmsg::src_projection, &pmsg::src_proximity};

const spirula::i18n::Msg* source_name(spirula::CovisibilitySource s) {
    for (int i = 0; i < 4; i++)
        if (kSources[i] == s) return kSourceNames[i];
    return &pmsg::src_auto;
}

ImVec4 part_vec(int part, float alpha = 1.0f) {
    float c[3];
    spirula::part_color(part, c);
    return ImVec4(c[0], c[1], c[2], alpha);
}

}  // namespace

PartitionPanel::PartitionPanel() = default;

PartitionPanel::~PartitionPanel() {
    _cancel = true;
    if (_worker.joinable()) _worker.join();
}

void PartitionPanel::open(const std::string& dataset_dir, Hooks hooks) {
    if (_busy.load()) return;
    if (_worker.joinable()) _worker.join();
    _open = true;
    _dataset = dataset_dir;
    _hooks = std::move(hooks);
    _loaded = _cov_valid = _part_valid = false;
    _ds = ParsedDataset{};
    _post = PostSplitCameras{};
    _tracks = spirula::SparseStats{};
    _part = spirula::ScenePartition{};
    _part_meshes.clear();
    _error.clear();
    _status.clear();
    _saved_path.clear();
    _merged_path.clear();
    _merge_status.clear();
    _merge_error.clear();
    _runs.clear();
    _scanned = false;
    _show_part = -1;
    _save_path = (fs::path(dataset_dir) / "partition.json").string();
    _runs_dir = (fs::path(dataset_dir) / "outputs").string();
    _merge_out = (fs::path(dataset_dir) / "merged.ply").string();
    _view.detach();
    start(Job::Load);
}

void PartitionPanel::close() {
    _open = false;
    // A split nobody will look at is not worth finishing.
    if (_job.load() == Job::Compute) _cancel = true;
    if (_worker.joinable() && !_busy.load()) _worker.join();
    _view.detach();
}

void PartitionPanel::destroy_gl() { _view.destroy_gl(); }

// ---------------------------------------------------------------------------
// The worker
// ---------------------------------------------------------------------------

void PartitionPanel::start(Job job) {
    if (_busy.load()) return;
    if (_worker.joinable()) _worker.join();
    _busy = true;
    _done = false;
    _cancel = false;
    _job = job;
    {
        std::lock_guard<std::mutex> lk(_mu);
        _error.clear();
        _notice = nullptr;
        _cancelled = false;
        _log_lines.clear();
    }
    _worker = std::thread([this, job] {
        try {
            switch (job) {
                case Job::Load: run_load(); break;
                case Job::Compute: run_compute(); break;
                case Job::Merge: run_merge(); break;
                case Job::None: break;
            }
        } catch (const spirula::PartitionCancelled&) {
            std::lock_guard<std::mutex> lk(_mu);
            _cancelled = true;
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lk(_mu);
            _error = e.what();
        }
        _done = true;
        _busy = false;
    });
}

void PartitionPanel::run_load() {
    DatasetParserConfig dcfg;
    dcfg.require_image_files = false;
    ParsedDataset ds = parse_dataset(_dataset, dcfg, "");
    PostSplitCameras post = bake_post_split(ds, false, false);
    spirula::SparseStats tracks = spirula::read_sparse_stats(_dataset);
    std::lock_guard<std::mutex> lk(_mu);
    _ds = std::move(ds);
    _post = std::move(post);
    _tracks = std::move(tracks);
    _loaded = true;
}

void PartitionPanel::run_compute() {
    auto log = [this](const std::string& s) {
        std::lock_guard<std::mutex> lk(_mu);
        _log_lines.push_back(s);
    };
    spirula::PartitionOptions opt;
    {
        std::lock_guard<std::mutex> lk(_mu);
        opt = _compute_opt;
    }
    const bool reuse = _cov_valid && _cov_opt.source == opt.source &&
                       _cov_opt.max_projected_points == opt.max_projected_points &&
                       _cov_opt.proximity_neighbours == opt.proximity_neighbours;
    if (!reuse) {
        spirula::Covisibility cov = spirula::build_covisibility(_ds, &_tracks, opt, log, &_cancel);
        std::lock_guard<std::mutex> lk(_mu);
        _cov = std::move(cov);
        _cov_opt = opt;
        _cov_valid = true;
    }
    spirula::ScenePartition part = spirula::partition_scene(_ds, _cov, opt, log, &_cancel);
    if (_cancel.load()) throw spirula::PartitionCancelled();
    std::vector<spirula::RegionMesh> meshes;
    if (part.field && !part.field->empty()) {
        std::vector<float> xyz;
        const int64_t n = _ds.points.num(), stride = std::max<int64_t>(1, n / 200000);
        for (int64_t i = 0; i < n; i += stride)
            for (int r = 0; r < 3; r++) xyz.push_back((float)(_ds.points.xyz[(size_t)i * 3 + r] + _ds.center[r]));
        const spirula::Aabb box = xyz.empty() ? part.field->bounds()
                                              : spirula::robust_bounds(xyz.data(), (int64_t)xyz.size() / 3);
        meshes = spirula::label_boundary_meshes(*part.field, box, 96, &_cancel);
        if (_cancel.load()) throw spirula::PartitionCancelled();
    }
    std::lock_guard<std::mutex> lk(_mu);
    _new_part = std::move(part);
    _new_meshes = std::move(meshes);
}

void PartitionPanel::run_merge() {
    std::vector<std::string> runs;
    std::string out, saved;
    {
        std::lock_guard<std::mutex> lk(_mu);
        runs = _runs;
        out = _merge_out;
        saved = _saved_path;
    }
    spirula::MergeStats stats;
    const spirula::SplatCloud merged = spirula::merge_partition_splats(_part, runs, stats);
    spirula::write_splat_ply(merged, out);
    std::lock_guard<std::mutex> lk(_mu);
    _merged_path = out;
    _merge_status = format(pmsg::merge_done, {out, (long long)merged.num});
}

void PartitionPanel::poll() {
    if (!_done.load()) return;
    _done = false;
    if (_worker.joinable()) _worker.join();
    const Job job = _job.exchange(Job::None);
    std::vector<std::string> lines;
    std::string error;
    bool cancelled = false;
    {
        std::lock_guard<std::mutex> lk(_mu);
        lines.swap(_log_lines);
        error = _error;
        cancelled = _cancelled;
    }
    if (_hooks.log)
        for (const std::string& l : lines) _hooks.log(l);
    if (cancelled) {
        _notice = &pmsg::status_cancelled;
        return;
    }
    if (!error.empty()) {
        if (job == Job::Merge) _merge_error = error;
        else _status = error;
        if (_hooks.log) _hooks.log(error);
        if (job == Job::Load) _loaded = false;
        return;
    }
    switch (job) {
        case Job::Load:
            _status.clear();
            _view_dirty = true;
            break;
        case Job::Compute:
            _part = std::move(_new_part);
            _part_meshes = std::move(_new_meshes);
            _part_opt = _compute_opt;
            _part_valid = true;
            _status.clear();
            _saved_path.clear();
            _scanned = false;
            if (_show_part >= _part.num_parts) _show_part = -1;
            _view_dirty = true;
            break;
        case Job::Merge:
            _merge_error.clear();
            break;
        case Job::None:
            break;
    }
}

bool PartitionPanel::options_changed() const {
    if (!_part_valid) return true;
    const spirula::PartitionOptions& a = _part_opt;
    const spirula::PartitionOptions& b = _opt;
    return a.parts != b.parts || a.max_images != b.max_images ||
           a.ring_fraction != b.ring_fraction || a.ring_min_points != b.ring_min_points ||
           a.method != b.method || a.outside_seed_fraction != b.outside_seed_fraction ||
           a.max_seeds != b.max_seeds ||
           a.source != b.source;
}

// ---------------------------------------------------------------------------
// The view: points and frusta coloured by part
// ---------------------------------------------------------------------------

void PartitionPanel::refresh_view() {
    _view_dirty = false;
    if (!_loaded || !_gl_ok) return;
    ParsedDataset show = _ds;
    const int64_t n_pts = _ds.points.num();
    const int64_t n_cam = _ds.num_cameras;
    std::vector<float> cam_rgb;
    std::shared_ptr<const spirula::RegionOverlay> overlay;
    if (_part_valid && _part.num_parts > 0 && (int64_t)_part.frame_label.size() == n_cam) {
        const int solo = _show_part;
        // Which cameras and points the shown part trains on.
        std::vector<uint8_t> cam_role((size_t)n_cam, 0);   // 0 other, 1 ring, 2 core
        if (solo >= 0) {
            for (int32_t f : _part.ring[(size_t)solo]) cam_role[(size_t)f] = 1;
            for (int64_t i = 0; i < n_cam; i++)
                if (_part.frame_label[(size_t)i] == solo) cam_role[(size_t)i] = 2;
        }
        cam_rgb.resize((size_t)n_cam * 3);
        for (int64_t i = 0; i < n_cam; i++) {
            float c[3];
            const int label = _part.frame_label[(size_t)i];
            spirula::part_color(label, c);
            float mul = 1.0f, add = 0.0f;
            if (solo >= 0) {
                if (cam_role[(size_t)i] == 0) { mul = 0.0f; add = 0.22f; }
                else if (cam_role[(size_t)i] == 1) { spirula::part_color(solo, c); mul = 0.45f; add = 0.18f; }
            }
            for (int k = 0; k < 3; k++) cam_rgb[(size_t)i * 3 + k] = c[k] * mul + add;
        }

        if ((int64_t)_part.point_label.size() == n_pts) {
            std::vector<uint8_t> in_part;
            if (solo >= 0) {
                in_part.assign((size_t)n_pts, 0);
                for (int64_t i : _part.part_points[(size_t)solo])
                    if (i >= 0 && i < n_pts) in_part[(size_t)i] = 1;
            }
            show.points.xyz.clear();
            show.points.rgb.clear();
            show.points.xyz.reserve((size_t)n_pts * 3);
            show.points.rgb.reserve((size_t)n_pts * 3);
            for (int64_t i = 0; i < n_pts; i++) {
                const int label = _part.point_label[(size_t)i];
                if (solo >= 0 && !in_part[(size_t)i]) continue;
                for (int k = 0; k < 3; k++) show.points.xyz.push_back(_ds.points.xyz[(size_t)i * 3 + k]);
                float c[3];
                if (_owner_colors) {
                    spirula::part_color(label, c);
                    // Borrowed points (seen by the ring, owned elsewhere) fade.
                    if (solo >= 0 && label != solo)
                        for (float& v : c) v = 0.35f * v + 0.25f;
                } else {
                    for (int k = 0; k < 3; k++)
                        c[k] = _ds.points.rgb.empty() ? 0.78f : _ds.points.rgb[(size_t)i * 3 + k] / 255.0f;
                }
                for (int k = 0; k < 3; k++)
                    show.points.rgb.push_back((uint8_t)std::lround(std::clamp(c[k], 0.0f, 1.0f) * 255.0f));
            }
        }
        if (_show_grid && !_part_meshes.empty()) {
            // Each part's boundary: the field is in the dataset's frame, the
            // points it is drawn with less their centre.
            const float to_points[12] = {1, 0, 0, (float)-_ds.center[0], 0, 1, 0, (float)-_ds.center[1],
                                         0, 0, 1, (float)-_ds.center[2]};
            auto ov = std::make_shared<spirula::RegionOverlay>();
            for (int k = 0; k < (int)_part_meshes.size(); k++) {
                if (solo >= 0 && k != solo) continue;
                float c[3];
                spirula::part_color(k, c);
                ov->add(_part_meshes[(size_t)k], c, to_points);
            }
            overlay = ov;
        }
    }
    _view.attach_preview_data(show, _post, "partition:" + _dataset, 1.0f, n_cam > 0, nullptr,
                              cam_rgb.empty() ? nullptr : cam_rgb.data());
    _view.set_region_overlay(nullptr, overlay);
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void PartitionPanel::draw() {
    if (!_open) return;
    poll();
    if (_view_dirty && !_busy.load()) refresh_view();

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x * 0.85f, vp->WorkSize.y * 0.85f),
                             ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    bool open = true;
    if (!ImGui::Begin(ui::detail::label(pmsg::panel_title), &open, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        if (!open) close();
        return;
    }
    ui::TextDisabledRaw(_dataset);

    const float side_w = std::clamp(px(360.0f), px(300.0f), ImGui::GetContentRegionAvail().x * 0.45f);
    ImGui::BeginChild("##partside", ImVec2(side_w, 0), ImGuiChildFlags_None);
    draw_controls();
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##partview", ImVec2(0, 0), ImGuiChildFlags_None);
    if (_view.preview_active()) {
        _view.draw(/*training=*/false);
    } else if (_busy.load()) {
        ui::TextDisabled(_job.load() == Job::Load ? pmsg::status_reading : pmsg::status_computing);
    }
    ImGui::EndChild();

    ImGui::End();
    if (!open) close();
}

void PartitionPanel::draw_controls() {
    const bool busy = _busy.load();
    if (busy) {
        ui::TextDisabled(_job.load() == Job::Load ? pmsg::status_reading
                         : _job.load() == Job::Merge ? pmsg::merge_head : pmsg::status_computing);
        if (_job.load() == Job::Compute) {
            ImGui::SameLine();
            ImGui::BeginDisabled(_cancel.load());
            if (ui::Button(gmsg::cancel)) _cancel = true;
            ImGui::EndDisabled();
        }
    } else if (!_status.empty()) {
        ui::TextColoredWrappedRaw(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), _status);
    } else if (_notice) {
        ui::TextDisabledWrapped(*_notice);
    } else if (_part_valid) {
        char cut[32];
        std::snprintf(cut, sizeof cut, "%.1f", 100.0 * _part.cut_fraction);
        ui::TextWrapped(pmsg::status_summary,
                        {_part.num_parts, (long long)_ds.num_cameras, (long long)_ds.points.num(),
                         cut, source_name(_part.source)->get()});
    }
    ImGui::Spacing();

    ImGui::BeginDisabled(busy || !_loaded);
    // ---- options ----
    {
        int m = _opt.method == spirula::PartitionMethod::ViewGraph ? 1 : 0;
        ImGui::SetNextItemWidth(px(-80.0f));
        if (ui::Combo(pmsg::lbl_method, &m, {&pmsg::meth_graph, &pmsg::meth_viewgraph}))
            _opt.method = m == 1 ? spirula::PartitionMethod::ViewGraph : spirula::PartitionMethod::Graph;
        ui::help_on_hover(pmsg::opt_method);
    }
    ImGui::SetNextItemWidth(px(-80.0f));
    if (ui::Combo(pmsg::lbl_source, &_source_idx,
                  {&pmsg::src_auto, &pmsg::src_tracks, &pmsg::src_projection, &pmsg::src_proximity}))
        _opt.source = kSources[std::clamp(_source_idx, 0, 3)];
    ui::help_on_hover(pmsg::lbl_source_help);
    if (_loaded) {
        const bool tracked = spirula::tracks_usable(_ds, _tracks);
        ui::TextDisabled(pmsg::log_dataset,
                         {(long long)_ds.num_cameras, (long long)_ds.points.num(),
                          tracked ? pmsg::word_yes.get() : pmsg::word_no.get()});
        if (!tracked) ui::TextColoredWrapped(ImVec4(1.0f, 0.75f, 0.30f, 1.0f), pmsg::warn_no_tracks);
    }

    ui::Text(pmsg::lbl_split_by);
    if (ui::RadioButton(pmsg::mode_parts, _mode == 0)) _mode = 0;
    ImGui::SameLine();
    if (ui::RadioButton(pmsg::mode_max, _mode == 1)) _mode = 1;
    ui::help_on_hover(pmsg::mode_help);
    ImGui::SetNextItemWidth(px(120.0f));
    if (_mode == 0) {
        int parts = std::max(2, _opt.parts);
        if (ui::InputIntRaw("##parts", &parts)) _opt.parts = std::max(2, parts);
        if (_opt.parts < 2) _opt.parts = 2;
    } else {
        _opt.parts = 0;
        int mx = _opt.max_images;
        if (ui::InputIntRaw("##maximages", &mx)) _opt.max_images = std::max(10, mx);
    }

    // ImGui::SetNextItemWidth(px(160.0f));
    // ui::SliderFloat(pmsg::lbl_ring, &_opt.ring_fraction, 0.0f, 0.5f, "%.2f");
    // ui::help_on_hover(pmsg::lbl_ring_help);
    {
        static const int kSeeds[] = {250000, 500000, 1000000, 2000000, 4000000};
        static const char* kSeedNames[] = {"250k", "500k", "1M", "2M", "4M"};
        int idx = 2;
        for (int i = 0; i < 5; i++)
            if (kSeeds[i] == _opt.max_seeds) idx = i;
        ImGui::SetNextItemWidth(px(120.0f));
        if (ui::ComboRaw(ui::detail::label(pmsg::lbl_seeds), &idx, kSeedNames, 5))
            _opt.max_seeds = kSeeds[idx];
        ui::help_on_hover(pmsg::lbl_seeds_help);
    }

    const bool stale = options_changed();
    if (stale) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.75f, 1.0f));
    if (ui::Button(pmsg::btn_compute)) {
        _compute_opt = _opt;
        start(Job::Compute);
    }
    if (stale) ImGui::PopStyleColor();
    ImGui::EndDisabled();

    ImGui::Separator();
    if (_part_valid) draw_parts_table();

    // ---- view switches ----
    ImGui::BeginDisabled(!_part_valid || busy);
    if (ui::Checkbox(pmsg::show_owner_colors, &_owner_colors)) _view_dirty = true;
    if (ui::Checkbox(pmsg::show_grid, &_show_grid)) _view_dirty = true;
    ui::help_on_hover(pmsg::show_grid_help);
    ui::TextDisabledWrapped(pmsg::legend);
    ImGui::EndDisabled();

    ImGui::Separator();
    // ---- save and queue ----
    ImGui::BeginDisabled(!_part_valid || busy);
    ui::Text(pmsg::lbl_file);
    ImGui::SetNextItemWidth(px(-8.0f));
    ui::InputTextRaw("##partsave", &_save_path);
    if (ui::Button(pmsg::btn_save)) {
        try {
            std::error_code ec;
            spirula::write_partition(_part, _save_path,
                                     fs::absolute(_dataset, ec).lexically_normal().string());
            _saved_path = _save_path;
            _status.clear();
            if (_hooks.log) _hooks.log(format(pmsg::saved_to, {_save_path}));
        } catch (const std::exception& e) {
            _status = e.what();
        }
    }
    ImGui::SameLine();
    if (ui::Button(pmsg::btn_batch) && _hooks.queue_batch) {
        try {
            if (_saved_path != _save_path) {
                std::error_code ec;
                spirula::write_partition(_part, _save_path,
                                         fs::absolute(_dataset, ec).lexically_normal().string());
                _saved_path = _save_path;
            }
            _hooks.queue_batch(_saved_path, _part.num_parts);
            _status.clear();
        } catch (const std::exception& e) {
            _status = e.what();
        }
    }
    ui::help_on_hover(pmsg::btn_batch_help);
    if (!_saved_path.empty()) ui::TextDisabled(pmsg::saved_to, {_saved_path});
    ImGui::EndDisabled();
    if (_hooks.open_batch && ui::Button(pmsg::btn_open_batch)) _hooks.open_batch();

    ImGui::Separator();
    draw_merge();
}

void PartitionPanel::draw_parts_table() {
    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                                  ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY;
    const float rows = (float)std::min(_part.num_parts + 1, 9);
    if (!ImGui::BeginTable("##parts", 4, flags,
                           ImVec2(0, ImGui::GetFrameHeightWithSpacing() * (rows + 0.5f))))
        return;
    ImGui::TableSetupScrollFreeze(0, 1);
    ui::TableSetupColumn(pmsg::col_part, ImGuiTableColumnFlags_WidthStretch, 1.4f);
    ui::TableSetupColumn(pmsg::col_core, ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ui::TableSetupColumn(pmsg::col_ring, ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ui::TableSetupColumn(pmsg::col_points, ImGuiTableColumnFlags_WidthStretch, 1.3f);
    ImGui::TableHeadersRow();

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    if (ui::Selectable(pmsg::show_all_parts, _show_part < 0)) {
        _show_part = -1;
        _view_dirty = true;
    }
    ui::help_on_hover(pmsg::show_help);
    ImGui::TableNextColumn();
    ui::TextRaw(std::to_string(_part.frame_names.size()));
    ImGui::TableNextColumn();
    ImGui::TableNextColumn();
    ui::TextRaw(std::to_string(_part.point_label.size()));

    for (int k = 0; k < _part.num_parts; k++) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::PushID(k);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float h = ImGui::GetTextLineHeight();
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(p.x, p.y + 2), ImVec2(p.x + h - 2, p.y + h - 2),
            ImGui::GetColorU32(part_vec(k)), 2.0f);
        ImGui::Dummy(ImVec2(h, h));
        ImGui::SameLine();
        if (ui::SelectableRaw(std::to_string(k), _show_part == k)) {
            _show_part = _show_part == k ? -1 : k;
            _view_dirty = true;
        }
        ImGui::PopID();
        ImGui::TableNextColumn();
        ui::TextRaw(std::to_string(_part.core_count(k)));
        ImGui::TableNextColumn();
        ui::TextRaw(std::to_string(_part.ring[(size_t)k].size()));
        ImGui::TableNextColumn();
        ui::TextRaw(std::to_string(_part.part_points[(size_t)k].size()));
    }
    ImGui::EndTable();
}

void PartitionPanel::draw_merge() {
    const bool busy = _busy.load();
    ui::SeparatorText(pmsg::merge_head);
    ImGui::BeginDisabled(!_part_valid || busy);
    ui::Text(pmsg::lbl_runs);
    ImGui::SetNextItemWidth(px(-8.0f));
    if (ui::InputTextRaw("##runsdir", &_runs_dir)) _scanned = false;
    if (ui::Button(pmsg::btn_scan)) {
        const std::string json = _saved_path.empty() ? _save_path : _saved_path;
        _runs = spirula::find_partition_runs(_runs_dir, json, _part.num_parts);
        _scanned = true;
    }
    ui::help_on_hover(pmsg::btn_scan_help);
    if (_scanned) {
        int found = 0;
        for (const std::string& r : _runs) found += !r.empty();
        ImGui::SameLine();
        ui::TextDisabled(pmsg::scan_result, {found, _part.num_parts});
        const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp |
                                      ImGuiTableFlags_ScrollY;
        const float rows = (float)std::min(_part.num_parts, 6);
        if (ImGui::BeginTable("##runs", 2, flags,
                              ImVec2(0, ImGui::GetFrameHeightWithSpacing() * (rows + 1.2f)))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ui::TableSetupColumn(pmsg::col_part, ImGuiTableColumnFlags_WidthStretch, 0.6f);
            ui::TableSetupColumn(pmsg::col_run, ImGuiTableColumnFlags_WidthStretch, 3.0f);
            ImGui::TableHeadersRow();
            for (int k = 0; k < _part.num_parts; k++) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ui::TextColoredRaw(part_vec(k), std::to_string(k));
                ImGui::TableNextColumn();
                const std::string& r = _runs[(size_t)k];
                if (r.empty()) ui::TextDisabled(pmsg::run_missing);
                else ui::TextRaw(fs::path(r).filename().string());
                if (!r.empty() && ImGui::IsItemHovered()) ui::SetTooltipRaw(r);
            }
            ImGui::EndTable();
        }
        ui::Text(pmsg::lbl_file);
        ImGui::SetNextItemWidth(px(-8.0f));
        ui::InputTextRaw("##mergeout", &_merge_out);
        ImGui::BeginDisabled(found == 0);
        if (ui::Button(pmsg::btn_merge)) {
            _merge_error.clear();
            _merge_status.clear();
            start(Job::Merge);
        }
        ImGui::EndDisabled();
    }
    ImGui::EndDisabled();
    if (!_merge_error.empty())
        ui::TextColoredWrappedRaw(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), _merge_error);
    if (!_merge_status.empty()) {
        ui::TextWrappedRaw(_merge_status);
        if (_hooks.open_splat && !_merged_path.empty() && ui::Button(pmsg::btn_open_merged))
            _hooks.open_splat(_merged_path);
    }
}

}  // namespace gui
