// RenderPanel.cpp -- the render mode's panel, and the keys that drive it, for
// the session in RenderSession.h.

#include "app/gui/render/RenderSession.h"

#include "app/gui/ProgressView.h"

#include "app/gui/Subprocess.h"
#include "app/gui/Ui.h"
#include "app/gui/ViewportPanel.h"
#include "data/DatasetParser.h"
#include "i18n/catalog/EditTransform.h"
#include "i18n/catalog/Render.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;
namespace msg = spirula::i18n::msg::render;
namespace xmsg = spirula::i18n::msg::xform;
using spirula::i18n::Msg;
using spirula::i18n::format;

namespace gui::render {

namespace {

constexpr double kPi = 3.14159265358979323846;
const ImVec4 kErr(1.0f, 0.45f, 0.45f, 1.0f);
const ImVec4 kOk(0.35f, 0.85f, 0.45f, 1.0f);    // GuiApp's, for a job done
const ImVec4 kDim(0.62f, 0.64f, 0.68f, 1.0f);

struct Resolution { int w, h; const Msg* name; };
const Resolution kResolutions[] = {
    {1280, 720, &msg::res_hd}, {1920, 1080, &msg::res_full_hd},
    {2560, 1440, &msg::res_qhd}, {3840, 2160, &msg::res_4k},
    {1080, 1920, &msg::res_vertical}, {1080, 1080, &msg::res_square},
    {3840, 1920, &msg::res_2to1_4k}, {5760, 2880, &msg::res_2to1_6k},
    {7680, 3840, &msg::res_2to1_8k},
};
constexpr int kNumResolutions = (int)(sizeof kResolutions / sizeof kResolutions[0]);

const Msg* const kProjections[kNumProjections] = {
    &msg::proj_perspective, &msg::proj_fisheye, &msg::proj_equisolid,
    &msg::proj_equirect};
const Msg* const kTransitions[kNumTransitions] = {
    &msg::tr_cut, &msg::tr_crossfade, &msg::tr_dip, &msg::tr_wipe, &msg::tr_iris, &msg::tr_zoom,
    &msg::tr_sweep, &msg::tr_grow, &msg::tr_dust, &msg::tr_spiral, &msg::tr_scatter,
    &msg::tr_rain, &msg::tr_dissolve, &msg::tr_ripple};
const Msg* const kPointStyles[kNumPointStyles] = {
    &msg::pt_square, &msg::pt_circle, &msg::pt_gaussian, &msg::pt_sphere};
const Msg* const kCurves[kNumCurves] = {&msg::curve_spline, &msg::curve_catmull,
                                        &msg::curve_linear};
const Msg* const kImageFormats[kNumImageFormats] = {&msg::format_png, &msg::format_png_alpha,
                                                    &msg::format_jpeg};
const Msg* const kCodecs[kNumCodecs] = {&msg::codec_h264, &msg::codec_h265, &msg::codec_av1,
                                        &msg::codec_gif, &msg::codec_av1_webm};
// As the list shows them: the WebM beside the other AV1.
const int kCodecOrder[kNumCodecs] = {0, 1, 2, 4, 3};
// The splat primitives a model can be rendered as: the automatic choice,
// then ViewportPanel's names.
const char* const kPrimitives[4] = {"", "3dgs", "mip", "3dgut"};

// Keys the whole panel answers to, beside the viewport's own. A key is an
// identifier, so it never gets translated.
struct KeyRow { const char* label; ImGuiKey key; bool ctrl, shift, alt; };

std::string preset_label(const LensPreset& p) {
    return p.arg.empty() ? std::string(p.name->get()) : format(*p.name, {p.arg});
}

std::string seconds(double t) {
    char b[32];
    std::snprintf(b, sizeof b, "%.2f s", t);
    return b;
}

bool combo_msgs(const char* id, int* cur, const Msg* const* items, int n) {
    std::vector<const Msg*> v(items, items + n);
    return ui::ComboRaw(id, cur, v);
}

}  // namespace


// ===========================================================================
// Keys
// ===========================================================================

void RenderSession::handle_keys(bool over_view, bool over_list) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput || ImGui::IsAnyItemActive() || _xform.active()) return;
    // A chord reaches the panel only while focus is inside the main window,
    // so a dialog keeps its own Ctrl+Z. Routed from the root so the viewport
    // and timeline count.
    const ImGuiInputFlags route = ImGuiInputFlags_RouteFocused |
                                  ImGuiInputFlags_RouteFromRootWindow;
    const ImGuiInputFlags repeat = route | ImGuiInputFlags_Repeat;
    auto pressed = [&](ImGuiKeyChord k) { return ImGui::Shortcut(k, route); };
    // All asked for before any acts, wherever the pointer is: a chord not
    // asked for this frame has no route on the next, and its press is lost.
    const bool undo_k = pressed(ImGuiMod_Ctrl | ImGuiKey_Z);
    const bool redo_y = pressed(ImGuiMod_Ctrl | ImGuiKey_Y);
    const bool redo_z = pressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z);
    const bool save_k = pressed(ImGuiMod_Ctrl | ImGuiKey_S);
    const bool all_k = pressed(ImGuiMod_Ctrl | ImGuiKey_A);
    const bool none_k = pressed(ImGuiMod_Ctrl | ImGuiKey_D);
    const bool play_k = pressed(ImGuiKey_Space);
    const bool home_k = pressed(ImGuiKey_Home);
    const bool end_k = pressed(ImGuiKey_End);
    const bool fwd_k = ImGui::Shortcut(ImGuiKey_Period, repeat);
    const bool back_k = ImGui::Shortcut(ImGuiKey_Comma, repeat);
    const bool next_k = pressed(ImGuiKey_PageDown);
    const bool prev_k = pressed(ImGuiKey_PageUp);
    const bool del_k = pressed(ImGuiKey_Delete);
    const bool bksp_k = pressed(ImGuiKey_Backspace);
    const bool a_k = pressed(ImGuiKey_A);
    const bool alt_a_k = pressed(ImGuiMod_Alt | ImGuiKey_A);
    const bool x_k = pressed(ImGuiKey_X);
    const bool k_k = pressed(ImGuiKey_K);
    const bool i_k = pressed(ImGuiKey_I);
    const bool zero_k = pressed(ImGuiKey_0);
    const bool kp0_k = pressed(ImGuiKey_Keypad0);
    const bool t_k = pressed(ImGuiKey_T);
    const bool g_k = pressed(ImGuiKey_G);
    const bool r_k = pressed(ImGuiKey_R);
    const bool s_k = pressed(ImGuiKey_S);
    // Flying: every other key is the flight's.
    if (_flying) {
        const bool keep = pressed(ImGuiKey_Enter);
        const bool keep_kp = pressed(ImGuiKey_KeypadEnter);
        const bool drop = pressed(ImGuiKey_Escape);
        if (keep || keep_kp) stop_flight(true);
        else if (drop) stop_flight(false);
        return;
    }

    if (undo_k) { undo(); return; }
    if (redo_y || redo_z) {
        redo();
        return;
    }
    if (save_k) {
        if (_project_path.empty()) {
            if (_pick) _pick(Pick::SaveProject, default_project_dir(_sources.empty() ? "" : _sources[0].path), suggested_project_name());
        } else {
            save_to(_project_path);
        }
        return;
    }
    if (all_k) {
        _sel.assign(_project.keys.size(), 1);
        return;
    }
    if (none_k) {
        _sel.assign(_project.keys.size(), 0);
        return;
    }
    if (play_k) {
        set_playing(!_playing);
        return;
    }
    const double frame = 1.0 / std::max(_project.output.fps, 1.0);
    if (home_k && !_project.keys.empty()) _time = _project.keys.front().time;
    if (end_k) _time = _project.duration();
    if (fwd_k) _time = std::min(_time + frame, _project.duration());
    if (back_k) _time = std::max(_time - frame, 0.0);
    if (next_k || prev_k) {
        double best = _time;
        for (double t : trajectory().key_times()) {
            if (next_k && t > _time + 1e-6 && (best == _time || t < best)) best = t;
            if (!next_k && t < _time - 1e-6 && (best == _time || t > best)) best = t;
        }
        _time = best;
    }
    bool any = false;
    for (uint8_t v : _sel) any = any || v;
    // Deleting is about the selection, wherever the pointer is; over the
    // viewport the keys left are refitted to the path.
    if (any && (del_k || bksp_k)) {
        delete_selected(over_view && !over_list);
        return;
    }
    // Over the list or the timeline, the letters that pick and delete.
    if (over_list && !over_view) {
        if (a_k) {
            bool all = !_project.keys.empty();
            for (uint8_t v : _sel) all = all && v;
            _sel.assign(_project.keys.size(), all ? 0 : 1);
        } else if (alt_a_k) {
            _sel.assign(_project.keys.size(), 0);
        } else if (x_k && any) {
            delete_selected();
        }
        return;
    }
    if (!over_view) return;

    // Letters the camera also flies with are the camera's until a key is
    // selected, which is what G / R / S then act on.
    const bool letters = blocks_fly_keys();
    if (k_k || i_k) {
        add_key(next_key_time(), true);
        _fly_block_key = k_k ? ImGuiKey_K : ImGuiKey_I;
        return;
    }
    if (kp0_k || zero_k) {
        look_through(_time);
        return;
    }
    if (t_k) {
        _pick_target = true;
        return;
    }
    if (!letters) return;
    if (a_k) {
        bool all = true;
        for (uint8_t s : _sel) all = all && s;
        _sel.assign(_project.keys.size(), all ? 0 : 1);
        return;
    }
    if (alt_a_k) {
        _sel.assign(_project.keys.size(), 0);
        return;
    }
    if (x_k) {
        delete_selected(true);
        return;
    }
    const struct { bool on; XformKind kind; } ops[] = {
        {g_k, XformKind::Move}, {r_k, XformKind::Rotate}, {s_k, XformKind::Scale}};
    for (const auto& op : ops)
        if (op.on) {
            begin_xform(op.kind, _mouse[0], _mouse[1], false);
            return;
        }
}


