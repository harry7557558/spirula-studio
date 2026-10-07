// MaskDoc.cpp -- see MaskDoc.h.

#include "app/gui/mask/MaskDoc.h"

#include "app/FrameLook.h"
#include "app/FrameMask.h"
#include "app/gui/edit/Selection.h"
#include "i18n/catalog/MaskEdit.h"

#include <cstddef>
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace fs = std::filesystem;
namespace msg = spirula::i18n::msg::maskedit;

namespace gui {
namespace mask {

Rect clip(const Rect& r, int W, int H) {
    Rect c;
    c.x0 = std::max(0, r.x0);
    c.y0 = std::max(0, r.y0);
    c.x1 = std::min(W, r.x1);
    c.y1 = std::min(H, r.y1);
    return c;
}

Rect join(const Rect& a, const Rect& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    return {std::min(a.x0, b.x0), std::min(a.y0, b.y0),
            std::max(a.x1, b.x1), std::max(a.y1, b.y1)};
}

void to_stored(const sfm::ExifTransform& t, int W, int H, float dx, float dy,
               float& sx, float& sy) {
    int dw = W, dh = H;
    spirula::oriented_size(t.turns_cw, dw, dh);
    const float mx = t.mirror ? (float)dw - dx : dx;
    switch (t.turns_cw & 3) {
        case 1:  sx = dy;            sy = (float)H - mx; break;
        case 2:  sx = (float)W - mx; sy = (float)H - dy; break;
        case 3:  sx = (float)W - dy; sy = mx;            break;
        default: sx = mx;            sy = dy;            break;
    }
}

void to_stored(const sfm::ExifTransform& t, int W, int H, int dx, int dy,
               int& sx, int& sy) {
    int dw = W, dh = H;
    spirula::oriented_size(t.turns_cw, dw, dh);
    const int mx = t.mirror ? dw - 1 - dx : dx;
    switch (t.turns_cw & 3) {
        case 1:  sx = dy;         sy = H - 1 - mx; break;
        case 2:  sx = W - 1 - mx; sy = H - 1 - dy; break;
        case 3:  sx = W - 1 - dy; sy = mx;         break;
        default: sx = mx;         sy = dy;         break;
    }
}

// The inverse turn's "stored" image is our displayed one, so the same
// function under app::inverse_turn goes the other way.
void to_displayed(const sfm::ExifTransform& t, int W, int H, int sx, int sy,
                  int& dx, int& dy) {
    int dw = W, dh = H;
    spirula::oriented_size(t.turns_cw, dw, dh);
    to_stored(app::inverse_turn(t), dw, dh, sx, sy, dx, dy);
}

ShapeStroke stroke_to_stored(const ShapeStroke& s, const sfm::ExifTransform& t,
                             int W, int H) {
    ShapeStroke out = s;
    for (size_t i = 0; i + 1 < s.pts.size(); i += 2)
        to_stored(t, W, H, s.pts[i], s.pts[i + 1], out.pts[i], out.pts[i + 1]);
    return out;
}

Rect rect_to_displayed(const Rect& stored, const sfm::ExifTransform& t, int W, int H) {
    if (stored.empty()) return {};
    const int cx[2] = {stored.x0, stored.x1 - 1}, cy[2] = {stored.y0, stored.y1 - 1};
    Rect out;
    bool first = true;
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++) {
            int dx, dy;
            to_displayed(t, W, H, cx[i], cy[j], dx, dy);
            if (first) { out = {dx, dy, dx + 1, dy + 1}; first = false; }
            else out = join(out, Rect{dx, dy, dx + 1, dy + 1});
        }
    return out;
}

Rect stroke_bounds(const ShapeStroke& s, int W, int H) {
    if (s.pts.size() < 2) return {};
    float x0 = s.pts[0], y0 = s.pts[1], x1 = x0, y1 = y0;
    for (size_t i = 0; i + 1 < s.pts.size(); i += 2) {
        x0 = std::min(x0, s.pts[i]);
        x1 = std::max(x1, s.pts[i]);
        y0 = std::min(y0, s.pts[i + 1]);
        y1 = std::max(y1, s.pts[i + 1]);
    }
    const float r = s.kind == ShapeKind::Brush ? std::max(s.brush_radius, 1.0f) : 0.0f;
    const Rect b{(int)std::floor(x0 - r) - 1, (int)std::floor(y0 - r) - 1,
                 (int)std::ceil(x1 + r) + 2, (int)std::ceil(y1 + r) + 2};
    return clip(b, W, H);
}

// ---------------------------------------------------------------------------
// MaskDoc
// ---------------------------------------------------------------------------

bool MaskDoc::load(const std::string& layer_root, const std::string& mask_root,
                   const std::string& key, int w, int h, LayerIndex& idx,
                   std::string& error, std::string& warning) {
    _key = key;
    _ops.clear();
    _ids.clear();
    _head = 0;
    _bytes = 0;
    _byte_cap = kMaxHistoryBytes;
    _revision = _saved = 0;
    _last = Rect{};
    if (!recomposite_frame(layer_root, mask_root, key, idx, _state, error)) return false;
    const std::string base_path = layer_file(layer_root, key, Layer::Base);
    const std::string mask_path = mask_file(mask_root, key);
    std::error_code ec;
    const std::string src = fs::exists(base_path, ec) ? base_path
                          : fs::exists(mask_path, ec) ? mask_path : std::string();
    if (src.empty()) {
        _w = w;
        _h = h;
        _base.assign((size_t)w * h, 255);
    } else if (!app::load_stencil(src, _w, _h, _base)) {
        error = src;
        return false;
    } else if (idx.mask_flipped) {
        flip_polarity(_base.data(), _base.size());
    }
    FrameLayers layers;
    // A layer that would not load reads as all zero, which is indistinguishable
    // from no correction and would be saved back over the real one.
    if (!read_layers(layer_root, key, _w, _h, layers, warning)) return false;
    _drop.swap(layers.drop);
    _keep.swap(layers.keep);
    _composite.resize(_base.size());
    mask::composite(_base.data(), _drop.data(), _keep.data(), _base.size(), _composite.data());
    _kept = 0;
    for (uint8_t v : _composite) _kept += v ? 1 : 0;
    return true;
}

float MaskDoc::kept_fraction() const {
    const size_t n = _composite.size();
    return n ? (float)_kept / (float)n : 0.0f;
}

void MaskDoc::recomposite(const Rect& r) {
    for (int y = r.y0; y < r.y1; y++) {
        const size_t row = (size_t)y * _w;
        for (int x = r.x0; x < r.x1; x++) {
            const size_t i = row + (size_t)x;
            const uint8_t was = _composite[i];
            const uint8_t now = _keep[i] ? 255 : _drop[i] ? 0 : _base[i];
            _composite[i] = now;
            _kept += (now ? 1 : 0) - (was ? 1 : 0);
        }
    }
}

void MaskDoc::paint_rect(Paint mode, const Stencil& st, const Rect& r) {
    for (int y = r.y0; y < r.y1; y++) {
        const size_t row = (size_t)y * _w;
        for (int x = r.x0; x < r.x1; x++) {
            const size_t i = row + (size_t)x;
            if (!st.in[i]) continue;
            switch (mode) {
                case Paint::ForceDrop: _drop[i] = 255; _keep[i] = 0; break;
                case Paint::ForceKeep: _keep[i] = 255; _drop[i] = 0; break;
                default:               _drop[i] = 0;   _keep[i] = 0; break;
            }
        }
    }
    recomposite(r);
}

void MaskDoc::read_rect(const Rect& r, std::vector<uint8_t>& drop_r,
                        std::vector<uint8_t>& keep_r) const {
    const size_t n = (size_t)std::max(0, r.w()) * (size_t)std::max(0, r.h());
    drop_r.resize(n);
    keep_r.resize(n);
    for (int y = r.y0; y < r.y1; y++) {
        const size_t src = (size_t)y * _w + (size_t)r.x0;
        const size_t dst = (size_t)(y - r.y0) * (size_t)r.w();
        std::copy(_drop.begin() + (ptrdiff_t)src, _drop.begin() + (ptrdiff_t)(src + r.w()),
                  drop_r.begin() + (ptrdiff_t)dst);
        std::copy(_keep.begin() + (ptrdiff_t)src, _keep.begin() + (ptrdiff_t)(src + r.w()),
                  keep_r.begin() + (ptrdiff_t)dst);
    }
}

void MaskDoc::write_rect(const Rect& r, const uint8_t* drop_r, const uint8_t* keep_r) {
    for (int y = r.y0; y < r.y1; y++) {
        const size_t dst = (size_t)y * _w + (size_t)r.x0;
        const size_t src = (size_t)(y - r.y0) * (size_t)r.w();
        std::copy(drop_r + src, drop_r + src + r.w(), _drop.begin() + (ptrdiff_t)dst);
        std::copy(keep_r + src, keep_r + src + r.w(), _keep.begin() + (ptrdiff_t)dst);
    }
    recomposite(r);
}

namespace {

// One stroke: the RLE of each layer's rectangle before and after. The first
// apply is the paint itself; a redo patches the after-state back in.
class StrokeOp : public MaskOp {
public:
    StrokeOp(Paint mode, Stencil st, const Rect& r)
        : _mode(mode), _st(std::move(st)), _r(r) {}

