// MaskPanel.cpp -- the mask editor's window: tool strip, canvas, status
// strip, navigation, and the window texture with its dirty-rect upload.
// ImGui is permitted here and in PathOverlay.cpp (the pen tool's overlay,
// carved out on purpose so mask_doc_test stays imgui-free); GL is called
// only here. Every other file in this directory has neither.

#include "app/gui/mask/MaskSession.h"

#include "app/gui/mask/PathOverlay.h"

#include "app/gui/DatasetPrep.h"
#include "app/gui/Layout.h"
#include "app/gui/MaskPrompt.h"
#include "app/gui/MaskSettings.h"
#include "app/gui/Ui.h"
#include "core/CubicBezier.h"
#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/MaskEdit.h"

#include "imgui.h"

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>

namespace msg = spirula::i18n::msg::maskedit;
namespace dmsg = spirula::i18n::msg::dataset;

namespace gui {
namespace mask {

namespace {

double now_ms() {
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

std::string one_decimal(double v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.1f", v);
    return buf;
}

// A closed pen path in pane pixels as the polygon commit_stroke fills,
// flattened to a quarter of a MASK pixel whatever the zoom.
ShapeStroke pen_stroke(const std::vector<float>& anchors, float pane_per_mask) {
    ShapeStroke s;
    s.kind = ShapeKind::Polygon;
    bezier::flatten_closed(anchors.data(), anchors.size() / bezier::kAnchorFloats, 1.0f, 1.0f,
                           0.25f * pane_per_mask, s.pts);
    return s;
}

}  // namespace

void MaskSession::destroy_gl() {
    if (_tex) glDeleteTextures(1, &_tex);
    if (_tex2) glDeleteTextures(1, &_tex2);
    if (_slide_tex) glDeleteTextures(1, &_slide_tex);
    _tex = _tex2 = _slide_tex = 0;
    _slide_tex_w = _slide_tex_h = 0;
    _win = Window{};
    _win2 = Window{};
    _win_dirty = true;
}

void MaskSession::upload_window(GLuint& tex, const Window& win, Style style,
                                std::vector<uint8_t>& rgba) {
    if (win.r.empty()) return;
    WindowSource src = window_source();
    src.style = style;
    derive_window(win, win.r, src, rgba);
    if (!tex) glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, win.tw, win.th, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 rgba.data());
}

void MaskSession::ensure_window(const Mapping& m, float pane_w, float pane_h) {
    const Window want = window_for(m, _dw, _dh, pane_w, pane_h);
    const PaneDerive d =
        plan_derive(_win_dirty, want, _view_mode, _peek, _win, _win_style, _win2, _win2_style);
    _win_dirty = false;
    if (d.left) upload_window(_tex, _win, _win_style, _rgba);
    if (d.right) upload_window(_tex2, _win2, _win2_style, _rgba2);
}

void MaskSession::upload_rect_to(GLuint tex, const Window& win, Style style,
                                 std::vector<uint8_t>& rgba, const Rect& shown) {
    if (!tex || win.r.empty() || shown.empty()) return;
    WindowSource src = window_source();
    src.style = style;
    const Rect t = derive_window(win, shown, src, rgba);
    if (t.empty()) return;
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, win.tw);
    glTexSubImage2D(GL_TEXTURE_2D, 0, t.x0, t.y0, t.w(), t.h(), GL_RGBA, GL_UNSIGNED_BYTE,
                    rgba.data() + ((size_t)t.y0 * win.tw + (size_t)t.x0) * 4);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

void MaskSession::upload_rect(const Rect& shown) {
    upload_rect_to(_tex, _win, _win_style, _rgba, shown);
    if (_view_mode == ViewMode::SideBySide) upload_rect_to(_tex2, _win2, _win2_style, _rgba2, shown);
}

void MaskSession::draw() {
    if (!_open) return;
    // close() joins the worker and, on a dirty frame, that means waiting out
    // an 8K MaskDoc::save() (~700 ms). Deferring to the NEXT call means the
    // frame that requested it still reaches the screen before the block.
    if (_close_requested) { close(); return; }
    _popup_at_start = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopup);
    pump();
    {
        const double t0 = now_ms();
        const int before[3] = {_sam_results, _sam_reapplies, _sam_margin_starts};
        upload_rect(sam_pump());
        note_sam_ui(before, now_ms() - t0);
    }
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos, ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(vp->WorkSize, ImGuiCond_Appearing);
    // No narrower than the tool row measured last frame: Revert has no key.
    ImGui::SetNextWindowSizeConstraints(ImVec2(_toolbar_w, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
    bool open = true;
    if (ImGui::Begin(ui::detail::label(msg::window_title), &open,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
        draw_toolbar();
        draw_canvas();
        const float status_y = ImGui::GetCursorPosY();
        draw_status();
        _status_h = ImGui::GetCursorPosY() - status_y;
        _strip.update(_status_h, (int)_mode * 3 + (int)_view_mode, ImGui::GetWindowWidth());
    }
    ImGui::End();
    if (!open) _close_requested = true;
    // The one thing that can still reach the screen once the deferred close
    // above starts blocking next frame (GuiMain.cpp applies it at the top of
    // the loop, ahead of that call).
    if (_close_requested && _doc && _doc->dirty())
        ImGui::SetMouseCursor(ImGuiMouseCursor_Wait);
}

void MaskSession::pick_tool(ToolId t) {
    _tool.set_id(t);
    set_mode(CanvasMode::Shape);
    _path.cancel();
    _pen.cancel();
}

// The eraser is the brush shape under a different paint mode, not a shape of
// its own: ToolId is the 3D editor's SELECTION-shape table and a row there
// would put an "Eraser" button in a panel where it means nothing.
void MaskSession::pick_eraser() {
    _tool.set_id(ToolId::Brush);
    set_mode(CanvasMode::Eraser);
    _path.cancel();
    _pen.cancel();
}

void MaskSession::pick_path() {
    set_mode(CanvasMode::Path);
    _tool.cancel();
    _pen.cancel();
}

void MaskSession::pick_pen() {
    set_mode(CanvasMode::Pen);
    _tool.cancel();
    _path.cancel();
}

// A click here is a prompt, not a stroke: nothing half drawn may survive into it.
void MaskSession::pick_sam() {
    set_mode(CanvasMode::Sam);
    _tool.cancel();
    _path.cancel();
    _pen.cancel();
}

void MaskSession::draw_toolbar() {
    const float w = px(96.0f);
    for (int i = (int)ToolId::Box; i <= (int)ToolId::Brush; i++) {
        const ToolRow& row = tool_table()[i];
        if (i != (int)ToolId::Box) ImGui::SameLine();
        // The pen takes the polygon's place and its P, as in a vector editor:
        // clicked, it draws one.
        if (row.id == ToolId::Polygon) {
            if (ui::KeyButton(msg::tool_pen, w, "P", pen_mode())) pick_pen();
            continue;
        }
        if (ui::KeyButton(tool_label(row.id), w, row.key,
                          mode() == CanvasMode::Shape && _tool.id() == row.id))
            pick_tool(row.id);
    }
    ImGui::SameLine();
    // X, not E: E is the Ellipse in the shared key table, and rebinding it
    // would move a shortcut the editor already documents.
    if (ui::KeyButton(msg::tool_eraser, w, "X", erasing())) pick_eraser();
    ImGui::SameLine();
    if (ui::KeyButton(msg::tool_path, w, "I", path_mode())) pick_path();
    ImGui::SameLine();
    // Armable with no checkpoint: the SAM strip is where one is picked and fetched.
    ImGui::BeginDisabled(!sam_available());
    if (ui::KeyButton(msg::tool_sam, w, "G", sam_mode())) pick_sam();
    ImGui::EndDisabled();
    if (!sam_available())
        ui::help_on_hover_raw(backends().masking_reason.c_str(),
                              ImGuiHoveredFlags_AllowWhenDisabled);
    ImGui::SameLine();
    ImGui::BeginDisabled(!_doc || !_doc->can_undo());
    if (ui::Button(msg::undo)) upload_rect(undo_step());
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!_doc || !_doc->can_redo());
    if (ui::Button(msg::redo)) upload_rect(redo());
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!_doc || !_doc->dirty());
    if (ui::Button(msg::save)) save();
    ImGui::EndDisabled();
    ui::help_on_hover(msg::save_help);
    ImGui::SameLine();
    ImGui::BeginDisabled(!_doc || !idle());
    if (ui::Button(msg::revert_frame)) revert_open_frame();
    ImGui::EndDisabled();
    ui::help_on_hover(msg::revert_frame_help);
    ImGui::SameLine();
    // Only when there is something to lose, and never in one click: it
    // deletes every hand correction in the dataset.
    ImGui::BeginDisabled(!idle() || _slide_playing ||
                         (corrected_count() == 0 && !(_doc && _doc->dirty())));
    if (ui::Button(msg::revert_all)) _revert_all_ask = true;
    ImGui::EndDisabled();
    ui::help_on_hover(msg::revert_all_help);
    draw_revert_all_modal();
    ImGui::SameLine(0.0f, px(24.0f));
    // The way out of the window, so it looks like one rather than one more tool.
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.52f, 0.28f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.62f, 0.34f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.13f, 0.44f, 0.24f, 1.0f));
    if (ui::Button(msg::done, ImVec2(px(120.0f), 0.0f))) _close_requested = true;
    ImGui::PopStyleColor(3);
    ui::help_on_hover(msg::done_help);
    _toolbar_w = ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x +
                 ImGui::GetStyle().WindowPadding.x;

