#include "app/gui/StencilRows.h"

#include <functional>
#include <iterator>
#include <map>

namespace gui {

namespace {

bool same_shape(const app::MaskShape& a, const app::MaskShape& b) {
    return a.kind == b.kind && a.remove == b.remove && a.cx == b.cx && a.cy == b.cy &&
           a.rx == b.rx && a.ry == b.ry && a.pts == b.pts;
}

bool same_shapes(const std::vector<app::MaskShape>& a, const std::vector<app::MaskShape>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (!same_shape(a[i], b[i])) return false;
    return true;
}

// What app::apply_mask_set puts on `camera`.
const std::vector<app::MaskShape>& list_for(const app::MaskSet& set, const std::string& camera) {
    const auto it = set.cameras.find(camera);
    return it == set.cameras.end() ? set.shapes : it->second;
}

// What a row of the table gets: a lens a per-camera set does not name takes
// its first camera's list, since the lenses of one camera usually share theirs.
const std::vector<app::MaskShape>& row_list(const app::MaskSet& set, const std::string& camera) {
    if (set.cameras.count(camera) || !set.shapes.empty() || set.cameras.empty())
        return list_for(set, camera);
    return set.cameras.begin()->second;
}

// Whether a row seen for the first time follows part `p` of `above`. Not the
// areas of a row whose fit differs: that is another kind of lens, whose drawn
// areas mean nothing on this one (a GoPro sphere under a dual fisheye).
bool starts_following(const app::CameraStencil& row, const app::CameraStencil& above,
                      StencilPart p) {
    return same_part(row, above, p) &&
           (p == StencilPart::Border || row.detect_border == above.detect_border);
}

// Writes `want` back to where each row of a touched input reads it from. An
// input whose rows all agree goes back to one stencil, unless its stencil
// names a camera no row covers, which keeps its own.
void store(std::vector<PrepInput>& in, const std::vector<StencilRow>& rows,
           const std::vector<app::CameraStencil>& want, const std::vector<bool>& touched) {
    for (size_t i = 0; i < in.size(); i++) {
        if (!touched[i]) continue;
        std::vector<size_t> mine;
        for (size_t r = 0; r < rows.size(); r++)
            if (rows[r].input == i) mine.push_back(r);
        if (mine.empty()) continue;
        app::FrameStencil& st = in[i].stencil;
        bool same = true;
        for (size_t r : mine)
            same = same && same_part(want[r], want[mine[0]], StencilPart::Border) &&
                   same_part(want[r], want[mine[0]], StencilPart::Areas);
        bool uncovered = false;
        for (const auto& [camera, cs] : st.cameras) {
            bool listed = false;
            for (size_t r : mine) listed = listed || rows[r].camera == camera;
            uncovered = uncovered || !listed;
        }
        if (same && !uncovered) {
            st.cameras.clear();
            static_cast<app::CameraStencil&>(st) = want[mine[0]];
            continue;
        }
        for (size_t r : mine) {
            if (rows[r].camera.empty()) static_cast<app::CameraStencil&>(st) = want[r];
            else st.cameras[rows[r].camera] = want[r];
        }
    }
}

bool& flag(StencilFollow& f, StencilPart p) {
    return p == StencilPart::Border ? f.border : f.areas;
}

// The row's choice, or before settle_follow has given it one, whether it
// matches the row above in `vals`.
bool follows_in(const std::vector<PrepInput>& in, const std::vector<StencilRow>& rows,
                const std::vector<app::CameraStencil>& vals, size_t j, StencilPart p) {
    const int a = rows[j].above;
    if (a < 0) return false;
    const auto& choices = in[rows[j].input].stencil_follow;
    const auto it = choices.find(rows[j].camera);
    if (it == choices.end()) return starts_following(vals[j], vals[(size_t)a], p);
    return p == StencilPart::Border ? it->second.border : it->second.areas;
}

// from[j] >= 0 marks a changed row: the row whose new value it takes. A row
// joins the one it follows, as `before` found them. Rows in `fixed` are
// neither joined nor written, so nothing passes through one but a seed.
using Assign = std::function<void(size_t row, size_t from, app::CameraStencil& cs)>;
void spread(std::vector<PrepInput>& in, const std::vector<StencilRow>& rows,
            const std::vector<app::CameraStencil>& before, StencilPart p, std::vector<int> from,
            const std::vector<bool>& fixed, const Assign& assign) {
    for (size_t j = 0; j < rows.size(); j++) {
        const int a = rows[j].above;
        if (from[j] >= 0 || fixed[j] || a < 0 || from[(size_t)a] < 0) continue;
        if (follows_in(in, rows, before, j, p)) from[j] = from[(size_t)a];
    }
    std::vector<app::CameraStencil> want = row_stencils(in, rows);
    std::vector<bool> touched(in.size(), false);
    for (size_t j = 0; j < rows.size(); j++) {
        if (from[j] < 0 || fixed[j]) continue;
        assign(j, (size_t)from[j], want[j]);
        touched[rows[j].input] = true;
    }
    store(in, rows, want, touched);
}

void set_border(std::vector<PrepInput>& in, const std::vector<StencilRow>& rows, size_t r,
                bool on, float shrink, bool follow) {
    settle_follow(in, rows);
    std::vector<int> from(rows.size(), -1);
    from[r] = (int)r;
    spread(in, rows, row_stencils(in, rows), StencilPart::Border, from,
           std::vector<bool>(rows.size(), false),
           [&](size_t, size_t, app::CameraStencil& cs) {
               cs.detect_border = on;
               // Kept while off, so turning it back on finds it.
               if (on) cs.shrink = shrink;
           });
    in[rows[r].input].stencil_follow[rows[r].camera].border = follow;
    settle_follow(in, rows);
}

void set_areas(std::vector<PrepInput>& in, const std::vector<StencilRow>& rows, size_t r,
               const app::MaskSet& set, bool follow) {
    settle_follow(in, rows);
    std::vector<int> from(rows.size(), -1);
    from[r] = (int)r;
    spread(in, rows, row_stencils(in, rows), StencilPart::Areas, from,
           std::vector<bool>(rows.size(), false),
           [&](size_t j, size_t, app::CameraStencil& cs) {
               cs.mask.shapes = row_list(set, rows[j].camera);
           });
    in[rows[r].input].stencil_follow[rows[r].camera].areas = follow;
    settle_follow(in, rows);
}

}  // namespace

std::vector<StencilRow> stencil_rows(const std::vector<std::vector<std::string>>& keys) {
    std::vector<StencilRow> out;
    size_t prev_first = 0, prev_end = 0;   // the rows of the input above
    for (size_t i = 0; i < keys.size(); i++) {
        const size_t first = out.size();
        for (const std::string& camera : keys[i]) {
            StencilRow r{i, camera, (int)out.size() - 1};
            for (size_t k = prev_first; out.size() == first && !camera.empty() && k < prev_end; k++)
                if (out[k].camera == camera) r.above = (int)k;
            out.push_back(r);
        }
        prev_first = first;
        prev_end = out.size();
    }
    return out;
}

const app::CameraStencil& row_stencil(const std::vector<PrepInput>& in, const StencilRow& r) {
    return in[r.input].stencil.for_camera(r.camera);
}

std::vector<app::CameraStencil> row_stencils(const std::vector<PrepInput>& in,
                                             const std::vector<StencilRow>& rows) {
    std::vector<app::CameraStencil> out;
    out.reserve(rows.size());
    for (const StencilRow& r : rows) out.push_back(row_stencil(in, r));
    return out;
}

bool same_part(const app::CameraStencil& a, const app::CameraStencil& b, StencilPart p) {
    if (p == StencilPart::Border)
        return a.detect_border == b.detect_border && (!a.detect_border || a.shrink == b.shrink);
    return same_shapes(a.mask.shapes, b.mask.shapes);
}

void settle_follow(std::vector<PrepInput>& in, const std::vector<StencilRow>& rows) {
    const std::vector<app::CameraStencil> vals = row_stencils(in, rows);
    for (size_t j = 0; j < rows.size(); j++) {
        std::map<std::string, StencilFollow>& choices = in[rows[j].input].stencil_follow;
        const int a = rows[j].above;
        const auto it = choices.find(rows[j].camera);
        for (const StencilPart p : {StencilPart::Border, StencilPart::Areas}) {
            if (it == choices.end())
                flag(choices[rows[j].camera], p) =
                    a >= 0 && starts_following(vals[j], vals[(size_t)a], p);
            else
                flag(it->second, p) =
                    flag(it->second, p) && a >= 0 && same_part(vals[j], vals[(size_t)a], p);
        }
    }
    for (size_t i = 0; i < in.size(); i++)
        for (auto it = in[i].stencil_follow.begin(); it != in[i].stencil_follow.end();) {
            bool listed = false;
            for (const StencilRow& r : rows) listed = listed || (r.input == i && r.camera == it->first);
            it = listed ? std::next(it) : in[i].stencil_follow.erase(it);
        }
}

bool row_follows(const std::vector<PrepInput>& in, const std::vector<StencilRow>& rows,
                 size_t r, StencilPart p) {
    const int a = rows[r].above;
    if (a < 0) return false;
    const auto& choices = in[rows[r].input].stencil_follow;
    const auto it = choices.find(rows[r].camera);
    if (it != choices.end()) return p == StencilPart::Border ? it->second.border : it->second.areas;
    return starts_following(row_stencil(in, rows[r]), row_stencil(in, rows[(size_t)a]), p);
}

void set_row_border(std::vector<PrepInput>& in, const std::vector<StencilRow>& rows, size_t r,
                    bool on, float shrink) {
    set_border(in, rows, r, on, shrink, /*follow=*/false);
}

void set_row_areas(std::vector<PrepInput>& in, const std::vector<StencilRow>& rows, size_t r,
                   const app::MaskSet& set) {
    set_areas(in, rows, r, set, /*follow=*/false);
}

void follow_above(std::vector<PrepInput>& in, const std::vector<StencilRow>& rows, size_t r,
                  StencilPart p) {
    const int a = rows[r].above;
    if (a < 0) return;
    const app::CameraStencil above = row_stencil(in, rows[(size_t)a]);
    if (p == StencilPart::Border) {
        set_border(in, rows, r, above.detect_border, above.shrink, /*follow=*/true);
        return;
    }
    app::MaskSet set;
    set.shapes = above.mask.shapes;
    set_areas(in, rows, r, set, /*follow=*/true);
}

void follow_edit(std::vector<PrepInput>& in, const std::vector<StencilRow>& rows,
                 const std::vector<app::CameraStencil>& before, size_t edited) {
    if (before.size() != rows.size()) return;
    const std::vector<app::CameraStencil> after = row_stencils(in, rows);
    std::vector<bool> fixed(rows.size(), false);
    for (size_t j = 0; j < rows.size(); j++) fixed[j] = rows[j].input == edited;
    for (const StencilPart p : {StencilPart::Border, StencilPart::Areas}) {
        std::vector<int> from(rows.size(), -1);
        bool any = false;
        for (size_t j = 0; j < rows.size(); j++)
            if (fixed[j] && !same_part(before[j], after[j], p)) {
                from[j] = (int)j;
                any = true;
            }
        if (!any) continue;
        spread(in, rows, before, p, from, fixed,
               [&](size_t, size_t src, app::CameraStencil& cs) {
                   if (p == StencilPart::Border) {
                       cs.detect_border = after[src].detect_border;
                       if (cs.detect_border) cs.shrink = after[src].shrink;
                   } else {
                       cs.mask.shapes = after[src].mask.shapes;
                   }
               });
    }
    settle_follow(in, rows);
}

void adopt_rows_above(std::vector<PrepInput>& in, const std::vector<StencilRow>& rows,
                      size_t first, const std::vector<bool>& fisheye) {
    std::vector<app::CameraStencil> want = row_stencils(in, rows);
    std::vector<bool> touched(in.size(), false);
    for (size_t j = 0; j < rows.size(); j++) {
        const int a = rows[j].above;
        const size_t i = rows[j].input;
        if (i < first || a < 0) continue;
        const size_t ai = rows[(size_t)a].input;
        if (i >= fisheye.size() || ai >= fisheye.size() || fisheye[i] != fisheye[ai]) continue;
        const app::CameraStencil& above = want[(size_t)a];
        if (want[j].mask.shapes.empty()) want[j].mask.shapes = above.mask.shapes;
        if (want[j].detect_border && above.detect_border) want[j].shrink = above.shrink;
        touched[i] = true;
    }
    store(in, rows, want, touched);
}

int row_set(const std::vector<app::MaskSet>& sets, const StencilRow& row,
            const app::CameraStencil& cs) {
    if (cs.mask.shapes.empty()) return -1;
    for (size_t s = 0; s < sets.size(); s++)
        if (same_shapes(row_list(sets[s], row.camera), cs.mask.shapes)) return (int)s;
    return -1;
}

int common_set(const std::vector<app::MaskSet>& sets, const std::vector<PrepInput>& in,
               const std::vector<StencilRow>& rows) {
    bool drawn = false;
    for (const StencilRow& r : rows) drawn = drawn || !row_stencil(in, r).mask.shapes.empty();
    if (!drawn) return -1;
    for (size_t s = 0; s < sets.size(); s++) {
        bool all = true;
        for (const StencilRow& r : rows)
            all = all && same_shapes(list_for(sets[s], r.camera), row_stencil(in, r).mask.shapes);
        if (all) return (int)s;
    }
    return -1;
}

}  // namespace gui