// ===========================================================================
// The panel
// ===========================================================================

void RenderSession::draw_status() {
    if (!_have_project) return;
    if (_flying) {
        ui::TextDisabled(msg::fly_hint);
        return;
    }
    if (_xform.active()) {
        ui::TextDisabled(_xform.kind() == XformKind::Scale && single_selected() >= 0
                             ? msg::hint_op_fov : msg::hint_op);
        return;
    }
    if (_pick_target || _pick_waiting) {
        ui::TextDisabled(msg::hint_pick);
        return;
    }
    bool any = false;
    for (uint8_t s : _sel) any = any || s;
    ui::TextDisabled(any ? msg::hint_selected : msg::hint_idle);
}

void RenderSession::draw_overwrite_popup() {
    if (_ask_overwrite) {
        ui::OpenPopup(msg::overwrite_title);
        _ask_overwrite = false;
    }
    ImGui::SetNextWindowSize(ImVec2(px(480.0f), 0.0f), ImGuiCond_Appearing);
    if (!ui::BeginPopupModal(msg::overwrite_title)) return;
    const std::string path = _project.output.path;
    if (_overwrite_frames > 0)
        ui::TextWrapped(msg::overwrite_frames, {path, (long long)_overwrite_frames});
    else
        ui::TextWrapped(msg::overwrite_file, {path});
    if (ui::Button(msg::overwrite_replace)) {
        ImGui::CloseCurrentPopup();
        start_export(false, true);
    }
    ImGui::SameLine();
    if (ui::Button(msg::render_cancel)) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void RenderSession::draw_panel() {
    if (!_panel) return;
    draw_overwrite_popup();
    {
        const ImVec2 wp = ImGui::GetWindowPos(), ws = ImGui::GetWindowSize();
        _panel_rect[0] = wp.x;
        _panel_rect[1] = wp.y;
        _panel_rect[2] = ws.x;
        _panel_rect[3] = ws.y;
    }
    handle_keys(_mouse_in, _timeline_hovered ||
                               ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows));
    const ImGuiStyle& st = ImGui::GetStyle();
    const float strip = ImGui::GetFrameHeightWithSpacing() * 2.0f;
    ImGui::BeginChild("##renderbody", ImVec2(0, -strip));
    const float full = ImGui::GetContentRegionAvail().x;

    // What comes out: the first choice, since it decides what the rest mean.
    {
        const float third = (full - 2.0f * st.ItemSpacing.x) / 3.0f;
        const Msg* kinds[3] = {&msg::kind_photo, &msg::kind_video, &msg::kind_frames};
        for (int i = 0; i < 3; i++) {
            if (i) ImGui::SameLine();
            if (ui::KeyButton(*kinds[i], third, nullptr, (int)_project.output.kind == i) &&
                (int)_project.output.kind != i) {
                _project.output.kind = (OutputKind)i;
                fit_output_path(_project.output);
                project_changed();
            }
            ui::help_on_hover(i == 0 ? msg::kind_photo_help
                              : i == 1 ? msg::kind_video_help : msg::kind_frames_help);
        }
    }
    ImGui::BeginDisabled(exporting());
    if (_project.keys.size() <= 1) {
        ImGui::Spacing();
        if (!_up_known) {
            ui::TextDisabledWrapped(msg::up_unknown);
            if (ui::Button(msg::up_from_view, ImVec2(full, 0))) take_up_from_view();
            ui::help_on_hover(msg::up_from_view_help);
        }
        ui::TextDisabledWrapped(_project.output.kind == OutputKind::Photo
                                    ? msg::start_hint_photo : msg::start_hint_video);
        if (_project.output.kind != OutputKind::Photo) {
            const float half = (full - st.ItemSpacing.x) * 0.5f;
            if (ui::Button(msg::quick_orbit, ImVec2(half, 0))) make_orbit();
            ui::help_on_hover(msg::quick_orbit_help);
            ImGui::SameLine();
            bool have_ds = false;
            for (const SourceInfo& s : _sources)
                have_ds = have_ds || (s.view.kind == SourceView::Points && s.view.ds &&
                                      s.view.ds->num_cameras > 1) ||
                          (s.dataset && s.dataset->num_cameras > 1);
            ImGui::BeginDisabled(!have_ds);
            if (ui::Button(msg::quick_capture, ImVec2(half, 0))) follow_capture();
            ImGui::EndDisabled();
            ui::help_on_hover_disabled(have_ds ? msg::quick_capture_help
                                               : msg::quick_capture_none);
            if (!_flying && ui::Button(msg::quick_fly, ImVec2(full, 0))) start_flight();
            ui::help_on_hover(msg::quick_fly_help);
        }
    }

    if (ui::CollapsingHeader(msg::sec_keys, ImGuiTreeNodeFlags_DefaultOpen))
        draw_keys_section(full);
    if (ui::CollapsingHeader(msg::sec_lens, ImGuiTreeNodeFlags_DefaultOpen))
        draw_lens_section(full);
    if (_project.output.kind != OutputKind::Photo && ui::CollapsingHeader(msg::sec_motion))
        draw_motion_section(full);
    if (ui::CollapsingHeader(msg::sec_effects)) draw_effects_section(full);
    if (ui::CollapsingHeader(msg::sec_project)) draw_project_section(full);
    if (ui::CollapsingHeader(msg::sec_history)) draw_history_section(full);
    ImGui::EndDisabled();
    if (ui::CollapsingHeader(msg::sec_output, ImGuiTreeNodeFlags_DefaultOpen))
        draw_output_section(full);
    ImGui::EndChild();

    // The way out, and the way to the editor, never scroll away.
    const float half = (full - st.ItemSpacing.x) * 0.5f;
    ImGui::BeginDisabled(exporting());
    if (ui::Button(msg::to_edit, ImVec2(half, 0)) && _to_edit) _to_edit();
    ui::help_on_hover(msg::to_edit_help);
    ImGui::SameLine();
    if (ui::Button(msg::leave, ImVec2(half, 0)) && _on_leave) _on_leave();
    ImGui::EndDisabled();
    if (!_status.empty()) {
        // One line; the whole of it is in the log and on hover.
        const std::string line = elide_middle(_status.substr(0, _status.find('\n')), full);
        if (_status_err) ui::TextColoredRaw(kErr, line);
        else if (_status_done) ui::TextColoredRaw(kOk, line);
        else ui::TextDisabledRaw(line);
        if (ImGui::IsItemHovered()) ui::SetTooltipRaw(_status);
    }
}