    const bool shape = shape_open();
    ImGui::BeginDisabled(!idle() || _slide_playing || shape);
    if (ui::ButtonRaw("<")) go_to(_idx - 1);
    if (shape) ui::help_on_hover_disabled(msg::nav_locked);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(px(260.0f));
    ui::SliderIntRaw("##maskframe", &_slider_idx, 0, std::max(0, frame_count() - 1), "%d");
    if (ImGui::IsItemDeactivatedAfterEdit() && _slider_idx != _idx) go_to(_slider_idx);
    ui::help_on_hover_disabled(shape ? msg::nav_locked : msg::hint_keys);
    ImGui::SameLine();
    if (ui::ButtonRaw(">")) go_to(_idx + 1);
    if (shape) ui::help_on_hover_disabled(msg::nav_locked);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ui::RadioButton(msg::mode_add, !_subtract)) _subtract = false;
    ui::help_on_hover(msg::mode_help);
    ImGui::SameLine();
    if (ui::RadioButton(msg::mode_subtract, _subtract)) _subtract = true;
    ui::help_on_hover(msg::mode_help);

    // On this row rather than a third one: a third row comes out of what
    // draw_canvas has to share between canvas and strip, raising the window
    // height at which the strip clips.
    if (erasing() || (mode() == CanvasMode::Shape && _tool.id() == ToolId::Brush)) {
        ImGui::SameLine();
        // 340 rather than the frame slider's 260 so the corner hint below
        // clears the centred value at its widest ("Eraser: 4096 px").
        ImGui::SetNextItemWidth(px(340.0f));
        // Logarithmic because the steps are multiplicative over twelve
        // octaves: linear travel would put every usable size in the first 2%.
        const std::string fmt =
            spirula::i18n::format(erasing() ? msg::eraser_radius : msg::brush_radius, {"%.0f"});
        float r = radius();
        if (ui::SliderFloatRaw("##maskbrush", &r, kMinBrush, kMaxBrush, fmt.c_str(),
                               ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp))
            set_radius(r);
        // The same corner the tool buttons carry their key in: the keys work
        // and always did, and the strip's `hint_view` was the only place
        // saying so, four lines down at the bottom of the window.
        ui::corner_key(msg::radius_keys.get());
        ui::help_on_hover(msg::radius_help);
    }
    draw_workflow_row();
}

