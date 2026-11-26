// The dataset screen's right pane: run steps, input preview and live SfM view.

#include "app/gui/GuiApp.h"

#include "app/CubeLut.h"
#include "app/FramePick.h"
#include "app/gui/DatasetPrep.h"
#include "app/gui/ProgressView.h"
#include "app/gui/Subprocess.h"
#include "app/gui/Ui.h"
#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/Dense.h"
#include "i18n/catalog/Lidar.h"
#include "i18n/catalog/SourcePreview.h"

#include "imgui.h"

#ifdef SS_BUILD_SAM
#include "nn/Device.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;
namespace i18n = spirula::i18n;
namespace dmsg = spirula::i18n::msg::dataset;
namespace svmsg = spirula::i18n::msg::preview;
using spirula::i18n::Msg;

namespace gui {

// ---------------------------------------------------------------------------
// What the run is doing
// ---------------------------------------------------------------------------

namespace {

const Msg& step_name(Stage s) {
    switch (s) {
        case Stage::Frames:    return dmsg::step_frames;
        case Stage::Masks:     return dmsg::step_masks;
        case Stage::Features:  return dmsg::step_features;
        case Stage::Matching:  return dmsg::step_matching;
        case Stage::Mapping:   return dmsg::step_mapping;
        case Stage::Align:     return spirula::i18n::msg::lidar::step_align;
        case Stage::Geometry:  return dmsg::step_geometry;
        case Stage::Dense:     return spirula::i18n::msg::dense::title;
        case Stage::Finishing: return dmsg::step_finishing;
    }
    return dmsg::step_frames;
}

ImVec4 step_color(StageStatus st) {
    switch (st) {
        case StageStatus::Running: return kOk;
        case StageStatus::Failed:  return kErr;
        case StageStatus::Done:    return ImGui::GetStyle().Colors[ImGuiCol_Text];
        default:                   return kDim;
    }
}

// A run's steps as a row, in the order it takes them, the running one with its
// own bar under it. This is what the log used to be the only view of.
struct RunStep {
    Stage stage;
    const Msg* name;
};

void draw_run_steps(const RunProgress& prog, const std::vector<RunStep>& steps) {
    Stage running = Stage::Frames;
    bool any = false;
    for (size_t i = 0; i < steps.size(); i++) {
        const StageProgress p = prog.stage(steps[i].stage);
        if (i) {
            ImGui::SameLine(0.0f, px(6.0f));
            ui::TextDisabledRaw(">");
            ImGui::SameLine(0.0f, px(6.0f));
        }
        ui::TextColored(step_color(p.status), *steps[i].name);
        if (p.status == StageStatus::Running) {
            running = steps[i].stage;
            any = true;
        }
    }

    // A step that can say how far through it is gets a bar; one that cannot
    // gets its own sentence. Either way the time it has taken is shown.
    if (any) {
        const StageProgress p = prog.stage(running);
        if (p.fraction >= 0.0f) {
            timed_progress_bar(p.clock, p.detail.empty() ? nullptr : p.detail.c_str());
        } else {
            if (!p.detail.empty()) ui::TextDisabledRaw(p.detail);
            progress_time_line(p.clock);
        }
    }
}

// The views that have something to show, as a row of choices; the one drawn
// follows `implied`, the running step's, until the user picks one. -1 when
// none has anything.
int draw_view_tabs(const Msg* const* names, const bool* avail, const int* order, int n,
                   int implied, int& pinned) {
    int tab = pinned >= 0 ? pinned : (implied >= 0 ? implied : 0);
    if (!avail[tab]) {
        tab = -1;
        for (int k = 0; k < n && tab < 0; k++)
            if (avail[order[k]]) tab = order[k];
    }
    bool first = true;
    for (int k = 0; k < n; k++) {
        const int i = order[k];
        if (!avail[i]) continue;
        if (!first) ImGui::SameLine();
        first = false;
        if (ui::RadioButton(*names[i], tab == i)) {
            tab = i;
            // Picking one pins it: following the run is the default, not the
            // rule, and a user who opened the match map to look at a seam
            // should not lose it the moment mapping starts.
            pinned = i;
        }
    }
    return tab;
}

}  // namespace

// ---------------------------------------------------------------------------
// The input preview (SourcePreview.h)
// ---------------------------------------------------------------------------

void GuiApp::select_source(const std::string& path, bool scan) {
    _source_sel = path;
    _source_sel_scan = scan;
    _right_input = true;
}

int GuiApp::selected_source() const {
    if (_source_sel_scan) return -1;
    for (size_t i = 0; i < _sources.size(); i++)
        if (_sources[i].path == _source_sel) return (int)i;
    return -1;
}

int GuiApp::selected_scan() const {
    if (!_source_sel_scan) return -1;
    for (size_t i = 0; i < _lidar.size(); i++)
        if (_lidar[i].path == _source_sel) return (int)i;
    return -1;
}

bool GuiApp::source_preview_available() const {
    return selected_source() >= 0 || selected_scan() >= 0;
}

bool GuiApp::run_preview_available() {
    return preview_has_content() || !dataset_steps()->scan().empty() ||
           !_dense_preview_error.empty() || _dense_live;
}

PreviewInput GuiApp::source_preview_input() const {
    PreviewInput in;
    if (const int k = selected_scan(); k >= 0) {
        in.kind = PreviewInput::Kind::Scan;
        in.path = _lidar[(size_t)k].path;
        return in;
    }
    const int i = selected_source();
    if (i < 0) return in;
    const PrepInput& s = _sources[(size_t)i];
    std::error_code ec;
    if (s.path.empty() || (s.is_video ? !fs::is_regular_file(s.path, ec)
                                      : !fs::is_directory(s.path, ec)))
        return in;
    in.kind = s.is_video ? PreviewInput::Kind::Video : PreviewInput::Kind::Photos;
    in.path = s.path;
    in.src = preview_source((size_t)i);
    in.src.clip_start = in.src.clip_end = 0.0;
    in.pano = s.pano360;
    in.pano_orient = _sfm_job.prep.pano;
    in.mask_dir = s.mask_dir;
    in.mask_flipped = _use_found_masks && _flip_found_masks;
    in.kept_fps = input_fps(_sources, _sfm_job.prep.video_fps, (size_t)i);
    in.clip_locked = dataset_busy();
    if (s.is_video)
        if (const std::string lut = input_lut(_sources, (size_t)i); !lut.empty())
            in.lut = app::is_builtin_lut(lut) ? app::lut_display_name(lut) : lut;
    return in;
}

void GuiApp::update_source_preview_gpu() {
#ifdef SS_BUILD_SAM
    if (_lost_device_pending && !native_work_busy()) {
        _lost_device_pending = false;
        stop_inference_users();
        nn::shutdown();
    }
#endif
    if (_ffmpeg_checked != _ffmpeg_exe) {
        _ffmpeg_checked = _ffmpeg_exe;
        _ffmpeg_found = command_exists(_ffmpeg_exe);
    }
    // The inference device is one set of queues: nothing else may be on it,
    // and the choice of device must already be made, which a preview never
    // makes on its own (the button under the frame does, on request).
    const bool gpu = _native_device_frozen && !native_work_busy() && !_segment.is_open() &&
                     !_geometry_panel.is_open() && !_mask_editor.is_open();
    _source_view.set_gpu(gpu, _ffmpeg_found);
}

void GuiApp::note_lost_device(const std::string& detail) {
    log(i18n::format(svmsg::log_gpu_lost, {detail}));
    _lost_device_pending = true;
}

void GuiApp::close_source_preview() {
    _source_view.destroy_gl();
    _source_view.clear();
}

void GuiApp::draw_dataset_right(float height) {
    const bool run = run_preview_available();
    const bool input = source_preview_available();
    if (run && input) {
        if (ui::RadioButton(svmsg::pane_input, _right_input)) _right_input = true;
        ui::help_on_hover(svmsg::pane_input_help);
        ImGui::SameLine();
        if (ui::RadioButton(svmsg::pane_run, !_right_input)) _right_input = false;
        ui::help_on_hover(svmsg::pane_run_help);
    }
    if (input && (!run || _right_input)) {
        draw_source_preview(height);
        return;
    }
    _source_view.pause();
    draw_dataset_preview(height);
}

void GuiApp::draw_source_preview(float height) {
    // On the GPU a video seeks in tens of milliseconds and plays; through
    // ffmpeg a 4K frame takes over a second. So the first video shown makes
    // the device choice, as opening a mask preview does.
    if (!_native_device_frozen && !_source_gpu_asked && !native_work_busy()) {
        const int i = selected_source();
        if (selected_scan() < 0 && i >= 0 && _sources[(size_t)i].is_video &&
            preview_source((size_t)i).builtin_decode) {
            _source_gpu_asked = true;
            if (freeze_native_device()) update_source_preview_gpu();
        }
    }
    _source_view.show(source_preview_input());
    if (!_source_view.active()) return;
    float h = height;
    if (h <= 0.0f) {
        // Under the form, as the run's preview is: a splitter sets its share.
        h = px(_source_preview_h);
        float want = h;
        if (splitter_h("##sourcepreviewsplit", &want, px(160.0f), px(900.0f),
                       ImGui::GetContentRegionAvail().x)) {
            _source_preview_h = want / ui_scale();
            _layout_dirty = true;
            h = want;
        }
    } else {
        h = std::max(px(80.0f), h - ImGui::GetCursorPosY() + ImGui::GetCursorStartPos().y);
    }
    _source_view.set_mask_preview_open(_segment.is_open());
    const int i = selected_source();
    PrepInput* sel = i >= 0 ? &_sources[(size_t)i] : nullptr;
    PrepInput* video = sel && sel->is_video ? sel : nullptr;
    // A photo folder has lenses to pick from only in a packed .insp's frames.
    PrepInput* lenses =
        video || (sel && sel->packed_lenses == 2 && sel->subcameras.empty()) ? sel : nullptr;
    ImGui::BeginChild("##sourcepreview", ImVec2(0, h));
    const SourcePreview::Result r =
        _source_view.draw(ImGui::GetContentRegionAvail().y, video ? &video->clip_start : nullptr,
                          video ? &video->clip_end : nullptr,
                          lenses ? &lenses->active_lens : nullptr);
    ImGui::EndChild();
    if (_segment.is_open() && _mask_preview_scan < 0 &&
        selected_source() == _mask_preview_input && _source_view.take_selection_changed()) {
        if (const std::optional<PreviewFrame> f = _source_view.selected_frame())
            _segment.show_frame(*f);
    }
    if (r.want_gpu) freeze_native_device();
    if (r.gpu_lost) note_lost_device(r.lost_detail);
    if (r.use_ffmpeg) _sfm_job.prep.force_external_decode = true;
    const auto drop_clicks = [&](const PrepInput& in, const auto& doomed) {
        auto& clicks = _mask.clicks;
        const size_t before = clicks.size();
        clicks.erase(std::remove_if(clicks.begin(), clicks.end(),
                                    [&](const MaskClick& c) {
                                        return (c.source == in.path ||
                                                (c.source.empty() && _sources.size() == 1)) &&
                                               doomed(c);
                                    }),
                     clicks.end());
        if (clicks.size() == before) return false;
        if (clicks.empty()) {
            _mask.object_count = 1;
            _mask.current_object = 0;
        }
        return true;
    };
    if (r.lens_changed && lenses) {
        if (_segment.is_open() || _geometry_panel.is_open()) close_native_previews();
        // seeds_from_clicks would move a click on the lens now off onto the other.
        if (lenses->active_lens >= 0) {
            const std::string kept = "cam" + std::to_string(lenses->active_lens);
            drop_clicks(*lenses, [&](const MaskClick& c) {
                return c.camera != kept && c.camera.rfind(kept + "/", 0) != 0;
            });
        }
    }
    if (r.clip_changed && video) {
        // Frames offered by an open mask preview, and clicks drawn on them,
        // were placed in the old clip.
        if (_segment.is_open() || _geometry_panel.is_open()) close_native_previews();
        if (drop_clicks(*video, [](const MaskClick&) { return true; }))
            log(i18n::format(svmsg::log_clicks_dropped_clip, {video->path}));
    }
}

bool GuiApp::row_select_begin(float last_h, bool selected) {
    // Behind the row's own widgets, which take the clicks they cover; the
    // rest of the row selects it.
    const ImVec2 at = ImGui::GetCursorScreenPos();
    ImGui::SetNextItemAllowOverlap();
    const bool hit = ui::SelectableRaw("##row", selected, ImGuiSelectableFlags_AllowOverlap,
                                       ImVec2(0.0f, last_h > 0.0f ? last_h : ImGui::GetFrameHeight()));
    ImGui::SetCursorScreenPos(at);
    return hit;
}

void GuiApp::draw_dataset_steps() {
    std::vector<RunStep> all;
    for (int i = 0; i < kNumStages; i++) {
        // Only a run with a laser scan has the step at all.
        if ((Stage)i == Stage::Align && _lidar.empty() &&
            dataset_steps()->stage(Stage::Align).status == StageStatus::Pending)
            continue;
        all.push_back({(Stage)i, &step_name((Stage)i)});
    }
    draw_run_steps(*dataset_steps(), all);
}

// How much the view changed along every input, as the adaptive pass measures
// it. One row per input in the order they were given, so a folder of photos
// among the clips is a row that says it has no motion rather than a gap.
void GuiApp::draw_scan_view(float h) {
    const std::vector<ScanRow> rows = dataset_steps()->scan();
    ImGui::BeginChild("##scanview", ImVec2(0, h));
    // One scale over all of them: a clip that moves twice as much as its
    // neighbour has to LOOK it, since that is what took the frames off it.
    float top = 0.0f;
    for (const ScanRow& r : rows)
        for (float v : r.speed) top = std::max(top, v);

    const float band = px(38.0f), strip = px(12.0f);
    const ImU32 back = ImGui::GetColorU32(ImGuiCol_FrameBg);
    const ImU32 ink = ImGui::GetColorU32(ImGuiCol_PlotHistogram);
    const ImU32 keep = ImGui::GetColorU32(kOk);
    const ImU32 veil = ImGui::GetColorU32(ImGuiCol_WindowBg, 0.55f);
    for (const ScanRow& r : rows) {
        ui::TextRaw(r.name);
        ImGui::SameLine();
        if (!r.video)
            ui::TextDisabled(dmsg::scan_photos, {(long long)r.frames});
        else if (r.kept_n > 0)
            ui::TextDisabled(dmsg::scan_kept_frames, {(long long)r.kept_n});
        else
            ui::TextDisabledRaw("");

        const float w = ImGui::GetContentRegionAvail().x;
        const float tall = band + (r.kept.empty() ? 0.0f : strip + px(2.0f));
        if (w < px(48.0f)) break;
        const ImVec2 at = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(at, ImVec2(at.x + w, at.y + band), back);
        const float step = w / (float)kScanSlices;
        for (size_t i = 0; i < r.speed.size(); i++) {
            if (r.hits[i] <= 0 || top <= 0.0f) continue;
            const float v = std::min(1.0f, r.speed[i] / top);
            const float x0 = at.x + step * (float)i;
            dl->AddRectFilled(ImVec2(x0, at.y + band * (1.0f - v)),
                              ImVec2(x0 + std::max(step - 1.0f, 1.0f), at.y + band),
                              ink);
        }
        // What is not measured yet, dimmed rather than left blank: an empty
        // stretch of a slow clip looks the same as one nobody has reached.
        if (r.video && r.done < 1.0f)
            dl->AddRectFilled(ImVec2(at.x + w * r.done, at.y),
                              ImVec2(at.x + w, at.y + band), veil);
        if (!r.kept.empty()) {
            const float y = at.y + band + px(2.0f);
            dl->AddRectFilled(ImVec2(at.x, y), ImVec2(at.x + w, y + strip), back);
            for (size_t i = 0; i < r.kept.size(); i++) {
                const float v = std::min(1.0f, std::max(0.0f, r.kept[i]));
                if (v <= 0.0f) continue;
                const float x0 = at.x + step * (float)i;
                // The slowest stretch still keeps frames, so it still gets a
                // mark: a strip that thins to nothing reads as a gap.
                const float tall = std::max(strip * v, px(2.0f));
                dl->AddRectFilled(ImVec2(x0, y + strip - tall),
                                  ImVec2(x0 + std::max(step - 1.0f, 1.0f), y + strip),
                                  keep);
            }
        }
        ImGui::Dummy(ImVec2(w, tall));
        ImGui::Spacing();
    }
    ImGui::EndChild();
}

// The frames are the ones wanted. The record is told first: the thresholds
// may have moved since the run started, and a workspace that restored the
// older ones would find its own frames "made with other settings".
void GuiApp::continue_frame_review() {
    bool made[kNumSteps] = {};
    made[(int)Step::Frames] = true;
    write_record_settings(_workspace, dataset_settings_json(capture_dataset_settings()),
                          encode_record_inputs({_sources, _mask.clicks}, _workspace), made);
    _sfm.continue_review();
}

namespace {

// A threshold set against the distribution it cuts: one bar a bin of `values`,
// dimmed where the threshold leaves them out, and a line a drag moves.
bool histogram_slider(const char* id, const std::vector<float>& values, float lo,
                      float hi, float* v, float v_min, float v_max, ImVec2 size,
                      bool cut_above, const std::function<ImU32(float)>& ink) {
    constexpr int kBins = 48;
    int count[kBins] = {};
    int top = 0;
    for (float x : values) {
        const int b = std::clamp((int)((x - lo) / (hi - lo) * kBins), 0, kBins - 1);
        top = std::max(top, ++count[b]);
    }
    const ImVec2 at = ImGui::GetCursorScreenPos();
    ui::InvisibleButtonRaw(id, size);
    bool changed = false;
    if (ImGui::IsItemActive()) {
        const float x = (ImGui::GetIO().MousePos.x - at.x) / size.x;
        const float to = std::clamp(lo + x * (hi - lo), v_min, v_max);
        changed = to != *v;
        *v = to;
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(at, ImVec2(at.x + size.x, at.y + size.y),
                      ImGui::GetColorU32(ImGuiCol_FrameBg), ImGui::GetStyle().FrameRounding);
    const float step = size.x / (float)kBins;
    for (int b = 0; b < kBins && top > 0; b++) {
        if (count[b] == 0) continue;
        const float mid = lo + ((float)b + 0.5f) / kBins * (hi - lo);
        const float tall = std::max(px(2.0f), size.y * (float)count[b] / (float)top);
        const ImU32 c = ink(mid);
        dl->AddRectFilled(ImVec2(at.x + step * (float)b, at.y + size.y - tall),
                          ImVec2(at.x + step * (float)b + std::max(step - 1.0f, 1.0f),
                                 at.y + size.y),
                          (mid < *v) != cut_above ? (c & 0x00FFFFFF) | 0x50000000 : c);
    }
    const float x = at.x + (*v - lo) / (hi - lo) * size.x;
    dl->AddLine(ImVec2(x, at.y), ImVec2(x, at.y + size.y),
                ImGui::GetColorU32(ImGuiCol_Text), px(2.0f));
    return changed;
}

}  // namespace

// Every frame the run chose, input by input, under the settings that decide
// which are left out. The picture follows the settings as they are moved: each
// pick's measurements are held, so another threshold is a recount, not a run.
void GuiApp::draw_sharpness_view(float h) {
    const float y0 = ImGui::GetCursorPosY();
    PrepJob& prep = _sfm_job.prep;
    const PickStyle style{kErr, kWarn, kOk, prep.blur_px};
    const std::vector<std::string> names = _picks.names();
    // What is shown is what pressing Update would leave: the run's own
    // verdicts when it would change nothing, the frames the dataset has
    // sorted again when it would sort, every frame when it makes them again.
    const bool reviewing = _sfm.reviewing();
    const StepPlan& frames = dataset_plan()[Step::Frames];
    const bool settled = !reviewing && !dataset_busy() && frames.act == Act::Reuse;
    const app::PickRule rule = pick_rule(prep);
    // Walked again only when the sheet or something the walk reads moves.
    char key[160];
    std::snprintf(key, sizeof key, "%llu %d %d %d %g %d %d",
                  (unsigned long long)_picks.version(), settled, frames.sort, reviewing,
                  rule.max_blur, rule.neighbour, rule.similar_bits);
    if (_pick_walk.key != key) {
        _pick_walk = PickWalk{};
        _pick_walk.key = key;
        _pick_walk.rows.resize(names.size());
        for (size_t ri = 0; ri < names.size(); ri++) {
            std::vector<Pick>& row = _pick_walk.rows[ri] = _picks.picks(ri);
            // Left out as the frames were made: it has no camera, so a sort
            // neither judges it nor brings it back.
            const bool in_place = _picks.in_place(ri);
            const auto as_run = [&](const Pick& p) {
                return settled && (!frames.sort || (!in_place && p.verdict != app::PickVerdict::Kept));
            };
            std::vector<Pick> walked;
            for (const Pick& p : row)
                if (!as_run(p)) walked.push_back(p);
            app::PickRule r = app::whole_capture(rule, walked);
            for (size_t i = 0; i < row.size(); i++) {
                Pick& p = row[i];
                const app::PickVerdict ran = p.verdict;
                if (as_run(p)) {
                    p.as_run = true;
                } else {
                    p.dist = r.distance(p.hash);
                    p.as_run = r.judge(p) == ran;
                }
                _pick_walk.differs = _pick_walk.differs || !p.as_run;
                if (p.blur >= 0.0f) _pick_walk.blurs.push_back(p.blur);
                if (i == 0) continue;
                // Against the frame before it, whatever was kept: what the
                // histogram shows must not move when its own threshold does.
                int bits = 0;
                for (uint64_t x = row[i].hash ^ row[i - 1].hash; x; x &= x - 1) bits++;
                _pick_walk.steps.push_back((float)bits);
            }
        }
    }
    const std::vector<std::vector<Pick>>& rows = _pick_walk.rows;
    const std::vector<float>& moved = _pick_walk.blurs;
    const std::vector<float>& steps = _pick_walk.steps;
    const bool differs = _pick_walk.differs;

    // ---- the settings, which stay put while the frames scroll under them
    const bool locked = dataset_locked(Stage::Frames) && !reviewing;
    if (reviewing) {
        if (ui::Button(dmsg::review_continue)) continue_frame_review();
        ImGui::SameLine();
        ui::TextColoredWrapped(kWarn, dmsg::review_hint);
    }
    ImGui::BeginDisabled(locked);
    ui::Checkbox(dmsg::skip_blurry, &prep.skip_blurry);
    ui::help_on_hover(dmsg::skip_blurry_help);
    ImGui::SameLine();
    ImGui::BeginDisabled(!prep.skip_blurry);
    ui::Checkbox(dmsg::blur_neighbour, &prep.blur_neighbour);
    ui::help_on_hover(dmsg::blur_neighbour_help);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::EndDisabled();
    ui::Checkbox(dmsg::scan_show_pictures, &_picks_pictures);
    if (_picks_pictures) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(px(140.0f));
        ui::SliderFloat(dmsg::scan_picture_size, &_picks_size, 24.0f, 256.0f, "%.0f");
    }
    ImGui::BeginDisabled(locked);
    const float half =
        (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    const ImVec2 bars(std::max(px(120.0f), half), px(44.0f));
    char value[32];
    ImGui::BeginGroup();
    ImGui::BeginDisabled(!prep.skip_blurry);
    ui::Text(dmsg::blur_px);
    ui::help_on_hover(dmsg::blur_px_help);
    ImGui::SameLine();
    std::snprintf(value, sizeof value, "%.0f px", prep.blur_px);
    ui::TextDisabledRaw(value);
    histogram_slider("##blurhist", moved, 0.0f, 40.0f, &prep.blur_px, 1.0f, 40.0f, bars,
                     /*cut_above=*/true,
                     [&](float blur) { return PickSheetView::colour(style, blur); });
    // Whole pixels: a finer threshold than the measurement means nothing, and
    // every other value is one more "made with other settings" in the plan.
    prep.blur_px = std::round(prep.blur_px);
    ImGui::EndDisabled();
    ImGui::EndGroup();
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::BeginDisabled(!prep.skip_blurry);
    ui::Text(dmsg::similar_bits);
    ui::help_on_hover(dmsg::similar_bits_help);
    ImGui::SameLine();
    std::snprintf(value, sizeof value, "%d", prep.similar_bits);
    ui::TextDisabledRaw(value);
    float similar = (float)prep.similar_bits;
    const ImU32 plain = ImGui::GetColorU32(ImGuiCol_PlotHistogram);
    if (histogram_slider("##similarhist", steps, 0.0f, 40.0f, &similar, 0.0f, 32.0f, bars,
                         /*cut_above=*/false, [plain](float) { return plain; }))
        prep.similar_bits = (int)std::lround(similar);
    ImGui::EndDisabled();
    ImGui::EndGroup();
    ImGui::EndDisabled();

    if (!prep.skip_blurry || moved.empty()) ui::TextDisabledWrapped(dmsg::blur_off_hint);
    if (differs && !dataset_busy()) {
        // The same button as the form's, where the settings were moved.
        ImGui::BeginDisabled(native_work_busy() || _batch_active);
        if (ui::Button(dmsg::sharpness_apply)) start_dataset_job();
        ImGui::EndDisabled();
        ImGui::SameLine();
        ui::TextColoredWrapped(kWarn, dmsg::sharpness_preview);
    }

    const float rest = std::max(px(60.0f), h - (ImGui::GetCursorPosY() - y0));
    if (_picks_view.viewing() && _picks_view.open_row() < rows.size()) {
        _picks_view.draw_viewer(_picks, rows[_picks_view.open_row()],
                                ImVec2(ImGui::GetContentRegionAvail().x, rest), style);
        return;
    }
    ImGui::BeginChild("##sharpview", ImVec2(0, rest));
    const float wheel = ImGui::GetIO().MouseWheel;
    if (ImGui::IsWindowHovered() && ImGui::GetIO().KeyCtrl && wheel != 0.0f) {
        _picks_pictures = true;
        _picks_size = std::clamp(_picks_size * std::pow(1.15f, wheel), 24.0f, 256.0f);
    }
    for (size_t ri = 0; ri < names.size(); ri++) {
        if (rows[ri].empty()) continue;
        long long kept = 0, blurry = 0;
        for (const Pick& p : rows[ri]) {
            kept += p.verdict == app::PickVerdict::Kept ? 1 : 0;
            blurry += p.verdict == app::PickVerdict::Blurry ? 1 : 0;
        }
        ui::Text(dmsg::sharpness_counts,
                 {names[ri], kept, blurry, (long long)rows[ri].size() - kept - blurry});
        const float w = ImGui::GetContentRegionAvail().x;
        if (w < px(48.0f)) break;
        _picks_view.draw(_picks, ri, rows[ri], w,
                         _picks_pictures ? px(_picks_size) : 0.0f, style);
        ImGui::Spacing();
    }
    ImGui::EndChild();
}

// Which of the three previews the running step implies. Frames, masks and
// feature extraction all show pictures, so they share one.
int GuiApp::preview_for_stage() {
    // A finished run keeps showing what its last step was on: the model is
    // what somebody was watching when it ended, and falling back to the frames
    // would hide it behind a click nobody knows to make.
    if (!dataset_busy()) return _preview_last_stage;
    switch (dataset_steps()->current()) {
        // Nothing is written while the motion is being measured, so the reel
        // has nothing to show and the curves have everything.
        case Stage::Frames:
            if (_sfm.reviewing()) return 7;
            return dataset_steps()->scanning() ? 6 : 0;
        case Stage::Masks:     return 1;
        case Stage::Features:  return 2;
        case Stage::Matching:  return 3;
        case Stage::Align:
        case Stage::Geometry:  return 5;
        case Stage::Dense:     return 4;
        case Stage::Mapping:
        case Stage::Finishing: return 4;
    }
    return -1;
}

// What the child has written about itself since the last look. Two stats and,
// when something changed, a file of about a megabyte -- so it runs at 2 Hz
// rather than every frame.
void GuiApp::poll_sfm_progress() {
    if (effective_engine() != Engine::BuiltIn) return;
    const double now = ImGui::GetTime();
    if (_sfm_polled_at > 0.0 && now - _sfm_polled_at < 0.5) return;
    _sfm_polled_at = now;

    const Stage stage = dataset_steps()->current();
    const bool preparing = dataset_busy() && (stage == Stage::Frames || stage == Stage::Masks);
    // Reused features still populate the reel when a run skips extraction.
    if (!preparing)
        _features.start(_sfm.sfm_image_dir(), _sfm.sfm_mask_dir(),
                        _sfm.features_dir(), &_film_features,
                        _sfm.thumbs_dir());
    _pairs_view.configure(_sfm.sfm_image_dir(), _sfm.sfm_mask_dir(),
                          _sfm.features_dir(), _sfm.matches_path(),
                          _sfm.live_matches_path());

    const std::string dir = _sfm.progress_dir();
    if (dir.empty()) return;

    PairMatrix pm;
    if (read_pair_matrix(dir, _pairs_mtime, pm)) _matrix.set(pm);
    // matches.bin is the whole truth and outlives the live file, which the run
    // deletes with the rest of the intermediates.
    const bool mapping = !preparing && stage != Stage::Features && stage != Stage::Matching;
    if ((!dataset_busy() || mapping) &&
        read_pair_matrix_from_matches(_sfm.matches_path(), _matches_mtime, pm))
        _matrix.set(pm);

    LiveModel lm;
    if (read_live_model(dir, _model_mtime, lm)) {
    #if 0
        // Each snapshot is framed on its own cameras, and the last is re-gauged
        // as well; carrying the camera along keeps the picture still through
        // both, where re-framing would jump.
        float moved[12];
        if (_model_attached && snapshot_motion(_live_model, lm, moved))
            _model_view.move_view(moved);
    #endif
        _live_model = std::move(lm);
    #if 1
        // The mapper's frame is the seed pair's, upside down as often as not:
        // navigate about the cameras' up until the model is levelled.
        float up[3] = {0, 0, 1};
        if (!_live_model.ds.gauge_oriented) snapshot_up(_live_model, up);
        _model_view.set_nav_up(up);
    #endif
        // The mapper's own output is a wall of per-registration detail, so the
        // default log used to go quiet for the longest step. These are the
        // model in hand, not the bar -- a seed retry starts one over.
        if (dataset_busy() && _live_model.n_images) {
            RunProgress& p = _sfm.steps();
            p.note(Stage::Mapping,
                   i18n::format(dmsg::model_live_counts,
                                {(long long)_live_model.n_registered,
                                 (long long)_live_model.n_images,
                                 (long long)_live_model.n_points}),
                   /*detail=*/false);
        }
        // Same key every time: the pose the user navigated to belongs to the
        // scene, not to the snapshot, and re-framing on every one of them
        // would make the view unusable while it is most worth watching.
        _model_view.attach_preview_data(_live_model.ds, _live_model.post,
                                        "sfm-live", /*radius=*/1.0f,
                                        /*with_cameras=*/true);
        _model_attached = true;
    }
}

// Gives up the previews of a run and the files behind them. The run leaves its
// features and matches on disk for this screen to read after it ends, so the
// screen being done with them is where they go.
void GuiApp::reset_dataset_preview(bool sweep) {
    close_native_previews();
    _features.stop();
    if (sweep) _sfm.sweep_intermediates();
    _model_view.detach();
    _model_view.destroy_gl();
    _model_attached = false;
    _live_model = LiveModel{};
    _matrix.clear();
    _matrix.destroy_gl();
    // _picks stays: a run that keeps the frames keeps what was measured on
    // them (launch_dataset_job drops it when it is another folder's).
    _picks_view.destroy_gl();
    _pairs_view.clear();
    _pairs_view.destroy_gl();
    for (FilmReel* f : {&_film_frames, &_film_masks, &_film_features,
                        &_film_geometry}) {
        f->clear();
        f->destroy_gl();
    }
    _model_mtime = _pairs_mtime = _matches_mtime = _dense_model_mtime = 0;
    _dense_model_read = {};
    _dense_preview_error.clear();
    _dense_polled_at = -1.0;
    _dense_refresh_seconds = 0.5;
    _dense_live = false;
    _preview_tab = -1;
    _preview_last_stage = -1;
}

bool GuiApp::preview_has_content() const {
    return _film_frames.has_frames() || _film_masks.has_frames() ||
           _film_features.has_frames() || !_matrix.empty() || _model_attached ||
           _film_geometry.has_frames() || (_picks.any() && _picks_workspace == _workspace);
}

// The frames / match map / model area; it follows the running step until the
// user picks one. `height` 0 asks for the splitter (the one-column layout); a
// column of its own hands its height down instead.
void GuiApp::draw_dataset_preview(float height) {
    // Null where the view is not a reel: the match map, the model, the motion.
    FilmReel* reels[8] = {&_film_frames, &_film_masks, &_film_features,
                          nullptr, nullptr, &_film_geometry, nullptr, nullptr};
    const bool avail[8] = {_film_frames.has_frames(), _film_masks.has_frames(),
                           _film_features.has_frames(), !_matrix.empty(),
                           _model_attached, _film_geometry.has_frames(),
                           !dataset_steps()->scan().empty(),
                           _picks.any() && _picks_workspace == _workspace};
    bool any_avail = false;
    for (bool v : avail) any_avail = any_avail || v;
    if (!any_avail && _dense_preview_error.empty() && !_dense_live) return;

    ImGui::SameLine();
    if (ui::Checkbox(dmsg::show_run_preview, &_show_preview)) {
        _dense_polled_at = -1.0;
        _dense_refresh_seconds = 0.5;
        _dense_preview_error.clear();
        if (!_show_preview && (_dense_live || _dense_model_mtime != 0)) {
            _dense_live = true;
            _model_view.detach(); _model_view.destroy_gl();
            _live_model = LiveModel{}; _model_attached = false;
            _dense_model_mtime = 0;
        }
        save_settings();
    }
    ui::help_on_hover(dmsg::show_run_preview_help);
    if (!_show_preview) return;
    if (!_dense_preview_error.empty()) {
        ui::TextWrapped(spirula::i18n::msg::dense::preview_paused, {_dense_preview_error});
        ui::help_on_hover(spirula::i18n::msg::dense::live_phase_help);
    }

    // The run has stopped to be looked at: a view pinned earlier gives way.
    if (_sfm.reviewing() && !_was_reviewing) _preview_tab = -1;
    _was_reviewing = _sfm.reviewing();
    const int implied = preview_for_stage();
    if (dataset_busy() && implied >= 0) _preview_last_stage = implied;
    const Msg* names[8] = {&dmsg::view_frames, &dmsg::view_masks,
                           &dmsg::view_features, &dmsg::view_matrix,
                           &dmsg::view_model, &dmsg::view_geometry,
                           &dmsg::view_motion, &dmsg::view_sharpness};
    // In the order a run reaches them: the frames are sorted before anything
    // is made from them.
    const int order[8] = {0, 7, 6, 1, 2, 3, 4, 5};
    const int tab = draw_view_tabs(names, avail, order, 8, implied, _preview_tab);
    if (tab == 3) ui::help_on_hover(dmsg::matrix_help_edit, {dmsg::matrix_help.get()});
    if (tab == 6) ui::help_on_hover(dmsg::frame_spacing_help);
    if (tab == 7) ui::help_on_hover(dmsg::sharpness_view_help);

    // In a column of its own the height is the column's; otherwise a splitter,
    // as the log has -- what any of these views is worth depends entirely on
    // which one the user is looking at, so it is theirs to set.
    float h = height;
    if (h <= 0.0f) {
        h = px(_preview_h);
        const float line = ImGui::GetTextLineHeightWithSpacing();
        float want = h;
        if (splitter_h("##previewsplit", &want, 2.0f * line, px(600.0f),
                       ImGui::GetContentRegionAvail().x)) {
            _preview_h = want / ui_scale();
            _layout_dirty = true;
            h = want;
        }
    } else {
        h = std::max(px(60.0f), h - ImGui::GetCursorPosY() +
                                    ImGui::GetCursorStartPos().y);
    }

    // -1 when only the dense live view is up and no tab has content yet.
    if (tab >= 0 && reels[tab]) {
        reels[tab]->draw(h - px(8.0f));
        return;
    }
    if (tab == 6) {
        draw_scan_view(h - px(8.0f));
        return;
    }
    if (tab == 7) {
        draw_sharpness_view(h - px(8.0f));
        return;
    }
    if (tab == 3) {
        ImGui::BeginChild("##matrix", ImVec2(0, h));
        const float avail = ImGui::GetContentRegionAvail().x;
        // The map keeps its square and the pair it points at gets the rest of
        // the row, down to nothing on a column too narrow to hold both. The
        // legend goes under the map, inside the same height.
        const float side =
            std::min(h - px(8.0f) - ImGui::GetTextLineHeightWithSpacing(),
                     avail > px(520.0f) ? avail * 0.5f : avail);
        PairBlock block;
        // Grouped so the pair view sits beside the map rather than beside the
        // legend under it.
        ImGui::BeginGroup();
        const bool over = _matrix.draw(side, block);
        if (over) _pairs_view.show(block);
        // Editing the matches is for the rare capture that needs it, so it
        // is on the map's context menu rather than a button under it.
        if (over && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
            ImGui::OpenPopup("##matrixmenu");
        if (ImGui::BeginPopup("##matrixmenu")) {
            std::error_code ec;
            const std::string matches = _sfm.matches_path();
            const bool can = !dataset_busy() && !matches.empty() &&
                             fs::exists(fs::u8path(matches), ec);
            if (ui::MenuItem(dmsg::matrix_edit_matches, nullptr, false, can)) {
                _matrix_editor.configure(_sfm.sfm_image_dir(), _sfm.sfm_mask_dir(),
                                         _sfm.features_dir(), matches);
                _matrix_editor.open();
            }
            ImGui::EndPopup();
        }
        ImGui::EndGroup();
        const float rest = avail - side - ImGui::GetStyle().ItemSpacing.x;
        if (rest > px(160.0f)) {
            ImGui::SameLine();
            ImGui::BeginChild("##pairview", ImVec2(rest, h - px(8.0f)));
            _pairs_view.draw(ImGui::GetContentRegionAvail());
            ImGui::EndChild();
        }
        ImGui::EndChild();
        return;
    }
    ui::Text(dmsg::model_live_counts,
             {(long long)_live_model.n_registered, (long long)_live_model.n_images,
              (long long)_live_model.n_points});
    if (_live_model.provisional) {
        ui::TextDisabledWrapped(spirula::i18n::msg::dense::provisional_preview);
        ui::help_on_hover(spirula::i18n::msg::dense::live_phase_help);
    }
    if (_live_model.filtered) {
        ui::TextDisabledWrapped(spirula::i18n::msg::dense::filtered_preview);
        ui::help_on_hover(spirula::i18n::msg::dense::live_phase_help);
    }
    if (_live_model.empty()) {
        ui::TextDisabledWrapped(dmsg::model_waiting);
        return;
    }
    // Width capped against the height: the band is as wide as the window, and
    // a 1600x150 letterbox of a 90-degree view shows a slice of the scene with
    // everything in it apparently enormous.
    const float w = std::min(ImGui::GetContentRegionAvail().x, h * 16.0f / 9.0f);
    ImGui::BeginChild("##livemodel", ImVec2(w, h));
    _model_view.draw(/*training=*/false);
    ImGui::EndChild();
}

}  // namespace gui