// The flight in progress, or the way the last one was fitted -- while the
// keys are still the ones it made.
void RenderSession::draw_flight_controls(float full) {
    const ImGuiStyle& st = ImGui::GetStyle();
    const float half = (full - st.ItemSpacing.x) * 0.5f;
    if (_flying) {
        char t[32];
        std::snprintf(t, sizeof t, "%.1f", _fly_elapsed);
        ui::TextColoredRaw(kErr, format(msg::fly_recording, {std::string(t)}));
        if (ui::KeyButton(msg::fly_keep, half, "Enter")) stop_flight(true);
        ImGui::SameLine();
        if (ui::KeyButton(msg::render_cancel, half, "Esc")) stop_flight(false);
        return;
    }
    if (_project.output.kind == OutputKind::Photo) return;
    if (ui::Button(msg::fly_new, ImVec2(full, 0))) start_flight();
    ui::help_on_hover(msg::quick_fly_help);
    if (_flight.size() < 2 || _fit_keys.empty() || keys_json() != _fit_keys) return;
    ui::SeparatorText(msg::fit_title);
    const float w = full * 0.55f;
    bool refit = false;
    float timing = (float)_fit.timing;
    ImGui::SetNextItemWidth(w);
    if (ui::SliderFloatRaw("##fittiming", &timing, 0.0f, 1.0f, "%.2f")) _fit.timing = timing;
    refit = ImGui::IsItemDeactivatedAfterEdit() || refit;
    ImGui::SameLine();
    ui::Text(msg::fit_timing);
    ui::help_on_hover(msg::fit_timing_help);
    float detail = (float)_fit.detail;
    ImGui::SetNextItemWidth(w);
    if (ui::SliderFloatRaw("##fitdetail", &detail, 0.0f, 1.0f, "%.2f")) _fit.detail = detail;
    refit = ImGui::IsItemDeactivatedAfterEdit() || refit;
    ImGui::SameLine();
    ui::Text(msg::fit_detail);
    ui::help_on_hover(msg::fit_detail_help);
    float length = (float)_fit.length;
    ImGui::SetNextItemWidth(w);
    if (ui::DragFloatRaw("##fitlength", &length, 0.05f, 0.5f, 3600.0f, "%.2f s")) {
        _fit.length = std::max(0.5f, length);
        _fit_length_set = true;
    }
    refit = ImGui::IsItemDeactivatedAfterEdit() || refit;
    ImGui::SameLine();
    ui::Text(msg::fit_length);
    ui::help_on_hover(msg::fit_length_help);
    ui::TextDisabled(msg::fit_keys, {(long long)_project.keys.size()});
    if (refit) refit_flight();
}

