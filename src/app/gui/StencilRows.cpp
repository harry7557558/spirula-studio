#include "app/gui/StencilRows.h"

#include <algorithm>
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

// Whether a row seen for the first time follows the areas of `above`. Not on
// a row whose fit differs: that is another kind of lens, whose drawn areas mean
// nothing on this one (a GoPro sphere under a dual fisheye).
bool starts_following(const app::CameraStencil& row, const app::CameraStencil& above) {
    return same_part(row, above, StencilPart::Areas) &&
           row.detect_border == above.detect_border;
}

// Writes `want` back to where each row of a touched input reads it from. An
// input whose rows all agree goes back to one stencil, unless its stencil
// names a camera no row covers, which keeps its own.
void store(const StencilOwners& in, const std::vector<StencilRow>& rows,
           const std::vector<app::CameraStencil>& want, const std::vector<bool>& touched) {
    for (size_t i = 0; i < in.size(); i++) {
        if (!touched[i]) continue;
        std::vector<size_t> mine;
        for (size_t r = 0; r < rows.size(); r++)
            if (rows[r].input == i) mine.push_back(r);
        if (mine.empty()) continue;
        app::FrameStencil& st = *in[i].stencil;
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

// Changes the rows in `which` and stores them.
void write_rows(const StencilOwners& in, const std::vector<StencilRow>& rows,
                const std::vector<size_t>& which,
                const std::function<void(app::CameraStencil&)>& assign) {
    std::vector<app::CameraStencil> want = row_stencils(in, rows);
    std::vector<bool> touched(in.size(), false);
    for (size_t j : which) {
        assign(want[j]);
        touched[rows[j].input] = true;
    }
    store(in, rows, want, touched);
}

StencilFollow* entry(const StencilOwners& in, const StencilRow& r) {
    const auto it = in[r.input].follow->find(r.camera);
    return it == in[r.input].follow->end() ? nullptr : &it->second;
}

int share_of(const StencilOwners& in, const StencilRow& r) {
    const StencilFollow* f = entry(in, r);
    return f ? f->share : 0;
}

int next_share(const StencilOwners& in) {
    int top = 0;
    for (const StencilOwner& o : in)
        for (const auto& [camera, f] : *o.follow) top = std::max(top, f.share);
    return top + 1;
}

// The row's choice, or before settle_follow has given it one, whether its
// areas match the row above in `vals`.
bool follows_in(const StencilOwners& in, const std::vector<StencilRow>& rows,
                const std::vector<app::CameraStencil>& vals, size_t j) {
    const int a = rows[j].above;
    if (a < 0) return false;
    const StencilFollow* f = entry(in, rows[j]);
    return f ? f->areas : starts_following(vals[j], vals[(size_t)a]);
}

// from[j] >= 0 marks a changed row: the row whose new areas it takes. A row
// joins the one it follows, as `before` found them. Rows in `fixed` are
// neither joined nor written, so nothing passes through one but a seed.
using Assign = std::function<void(size_t row, size_t from, app::CameraStencil& cs)>;
void spread(const StencilOwners& in, const std::vector<StencilRow>& rows,
            const std::vector<app::CameraStencil>& before, std::vector<int> from,
            const std::vector<bool>& fixed, const Assign& assign) {
    for (size_t j = 0; j < rows.size(); j++) {
        const int a = rows[j].above;
        if (from[j] >= 0 || fixed[j] || a < 0 || from[(size_t)a] < 0) continue;
        if (follows_in(in, rows, before, j)) from[j] = from[(size_t)a];
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

void set_areas(const StencilOwners& in, const std::vector<StencilRow>& rows, size_t r,
               const app::MaskSet& set, bool follow) {
    settle_follow(in, rows);
    std::vector<int> from(rows.size(), -1);
    from[r] = (int)r;
    spread(in, rows, row_stencils(in, rows), from, std::vector<bool>(rows.size(), false),
           [&](size_t j, size_t, app::CameraStencil& cs) {
               cs.mask.shapes = row_list(set, rows[j].camera);
           });
    (*in[rows[r].input].follow)[rows[r].camera].areas = follow;
    settle_follow(in, rows);
}

// What Edit border ellipse leaves: the fit turned off and a keep ellipse put
// first, which is this lens's circle rather than anything drawn.
bool border_edited(const app::CameraStencil& before, const app::CameraStencil& after) {
    const auto& s = after.mask.shapes;
    return before.detect_border && !after.detect_border && !s.empty() && !s[0].remove &&
           s[0].kind == app::MaskShape::Kind::Ellipse &&
           (before.mask.shapes.empty() || !same_shape(before.mask.shapes[0], s[0]));
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

void append_rows(std::vector<StencilRow>& rows,
                 const std::vector<std::vector<std::string>>& keys, size_t first_owner) {
    const int first = (int)rows.size();
    for (StencilRow r : stencil_rows(keys)) {
        r.input += first_owner;
        if (r.above >= 0) r.above += first;
        rows.push_back(std::move(r));
    }
}

const app::CameraStencil& row_stencil(const StencilOwners& in, const StencilRow& r) {
    return in[r.input].stencil->for_camera(r.camera);
}

std::vector<app::CameraStencil> row_stencils(const StencilOwners& in,
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

void settle_follow(const StencilOwners& in, const std::vector<StencilRow>& rows) {
    const std::vector<app::CameraStencil> vals = row_stencils(in, rows);
    std::vector<app::CameraStencil> want = vals;
    std::vector<bool> joined(in.size(), false);
    for (size_t j = 0; j < rows.size(); j++) {
        const size_t i = rows[j].input;
        const std::string& camera = rows[j].camera;
        const int a = rows[j].above;
        if (StencilFollow* f = entry(in, rows[j])) {
            f->areas = f->areas && a >= 0 &&
                       same_part(vals[j], vals[(size_t)a], StencilPart::Areas);
            continue;
        }
        StencilFollow f;
        f.areas = a >= 0 && starts_following(vals[j], vals[(size_t)a]);
        // Rows above were settled first, so each has its entry.
        const bool shares = !in[i].setup.empty() && !camera.empty() && vals[j].detect_border;
        for (size_t k = j; shares && k-- > 0;) {
            const size_t ki = rows[k].input;
            if (ki == i || in[ki].setup != in[i].setup || rows[k].camera != camera ||
                !vals[k].detect_border)
                continue;
            StencilFollow& above = (*in[ki].follow)[camera];
            if (!above.share) above.share = next_share(in);
            f.share = above.share;
            want[j].shrink = want[k].shrink;
            joined[i] = true;
            break;
        }
        (*in[i].follow)[camera] = f;
    }
    store(in, rows, want, joined);
    std::map<int, int> count;
    for (size_t j = 0; j < rows.size(); j++) {
        StencilFollow& f = *entry(in, rows[j]);
        if (!vals[j].detect_border) f.share = 0;
        if (f.share) count[f.share]++;
    }
    for (const StencilRow& r : rows) {
        StencilFollow& f = *entry(in, r);
        if (f.share && count[f.share] < 2) f.share = 0;
    }
    for (size_t i = 0; i < in.size(); i++)
        for (auto it = (*in[i].follow).begin(); it != (*in[i].follow).end();) {
            bool listed = false;
            for (const StencilRow& r : rows)
                listed = listed || (r.input == i && r.camera == it->first);
            it = listed ? std::next(it) : (*in[i].follow).erase(it);
        }
}

bool row_follows(const StencilOwners& in, const std::vector<StencilRow>& rows, size_t r) {
    const int a = rows[r].above;
    if (a < 0) return false;
    if (const StencilFollow* f = entry(in, rows[r])) return f->areas;
    return starts_following(row_stencil(in, rows[r]), row_stencil(in, rows[(size_t)a]));
}

void set_row_border(const StencilOwners& in, const std::vector<StencilRow>& rows, size_t r,
                    bool on, float shrink) {
    settle_follow(in, rows);
    entry(in, rows[r])->share = 0;
    write_rows(in, rows, {r}, [&](app::CameraStencil& cs) {
        cs.detect_border = on;
        // Kept while off, so turning it back on finds it.
        if (on) cs.shrink = shrink;
    });
    settle_follow(in, rows);
}

void set_row_shrink(const StencilOwners& in, const std::vector<StencilRow>& rows, size_t r,
                    float shrink) {
    settle_follow(in, rows);
    std::vector<size_t> which = border_sharers(in, rows, r);
    if (which.empty()) which = {r};
    write_rows(in, rows, which, [&](app::CameraStencil& cs) { cs.shrink = shrink; });
    settle_follow(in, rows);
}

void share_border(const StencilOwners& in, const std::vector<StencilRow>& rows, size_t r,
                  size_t with) {
    settle_follow(in, rows);
    const app::CameraStencil target = row_stencil(in, rows[with]);
    if (with == r || !target.detect_border) return;
    StencilFollow& t = *entry(in, rows[with]);
    if (!t.share) t.share = next_share(in);
    entry(in, rows[r])->share = t.share;
    write_rows(in, rows, {r}, [&](app::CameraStencil& cs) {
        cs.detect_border = true;
        cs.shrink = target.shrink;
    });
    settle_follow(in, rows);
}

std::vector<size_t> border_sharers(const StencilOwners& in, const std::vector<StencilRow>& rows,
                                   size_t r) {
    std::vector<size_t> out;
    const int share = share_of(in, rows[r]);
    for (size_t j = 0; share && j < rows.size(); j++)
        if (share_of(in, rows[j]) == share) out.push_back(j);
    return out;
}

void set_row_areas(const StencilOwners& in, const std::vector<StencilRow>& rows, size_t r,
                   const app::MaskSet& set) {
    set_areas(in, rows, r, set, /*follow=*/false);
}

void follow_above(const StencilOwners& in, const std::vector<StencilRow>& rows, size_t r) {
    const int a = rows[r].above;
    if (a < 0) return;
    app::MaskSet set;
    set.shapes = row_stencil(in, rows[(size_t)a]).mask.shapes;
    set_areas(in, rows, r, set, /*follow=*/true);
}

void follow_edit(const StencilOwners& in, const std::vector<StencilRow>& rows,
                 const std::vector<app::CameraStencil>& before, size_t edited) {
    if (before.size() != rows.size()) return;
    const std::vector<app::CameraStencil> after = row_stencils(in, rows);
    std::vector<bool> fixed(rows.size(), false), ellipse(rows.size(), false);
    // The areas as drawn: an edited border ellipse belongs to the lens and goes
    // only where its circle is shared, never down the areas.
    std::vector<app::CameraStencil> drawn = after;
    for (size_t j = 0; j < rows.size(); j++) {
        fixed[j] = rows[j].input == edited;
        ellipse[j] = fixed[j] && border_edited(before[j], after[j]);
        if (ellipse[j]) drawn[j].mask.shapes.erase(drawn[j].mask.shapes.begin());
    }
    std::vector<int> from(rows.size(), -1);
    bool any = false;
    for (size_t j = 0; j < rows.size(); j++)
        if (fixed[j] && !same_part(before[j], drawn[j], StencilPart::Areas)) {
            from[j] = (int)j;
            any = true;
        }
    if (any)
        spread(in, rows, before, from, fixed, [&](size_t, size_t src, app::CameraStencil& cs) {
            cs.mask.shapes = drawn[src].mask.shapes;
        });

    std::vector<app::CameraStencil> want = row_stencils(in, rows);
    std::vector<bool> touched(in.size(), false), done(rows.size(), false);
    for (size_t j = 0; j < rows.size(); j++) {
        if (!fixed[j]) continue;
        const bool shrink = before[j].detect_border && after[j].detect_border &&
                            before[j].shrink != after[j].shrink;
        if (!shrink && !ellipse[j]) continue;
        for (size_t k : border_sharers(in, rows, j)) {
            if (fixed[k] || done[k]) continue;
            done[k] = true;
            touched[rows[k].input] = true;
            if (!ellipse[j]) {
                want[k].shrink = after[j].shrink;
                continue;
            }
            want[k].detect_border = false;
            want[k].mask.shapes.insert(want[k].mask.shapes.begin(), after[j].mask.shapes[0]);
        }
    }
    store(in, rows, want, touched);
    settle_follow(in, rows);
}

void adopt_rows_above(const StencilOwners& in, const std::vector<StencilRow>& rows,
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

int common_set(const std::vector<app::MaskSet>& sets, const StencilOwners& in,
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

StencilOwners stencil_owners(std::vector<PrepInput>& in, const std::vector<std::string>& setups) {
    StencilOwners out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); i++)
        out.push_back({&in[i].stencil, &in[i].stencil_follow,
                       i < setups.size() ? setups[i] : std::string()});
    return out;
}

}  // namespace gui
