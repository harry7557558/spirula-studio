#pragma once

// The dataset screen's fixed-areas table: a row per input, or per camera of an
// input with several, each a lens border and drawn areas. Each part of a row
// either follows its `above` row, reading "same as above" and changing with it,
// or is its own (PrepInput::stencil_follow). Rows edit PrepInput::stencil in
// place; the run reads that.

#include "app/FrameMask.h"
#include "app/gui/DatasetPrep.h"

#include <string>
#include <vector>

namespace gui {

struct StencilRow {
    size_t input = 0;
    std::string camera;   // FrameStencil::cameras key; "" = the whole input
    int above = -1;       // the row this one follows; -1 for none
};

// keys[i] is input i's cameras (stencil_cameras). An input's first row follows
// the same camera of the input directly above, else the row before it; its
// other rows follow the row before them, so one camera's lenses share.
std::vector<StencilRow> stencil_rows(const std::vector<std::vector<std::string>>& keys);

enum class StencilPart { Border, Areas };

const app::CameraStencil& row_stencil(const std::vector<PrepInput>& in, const StencilRow& r);
std::vector<app::CameraStencil> row_stencils(const std::vector<PrepInput>& in,
                                             const std::vector<StencilRow>& rows);
// A border that is off matches any other that is off, whatever its shrink.
bool same_part(const app::CameraStencil& a, const app::CameraStencil& b, StencilPart p);
// A row seen for the first time follows each part matching its row above (the
// areas only if the fit is alike); one following a part it no longer matches
// (drawn on in the panel, input removed) keeps it as its own. Call before use.
void settle_follow(std::vector<PrepInput>& in, const std::vector<StencilRow>& rows);
bool row_follows(const std::vector<PrepInput>& in, const std::vector<StencilRow>& rows,
                 size_t r, StencilPart p);

// Each makes the part row `r`'s own, even when it equals the row above, and
// sets it on every row following it, through any number of rows.
void set_row_border(std::vector<PrepInput>& in, const std::vector<StencilRow>& rows, size_t r,
                    bool on, float shrink);
// Each row gets the set's list for its camera; a lens a per-camera set does
// not name gets the first camera's. The border is left as it is.
void set_row_areas(std::vector<PrepInput>& in, const std::vector<StencilRow>& rows, size_t r,
                   const app::MaskSet& set);
// "(same as above)": takes the row above's part and follows it from now on.
void follow_above(std::vector<PrepInput>& in, const std::vector<StencilRow>& rows, size_t r,
                  StencilPart p);

// After the Try the mask panel edited input `edited`, from `before` (its
// row_stencils): each changed row of it carries the change to the rows of
// other inputs that followed it. Its own rows stay as the panel left them.
void follow_edit(std::vector<PrepInput>& in, const std::vector<StencilRow>& rows,
                 const std::vector<app::CameraStencil>& before, size_t edited);

// Rows of inputs from `first` on, as they arrive: each takes the drawn areas
// and the shrink of the row it follows, when it has none drawn and both
// inputs agree in `fisheye`.
void adopt_rows_above(std::vector<PrepInput>& in, const std::vector<StencilRow>& rows,
                      size_t first, const std::vector<bool>& fisheye);

// The first of `sets` whose list for the row's camera is the row's drawn
// areas, or -1. Nothing drawn matches none.
int row_set(const std::vector<app::MaskSet>& sets, const StencilRow& row,
            const app::CameraStencil& cs);
// The set every row draws -- the row's camera's list, empty ones included --
// when at least one row draws something; else -1.
int common_set(const std::vector<app::MaskSet>& sets, const std::vector<PrepInput>& in,
               const std::vector<StencilRow>& rows);

}  // namespace gui