    void apply(MaskDoc& doc) override {
        if (!_applied) {
            std::vector<uint8_t> d, k;
            doc.read_rect(_r, d, k);
            _before_drop = rle_encode(d);
            _before_keep = rle_encode(k);
            doc.paint_rect(_mode, _st, _r);
            doc.read_rect(_r, d, k);
            _after_drop = rle_encode(d);
            _after_keep = rle_encode(k);
            _changed = _before_drop != _after_drop || _before_keep != _after_keep;
            std::vector<uint8_t>().swap(_st.in);
            _applied = true;
            return;
        }
        patch(doc, _after_drop, _after_keep);
    }
    void undo(MaskDoc& doc) override { patch(doc, _before_drop, _before_keep); }
    const spirula::i18n::Msg& label() const override {
        return _mode == Paint::ForceDrop ? msg::op_drop
             : _mode == Paint::ForceKeep ? msg::op_keep : msg::op_clear;
    }
    size_t bytes() const override {
        return _before_drop.size() + _before_keep.size() + _after_drop.size() +
               _after_keep.size() + _st.in.size();
    }
    Rect touched() const override { return _r; }
    bool changed() const override { return _changed; }

private:
    void patch(MaskDoc& doc, const std::vector<uint8_t>& d, const std::vector<uint8_t>& k) {
        const size_t n = (size_t)_r.w() * (size_t)_r.h();
        std::vector<uint8_t> dd(n, 0), kk(n, 0);
        rle_decode(d, dd);
        rle_decode(k, kk);
        doc.write_rect(_r, dd.data(), kk.data());
    }

