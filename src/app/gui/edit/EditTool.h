#pragma once

// The active tool: a modal state machine over viewport input.
//
// Camera navigation is itself a tool, which is what keeps "does this drag
// orbit or lasso?" from being a question each feature answers for itself.
// With a tool active it owns the left button for its whole lifetime and the
// other two stay with navigation.
//
// What a tool produces is a ShapeStroke and nothing else: the stencil, the
// document and the selection are none of its business.

#include "app/gui/edit/SelectShape.h"
#include "app/gui/ViewportInput.h"

#include <cstdint>
#include <vector>

struct ImVec2;

namespace spirula { namespace i18n { struct Msg; } }

namespace gui {

// The first kNumSelectTools are what the Select tab lays out; Transform and
// Eyedropper are reached from their own parts of the panel.
enum class ToolId {
    Navigate = 0, Box, Ellipse, Lasso, Polygon, Brush, Piece,
    Transform, Eyedropper
};
inline constexpr int kNumSelectTools = 7;
inline constexpr int kNumTools = 9;

class EditTool {
public:
    ToolId id() const { return _id; }
    void set_id(ToolId t);
    // The left button: Transform leaves it to the camera except over a
    // handle, which is the session's call and not a property of the tool.
    bool owns_pointer() const {
        return _id != ToolId::Navigate && _id != ToolId::Transform;
    }
    // The letter keys, which Transform does take: S has to mean scale.
    bool owns_keys() const { return _id != ToolId::Navigate; }
    bool in_progress() const { return _active; }

    float brush_radius() const { return _brush; }
    void set_brush_radius(float r) { _brush = r; }

    // Feeds one frame. When a stroke completes, `out` is filled and true
    // comes back; `consumed` says the left button belonged to the tool.
    bool update(const ViewportInput& in, ShapeStroke& out, bool& consumed);
    // A polygon is closed by Enter or a right click; Esc drops the stroke.
    bool commit_pending(ShapeStroke& out);
    void cancel();
    // Take the last corner back, which is what Ctrl+Z means while a polygon
    // is still being drawn.
    bool pop_point();

    void draw_overlay(ImDrawList* dl, const ImVec2& origin) const;

    // The strip under the viewport: what this tool is, and its two or three
    // keys. A modal grammar is invisible without it.
    const spirula::i18n::Msg& label() const;
    const spirula::i18n::Msg& hint() const;

private:
    ToolId _id = ToolId::Navigate;
    bool _active = false;
    float _brush = 24.0f;
    std::vector<float> _pts;   // x,y pairs
    float _cur[2] = {0, 0};
};

// One row per tool, in the order the panel lays them out, and the only place
// the key mapping is written. A key is an identifier, not interface copy: B
// is B in every language, exactly as --sh-degree is.
struct ToolRow {
    ToolId id;
    const char* key;        // what the button prints in its corner
    int imgui_key;          // ImGuiKey, as an int so this header needs no imgui
};
const ToolRow* tool_table();
const spirula::i18n::Msg& tool_label(ToolId t);
const spirula::i18n::Msg& tool_hint(ToolId t);
ShapeKind shape_of(ToolId t);

}  // namespace gui