// The window may not be narrower than any toolbar row, or that row's right
// end clips unseen; _toolbar_w starts each frame at the tool row's width.
bool MaskSession::shape_open() const {
    return _tool.in_progress() || _path.in_progress() || _pen.in_progress();
}

void MaskSession::note_row_width() {
    _toolbar_w = std::max(_toolbar_w, ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x +
                                          ImGui::GetStyle().WindowPadding.x);
}

// Rows A to D: the view and Play, propagate, its warning, find missing.
void MaskSession::draw_workflow_row() {
    const bool locked = _tool.in_progress();
    const spirula::i18n::Msg* names[3] = {&msg::view_overlay, &msg::view_mask_only,
                                          &msg::view_side_by_side};
    ImGui::BeginDisabled(locked);
    for (int i = 0; i < 3; i++) {
        if (i > 0) ImGui::SameLine();
        if (ui::RadioButton(*names[i], (int)_view_mode == i))
            _view_mode = switch_view(_view_mode, (ViewMode)i, locked);
        if (locked) ui::help_on_hover_disabled(msg::view_locked);
        else ui::help_on_hover(msg::view_help);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    // Play waits for anything that would land on the document it releases.
    ImGui::BeginDisabled(frame_count() < 2 ||
                         (!_slide_playing && (!_doc || !idle() || sam_work_pending() ||
                                              shape_open())));
    if (ui::Button(_slide_playing ? msg::slide_stop : msg::slide_play)) {
        if (_slide_playing) stop_slideshow();
        else start_slideshow();
    }
    ImGui::EndDisabled();
    ui::help_on_hover_disabled(msg::slide_help);
    ImGui::SameLine();
    int fps = (int)std::lround(_slide_fps);
    ImGui::SetNextItemWidth(px(160.0f));
    if (ui::SliderInt(msg::slide_fps, &fps, 5, 30)) set_slide_fps((float)fps);
    note_row_width();

    // Row B: propagate. Row C: its warning, drawn whether or not B is enabled.
    ImGui::BeginDisabled(!_doc || !idle() || _slide_playing);
    if (ui::RadioButton(msg::prop_scope_next, _prop_scope == 0)) _prop_scope = 0;
    ImGui::SameLine();
    if (ui::RadioButton(msg::prop_scope_range, _prop_scope == 1)) _prop_scope = 1;
    ImGui::SameLine();
    if (ui::RadioButton(msg::prop_scope_camera, _prop_scope == 2)) _prop_scope = 2;
    // Always drawn, so the row never changes width with the scope.
    ImGui::BeginDisabled(_prop_scope != 1);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(px(90.0f));
    ui::InputInt(msg::prop_from, &_prop_from);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(px(90.0f));
    ui::InputInt(msg::prop_to, &_prop_to);
    ImGui::EndDisabled();
    _prop_from = std::clamp(_prop_from, 1, std::max(1, frame_count()));
    _prop_to = std::clamp(_prop_to, 1, std::max(1, frame_count()));
    ImGui::SameLine();
    ImGui::BeginDisabled(sam_work_pending());
    if (ui::Button(msg::prop_go)) {
        const PropagateScope scope = _prop_scope == 0 ? PropagateScope::Next
                                   : _prop_scope == 1 ? PropagateScope::Range
                                                      : PropagateScope::Camera;
        propagate(scope, _prop_from - 1, _prop_to - 1);   // the one 1-to-0-based step
    }
    ImGui::EndDisabled();
    ui::help_on_hover_disabled(msg::prop_go_help);
    ImGui::SameLine();
    ImGui::BeginDisabled(!can_undo_propagate());
    if (ui::Button(msg::prop_undo)) undo_propagate();
    ImGui::EndDisabled();
    ui::help_on_hover_disabled(msg::prop_undo_help);
    ImGui::EndDisabled();
    note_row_width();
    // In the warning's row and at its height, so a run moves nothing below it.
    if (const int total = propagate_total(); total > 0) {
        const int done = std::min(propagate_done(), total);
        ui::ProgressBar((float)done / (float)total,
                        ImVec2(px(420.0f), ImGui::GetTextLineHeight()), msg::prop_progress,
                        {done, total});
        ImGui::SameLine();
        ImGui::BeginDisabled(propagate_cancelling());
        if (ui::SmallButton(msg::prop_cancel)) cancel_propagate();
        ImGui::EndDisabled();
        ui::help_on_hover_disabled(msg::prop_cancel_help);
    } else {
        ui::TextDisabled(msg::prop_warn_moves);
    }
    note_row_width();

    // Row D: find missing. The count's width changes as the scan runs, which
    // moves the minimum width, never the height.
    const spirula::i18n::Msg& find_tip = shape_open() ? msg::nav_locked : msg::find_help;
    ImGui::BeginDisabled(!idle() || _slide_playing || shape_open());
    if (ui::Button(msg::find_first)) go_to(0);
    ui::help_on_hover_disabled(find_tip);
    ImGui::SameLine();
    if (ui::Button(msg::find_prev)) go_to_missing(-1);
    ui::help_on_hover_disabled(find_tip);
    ImGui::SameLine();
    if (ui::Button(msg::find_next)) go_to_missing(+1);
    ui::help_on_hover_disabled(find_tip);
    ImGui::SameLine();
    if (ui::Button(msg::find_last)) go_to(frame_count() - 1);
    ui::help_on_hover_disabled(find_tip);
    ImGui::EndDisabled();
    ImGui::SameLine();
    int lo_pct = (int)std::lround(100.0f * _band_lo), hi_pct = (int)std::lround(100.0f * _band_hi);
    ImGui::SetNextItemWidth(px(110.0f));
    const bool lo_changed = ui::InputInt(msg::find_band_lo, &lo_pct);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(px(110.0f));
    const bool hi_changed = ui::InputInt(msg::find_band_hi, &hi_pct);
    if (lo_changed || hi_changed) {
        band_edit(lo_pct, hi_pct, lo_changed);
        set_band(0.01f * (float)lo_pct, 0.01f * (float)hi_pct);
    }
    ImGui::SameLine();
    if (scanned_count() < frame_count()) ui::Text(msg::find_scanning, {scanned_count(), frame_count()});
    else ui::Text(msg::find_count, {missing_count()});
    note_row_width();
}

void MaskSession::draw_canvas() {
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    // What the strip actually took last frame. Its height is however many
    // lines the tool and the wrapped hints produce, which no constant can
    // know; frame one has no measurement, so seed the pen tool's eight.
    const float status_h = _strip.h > 0.0f ? _strip.h
                                           : 8.0f * ImGui::GetTextLineHeightWithSpacing();
    const ImVec2 size(std::max(avail.x, px(64.0f)), std::max(avail.y - status_h, px(64.0f)));
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 far_corner(origin.x + size.x, origin.y + size.y);
    _canvas_h = size.y;
    ui::InvisibleButtonRaw("##maskcanvas", size,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered = ImGui::IsItemHovered();
    // Owning Tab stops imgui's nav tabbing (it polls Tab with NoOwner). Not
    // while a text field has focus: the canvas still reads hovered then, and
    // owning Tab kept the focus in the field and peeked instead of tabbing out.
    const bool typing = ImGui::GetIO().WantTextInput;
    if (!typing) ImGui::SetItemKeyOwner(ImGuiKey_Tab);
    const bool tab = !typing && (hovered || ImGui::IsItemActive()) &&
                     ImGui::IsKeyDown(ImGuiKey_Tab);
    _peek = !tab ? Peek::None : ImGui::GetIO().KeyShift ? Peek::Mask : Peek::Photo;
    if (_peek != Peek::None) _peek_total++;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(origin, far_corner, IM_COL32(24, 24, 24, 255));
    if (_slide_playing) {
        _shown_valid = false;   // what is drawn is no document's picture
        draw_slideshow(dl, origin.x, origin.y, size.x, size.y);
        return;
    }
    if (!_doc) {
        _shown_valid = false;
        _held_pane = -1;
        dl->AddText(ImVec2(origin.x + px(8.0f), origin.y + px(8.0f)),
                    IM_COL32(200, 200, 200, 255), msg::working.get());
        return;
    }

    if (_tool_reset) {
        _tool.cancel();
        if (_tool.id() == ToolId::Navigate) _tool.set_id(ToolId::Brush);
        _tool_reset = false;
    }
    const ImGuiIO& io = ImGui::GetIO();
    const bool space = ImGui::IsKeyDown(ImGuiKey_Space);
    // One pane, or two over one view with a gap.
    const int npanes = _view_mode == ViewMode::SideBySide ? 2 : 1;
    const float gap = npanes == 2 ? px(6.0f) : 0.0f;
    const float pane_w = (size.x - gap * (float)(npanes - 1)) / (float)npanes;
    const int hover_pane = hovered ? pane_at(io.MousePos.x - origin.x, npanes, pane_w, gap) : -1;
    const bool over = hover_pane >= 0;
    const int active = bind_pane(hover_pane, ImGui::IsMouseClicked(ImGuiMouseButton_Left),
                                 ImGui::IsMouseDown(ImGuiMouseButton_Left), npanes);
    const ImVec2 porg(origin.x + pane_left(active, pane_w, gap), origin.y);
    // The view. Never while a stroke is in progress: its points are pane pixels.
    if (!_tool.in_progress()) {
        const Mapping m0 = mapping(_view, _dw, _dh, pane_w, size.y);
        // Alt, because the bare wheel is the zoom and Shift/Ctrl are the
        // paint modes. Guarded by in_progress() with the view: a stroke
        // carries one radius, so changing it mid-stroke would resize all of it.
        if (over && io.MouseWheel != 0.0f) {
            if (io.KeyAlt) set_radius(wheel_brush(radius(), io.MouseWheel));
            else
                zoom_about(_view, std::pow(1.2f, io.MouseWheel), io.MousePos.x - porg.x,
                           io.MousePos.y - porg.y, _dw, _dh, pane_w, size.y);
        }
        const bool pan_down = ImGui::IsMouseDown(ImGuiMouseButton_Middle) ||
                              (space && ImGui::IsMouseDown(ImGuiMouseButton_Left));
        if (over && (ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
                        (space && ImGui::IsMouseClicked(ImGuiMouseButton_Left))))
            _panning = true;
        if (_panning) {
            if (!pan_down) _panning = false;
            else pan(_view, io.MouseDelta.x, io.MouseDelta.y, m0, _dw, _dh);
        }
    }
    const Mapping m = mapping(_view, _dw, _dh, pane_w, size.y);
    ensure_window(m, pane_w, size.y);

    for (int p = 0; p < npanes; p++) {
        const ImVec2 o(origin.x + (float)p * (pane_w + gap), origin.y);
        const GLuint tex = p == 0 ? _tex : _tex2;
        const Window& win = p == 0 ? _win : _win2;
        dl->PushClipRect(o, ImVec2(o.x + pane_w, far_corner.y), true);
        if (tex && !win.r.empty()) {
            const ImVec2 a(o.x + m.to_screen_x((float)win.r.x0),
                           o.y + m.to_screen_y((float)win.r.y0));
            const ImVec2 b(o.x + m.to_screen_x((float)(win.r.x0 + win.tw * win.step)),
                           o.y + m.to_screen_y((float)(win.r.y0 + win.th * win.step)));
            dl->AddImage((ImTextureID)(intptr_t)tex, a, b);
        }
        dl->PopClipRect();
    }
    // Every pane draws the tool at the same pane pixels: a stroke over the photo
    // lands in the mask pane, so the ring has to show in both.
    auto in_each_pane = [&](auto&& draw) {
        for (int p = 0; p < npanes; p++) {
            const ImVec2 o(origin.x + pane_left(p, pane_w, gap), origin.y);
            dl->PushClipRect(o, ImVec2(o.x + pane_w, far_corner.y), true);
            draw(o);
            dl->PopClipRect();
        }
    };

    // The tool, fed pane pixels, the left button only. The modifiers are
    // read from the frame the stroke completes, as EditSession does.
    const bool can_stroke = over && !space && !_panning;
    _tool.set_brush_radius(radius() * m.scale);
    ViewportInput in;
    in.hovered = can_stroke;
    in.x = io.MousePos.x - porg.x;
    in.y = io.MousePos.y - porg.y;
    in.W = (int)pane_w;
    in.H = (int)size.y;
    in.down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    in.clicked = can_stroke && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    in.released = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
    in.right_clicked = can_stroke && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
    in.double_clicked = can_stroke && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    in.shift = io.KeyShift;
    in.ctrl = io.KeyCtrl;
    in.alt = io.KeyAlt;
    if (sam_mode()) {
        // Left is "this", right "not this": the dataset screen's grammar.
        float fx = 0.0f, fy = 0.0f;
        if ((in.clicked || in.right_clicked) && !sam_busy() && sam_has_model() &&
            shown_to_frame(io.MousePos.x, io.MousePos.y, fx, fy)) {
            sam_prompt_point(fx, fy, sam_click_mode(io.KeyShift, io.KeyCtrl), in.clicked);
        }
        for (int p = 0; p < npanes; p++) {
            const float ox = origin.x + (float)p * (pane_w + gap);
            dl->PushClipRect(ImVec2(ox, origin.y), ImVec2(ox + pane_w, far_corner.y), false);
            draw_sam_clicks(dl, m, ox, origin.y);
            dl->PopClipRect();
        }
    } else if (path_mode()) {
        ensure_livewire();
        _path.set_space(path_space(m));
        _path.note_modifiers(io.KeyShift, io.KeyCtrl);
        std::vector<float> poly;
        bool consumed = false;
        if (_path.update(in, poly, consumed)) {
            const double t0 = now_ms();
            ShapeStroke stroke;
            stroke.kind = ShapeKind::Polygon;
            stroke.pts = std::move(poly);
            upload_rect(commit_stroke(stroke, paint_now(_path.mode_shift(), _path.mode_ctrl()), m));
            _last_commit_ms = now_ms() - t0;
        }
        in_each_pane([&](const ImVec2& o) { draw_path_overlay(dl, o, _path); });
    } else if (pen_mode()) {
        _pen.set_space(path_space(m));
        _pen.note_modifiers(io.KeyShift, io.KeyCtrl);
        _pen.note_space(space);
        std::vector<float> anchors;
        bool consumed = false;
        if (_pen.update(in, anchors, consumed)) {
            const double t0 = now_ms();
            upload_rect(commit_stroke(pen_stroke(anchors, m.scale),
                                      paint_now(_pen.mode_shift(), _pen.mode_ctrl()), m));
            _last_commit_ms = now_ms() - t0;
        }
        in_each_pane([&](const ImVec2& o) { draw_pen_overlay(dl, o, _pen); });
    } else {
        ShapeStroke stroke;
        bool consumed = false;
        if (_tool.update(in, stroke, consumed)) {
            const double t0 = now_ms();
            upload_rect(commit_stroke(stroke, paint_now(in.shift, in.ctrl), m));
            _last_commit_ms = now_ms() - t0;
        }
        in_each_pane([&](const ImVec2& o) { _tool.draw_overlay(dl, o); });
    }
    note_shown(m, origin.x, origin.y, npanes, pane_w, gap);
    handle_keys(m);
}

// Any input stops it on the frame shown. The frame Play fired on is exempt for
// a KEYBOARD Play: ImGui::Button fires on mouse release, so the starting click is
// never seen here, but the Space or Enter that pressed it is.
void MaskSession::draw_slideshow(ImDrawList* dl, float ox, float oy, float w, float h) {
    const ImGuiIO& io = ImGui::GetIO();
    bool input = io.MouseWheel != 0.0f;
    for (int b = 0; b < ImGuiMouseButton_COUNT; b++) input = input || ImGui::IsMouseClicked((ImGuiMouseButton)b);
    for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; k++)
        input = input || ImGui::IsKeyPressed((ImGuiKey)k, false);
    const bool fresh = _slide_fresh;
    _slide_fresh = false;
    if (!fresh && input) {
        stop_slideshow();
        return;
    }
    const int side = std::clamp((((int)std::max(w, h) + 255) / 256) * 256, 256, 4096);
    if (slideshow_tick(ImGui::GetTime(), side, _slide_pic) && !_slide_pic.empty()) {
        if (!_slide_tex) glGenTextures(1, &_slide_tex);
        glBindTexture(GL_TEXTURE_2D, _slide_tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        // The storage is re-specified only when the picture's size changes.
        if (_slide_pic.w == _slide_tex_w && _slide_pic.h == _slide_tex_h) {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _slide_pic.w, _slide_pic.h, GL_RGB, GL_UNSIGNED_BYTE, _slide_pic.rgb.data());
        } else {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, _slide_pic.w, _slide_pic.h, 0, GL_RGB, GL_UNSIGNED_BYTE,
                         _slide_pic.rgb.data());
            _slide_tex_w = _slide_pic.w;
            _slide_tex_h = _slide_pic.h;
        }
    }
    if (_slide_tex && _slide_tex_w > 0) {
        const float s = std::min(w / (float)_slide_tex_w, h / (float)_slide_tex_h);
        const float pw = (float)_slide_tex_w * s, ph = (float)_slide_tex_h * s;
        const ImVec2 a(ox + 0.5f * (w - pw), oy + 0.5f * (h - ph));
        dl->AddImage((ImTextureID)(intptr_t)_slide_tex, a, ImVec2(a.x + pw, a.y + ph));
    } else {
        dl->AddText(ImVec2(ox + px(8.0f), oy + px(8.0f)), IM_COL32(200, 200, 200, 255),
                    msg::slide_waiting.get());
    }
}

