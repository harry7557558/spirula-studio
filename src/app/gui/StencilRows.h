#pragma once

// The dataset screen's fixed-areas table: a row per input, or per camera of an
// input with several, each a lens border and drawn areas. A row's drawn areas
// either follow its `above` row, reading "same as above" and changing with it,
// or are its own; its border is its own fit, off, or one circle shared with
// other rows (PrepInput::stencil_follow). Rows edit the stencils in place --
// an input's, or a laser scan's photographs' -- and the run reads those.

#include "app/FrameMask.h"
#include "app/gui/DatasetPrep.h"

#include <map>
#include <string>
#include <vector>

namespace gui {

// What one row's `input` names: a stencil and its rows' follow and share state.
struct StencilOwner {
    app::FrameStencil* stencil = nullptr;
    std::map<std::string, StencilFollow>* follow = nullptr;
    std::string setup;   // lens_setup
};
using StencilOwners = std::vector<StencilOwner>;
// `setups`: each input's lens_setup; none when left out.
StencilOwners stencil_owners(std::vector<PrepInput>& in,
                             const std::vector<std::string>& setups = {});

struct StencilRow {
    size_t input = 0;     // index into the owners
    std::string camera;   // FrameStencil::cameras key; "" = the whole input
    int above = -1;       // the row this one follows; -1 for none
};

// keys[i] is input i's cameras (stencil_cameras). An input's first row follows
// the same camera of the input directly above, else the row before it; its
// other rows follow the row before them, so one camera's lenses share.
std::vector<StencilRow> stencil_rows(const std::vector<std::vector<std::string>>& keys);
// Rows for the owners from `first_owner` on, keys as above, appended as a chain
// of their own whose first row follows nothing: a laser scan's photographs.
void append_rows(std::vector<StencilRow>& rows,
                 const std::vector<std::vector<std::string>>& keys, size_t first_owner);

enum class StencilPart { Border, Areas };

const app::CameraStencil& row_stencil(const StencilOwners& in, const StencilRow& r);
std::vector<app::CameraStencil> row_stencils(const StencilOwners& in,
                                             const std::vector<StencilRow>& rows);
// A border that is off matches any other that is off, whatever its shrink.
bool same_part(const app::CameraStencil& a, const app::CameraStencil& b, StencilPart p);
// New rows follow matching areas above (if the fit is alike), and a dual lens
// that cuts shares the same lens's circle above, of its setup. What stops
// matching or cutting becomes the row's own. Call before use.
void settle_follow(const StencilOwners& in, const std::vector<StencilRow>& rows);
bool row_follows(const StencilOwners& in, const std::vector<StencilRow>& rows, size_t r);

// The row's own border fit, on or off; it leaves any circle it shared.
void set_row_border(const StencilOwners& in, const std::vector<StencilRow>& rows, size_t r,
                    bool on, float shrink);
// The shrink of the row and of every row sharing its circle.
void set_row_shrink(const StencilOwners& in, const std::vector<StencilRow>& rows, size_t r,
                    float shrink);
// Row `r` joins the circle of row `with`, which must cut, and takes its shrink.
void share_border(const StencilOwners& in, const std::vector<StencilRow>& rows, size_t r,
                  size_t with);
// The rows sharing row `r`'s circle, `r` included; empty for a fit of its own.
std::vector<size_t> border_sharers(const StencilOwners& in, const std::vector<StencilRow>& rows,
                                   size_t r);

// Each row gets the set's list for its camera; a lens a per-camera set does
// not name gets the first camera's. Rows following it get it too.
void set_row_areas(const StencilOwners& in, const std::vector<StencilRow>& rows, size_t r,
                   const app::MaskSet& set);
// "(same as above)": takes the row above's areas and follows them from now on.
void follow_above(const StencilOwners& in, const std::vector<StencilRow>& rows, size_t r);

// After Try the mask edited input `edited` from `before` (row_stencils), its
// changed areas reach other inputs' rows that follow them, and a shrink or an
// edited border ellipse the rows sharing its circle. Its own rows stay put.
void follow_edit(const StencilOwners& in, const std::vector<StencilRow>& rows,
                 const std::vector<app::CameraStencil>& before, size_t edited);

// Rows of inputs from `first` on, as they arrive: each takes the drawn areas
// and the shrink of the row it follows, when it has none drawn and both
// inputs agree in `fisheye`.
void adopt_rows_above(const StencilOwners& in, const std::vector<StencilRow>& rows,
                      size_t first, const std::vector<bool>& fisheye);

// The first of `sets` whose list for the row's camera is the row's drawn
// areas, or -1. Nothing drawn matches none.
int row_set(const std::vector<app::MaskSet>& sets, const StencilRow& row,
            const app::CameraStencil& cs);
// The set every row draws -- the row's camera's list, empty ones included --
// when at least one row draws something; else -1.
int common_set(const std::vector<app::MaskSet>& sets, const StencilOwners& in,
               const std::vector<StencilRow>& rows);

// The same over a list of inputs, which owns what it edits.
inline void settle_follow(std::vector<PrepInput>& in, const std::vector<StencilRow>& rows,
                          const std::vector<std::string>& setups = {}) {
    settle_follow(stencil_owners(in, setups), rows);
}
inline const app::CameraStencil& row_stencil(const std::vector<PrepInput>& in,
                                             const StencilRow& r) {
    return in[r.input].stencil.for_camera(r.camera);
}
inline std::vector<app::CameraStencil> row_stencils(const std::vector<PrepInput>& in,
                                                    const std::vector<StencilRow>& rows) {
    return row_stencils(stencil_owners(const_cast<std::vector<PrepInput>&>(in)), rows);
}
inline bool row_follows(const std::vector<PrepInput>& in, const std::vector<StencilRow>& rows,
                        size_t r) {
    return row_follows(stencil_owners(const_cast<std::vector<PrepInput>&>(in)), rows, r);
}
inline void set_row_border(std::vector<PrepInput>& in, const std::vector<StencilRow>& rows,
                           size_t r, bool on, float shrink) {
    set_row_border(stencil_owners(in), rows, r, on, shrink);
}
inline void set_row_shrink(std::vector<PrepInput>& in, const std::vector<StencilRow>& rows,
                           size_t r, float shrink) {
    set_row_shrink(stencil_owners(in), rows, r, shrink);
}
inline void share_border(std::vector<PrepInput>& in, const std::vector<StencilRow>& rows,
                         size_t r, size_t with) {
    share_border(stencil_owners(in), rows, r, with);
}
inline std::vector<size_t> border_sharers(const std::vector<PrepInput>& in,
                                          const std::vector<StencilRow>& rows, size_t r) {
    return border_sharers(stencil_owners(const_cast<std::vector<PrepInput>&>(in)), rows, r);
}
inline void set_row_areas(std::vector<PrepInput>& in, const std::vector<StencilRow>& rows,
                          size_t r, const app::MaskSet& set) {
    set_row_areas(stencil_owners(in), rows, r, set);
}
inline void follow_above(std::vector<PrepInput>& in, const std::vector<StencilRow>& rows,
                         size_t r) {
    follow_above(stencil_owners(in), rows, r);
}
inline void follow_edit(std::vector<PrepInput>& in, const std::vector<StencilRow>& rows,
                        const std::vector<app::CameraStencil>& before, size_t edited) {
    follow_edit(stencil_owners(in), rows, before, edited);
}
inline void adopt_rows_above(std::vector<PrepInput>& in, const std::vector<StencilRow>& rows,
                             size_t first, const std::vector<bool>& fisheye) {
    adopt_rows_above(stencil_owners(in), rows, first, fisheye);
}
inline int common_set(const std::vector<app::MaskSet>& sets, const std::vector<PrepInput>& in,
                      const std::vector<StencilRow>& rows) {
    return common_set(sets, stencil_owners(const_cast<std::vector<PrepInput>&>(in)), rows);
}

}  // namespace gui
