#pragma once

// PickSheetView -- a PickSheet on screen: one row as squares or as a contact
// sheet of thumbnails, and one frame at full size once it is clicked, which is
// what "is this really blurry?" needs.
//
// Thumbnail textures are decoded from the sheet's JPEGs as cells come into
// view, a few a frame, and the least recently drawn go past kPickTextures. The
// full frame is read from its file on a thread of its own.

#include "app/gui/GlLoader.h"
#include "app/gui/PickSheet.h"

#include "imgui.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace gui {

inline constexpr size_t kPickTextures = 320;

// The scale a pick is coloured on by how blurred it is: `good` at none,
// `mid` at `ref` pixels (the threshold in force) and `bad` from twice that.
struct PickStyle {
    ImVec4 bad, mid, good;
    float ref = 8.0f;
};

class PickSheetView {
public:
    ~PickSheetView();

    // Where a blur (FramePick::blur) falls on the scale; grey unmeasured.
    static ImU32 colour(const PickStyle& style, float blur);

    // `picks` is the sheet's row with the verdicts to SHOW, which a setting
    // moved after the run may have changed. `cell` is a thumbnail's width in
    // pixels, 0 for squares. A click opens that frame.
    void draw(const PickSheet& sheet, size_t row, const std::vector<Pick>& picks,
              float width, float cell, const PickStyle& style);

    // The frame a click opened, in `size`; `picks` is its row, as for draw().
    bool viewing() const { return _open >= 0; }
    size_t open_row() const { return _open_row; }
    void draw_viewer(const PickSheet& sheet, const std::vector<Pick>& picks, ImVec2 size,
                     const PickStyle& style);
    void close();

    void destroy_gl();

private:
    struct Tex {
        GLuint id = 0;       // 0: the pick came without pixels
        int w = 0, h = 0;
        uint64_t used = 0;
        int frame = -1;      // last drawn in; the draw list still names it
    };
    // Null while it is still to be decoded. `now` skips this frame's budget.
    const Tex* texture(const PickSheet& sheet, size_t row, size_t index, bool now);
    void open(const PickSheet& sheet, size_t row, int index);

    std::map<uint64_t, Tex> _tex;        // row << 32 | index
    std::map<size_t, float> _aspect;     // height over width, per row
    uint64_t _gen = 0, _clock = 0;
    int _frame = -1, _budget = 0;

    // ---- the open frame ----
    struct Full {
        std::string path;
        std::vector<uint8_t> rgb;
        int w = 0, h = 0;
        bool done = false;
    };
    size_t _open_row = 0;
    int _open = -1;
    std::string _want;           // the file the loader was sent for
    std::thread _loader;
    std::mutex _mu;              // guards _full
    Full _full;
    GLuint _full_tex = 0;
    int _full_w = 0, _full_h = 0;
    std::string _full_path;      // the file _full_tex holds
    // Magnification over "fits the pane", and the point of the picture at the
    // pane's centre as a fraction of it: neither moves when the full frame
    // replaces the thumbnail it was standing in for.
    float _mag = 1.0f;
    ImVec2 _centre = ImVec2(0.5f, 0.5f);
};

}  // namespace gui