void MaskSession::handle_keys(const Mapping& m) {
    const ImGuiIO& io = ImGui::GetIO();
    // RootAndChildWindows counts a popup opened from this window as focused, so
    // a modal (or one closed earlier this frame by the same Esc) masks the keys.
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) || io.WantTextInput ||
        _popup_at_start || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopup))
        return;
    if (!io.KeyCtrl) {
        for (int i = (int)ToolId::Box; i <= (int)ToolId::Brush; i++) {
            const ToolRow& row = tool_table()[i];
            if (row.id == ToolId::Polygon) continue;
            if (ImGui::IsKeyPressed((ImGuiKey)row.imgui_key, false)) pick_tool(row.id);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_P, false)) pick_pen();
        if (ImGui::IsKeyPressed(ImGuiKey_I, false)) pick_path();
        if (ImGui::IsKeyPressed(ImGuiKey_X, false)) pick_eraser();
        if (ImGui::IsKeyPressed(ImGuiKey_G, false) && sam_available()) pick_sam();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        if (sam_mode()) sam_cancel();
        else if (path_mode()) _path.cancel();
        else if (pen_mode()) _pen.cancel();
        else _tool.cancel();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
        ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) {
        const Paint mode = path_mode() ? paint_now(_path.mode_shift(), _path.mode_ctrl())
                           : pen_mode() ? paint_now(_pen.mode_shift(), _pen.mode_ctrl())
                                        : paint_now(io.KeyShift, io.KeyCtrl);
        ShapeStroke s;
        bool pending = false;
        if (path_mode()) {
            std::vector<float> poly;
            pending = _path.commit_pending(poly);
            s.kind = ShapeKind::Polygon;
            s.pts = std::move(poly);
        } else if (pen_mode()) {
            std::vector<float> anchors;
            pending = _pen.commit_pending(anchors);
            if (pending) s = pen_stroke(anchors, m.scale);
        } else {
            pending = _tool.commit_pending(s);
        }
        if (pending) {
            const double t0 = now_ms();
            upload_rect(commit_stroke(s, mode, m));
            _last_commit_ms = now_ms() - t0;
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket, true)) set_radius(step_brush(radius(), false));
    if (ImGui::IsKeyPressed(ImGuiKey_RightBracket, true)) set_radius(step_brush(radius(), true));
    if (pen_mode() && _pen.in_progress() &&
        (ImGui::IsKeyPressed(ImGuiKey_Backspace, false) || ImGui::IsKeyPressed(ImGuiKey_Delete, false)))
        _pen.pop_anchor();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        if (!io.KeyShift && pen_mode() && _pen.in_progress() && _pen.pop_anchor()) return;
        if (!io.KeyShift && path_mode() && _path.in_progress() && _path.pop_anchor()) return;
        if (!io.KeyShift && !path_mode() && _tool.id() == ToolId::Polygon && _tool.in_progress() &&
            _tool.pop_point())
            return;
        upload_rect(io.KeyShift ? redo() : undo_step());
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) save();
    const ImGuiInputFlags route = ImGuiInputFlags_RouteFocused |
                                  ImGuiInputFlags_RouteFromRootWindow;
    if (!io.KeyCtrl && ImGui::Shortcut(ImGuiKey_V, route))
        _view_mode = switch_view(_view_mode, (ViewMode)(((int)_view_mode + 1) % 3),
                                 _tool.in_progress());
    // Not while a pen path is open: pump() cancels it on the new frame, silently.
    const ImGuiInputFlags rep = route | ImGuiInputFlags_Repeat;
    if (!shape_open() && idle() && !io.KeyCtrl) {
        if (ImGui::Shortcut(ImGuiKey_LeftArrow, rep)) go_to(_idx - 1);
        if (ImGui::Shortcut(ImGuiKey_RightArrow, rep)) go_to(_idx + 1);
        if (ImGui::Shortcut(ImGuiKey_PageUp, rep)) go_to(std::max(0, _idx - 10));
        if (ImGui::Shortcut(ImGuiKey_PageDown, rep)) go_to(std::min(frame_count() - 1, _idx + 10));
        if (ImGui::Shortcut(ImGuiKey_Home, route)) go_to(0);
        if (ImGui::Shortcut(ImGuiKey_End, route)) go_to(frame_count() - 1);
        if (ImGui::Shortcut(ImGuiKey_M, route)) go_to_missing(+1);
        if (ImGui::Shortcut(ImGuiMod_Shift | ImGuiKey_M, route)) go_to_missing(-1);
    }
}