void RenderSession::draw_keys_section(float full) {
    const ImGuiStyle& st = ImGui::GetStyle();
    const float half = (full - st.ItemSpacing.x) * 0.5f;
    if (ui::KeyButton(msg::key_add, half, "K")) add_key(next_key_time(), true);
    ui::help_on_hover(msg::key_add_help);
    ImGui::SameLine();
    int one = single_selected();
    ImGui::BeginDisabled(one < 0);
    if (ui::Button(msg::key_update, ImVec2(half, 0))) update_key_from_view(one);
    ImGui::EndDisabled();
    ui::help_on_hover(msg::key_update_help);
    if (ui::KeyButton(msg::key_look_through, half, "0")) look_through(_time);
    ui::help_on_hover(msg::key_look_through_help);
    ImGui::SameLine();
    bool any = false;
    for (uint8_t s : _sel) any = any || s;
    ImGui::BeginDisabled(!any);
    if (ui::KeyButton(msg::key_delete, half, "X")) delete_selected();
    ImGui::EndDisabled();
    draw_flight_controls(full);
    // The buttons may have added or deleted keys.
    one = single_selected();

    // The keys as a list: the viewport's cameras, in time order.
    const int n = (int)_project.keys.size();
    const std::vector<double> visits = trajectory().key_times();
    if (n > 0 && ImGui::BeginTable("##keys", 4,
                                   ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
                                   ImGuiTableFlags_ScrollY,
                                   ImVec2(0, std::min(n, 6) * ImGui::GetFrameHeight() +
                                                 ImGui::GetFrameHeightWithSpacing()))) {
        ui::TableSetupColumnRaw("#", ImGuiTableColumnFlags_WidthFixed, px(26.0f));
        ui::TableSetupColumn(msg::col_time, ImGuiTableColumnFlags_WidthStretch);
        ui::TableSetupColumn(msg::col_lens, ImGuiTableColumnFlags_WidthStretch);
        ui::TableSetupColumn(msg::col_aim, ImGuiTableColumnFlags_WidthFixed, px(48.0f));
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        for (int i = 0; i < n; i++) {
            const Keyframe& k = _project.keys[(size_t)i];
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushID(i);
            char label[16];
            std::snprintf(label, sizeof label, "%d", i + 1);
            if (ui::SelectableRaw(label, selected(i), ImGuiSelectableFlags_SpanAllColumns)) {
                click_select(i, ImGui::GetIO().KeyCtrl, ImGui::GetIO().KeyShift);
                _time = key_visit(i);
            }
            ImGui::TableNextColumn();
            ui::TextRaw(seconds(i < (int)visits.size() ? visits[(size_t)i] : k.time));
            ImGui::TableNextColumn();
            char b[48];
            std::snprintf(b, sizeof b, "%.0f\xc2\xb0", lens_fov(_project.lens_at(i)));
            if (k.own_lens || i == 0) {
                ui::TextRaw(b);
            } else {
                ui::TextDisabledRaw(b);
                ui::help_on_hover(msg::lens_auto_help);
            }
            ImGui::TableNextColumn();
            if (k.aim) ui::TextRaw("\xe2\x97\x8e");
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (one >= 0) {
        Keyframe& k = _project.keys[(size_t)one];
        const float w = full * 0.55f;
        ImGui::SetNextItemWidth(w);
        // At constant speed the path decides when the keys between the ends
        // are passed.
        const bool paced = _project.motion.constant_speed && one > 0 &&
                           one + 1 < (int)_project.keys.size();
        double t = paced ? key_visit(one) : k.time;
        ImGui::BeginDisabled(paced);
        const bool edited = ui::InputDoubleRaw("##ktime", &t, 0.1, 1.0, "%.3f s");
        ImGui::EndDisabled();
        if (paced) ui::help_on_hover_disabled(msg::key_time_paced);
        if (edited && !paced) {
            k.time = std::max(0.0, t);
            const double keep = k.time;
            _project.sort_keys();
            for (int i = 0; i < (int)_project.keys.size(); i++)
                if (_project.keys[(size_t)i].time == keep) { select_only(i); break; }
            _time = keep;
            project_changed();
        }
        ImGui::SameLine();
        ui::Text(msg::key_time);
        one = single_selected();
        if (one < 0) return;
        Keyframe& kk = _project.keys[(size_t)one];
        if (ui::Checkbox(msg::key_hold, &kk.hold)) project_changed();
        ui::help_on_hover(msg::key_hold_help);
        if (ui::Checkbox(msg::key_aim, &kk.aim)) {
            if (kk.aim) aim_ahead(kk);
            project_changed();
        }
        ui::help_on_hover(msg::key_aim_help);
        if (kk.aim) {
            ImGui::SameLine();
            if (ui::KeyButton(msg::key_pick_target, 0.0f, "T", _pick_target)) _pick_target = true;
            ui::help_on_hover(msg::key_pick_target_help);
            ImGui::SetNextItemWidth(w);
            float roll = (float)kk.roll;
            if (ui::SliderFloatRaw("##roll", &roll, -180.0f, 180.0f, "%.1f\xc2\xb0")) {
                kk.roll = roll;
                update_aim(kk, _project.up);
                project_changed();
            }
            ImGui::SameLine();
            ui::Text(msg::key_roll);
            float tg[3] = {(float)kk.target[0], (float)kk.target[1], (float)kk.target[2]};
            ImGui::SetNextItemWidth(w);
            if (ui::DragFloat3Raw("##target", tg, 0.01f / (float)std::max(_w2s.s, 1e-9), "%.3f")) {
                for (int a = 0; a < 3; a++) kk.target[a] = tg[a];
                update_aim(kk, _project.up);
                project_changed();
            }
            ImGui::SameLine();
            ui::Text(msg::key_target);
        }
        float p[3] = {(float)kk.pos[0], (float)kk.pos[1], (float)kk.pos[2]};
        ImGui::SetNextItemWidth(w);
        if (ui::DragFloat3Raw("##pos", p, 0.01f / (float)std::max(_w2s.s, 1e-9), "%.3f")) {
            for (int a = 0; a < 3; a++) kk.pos[a] = p[a];
            update_aim(kk, _project.up);
            project_changed();
        }
        ImGui::SameLine();
        ui::Text(msg::key_position);
    } else if (any) {
        ui::TextDisabledWrapped(msg::keys_many_hint);
        // Aimed or not, all of them at once; or all at one point.
        bool all = true;
        for (int i = 0; i < n; i++)
            if (selected(i)) all = all && _project.keys[(size_t)i].aim;
        if (ui::Checkbox(msg::key_aim, &all)) {
            for (int i = 0; i < n; i++) {
                Keyframe& k = _project.keys[(size_t)i];
                if (!selected(i) || k.aim == all) continue;
                k.aim = all;
                if (all) aim_ahead(k);
            }
            project_changed();
        }
        ui::help_on_hover(msg::keys_aim_many_help);
        ImGui::SameLine();
        if (ui::KeyButton(msg::key_pick_target, 0.0f, "T", _pick_target)) _pick_target = true;
        ui::help_on_hover(msg::keys_pick_many_help);
        const Msg* pivots[3] = {&xmsg::pivot_origin, &xmsg::pivot_median, &xmsg::pivot_mean};
        ImGui::SetNextItemWidth(full * 0.55f);
        combo_msgs("##pivot", &_pivot, pivots, 3);
        ImGui::SameLine();
        ui::Text(xmsg::pivot);
        ui::help_on_hover(xmsg::pivot_help);
    }

    ImGui::SetNextItemWidth(full * 0.45f);
    ui::SliderFloatRaw("##camsize", &_cam_size, 0.1f, 10.0f, "\xc3\x97%.2f",
                       ImGuiSliderFlags_Logarithmic);
    ImGui::SameLine();
    ui::Text(msg::key_camera_size);
    ui::help_on_hover(msg::key_camera_size_help);

    if (n >= 2) {
        static float total = 0.0f;
        if (total <= 0.0f) total = (float)(_project.duration() - _project.keys.front().time);
        ImGui::SetNextItemWidth(full * 0.3f);
        ui::InputFloatRaw("##total", &total, "%.1f s");
        ImGui::SameLine();
        if (ui::Button(msg::keys_space_evenly)) space_evenly(total);
        ui::help_on_hover(msg::keys_space_evenly_help);
    }
}

void RenderSession::draw_lens_section(float full) {
    if (_project.keys.empty()) return;
    int at = single_selected();
    if (at < 0) {
        // No one key chosen: the key last passed, held still while playing
        // so the section does not change under the pointer.
        if (!_playing) {
            const std::vector<double> visits = trajectory().key_times();
            _lens_key = 0;
            for (int i = 0; i < (int)visits.size(); i++)
                if (visits[(size_t)i] <= _time + 1e-9) _lens_key = i;
        }
        at = std::clamp(_lens_key, 0, (int)_project.keys.size() - 1);
    }
    Keyframe& k = _project.keys[(size_t)at];
    ui::TextDisabled(msg::lens_of_key, {(long long)(at + 1)});
    if (at > 0) {
        bool glide = !k.own_lens;
        if (ui::Checkbox(msg::lens_auto, &glide)) {
            const Lens now = _project.lens_at(at);
            k.own_lens = !glide;
            if (k.own_lens) k.lens = now;
            project_changed();
        }
        ui::help_on_hover(msg::lens_auto_help);
        if (glide) {
            const Lens l = _project.lens_at(at);
            ui::TextDisabled(msg::lens_between,
                             {kProjections[(int)l.projection]->get(), (long long)std::lround(lens_fov(l))});
            return;
        }
    }
    Lens& l = k.lens;
    const float w = full * 0.62f;
    const bool first_key = at == 0;
    (void)first_key;

    int proj = (int)l.projection;
    ImGui::SetNextItemWidth(w);
    if (combo_msgs("##proj", &proj, kProjections, kNumProjections)) {
        const double fov = lens_fov(l);
        l.projection = (Projection)proj;
        if (l.projection == Projection::Equirect) {
            l.tier = 0;
            for (float& d : l.dist) d = 0.0f;
            // The whole sphere wants a 2:1 frame.
            if (_project.output.width != 2 * _project.output.height) {
                _project.output.height = std::max(16, _project.output.width / 2);
            }
        } else {
            lens_set_fov(l, l.projection == Projection::Perspective ? std::min(fov, 120.0)
                                                                    : std::max(fov, 120.0));
        }
        project_changed();
    }
    ImGui::SameLine();
    ui::Text(msg::lens_projection);
    ui::help_on_hover(msg::lens_projection_help);

    // Presets: what a photographer names, what a 360 camera makes, and what
    // this dataset was shot with.
    ImGui::SetNextItemWidth(w);
    if (ui::BeginCombo(msg::lens_preset, msg::lens_preset_pick.get(), ImGuiComboFlags_HeightLarge)) {
        auto apply = [&](const Lens& lens, int pw, int ph) {
            l = lens;
            if (pw > 0 && ph > 0) {
                _project.output.width = pw;
                _project.output.height = ph;
            }
            project_changed();
        };
        ui::SeparatorText(msg::lens_presets_common);
        int id = 0;
        for (const LensPreset& p : generic_lens_presets()) {
            ImGui::PushID(id++);
            if (ui::SelectableRaw(preset_label(p))) apply(p.lens, p.width, p.height);
            ImGui::PopID();
        }
        ui::SeparatorText(msg::lens_presets_360);
        for (const LensPreset& p : camera_lens_presets()) {
            ImGui::PushID(id++);
            if (ui::SelectableRaw(preset_label(p))) apply(p.lens, p.width, p.height);
            ImGui::PopID();
        }
        // The dataset's own lenses, clustered once per dataset.
        const ParsedDataset* ds = nullptr;
        for (const SourceInfo& s : _sources) {
            if (s.view.kind == SourceView::Points && s.view.ds) { ds = s.view.ds; break; }
            if (s.dataset) { ds = s.dataset; break; }
        }
        if (ds && ds->num_cameras > 0) {
            const std::string key = std::to_string((uintptr_t)ds) + ":" +
                                    std::to_string(ds->num_cameras);
            if (key != _dataset_lenses_key) {
                _dataset_lenses = cluster_dataset_lenses(*ds);
                _dataset_lenses_key = key;
            }
            ui::SeparatorText(msg::lens_presets_dataset);
            for (const DatasetLens& d : _dataset_lenses) {
                if (d.lens.projection == Projection::Equirect) continue;
                char b[160];
                std::snprintf(b, sizeof b, "%s  %dx%d  %.0f\xc2\xb0",
                              kProjections[(int)d.lens.projection]->get(), d.width,
                              d.height, lens_fov(d.lens));
                ImGui::PushID(id++);
                const std::string label = format(msg::lens_dataset_entry, {std::string(b), (long long)d.count});
                if (ui::SelectableRaw(label)) apply(d.lens, d.width, d.height);
                ImGui::PopID();
            }
        }
        ImGui::EndCombo();
    }

    if (l.projection != Projection::Equirect) {
        float fov = (float)lens_fov(l);
        const float lo = 5.0f, hi = l.projection == Projection::Perspective ? 150.0f : 360.0f;
        ImGui::SetNextItemWidth(w);
        if (ui::SliderFloatRaw("##fov", &fov, lo, hi, "%.1f\xc2\xb0")) {
            lens_set_fov(l, fov);
            project_changed();
        }
        ImGui::SameLine();
        ui::Text(msg::lens_fov);
        ui::help_on_hover(msg::lens_fov_help);
        if (l.projection == Projection::Perspective) {
            float mm = (float)lens_mm(l);
            ImGui::SetNextItemWidth(w);
            if (ui::InputFloatRaw("##mm", &mm, 1.0f, 10.0f, "%.1f mm")) {
                l.focal = std::clamp((double)mm, 2.0, 2000.0) / 36.0;
                project_changed();
            }
            ImGui::SameLine();
            ui::Text(msg::lens_mm);
            ui::help_on_hover(msg::lens_mm_help);
        }
        // k1 alone covers most of what a lens does; the rest is Advanced.
        float k1 = l.tier ? l.dist[0] : 0.0f;
        ImGui::SetNextItemWidth(w);
        if (ui::SliderFloatRaw("##k1", &k1, -0.5f, 0.5f, "k1 %.4f")) {
            if (!l.tier) l.tier = 1;
            l.dist[0] = k1;
            project_changed();
        }
        ImGui::SameLine();
        ui::Text(msg::lens_distortion);
        ui::help_on_hover(msg::lens_distortion_help);
        if (ui::TreeNode(msg::lens_all_coefficients)) {
            int tier = l.tier;
            ImGui::SetNextItemWidth(w);
            if (ui::ComboRaw("##tier", &tier, {&msg::tier_none, &msg::tier_opencv,
                                               &msg::tier_full})) {
                l.tier = tier;
                if (!tier) for (float& d : l.dist) d = 0.0f;
                project_changed();
            }
            static const char* const kNames[2][8] = {
                {"k1", "k2", "p1", "p2", "", "", "", ""},
                {"k1", "k2", "k3", "k4", "p1", "p2", "s1", "s2"}};
            const int n = l.tier == 1 ? 4 : l.tier == 2 ? 8 : 0;
            for (int i = 0; i < n; i++) {
                ImGui::PushID(i);
                ImGui::SetNextItemWidth(w);
                if (ui::InputFloatRaw("##c", &l.dist[i], "%.6f")) project_changed();
                ImGui::SameLine();
                ui::TextRaw(kNames[l.tier - 1][i]);
                ImGui::PopID();
            }
            if (n) {
                // Paste a whole row -- COLMAP's order for the full tier.
                static std::string paste;
                ImGui::SetNextItemWidth(w);
                if (ui::InputTextWithHintRaw("##paste", msg::lens_paste_hint, &paste,
                                             ImGuiInputTextFlags_EnterReturnsTrue)) {
                    float v[8] = {};
                    int got = 0;
                    const char* s = paste.c_str();
                    while (got < 8 && *s) {
                        char* end = nullptr;
                        const float f = std::strtof(s, &end);
                        if (end == s) { s++; continue; }
                        v[got++] = f;
                        s = end;
                    }
                    if (got >= n) {
                        if (l.tier == 2 && got == 8) {
                            // COLMAP THIN_PRISM_FISHEYE: k1 k2 p1 p2 k3 k4 sx1 sy1.
                            const float r[8] = {v[0], v[1], v[4], v[5], v[2], v[3], v[6], v[7]};
                            for (int i = 0; i < 8; i++) l.dist[i] = r[i];
                        } else {
                            for (int i = 0; i < n; i++) l.dist[i] = v[i];
                        }
                        project_changed();
                    }
                    paste.clear();
                }
                ui::help_on_hover(msg::lens_paste_help);
            }
            ImGui::TreePop();
        }
    } else {
        ui::TextDisabled(msg::lens_equirect_note);
    }
}

void RenderSession::draw_motion_section(float full) {
    Motion& m = _project.motion;
    const float w = full * 0.55f;
    int curve = (int)m.curve;
    ImGui::SetNextItemWidth(w);
    if (combo_msgs("##curve", &curve, kCurves, kNumCurves)) {
        m.curve = (Curve)curve;
        project_changed();
    }
    ImGui::SameLine();
    ui::Text(msg::motion_curve);
    ui::help_on_hover(msg::motion_curve_help);
    if (m.curve == Curve::CatmullRom) {
        float t = (float)m.tension;
        ImGui::SetNextItemWidth(w);
        if (ui::SliderFloatRaw("##tension", &t, 0.0f, 1.0f, "%.2f")) {
            m.tension = t;
            project_changed();
        }
        ImGui::SameLine();
        ui::Text(msg::motion_tension);
        ui::help_on_hover(msg::motion_tension_help);
    }
    if (ui::Checkbox(msg::motion_loop, &m.loop)) project_changed();
    ui::help_on_hover(msg::motion_loop_help);
    if (m.loop && _project.keys.size() >= 2) {
        float end = (float)_project.duration();
        ImGui::SetNextItemWidth(w);
        if (ui::InputFloatRaw("##loopend", &end, 0.1f, 1.0f, "%.2f s")) {
            _project.end = std::max((double)end, _project.keys.back().time + 0.1);
            project_changed();
        }
        ImGui::SameLine();
        ui::Text(msg::motion_loop_end);
    }
    ImGui::BeginDisabled(m.loop);
    if (ui::Checkbox(msg::motion_ease, &m.ease)) project_changed();
    ImGui::EndDisabled();
    ui::help_on_hover(msg::motion_ease_help);
    if (ui::Checkbox(msg::motion_constant, &m.constant_speed)) project_changed();
    ui::help_on_hover(msg::motion_constant_help);

    // Ironing out kinks by moving the keys themselves.
    ImGui::BeginDisabled(_project.keys.size() < 3);
    if (ui::Button(msg::smooth_keys)) smooth_keys(_smooth_strength, _smooth_what);
    ui::help_on_hover(msg::smooth_keys_help);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(full * 0.3f);
    ui::SliderFloatRaw("##smooth", &_smooth_strength, 0.05f, 1.0f, "%.2f");
    ImGui::SameLine();
    ui::TextDisabled(msg::smooth_strength);
    ImGui::SetNextItemWidth(full * 0.45f);
    ui::ComboRaw("##smoothwhat", &_smooth_what,
                 {&msg::smooth_poses, &msg::smooth_timing, &msg::smooth_both});
    ImGui::SameLine();
    ui::TextDisabled(msg::smooth_what);
    ui::help_on_hover(msg::smooth_what_help);
    ImGui::EndDisabled();

    if (ui::Button(msg::up_from_view)) take_up_from_view();
    ui::help_on_hover(msg::up_from_view_help);
    char len[32];
    std::snprintf(len, sizeof len, "%.3g", trajectory().length());
    ui::TextDisabled(msg::motion_length, {std::string(len), seconds(_project.duration())});
}

// Under a shot, what its transition can be told: a colour, a direction, a
// strength. Nothing for the few that need nothing.
void RenderSession::draw_transition_settings(Transition kind, float param[2], float colour[3],
                                             bool& camera, float full) {
    const float w = full * 0.5f;
    bool changed = false;
    ImGui::Indent();
    auto slider = [&](const char* id, float* v, float lo, float hi, const char* fmt, const Msg& name) {
        ImGui::SetNextItemWidth(w);
        changed = ui::SliderFloatRaw(id, v, lo, hi, fmt) || changed;
        ImGui::SameLine();
        ui::Text(name);
    };
    auto tint = [&]() {
        changed = ui::ColorEdit3Raw("##col", colour, ImGuiColorEditFlags_NoInputs) || changed;
        ImGui::SameLine();
        ui::Text(msg::tp_colour);
    };
    switch (kind) {
        case Transition::Dip: tint(); break;
        case Transition::Wipe:
            slider("##p0", &param[0], 0.0f, 360.0f, "%.0f\xc2\xb0", msg::tp_direction);
            slider("##p1", &param[1], 0.0f, 0.3f, "%.2f", msg::tp_softness);
            break;
        case Transition::Iris:
            slider("##p1", &param[1], 0.0f, 0.3f, "%.2f", msg::tp_softness);
            break;
        case Transition::Zoom:
            slider("##p0", &param[0], 0.1f, 1.5f, "%.2f", msg::tp_strength);
            break;
        case Transition::Sweep: {
            bool down = param[0] >= 0.5f;
            if (ui::Checkbox(msg::tp_downward, &down)) {
                param[0] = down ? 1.0f : 0.0f;
                changed = true;
            }
            slider("##p1", &param[1], 0.0f, 1.0f, "%.2f", msg::tp_glow);
            tint();
            break;
        }
        case Transition::Dust: {
            int mode = std::clamp((int)(param[0] + 0.5f), 0, 2);
            ImGui::SetNextItemWidth(w);
            if (ui::ComboRaw("##p0", &mode, {&msg::tp_fall, &msg::tp_rise, &msg::tp_blow})) {
                param[0] = (float)mode;
                changed = true;
            }
            ImGui::SameLine();
            ui::Text(msg::tp_direction);
            slider("##p1", &param[1], 0.0f, 1.0f, "%.2f", msg::tp_turbulence);
            break;
        }
        case Transition::Spiral:
            slider("##p0", &param[0], 0.25f, 4.0f, "%.2f", msg::tp_turns);
            slider("##p1", &param[1], 0.0f, 3.0f, "%.2f", msg::tp_spread);
            break;
        case Transition::Scatter:
            slider("##p0", &param[0], 0.2f, 3.0f, "%.2f", msg::tp_distance);
            slider("##p1", &param[1], 0.0f, 1.0f, "%.2f", msg::tp_randomness);
            break;
        case Transition::Rain:
            slider("##p0", &param[0], 0.2f, 3.0f, "%.2f", msg::tp_height);
            slider("##p1", &param[1], 0.0f, 1.0f, "%.2f", msg::tp_stagger);
            break;
        case Transition::Dissolve:
            slider("##p0", &param[0], 0.0f, 1.0f, "%.2f", msg::tp_sparkle);
            break;
        case Transition::Ripple:
            slider("##p0", &param[0], 0.0f, 1.0f, "%.2f", msg::tp_height);
            slider("##p1", &param[1], 0.05f, 1.0f, "%.2f", msg::tp_width);
            break;
        default:
            break;
    }
    if (transition_has_camera(kind)) {
        changed = ui::Checkbox(msg::tp_camera, &camera) || changed;
        ui::help_on_hover(msg::tp_camera_help);
    }
    ImGui::Unindent();
    if (changed) project_changed();
}

void RenderSession::draw_effects_section(float full) {
    const ImGuiStyle& st = ImGui::GetStyle();
    const float w = full * 0.5f;

    // Fades: the two every editor has.
    auto fade = [&](const Msg& name, Fade& f, const char* id, bool in) {
        ImGui::PushID(id);
        int c = (int)f.colour;
        ImGui::SetNextItemWidth(w * 0.7f);
        if (ui::ComboRaw("##c", &c, {&msg::fade_none, in ? &msg::fade_from_black : &msg::fade_to_black,
                                     in ? &msg::fade_from_white : &msg::fade_to_white})) {
            f.colour = (FadeColour)c;
            project_changed();
        }
        ImGui::SameLine();
        if (f.colour != FadeColour::None) {
            float s = (float)f.seconds;
            ImGui::SetNextItemWidth(w * 0.55f);
            if (ui::SliderFloatRaw("##s", &s, 0.1f, 5.0f, "%.1f s")) {
                f.seconds = s;
                project_changed();
            }
            ImGui::SameLine();
        }
        ui::Text(name);
        ImGui::PopID();
    };
    // The shots fade in and out now; a fade of a file from before them that
    // could not become one stays editable here until it is set to none.
    if (_project.fade_in.colour != FadeColour::None) fade(msg::fade_in, _project.fade_in, "fi", true);
    if (_project.fade_out.colour != FadeColour::None) fade(msg::fade_out, _project.fade_out, "fo", false);
    if (ui::ColorEdit3Raw("##bg", _project.background, ImGuiColorEditFlags_NoInputs))
        project_changed();
    ImGui::SameLine();
    ui::Text(msg::background);

    // The models, and how each is drawn.
    ui::SeparatorText(msg::sec_models);
    int remove = -1;
    const int one = single_selected();
    int view = -1, edit = -1;
    for (int i = 0; i < (int)_project.sources.size(); i++) {
        ImGui::PushID(i);
        const int r = rt(i);
        if (r < 0) {
            // Asked for, still being read.
            ui::TextDisabled(msg::model_opening, {(long long)(i + 1), source_name(i)});
            ImGui::PopID();
            continue;
        }
        const SourceInfo& s = _sources[(size_t)r];
        const Msg& kind = s.view.kind == SourceView::Points ? msg::model_points
                          : s.view.kind == SourceView::Mesh ? msg::model_mesh
                                                            : msg::model_splats;
        ui::TextRaw(format(msg::model_line, {(long long)(i + 1), kind.get(), s.name}));
        if (ImGui::IsItemHovered()) ui::SetTooltipRaw(s.path);
        // Shown in the viewport, edited there, or closed.
        const ImGuiStyle& gst = ImGui::GetStyle();
        auto bw = [&](const Msg& m) { return ImGui::CalcTextSize(m.get()).x + 2.0f * gst.FramePadding.x; };
        const float xw = px(24.0f);
        const float row = bw(msg::model_view) + bw(msg::model_edit) + xw + 2.0f * gst.ItemSpacing.x;
        ImGui::SameLine(std::max(ImGui::GetCursorPosX(), full - row));
        ImGui::BeginDisabled(r == 0);
        if (ui::Button(msg::model_view)) view = i;
        ImGui::EndDisabled();
        ui::help_on_hover_disabled(r == 0 ? msg::model_viewing : msg::model_view_help);
        ImGui::SameLine();
        if (ui::Button(msg::model_edit)) edit = i;
        ui::help_on_hover(msg::model_edit_help);
        ImGui::SameLine();
        ImGui::BeginDisabled(_project.sources.size() < 2);
        if (ui::ButtonRaw("x##rm", ImVec2(xw, 0))) remove = i;
        ImGui::EndDisabled();
        ui::help_on_hover(msg::model_remove_help);
        ImGui::Indent();
        // The look from the start, or the one a selected key changes it to.
        SourceStyle* yp = &_project.sources[(size_t)i].style;
        bool keyed = false;
        for (const Keyframe& k : _project.keys)
            for (const KeyLook& l : k.looks) keyed = keyed || l.source == i;
        if (one >= 0) {
            std::vector<KeyLook>& looks = _project.keys[(size_t)one].looks;
            auto it = std::find_if(looks.begin(), looks.end(),
                                   [&](const KeyLook& l) { return l.source == i; });
            bool own = it != looks.end();
            const std::string label = format(msg::look_at_key, {(long long)(one + 1)}) + "##look";
            if (ui::CheckboxRaw(label.c_str(), &own)) {
                if (own) {
                    SourceStyle from, to;
                    float mix = 0.0f;
                    _project.look_at(i, key_visit(one), trajectory().key_times(), from, to, mix);
                    looks.push_back({i, mix > 0.5f ? to : from});
                } else {
                    looks.erase(it);
                }
                project_changed();
            }
            ui::help_on_hover(msg::look_at_key_help);
            for (KeyLook& l : looks)
                if (l.source == i) yp = &l.style;
        } else if (keyed) {
            ui::TextDisabledWrapped(msg::look_from_start);
        }
        SourceStyle& y = *yp;
        if (s.view.kind == SourceView::Points) {
            int ps = (int)y.point_style;
            ImGui::SetNextItemWidth(w);
            if (combo_msgs("##ps", &ps, kPointStyles, kNumPointStyles)) {
                y.point_style = (PointStyle)ps;
                project_changed();
            }
            ImGui::SameLine();
            ui::Text(msg::point_style);
            ImGui::SetNextItemWidth(w);
            if (y.point_style == PointStyle::Sphere) {
                if (ui::SliderFloatRaw("##pr", &y.sphere_radius, 0.0005f, 0.05f, "%.4f",
                                       ImGuiSliderFlags_Logarithmic))
                    project_changed();
            } else if (ui::SliderFloatRaw("##px", &y.point_px, 1.0f, 24.0f, "%.1f px")) {
                project_changed();
            }
            ImGui::SameLine();
            ui::Text(msg::point_size);
            ui::help_on_hover(msg::point_size_help);
            if (ui::Checkbox(msg::point_cameras, &y.cameras)) project_changed();
        } else if (s.view.kind == SourceView::Mesh) {
            if (ui::Checkbox(msg::mesh_shade, &y.shade)) project_changed();
            ImGui::SameLine();
            if (ui::Checkbox(msg::mesh_flat, &y.flat)) project_changed();
            ImGui::SameLine();
            if (ui::Checkbox(msg::mesh_colour, &y.colour)) project_changed();
        } else {
            // What it is rendered as: the viewport's choice unless set here.
            // Automatic unless one is chosen; only that choice is a word, the
            // rest are identifiers.
            int prim = 0;
            for (int k = 1; k < 4; k++)
                if (y.primitive == kPrimitives[k]) prim = k;
            std::vector<std::string> names = {msg::primitive_auto.get()};
            for (int k = 1; k < 4; k++) names.push_back(kPrimitives[k]);
            ImGui::SetNextItemWidth(w);
            if (ui::BeginComboRaw("##prim", names[(size_t)prim].c_str())) {
                for (int k = 0; k < 4; k++)
                    if (ui::SelectableRaw(names[(size_t)k], k == prim)) {
                        y.primitive = kPrimitives[k];
                        project_changed();
                    }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            ui::Text(msg::model_primitive);
            ui::help_on_hover(msg::model_primitive_help);
            if (s.view.sh_max > 0) {
                int sh = y.sh_degree < 0 ? s.view.sh_max : y.sh_degree;
                ImGui::SetNextItemWidth(w);
                if (ui::SliderIntRaw("##sh", &sh, 0, s.view.sh_max, "SH %d")) {
                    y.sh_degree = sh == s.view.sh_max ? -1 : sh;
                    project_changed();
                }
            }
        }
        ImGui::Unindent();
        ImGui::PopID();
    }
    if (remove >= 0) remove_source(remove);
    else if (view >= 0) view_source(view, false);
    else if (edit >= 0) view_source(edit, true);
    if (ui::Button(msg::model_add) && _pick)
        _pick(Pick::AddModel,
              _sources.empty() ? std::string()
                               : fs::u8path(_sources[0].path).parent_path().string(), "");
    ui::help_on_hover(msg::model_add_help);

    // Shots: which model is shown from when, how it arrives and how it goes.
    ui::SeparatorText(msg::sec_shots);
    remove = -1;
    int swap = -1;
    // A way out may be any transition but a dip, which is the whole picture's
    // -- except the last, whose dip is the fade out.
    std::vector<const Msg*> exits = {&msg::shot_exit_as_next};
    std::vector<const Msg*> exits_last = {&msg::shot_exit_stays};
    for (int k = 0; k < kNumTransitions; k++) {
        if (k != (int)Transition::Dip) exits.push_back(kTransitions[k]);
        exits_last.push_back(kTransitions[k]);
    }
    auto exit_index = [](const ShotExit& e, bool last) {
        if (!e.own) return 0;
        const int k = (int)e.transition;
        return last || k < (int)Transition::Dip ? k + 1 : k;
    };
    const int nshots = (int)_project.shots.size();
    for (int i = 0; i < nshots; i++) {
        Shot& s = _project.shots[(size_t)i];
        ImGui::PushID(1000 + i);
        float start = (float)s.start;
        ImGui::SetNextItemWidth(px(70.0f));
        if (ui::DragFloatRaw("##start", &start, 0.05f, 0.0f, 3600.0f, "%.2f s")) {
            s.start = std::max(0.0f, start);
            project_changed();
        }
        ImGui::SameLine();
        std::vector<std::string> names;
        names.push_back(msg::model_nothing.get());
        for (int k = 0; k < (int)_project.sources.size(); k++) names.push_back(source_name(k));
        int src = s.source + 1;
        ImGui::SetNextItemWidth(full - px(70.0f) - px(28.0f) - 3.0f * st.ItemSpacing.x - w * 0.9f);
        if (ui::BeginComboRaw("##src", names[(size_t)std::clamp(src, 0, (int)names.size() - 1)].c_str())) {
            // Two copies of one file have one name.
            for (int j = 0; j < (int)names.size(); j++) {
                ImGui::PushID(j);
                if (ui::SelectableRaw(names[(size_t)j], j == src)) {
                    s.source = j - 1;
                    project_changed();
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        int tr = (int)s.transition;
        ImGui::SetNextItemWidth(w * 0.9f);
        if (combo_msgs("##tr", &tr, kTransitions, kNumTransitions)) {
            s.transition = (Transition)tr;
            shot_defaults(s);
            project_changed();
        }
        ui::help_on_hover(msg::tr_help);
        ImGui::SameLine();
        // One shot is always left: something is on screen.
        ImGui::BeginDisabled(nshots < 2);
        if (ui::ButtonRaw("x##del", ImVec2(px(28.0f), 0))) remove = i;
        ImGui::EndDisabled();
        if (nshots < 2) ui::help_on_hover_disabled(msg::shot_keep_one);
        // Earlier or later: the models trade places, the times stay.
        ImGui::BeginDisabled(i == 0);
        if (ui::ArrowButtonRaw("##up", ImGuiDir_Up)) swap = i - 1;
        ImGui::EndDisabled();
        ui::help_on_hover(msg::shot_move_help);
        ImGui::SameLine();
        ImGui::BeginDisabled(i + 1 >= nshots);
        if (ui::ArrowButtonRaw("##down", ImGuiDir_Down)) swap = i;
        ImGui::EndDisabled();
        ui::help_on_hover(msg::shot_move_help);
        if (s.transition != Transition::Cut) {
            ImGui::SameLine();
            float d = (float)s.duration;
            ImGui::SetNextItemWidth(px(120.0f));
            if (ui::SliderFloatRaw("##dur", &d, 0.1f, 6.0f, "%.1f s")) {
                s.duration = d;
                project_changed();
            }
            ImGui::SameLine();
            ui::TextDisabled(msg::shot_transition_time);
            ImGui::PushID("in");
            draw_transition_settings(s.transition, s.param, s.colour, s.camera, full);
            ImGui::PopID();
        }
        // How it goes: as the next arrives, or its own way; the last one,
        // at the end.
        {
            const bool last = i + 1 >= nshots;
            ShotExit& e = s.exit;
            ImGui::Indent();
            int ex = exit_index(e, last);
            ImGui::SetNextItemWidth(w * 0.9f);
            if (ui::ComboRaw("##exit", &ex, last ? exits_last : exits)) {
                const bool was = e.own;
                e.own = ex > 0;
                if (e.own) {
                    e.transition = (Transition)(last || ex - 1 < (int)Transition::Dip ? ex - 1 : ex);
                    exit_defaults(e);
                    // Taken up as the next arrival was: the same length.
                    if (!was) e.duration = last ? 1.0 : std::max(_project.shots[(size_t)i + 1].duration, 0.1);
                    if (!was && last) e.offset = 0.0;
                }
                project_changed();
            }
            ImGui::SameLine();
            ui::Text(msg::shot_exit);
            ui::help_on_hover(last ? msg::shot_exit_last_help : msg::shot_exit_help);
            if (e.own) {
                if (e.transition != Transition::Cut) {
                    float d = (float)e.duration;
                    ImGui::SetNextItemWidth(px(110.0f));
                    if (ui::SliderFloatRaw("##xdur", &d, 0.1f, 6.0f, "%.1f s")) {
                        e.duration = d;
                        project_changed();
                    }
                    ImGui::SameLine();
                    ui::TextDisabled(msg::shot_transition_time);
                    ImGui::SameLine();
                }
                float off = (float)e.offset;
                ImGui::SetNextItemWidth(px(110.0f));
                if (ui::SliderFloatRaw("##xoff", &off, -6.0f, last ? 0.0f : 6.0f, "%+.1f s")) {
                    e.offset = off;
                    project_changed();
                }
                ImGui::SameLine();
                ui::TextDisabled(msg::shot_exit_offset);
                ui::help_on_hover(last ? msg::shot_exit_offset_last_help : msg::shot_exit_offset_help);
                ImGui::PushID("out");
                draw_transition_settings(e.transition, e.param, e.colour, e.camera, full);
                ImGui::PopID();
            }
            ImGui::Unindent();
        }
        ImGui::PopID();
    }
    if (remove >= 0 && nshots > 1) {
        _project.shots.erase(_project.shots.begin() + remove);
        project_changed();
    }
    if (swap >= 0 && swap + 1 < (int)_project.shots.size()) {
        // Everything but the time: the model, how it arrives and how it goes.
        Shot& a = _project.shots[(size_t)swap];
        Shot& b = _project.shots[(size_t)swap + 1];
        std::swap(a.start, b.start);
        std::swap(a, b);
        project_changed();
    }
    if (ui::Button(msg::shot_add)) {
        Shot s;
        s.start = _time;
        s.source = _project.shots.empty() ? 0 : _project.shots.back().source;
        s.transition = Transition::Crossfade;
        shot_defaults(s);
        if (_project.shots.empty() && _time > 0.0) _project.shots.push_back(Shot{});
        _project.shots.push_back(s);
        std::stable_sort(_project.shots.begin(), _project.shots.end(),
                         [](const Shot& a, const Shot& b) { return a.start < b.start; });
        project_changed();
    }
    ui::help_on_hover(msg::shot_add_help);

    // Every model in turn, points before splats before meshes: the story a
    // reconstruction tells, each arriving the way that suits it.
    std::vector<int> order;
    for (SourceView::Kind k : {SourceView::Points, SourceView::Splats, SourceView::Mesh})
        for (int i = 0; i < (int)_project.sources.size(); i++)
            if (rt(i) >= 0 && _sources[(size_t)rt(i)].view.kind == k) order.push_back(i);
    const bool can = order.size() >= 2;
    ImGui::BeginDisabled(!can || _project.duration() <= 0.0);
    if (ui::Button(msg::story_button, ImVec2(full, 0))) {
        const double t0 = _project.keys.front().time, T = _project.duration() - t0;
        const double each = T / (double)order.size();
        _project.shots.clear();
        for (size_t j = 0; j < order.size(); j++) {
            Shot sh;
            sh.start = t0 + each * (double)j;
            sh.source = order[j];
            const SourceView::Kind k = _sources[(size_t)rt(order[j])].view.kind;
            const SourceView::Kind before = j ? _sources[(size_t)rt(order[j - 1])].view.kind : k;
            sh.transition = j == 0 ? Transition::Cut
                            : k == SourceView::Splats && before == SourceView::Points ? Transition::Grow
                            : k == SourceView::Mesh ? Transition::Sweep
                                                    : Transition::Crossfade;
            sh.duration = j == 0 ? 0.0 : std::min(each * 0.6, 3.0);
            shot_defaults(sh);
            _project.shots.push_back(sh);
        }
        project_changed();
    }
    ImGui::EndDisabled();
    ui::help_on_hover_disabled(can ? msg::story_help : msg::story_needs);
}

// Every step back to where the project was opened: one click goes back ten.
void RenderSession::draw_history_section(float full) {
    const float half = (full - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    ImGui::BeginDisabled(_head <= 0);
    if (ui::KeyButton(msg::hist_undo, half, "Ctrl+Z")) undo();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(_head + 1 >= (int)_hist.size());
    if (ui::KeyButton(msg::hist_redo, half, "Ctrl+Y")) redo();
    ImGui::EndDisabled();
    const int n = (int)_hist.size();
    const float rows = (float)std::clamp(n, 3, 8);
    ImGui::BeginChild("##hist", ImVec2(0, rows * ImGui::GetTextLineHeightWithSpacing()),
                      ImGuiChildFlags_Borders);
    int go = -1;
    for (int i = 0; i < n; i++) {
        ImGui::PushID(i);
        // Greyed past the current step: those are a redo away.
        const bool ahead = i > _head;
        if (ahead) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        const Msg& label = i == 0 ? msg::hist_start : *_hist[(size_t)i].label;
        if (ui::Selectable(label, i == _head)) go = i;
        if (ahead) ImGui::PopStyleColor();
        if (i == _head && _hist_scroll) {
            ImGui::SetScrollHereY(1.0f);
            _hist_scroll = false;
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    if (go >= 0) goto_step(go);
}

void RenderSession::draw_project_section(float full) {
    const ImGuiStyle& st = ImGui::GetStyle();
    const float third = (full - 2.0f * st.ItemSpacing.x) / 3.0f;
    const std::string dir = default_project_dir(_sources.empty() ? "" : _sources[0].path);
    if (ui::KeyButton(msg::project_save, third, "Ctrl+S")) {
        if (_project_path.empty()) {
            if (_pick) _pick(Pick::SaveProject, dir, suggested_project_name());
        } else {
            save_to(_project_path);
        }
    }
    ImGui::SameLine();
    if (ui::Button(msg::project_save_as, ImVec2(third, 0))) ask_save_as();
    ImGui::SameLine();
    if (ui::Button(msg::project_open, ImVec2(third, 0)) && _pick)
        _pick(Pick::OpenProject, dir, "");
    if (!_project_path.empty()) {
        ui::TextDisabledRaw(elide_middle(_project_path, full));
        if (ImGui::IsItemHovered()) ui::SetTooltipRaw(_project_path);
    } else {
        ui::TextDisabledWrapped(msg::project_where, {dir});
    }
    // What is already there, one click away.
    std::error_code ec;
    if (fs::is_directory(fs::u8path(dir), ec)) {
        int shown = 0;
        for (const auto& e : fs::directory_iterator(fs::u8path(dir), ec)) {
            if (e.path().extension() != ".json" || shown >= 8) continue;
            ImGui::PushID(shown++);
            if (ui::SelectableRaw(e.path().filename().string())) open_from(e.path().string());
            ImGui::PopID();
        }
    }

    // The whole move at once, for a model turned in the editor after the
    // path was laid out.
    ui::SeparatorText(msg::sec_whole_path);
    ui::TextDisabledWrapped(msg::whole_path_hint);
    const char* axes[3] = {"X", "Y", "Z"};
    for (int a = 0; a < 3; a++) {
        ImGui::PushID(a);
        if (a) ImGui::SameLine();
        if (ui::Button(msg::whole_path_turn, {std::string(axes[a])})) {
            double axis[3] = {0, 0, 0};
            axis[a] = 1.0;
            double c[3] = {0, 0, 0};
            for (const Keyframe& k : _project.keys)
                for (int d = 0; d < 3; d++) c[d] += k.pos[d] / (double)_project.keys.size();
            transform_project(_project, spirula::Sim3::rotation_about(axis, kPi / 2, c));
            project_changed();
        }
        ImGui::PopID();
    }
}

void RenderSession::draw_output_section(float full) {
    Output& o = _project.output;
    const ImGuiStyle& st = ImGui::GetStyle();
    const float w = full * 0.62f;
    ImGui::BeginDisabled(exporting());

    // Size: named first, numbers for anyone who has their own.
    char cur[64];
    std::snprintf(cur, sizeof cur, "%d \xc3\x97 %d", o.width, o.height);
    ImGui::SetNextItemWidth(w);
    if (ui::BeginComboRaw("##res", cur)) {
        for (int i = 0; i < kNumResolutions; i++) {
            const Resolution& r = kResolutions[i];
            char b[96];
            std::snprintf(b, sizeof b, "%d \xc3\x97 %d", r.w, r.h);
            ImGui::PushID(i);
            if (ui::SelectableRaw(format(msg::res_entry, {r.name->get(), std::string(b)}),
                                  o.width == r.w && o.height == r.h)) {
                o.width = r.w;
                o.height = r.h;
                project_changed();
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ui::Text(msg::out_size);
    int wh[2] = {o.width, o.height};
    ImGui::SetNextItemWidth(w);
    if (ui::InputInt2Raw("##wh", wh, ImGuiInputTextFlags_EnterReturnsTrue)) {
        o.width = std::clamp(wh[0], 16, 16384);
        o.height = std::clamp(wh[1], 16, 16384);
        project_changed();
    }
    ImGui::SameLine();
    ui::TextDisabled(msg::out_pixels);
    bool sphere = false;
    for (int i = 0; i < (int)_project.keys.size() && !sphere; i++)
        sphere = _project.lens_at(i).projection == Projection::Equirect;
    if (sphere && o.width != 2 * o.height) {
        ui::TextColoredWrapped(kErr, msg::out_not_2to1, {(long long)o.width, (long long)o.height});
        if (ui::Button(msg::out_make_2to1)) {
            o.height = std::max(8, o.width / 2);
            o.width = 2 * o.height;
            project_changed();
        }
    }

    if (o.kind == OutputKind::Video || o.kind == OutputKind::Frames) {
        static const double kRates[] = {24, 25, 30, 50, 60};
        char r[32];
        std::snprintf(r, sizeof r, "%.3g fps", o.fps);
        ImGui::SetNextItemWidth(w);
        if (ui::BeginComboRaw("##fps", r)) {
            for (double f : kRates) {
                char b[32];
                std::snprintf(b, sizeof b, "%.3g fps", f);
                if (ui::SelectableRaw(b, o.fps == f)) { o.fps = f; project_changed(); }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ui::Text(msg::out_fps);
    }
    if (o.kind == OutputKind::Video) {
        const Msg* shown[kNumCodecs];
        int codec = 0;
        for (int i = 0; i < kNumCodecs; i++) {
            shown[i] = kCodecs[kCodecOrder[i]];
            if (kCodecOrder[i] == (int)o.codec) codec = i;
        }
        ImGui::SetNextItemWidth(w);
        if (combo_msgs("##codec", &codec, shown, kNumCodecs)) {
            o.codec = (Codec)kCodecOrder[codec];
            fit_output_path(o);
            project_changed();
        }
        ImGui::SameLine();
        ui::Text(msg::out_format);
        ui::help_on_hover(msg::out_codec_help);
        ImGui::SetNextItemWidth(w);
        if (ui::ComboRaw("##q", &o.quality, {&msg::quality_best, &msg::quality_standard,
                                             &msg::quality_small}))
            project_changed();
        ImGui::SameLine();
        ui::Text(msg::out_quality);
        // Which program will make it, and why when it is not the GPU.
        if (o.codec == Codec::Gif) {
            ui::TextDisabledWrapped(msg::encoder_gif);
        } else if (_encoder_probe.load() == 1) {
            ui::TextDisabledWrapped(msg::encoder_checking);
        } else {
            const int W = o.width + (o.width & 1), H = o.height + (o.height & 1);
            const Encoder e = pick_encoder();
            const int c = std::min((int)o.codec, 2);
            const bool gpu_codec = o.codec != Codec::Av1Webm &&
                                   (_encoder_codecs.load() & (1 << c)) != 0;
            if (e.kind == Encoder::BuiltIn) {
                ui::TextDisabledWrapped(msg::encoder_builtin);
            } else if (gpu_codec && !builtin_encodes(o.codec, W, H)) {
                ui::TextDisabledWrapped(e.kind == Encoder::Ffmpeg ? msg::encoder_too_large
                                                                  : msg::encoder_too_large_none,
                                        {kCodecs[c]->get(), (long long)_encoder_max[c][0],
                                         (long long)_encoder_max[c][1]});
            } else if (e.kind == Encoder::Ffmpeg) {
                ui::TextDisabledWrapped(msg::encoder_ffmpeg, {e.codec});
            } else {
                ui::TextDisabledWrapped(msg::encoder_none);
            }
        }
    } else {
        int f = (int)o.format;
        ImGui::SetNextItemWidth(w);
        if (combo_msgs("##fmt", &f, kImageFormats, kNumImageFormats)) {
            o.format = (ImageFormat)f;
            fit_output_path(o);
            project_changed();
        }
        ImGui::SameLine();
        ui::Text(msg::out_format);
        if (o.format == ImageFormat::Jpeg) {
            ImGui::SetNextItemWidth(w);
            if (ui::SliderIntRaw("##jq", &o.jpeg_quality, 50, 100, "%d")) project_changed();
            ImGui::SameLine();
            ui::Text(msg::out_quality);
        }
    }

    // Where it goes. A video and a photo are files; frames go in a folder.
    ImGui::SetNextItemWidth(full - px(90.0f) - st.ItemSpacing.x);
    std::string path = o.path;
    if (ui::InputTextWithHintRaw("##out", msg::out_path_hint, &path,
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
        o.path = path;
        project_changed();
    }
    ImGui::SameLine();
    if (ui::Button(msg::out_browse, ImVec2(px(90.0f), 0)) && _pick) {
        _export_after_pick = false;
        _pick(Pick::Output, _sources.empty() ? "" : default_project_dir(_sources[0].path),
              suggested_output_name());
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
    if (!exporting()) {
        const Msg& go = o.kind == OutputKind::Photo ? msg::render_photo
                        : o.kind == OutputKind::Video ? msg::render_video : msg::render_frames;
        ImGui::BeginDisabled(_project.keys.empty());
        if (ui::Button(go, ImVec2(full, ImGui::GetFrameHeight() * 1.6f))) start_export();
        ImGui::EndDisabled();
        const double T = _project.duration() - (_project.keys.empty() ? 0.0 : _project.keys.front().time);
        if (o.kind != OutputKind::Photo)
            ui::TextDisabled(msg::out_summary, {seconds(T), (long long)frame_count()});
    } else {
        const double now = progress_now();
        if (!_job_clock.started() || _job.frame == 0) _job_clock.start(now);
        if (_job.frames > 0) _job_clock.work(now, (double)_job.frame / (double)_job.frames);
        ui::ProgressBar(_job_clock.fraction(), ImVec2(full, 0), msg::render_progress,
                        {(long long)_job.frame, (long long)_job.frames});
        progress_time_line(_job_clock);
        if (ui::Button(msg::render_cancel, ImVec2(full, 0))) cancel_export();
    }
}

}  // namespace gui::render