    Paint _mode;
    Stencil _st;
    Rect _r;
    bool _applied = false;
    bool _changed = true;
    std::vector<uint8_t> _before_drop, _before_keep, _after_drop, _after_keep;
};

}  // namespace

void MaskDoc::paint(Paint mode, Stencil st, const Rect& bounds) {
    const Rect r = clip(bounds, _w, _h);
    if (r.empty() || st.W != _w || st.H != _h) return;
    run(std::make_unique<StrokeOp>(mode, std::move(st), r));
}

void MaskDoc::run(std::unique_ptr<MaskOp> op) {
    op->apply(*this);
    if (!op->changed()) return;
    drop_redo();
    _bytes += op->bytes();
    _last = op->touched();
    _ops.push_back(std::move(op));
    _ids.push_back(++_next_id);
    _head = (int)_ops.size();
    while ((int)_ops.size() > kMaxHistoryOps ||
           (_bytes > _byte_cap && _ops.size() > 1)) {
        _bytes -= _ops.front()->bytes();
        _ops.erase(_ops.begin());
        _ids.erase(_ids.begin());
        _head--;
    }
    _revision++;
}

bool MaskDoc::redo_reaches(uint64_t step) const {
    const auto it = std::find(_ids.begin(), _ids.end(), step);
    return it != _ids.end() && it - _ids.begin() >= (ptrdiff_t)_head - 1;
}

void MaskDoc::drop_redo() {
    _ops.resize((size_t)_head);
    _ids.resize((size_t)_head);
    _bytes = 0;
    for (auto& o : _ops) _bytes += o->bytes();
}

void MaskDoc::undo() {
    if (!can_undo()) return;
    _ops[(size_t)--_head]->undo(*this);
    _last = _ops[(size_t)_head]->touched();
    _revision++;
}

void MaskDoc::redo() {
    if (!can_redo()) return;
    _ops[(size_t)_head]->apply(*this);
    _last = _ops[(size_t)_head]->touched();
    _head++;
    _revision++;
}

const spirula::i18n::Msg* MaskDoc::last_label() const {
    return _head > 0 ? &_ops[(size_t)_head - 1]->label() : nullptr;
}

bool MaskDoc::save(const std::string& layer_root, const std::string& mask_root,
                   LayerIndex& idx, std::string& error) {
    const bool write_comp = _state != BaseState::Missing;
    if (!save_frame(layer_root, mask_root, _key, _w, _h, _base.data(), _drop.data(),
                    _keep.data(), write_comp, idx, error))
        return false;
    _saved = _revision;
    if (write_comp) _state = BaseState::Unchanged;
    return true;
}

bool propagate_to(const std::string& layer_root, const std::string& mask_root,
                  const std::string& key, const std::string& image_file, int W, int H,
                  const uint8_t* drop, const uint8_t* keep, LayerIndex& idx,
                  PropagateRefusal& refused, std::string& error) {
    error.clear();
    refused = PropagateRefusal{};
    int w = 0, h = 0;
    std::string from;
    if (!frame_size(layer_root, mask_root, key, image_file, w, h, from)) {
        error = from;
        return false;
    }
    if (w != W || h != H) {
        refused.w = w;
        refused.h = h;
        return false;
    }
    MaskDoc t;
    std::string warning;
    // `warning` names the target's own mis-sized layer file. The propagate
    // would replace it anyway; refuse, so the file is looked at.
    if (!t.load(layer_root, mask_root, key, w, h, idx, error, warning)) {
        if (error.empty()) error = warning;
        return false;
    }
    if (t.width() != W || t.height() != H) {
        refused.w = t.width();
        refused.h = t.height();
        return false;
    }
    t.write_rect(Rect{0, 0, W, H}, drop, keep);
    return t.save(layer_root, mask_root, idx, error);
}

}  // namespace mask
}  // namespace gui