// Names what goes -- the corrected-frame count, and the open frame's unsaved
// strokes -- and needs a second, explicit click. Cancel is the safe default.
void MaskSession::draw_revert_all_modal() {
    if (_revert_all_ask) {
        ui::OpenPopup(msg::revert_all_title);
        _revert_all_ask = false;
    }
    if (!ui::BeginPopupModal(msg::revert_all_title, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;
    ImGui::PushTextWrapPos(px(460.0f));
    ui::Text(msg::revert_all_confirm);
    ImGui::Spacing();
    ui::Text(msg::corrected_count, {corrected_count()});
    if (_doc && _doc->dirty()) ui::Text(msg::revert_all_unsaved);
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    // Esc is Cancel, the convention; handle_keys skips this frame, so the same Esc
    // cannot also cancel a stroke or a path behind the modal.
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) ImGui::CloseCurrentPopup();
    if (ui::Button(msg::revert_all_button, ImVec2(px(220.0f), 0)) && idle()) {
        revert_every_frame();
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ui::Button(dmsg::cancel, ImVec2(px(150.0f), 0))) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void MaskSession::draw_status() {
    // First, where a short window clips it last: a failed save's error.
    const std::string err = error();
    const std::string st = err.empty() ? status() : std::string();
    if (!err.empty()) ui::TextColoredRaw(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), err);
    else if (!st.empty()) ui::TextDisabledRaw(st);
    else ImGui::Dummy(ImVec2(0.0f, ImGui::GetTextLineHeight()));
    const int at = _slide_playing ? _slide_index : _idx;
    if (at >= 0 && at < frame_count()) {
        const FrameRef& f = _frames[(size_t)at];
        ui::Text(msg::status_frame, {at + 1, frame_count(), f.key});
        ImGui::SameLine();
        ui::Text(msg::status_camera, {f.camera.empty() ? std::string("/") : f.camera});
        if (_slide_shown > 0) {
            ImGui::SameLine();
            ui::Text(msg::slide_stats, {one_decimal(slide_shown_fps()), one_decimal(_slide_max_gap)});
        }
    }
    if (_doc) {
        ui::Text(msg::status_kept, {one_decimal(100.0 * _doc->kept_fraction())});
        ImGui::SameLine();
        ui::TextDisabled(_doc->dirty() ? msg::status_unsaved : msg::status_saved);
        ImGui::SameLine();
        ui::Text(msg::corrected_count, {corrected_count()});
        if (_doc->base_state() == BaseState::Regenerated)
            ui::TextDisabledWrapped(msg::status_base_regenerated);
        if (_doc->base_state() == BaseState::Missing)
            ui::TextDisabledWrapped(msg::status_base_missing);
    }
    if (sam_mode()) {
        // Playing, the list would describe the frame play started on.
        ImGui::BeginDisabled(_slide_playing);
        draw_sam_status();
        ImGui::EndDisabled();
    } else {
        ui::Text(erasing() ? msg::eraser_radius : msg::brush_radius,
                 {(int)std::lround(radius())});
        ImGui::SameLine();
        ui::Text(msg::status_commit, {one_decimal(_last_commit_ms)});
        ui::TextDisabledWrapped(erasing() != _subtract ? msg::hint_eraser : msg::hint_buttons);
    }
    if (path_mode()) {
        ui::TextDisabledWrapped(msg::hint_path);
        ui::Text(msg::path_anchors, {_path.anchor_count()});
        if (!_path.snapping()) ui::TextDisabledWrapped(msg::path_straight);
    }
    if (pen_mode()) {
        ui::TextDisabledWrapped(msg::hint_pen);
        ui::Text(msg::pen_anchors, {_pen.anchor_count()});
    }
    ui::TextDisabledWrapped(msg::hint_view);
    ui::TextDisabledWrapped(_view_mode == ViewMode::SideBySide ? msg::peek_hint_side
                                                               : msg::peek_hint, {"Tab"});
}

// The editor's clicks on this frame, as SegmentPanel draws them: the object's
// colour, and red with a cross for "not this" so colour is not the only cue.
void MaskSession::draw_sam_clicks(ImDrawList* dl, const Mapping& m, float ox, float oy) {
    if (!_sam || _idx < 0 || _idx >= frame_count()) return;
    const PathSpace ps = path_space(m);
    const std::string& camera = _frames[(size_t)_idx].camera;
    const float r = px(6.0f), k = px(3.0f);
    for (const MaskClick& c : sam_prompt().clicks) {
        if (!c.source.empty() || c.frame != _idx || c.camera != camera) continue;
        float x = 0.0f, y = 0.0f;
        ps.from_frame(c.x, c.y, x, y);
        const ImVec2 p(ox + x, oy + y);
        dl->AddCircleFilled(p, r, c.positive ? (ImU32)mask_object_color(c.object)
                                             : IM_COL32(240, 90, 90, 255));
        dl->AddCircle(p, r, IM_COL32(20, 20, 20, 200), 0, 1.5f);
        if (c.positive) continue;
        dl->AddLine(ImVec2(p.x - k, p.y - k), ImVec2(p.x + k, p.y + k), IM_COL32(255, 255, 255, 255), 1.5f);
        dl->AddLine(ImVec2(p.x - k, p.y + k), ImVec2(p.x + k, p.y - k), IM_COL32(255, 255, 255, 255), 1.5f);
    }
}

// GuiApp's picker over the app's one model, the hint, then a two-line slot the
// busy, error and result lines share: none of them may resize the canvas.
void MaskSession::draw_sam_status() {
    if (_model_picker) _model_picker();
    if (!sam_has_model()) ui::TextDisabled(dmsg::mask_model_first);
    // The editor's own margin, never the dataset's. Every add takes it, and a
    // release re-applies it to the add just made (sam_margin_changed).
    MaskSettings& p = sam_prompt();
    if (draw_margin_slider(p.dilate_ratio, p.shrink_ratio, /*keep=*/false, px(220.0f),
                           /*inline_label=*/true))
        _sam_margin_moved = true;
    if (_sam_margin_moved && !ImGui::IsAnyItemActive()) {
        _sam_margin_moved = false;
        sam_margin_changed();
    }
    ui::TextDisabledWrapped(msg::sam_hint);
    draw_sam_objects();
    draw_sam_text();
    const float y0 = ImGui::GetCursorPosY();
    const std::string sam_err = sam_error();
    if (sam_busy()) {
        ui::TextDisabledRaw(sam_status());
        ui::TextDisabledWrapped(msg::sam_cancel_slow);
    } else if (!sam_err.empty()) {
        ui::TextColoredRaw(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), sam_err);
    } else if (const spirula::i18n::Msg* note = sam_empty_note()) {
        ui::TextDisabledWrapped(*note);
    } else if (sam_results() > 0) {
        char score[16];
        std::snprintf(score, sizeof score, "%.2f", sam_last_score());
        ui::Text(msg::sam_result, {(long long)sam_last_area(), sam_last_detections(),
                                   std::string(score), one_decimal(sam_last_ms())});
    }
    const float slot = 2.0f * ImGui::GetTextLineHeightWithSpacing();
    const float used = ImGui::GetCursorPosY() - y0;
    const float pad = slot - used - ImGui::GetStyle().ItemSpacing.y;
    if (pad > 0.0f) ImGui::Dummy(ImVec2(0.0f, pad));
}

