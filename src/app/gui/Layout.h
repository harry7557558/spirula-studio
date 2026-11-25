#pragma once

// How big the interface is, the drag handles that let the user say otherwise,
// and the widths and status colours every screen shares.
//
// Everything the screens lay out against is stored UNSCALED and multiplied by
// ui_scale() at the point of use, so a saved layout survives a change of
// monitor, of DPI, or of the user's size preference.

#include "imgui.h"

#include <string>

namespace gui {

// Rebuilds ImGuiStyle from defaults at `scale`. Not incremental:
// ImGuiStyle::ScaleAllSizes() multiplies in place, so applying it to a style
// that already carries a scale compounds.
void apply_style(float scale);

// The scale the style currently carries. FontScaleMain is where apply_style
// puts it, so this cannot drift from what is on screen.
inline float ui_scale() { return ImGui::GetStyle().FontScaleMain; }

// A hand-written pixel constant, at the current scale.
inline float px(float unscaled) { return unscaled * ui_scale(); }

// Unscaled widths for SetNextItemWidth(px(...)). Fields in a settings form:
inline constexpr float kFieldW = 220.0f;
inline constexpr float kTextFieldW = 320.0f;    // free text: prompts, names
inline constexpr float kPathFieldW = 300.0f;
inline constexpr float kCompactFieldW = 120.0f; // toolbar sliders and combos

// Wrap widths for PushTextWrapPos(px(...)), and button sizes: a modal's, a
// screen's main action, and the "..." that opens a picker beside a path.
inline constexpr float kTooltipWrapW = 420.0f;
inline constexpr float kModalWrapW = 460.0f;
inline constexpr float kModalButtonW = 170.0f;
inline constexpr float kActionButtonW = 200.0f;
inline constexpr float kActionButtonH = 34.0f;
inline constexpr float kBrowseButtonW = 60.0f;

// Status colours for text and marks.
inline constexpr ImVec4 kOk(0.35f, 0.85f, 0.45f, 1.0f);
inline constexpr ImVec4 kWarn(0.95f, 0.75f, 0.30f, 1.0f);
inline constexpr ImVec4 kErr(1.0f, 0.42f, 0.42f, 1.0f);
inline constexpr ImVec4 kDim(0.6f, 0.6f, 0.6f, 1.0f);

// The DPI scale and the user's size preference, resolved into one number once
// per frame.
class UiScale {
public:
    void set_dpi(float s) { _dpi = s > 0.0f ? s : 1.0f; }

    // 0 = follow the window size, which is the default; otherwise a fixed
    // multiplier on the DPI scale.
    void set_user(float f) { _user = f; }
    float user() const { return _user; }

    // Call before the first widget of the frame: a mid-frame style swap would
    // measure half the window against one scale and half against the other.
    void update(ImVec2 display_size);

private:
    float _dpi = 1.0f;
    float _user = 0.0f;
    float _applied = 0.0f;
};

// The gap a splitter occupies between two panes, including the spacing ImGui
// puts after it.
float splitter_extent();

// Drag handles. `*extent` is the pane size being edited in PIXELS (callers
// hold unscaled units and convert), clamped to [lo, hi]; `span` is the length
// of the handle across the other axis. True when it moved this frame.
bool splitter_v(const char* id, float* extent, float lo, float hi, float span);
bool splitter_h(const char* id, float* extent, float lo, float hi, float span);

// `s`, cut down the middle with an ellipsis until it fits `max_w` pixels.
// Cuts on UTF-8 boundaries. The middle is what goes: for a path, the two ends
// are the parts that identify it.
std::string elide_middle(const std::string& s, float max_w);

}  // namespace gui
