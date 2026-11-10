#pragma once

// The dataset screen's fixed-areas table: a row per input, or per camera of an
// input with several, each a lens border and drawn areas. Each part of a row
// either follows its `above` row, reading "same as above" and changing with it,
// or is its own (PrepInput::stencil_follow). Rows edit the stencils in place --
// an input's, or a laser scan's photographs' -- and the run reads those.

#include "app/FrameMask.h"
#include "app/gui/DatasetPrep.h"

#include <map>
#include <string>
#include <vector>

namespace gui {

// What one row's `input` names: a stencil and its rows' "same as above".
struct StencilOwner {
    app::FrameStencil* stencil = nullptr;
    std::map<std::string, StencilFollow>* follow = nullptr;
};
using StencilOwners = std::vector<StencilOwner>;
StencilOwners stencil_owners(std::vector<PrepInput>& in);

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
// A row seen for the first time follows each part matching its row above (the
// areas only if the fit is alike); one following a part it no longer matches
// (drawn on in the panel, input removed) keeps it as its own. Call before use.
void settle_follow(const StencilOwners& in, const std::vector<StencilRow>& rows);
bool row_follows(const StencilOwners& in, const std::vector<StencilRow>& rows,
                 size_t r, StencilPart p);

// Each makes the part row `r`'s own, even when it equals the row above, and
// sets it on every row following it, through any number of rows.
void set_row_border(const StencilOwners& in, const std::vector<StencilRow>& rows, size_t r,
                    bool on, float shrink);
// Each row gets the set's list for its camera; a lens a per-camera set does
// not name gets the first camera's. The border is left as it is.
void set_row_areas(const StencilOwners& in, const std::vector<StencilRow>& rows, size_t r,
                   const app::MaskSet& set);
// "(same as above)": takes the row above's part and follows it from now on.
void follow_above(const StencilOwners& in, const std::vector<StencilRow>& rows, size_t r,
                  StencilPart p);

// After the Try the mask panel edited input `edited`, from `before` (its
// row_stencils): each changed row of it carries the change to the rows of
// other inputs that followed it. Its own rows stay as the panel left them.
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
inline void settle_follow(std::vector<PrepInput>& in, const std::vector<StencilRow>& rows) {
    settle_follow(stencil_owners(in), rows);
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
                        size_t r, StencilPart p) {
    return row_follows(stencil_owners(const_cast<std::vector<PrepInput>&>(in)), rows, r, p);
}
inline void set_row_border(std::vector<PrepInput>& in, const std::vector<StencilRow>& rows,
                           size_t r, bool on, float shrink) {
    set_row_border(stencil_owners(in), rows, r, on, shrink);
}
inline void set_row_areas(std::vector<PrepInput>& in, const std::vector<StencilRow>& rows,
                          size_t r, const app::MaskSet& set) {
    set_row_areas(stencil_owners(in), rows, r, set);
}
inline void follow_above(std::vector<PrepInput>& in, const std::vector<StencilRow>& rows,
                         size_t r, StencilPart p) {
    follow_above(stencil_owners(in), rows, r, p);
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