// The phrase field on one row with Find and the palette's button, so the strip
// never grows. SAM 2 has no text tower: a phrase is still typed and submitted,
// and the refusal says to switch to SAM 3.
void MaskSession::draw_sam_text() {
    MaskSettings& p = sam_prompt();
    const bool no_text = sam_has_model() && !sam_text_supported();
    ImGui::BeginDisabled(!sam_has_model());
    ImGui::SetNextItemWidth(px(420.0f));
    if (ui::InputTextEnglish(msg::sam_text_label, "person; monopod", &p.prompt,
                             ImGuiInputTextFlags_EnterReturnsTrue))
        sam_submit_text();
    ImGui::EndDisabled();
    if (!sam_has_model()) ui::help_on_hover_disabled(dmsg::mask_model_first);
    ImGui::SameLine();
    const std::string why = sam_text_refused();
    ImGui::BeginDisabled(!why.empty() && !no_text);
    if (ui::Button(msg::sam_text_find)) sam_submit_text();
    ImGui::EndDisabled();
    ui::help_on_hover_raw(why.c_str(), ImGuiHoveredFlags_AllowWhenDisabled);
    if (!sam_has_model()) return;
    ImGui::SameLine();
    if (no_text) {
        ui::TextDisabledWrapped(msg::sam_text_unsupported);
        return;
    }
    if (ui::Button(dmsg::mask_subjects)) ImGui::OpenPopup("##samsubjects");
    // A popup, not a section: open, the chips would push the picture up.
    ImGui::SetNextWindowSizeConstraints(ImVec2(px(560.0f), 0.0f), ImVec2(px(560.0f), FLT_MAX));
    if (!ImGui::BeginPopup("##samsubjects")) return;
    if (spirula::i18n::current() != spirula::i18n::Lang::en)
        ui::TextDisabledWrapped(dmsg::mask_english_only);
    ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
    // `false`: the editor's phrase always names what to drop.
    draw_subject_palette(p.prompt, p.negative_prompt, /*keep_subject=*/false);
    ImGui::EndPopup();
}

