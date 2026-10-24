#pragma once

// Interactive match matrix viewer and pair editor.
// Allows zooming, panning, selecting regions, inspecting pairs with feature/match
// overlays, and adding or removing matches directly from matches.bin.

#include "app/gui/MatchMatrix.h"
#include "app/gui/PairPreview.h"
#include "app/gui/SfmProgress.h"

#ifdef SS_TOOL_SFM
#include "sfm/core/Matches.h"
#endif

#include "imgui.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace gui {

struct MatrixSelection {
    bool active = false;
    uint32_t r0 = 0, r1 = 0;
    uint32_t c0 = 0, c1 = 0;
    bool symmetric = true;

    bool contains(uint32_t r, uint32_t c) const {
        if (!active) return false;
        bool in = (r >= r0 && r < r1 && c >= c0 && c < c1);
        if (symmetric) in = in || (c >= r0 && c < r1 && r >= c0 && r < c1);
        return in;
    }

    bool is_single() const {
        return active && (r1 == r0 + 1) && (c1 == c0 + 1);
    }

    void clear() {
        active = false;
        r0 = r1 = c0 = c1 = 0;
    }
};

class MatchMatrixEditor {
public:
    MatchMatrixEditor();
    ~MatchMatrixEditor();

    void configure(const std::string& image_dir, const std::string& mask_dir,
                   const std::string& features_dir, const std::string& matches_path);

    void open();
    bool is_open() const { return _open; }
    void close() { _open = false; }

    bool draw(PairMatrix& pm, MatchMatrix& mm, PairPreview& preview);

    bool remove_selected_matches(PairMatrix& pm, MatchMatrix& mm, PairPreview& preview);
    bool undo_removal(PairMatrix& pm, MatchMatrix& mm, PairPreview& preview);
    bool match_single_pair(uint32_t a, uint32_t b, PairMatrix& pm, MatchMatrix& mm, PairPreview& preview);

    const MatrixSelection& selection() const { return _selection; }
    void set_selection(const MatrixSelection& sel) { _selection = sel; }
    bool can_undo() const {
#ifdef SS_TOOL_SFM
        return !_undo_stack.empty();
#else
        return false;
#endif
    }

private:
#ifdef SS_TOOL_SFM
    struct UndoStep {
        std::vector<sfm::TwoViewMatches> removed;
    };
    std::vector<UndoStep> _undo_stack;
#endif

    bool _open = false;
    float _zoom = 1.0f;
    ImVec2 _pan = {0.0f, 0.0f};

    MatrixSelection _selection;
    bool _dragging = false;
    uint32_t _drag_r0 = 0, _drag_c0 = 0;
    uint32_t _drag_r1 = 0, _drag_c1 = 0;

    bool _panning = false;
    ImVec2 _pan_start_mouse = {0.0f, 0.0f};
    ImVec2 _pan_start_pan = {0.0f, 0.0f};

    std::string _image_dir, _mask_dir, _features_dir, _matches_path;
    std::vector<std::string> _stems;

    std::vector<std::pair<uint32_t, uint32_t>> _selected_matched_pairs;
    int _selected_pair_idx = 0;

    std::string _status;

    void update_selected_pairs(const PairMatrix& pm, PairPreview& preview);
    uint64_t count_selected_matches(const PairMatrix& pm) const;
    uint32_t count_selected_pairs(const PairMatrix& pm) const;
};

}  // namespace gui

