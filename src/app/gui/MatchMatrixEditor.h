#pragma once

// The match map at full size, for taking false pairs out of a finished run's
// matches.bin by hand (a loop closure between two sides of a symmetric object).

#include "app/gui/MatchMatrix.h"
#include "app/gui/PairPreview.h"
#include "app/gui/SfmProgress.h"

#include "imgui.h"

#include <cstdint>
#include <string>
#include <vector>

namespace gui {

// Finer than the dataset screen's map, which keeps the live matrix's 512: one
// image per cell to 2048 images, a 12 MB texture held only while open.
inline constexpr uint32_t kEditorMatrixBins = 2048;

// Rows [r0, r1) against columns [c0, c1), in cells. A pair is unordered, so it
// is in the selection when either of its two orders is in the box.
struct MatrixSelection {
    bool active = false;
    uint32_t r0 = 0, r1 = 0, c0 = 0, c1 = 0;
    bool contains(uint32_t r, uint32_t c) const {
        if (!active) return false;
        return (r >= r0 && r < r1 && c >= c0 && c < c1) ||
               (c >= r0 && c < r1 && r >= c0 && r < c1);
    }
    bool single() const { return active && r1 == r0 + 1 && c1 == c0 + 1; }
};

class MatchMatrixEditor {
public:
    MatchMatrixEditor();
    ~MatchMatrixEditor();

    // The run's files. Another matches.bin forgets the undo history.
    void configure(const std::string& image_dir, const std::string& mask_dir,
                   const std::string& features_dir, const std::string& matches_path);
    void open();
    bool is_open() const { return _open || _want_open; }
    // True on the frame matches.bin was rewritten. Nothing is written while
    // `busy`: a run may be reading the file.
    bool draw(bool busy);

    // What draw() does on a click; public for the test. Without a GL context.
    void select(const MatrixSelection& sel);
    bool remove_selected();
    bool undo();
    bool can_undo() const;
    bool restore_backup();
    bool can_restore_backup() const;
    const PairMatrix& matrix() const { return _mm.matrix(); }
    const std::string& status() const { return _status; }

private:
    struct PairRef {
        uint32_t a = 0, b = 0, count = 0;
    };
    struct UndoStep;

    void reload();
    // False, with the undo history dropped, when something else rewrote the file.
    bool file_is_ours();
    void show_selected();
    void close();

    std::string _image_dir, _mask_dir, _features_dir, _matches_path;
    std::vector<std::string> _names;   // image index -> name, from matches.bin
    std::vector<PairRef> _pairs;
    int64_t _stamp = 0;                // matches.bin's mtime as last read
    MatchMatrix _mm;
    PairPreview _preview;
    std::vector<UndoStep> _undo;

    bool _open = false, _want_open = false;
    float _zoom = 1.0f;
    ImVec2 _pan = {0.0f, 0.0f};
    bool _dragging = false, _panning = false;
    uint32_t _drag_r0 = 0, _drag_c0 = 0, _drag_r1 = 0, _drag_c1 = 0;
    ImVec2 _pan_start_mouse = {0.0f, 0.0f}, _pan_start = {0.0f, 0.0f};

    MatrixSelection _sel;
    std::vector<PairRef> _sel_pairs;   // oriented row image first
    uint64_t _sel_matches = 0;
    size_t _sel_idx = 0;
    std::string _status;
};

}  // namespace gui