// A frame's pump-and-upload time, credited to what it did: a prompt landing,
// a margin landing, or a margin job starting.
void MaskSession::note_sam_ui(const int before[3], double ms) {
    if (_sam_results != before[0]) _sam_ui_ms = ms;
    if (_sam_reapplies != before[1]) _sam_reapply_ms = ms;
    if (_sam_margin_starts != before[2]) _sam_margin_start_ms = ms;
}

// The dataset screen's object list over the editor's clicks, in a fixed-height
// box (two objects, then it scrolls, to the end on a new one) so the picture
// never moves. Disabled with no checkpoint: the line above says why.
void MaskSession::draw_sam_objects() {
    if (_idx < 0 || _idx >= frame_count()) return;
    const float h = ImGui::GetTextLineHeightWithSpacing() + 3.0f * ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginDisabled(!sam_has_model());
    if (ImGui::BeginChild("##samobjects", ImVec2(0.0f, h))) {
        MaskSettings& p = sam_prompt();
        bool edited = false;
        draw_mask_objects(p, (long long)_idx, _frames[(size_t)_idx].camera, std::string(), edited);
        if (edited) sam_objects_edited();
        // Twice: the first frame clamps to the content size before the new row.
        if (p.object_count != _sam_objects_drawn) _sam_scroll_frames = 2;
        if (_sam_scroll_frames > 0 && _sam_scroll_frames--) ImGui::SetScrollHereY(1.0f);
        _sam_objects_drawn = p.object_count;
    }
    ImGui::EndChild();
    ImGui::EndDisabled();
}

}  // namespace mask
}  // namespace gui
