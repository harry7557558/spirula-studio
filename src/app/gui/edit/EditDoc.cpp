// EditDoc.cpp -- see EditDoc.h.

#include "app/gui/edit/EditDoc.h"

#include "i18n/catalog/Edit.h"

#include <cstddef>
#include <algorithm>
#include <cmath>

namespace msg = spirula::i18n::msg::edit;

namespace gui {

namespace {

// What the history may hold before the oldest entries are dropped: a session
// going for an hour must not be the reason the machine runs out of memory.
constexpr size_t kMaxHistoryBytes = 256u << 20;
constexpr int kMaxHistoryOps = 96;

// The layer an op was made on, restored however the op returns.
class LayerScope {
public:
    LayerScope(EditDoc& doc, int layer) : _doc(doc), _was(doc.layer()) {
        _doc.set_layer(layer);
    }
    ~LayerScope() { _doc.set_layer(_was); }

private:
    EditDoc& _doc;
    int _was;
};

}  // namespace


void EditDoc::add_layer(const spirula::i18n::Msg& name, int64_t n,
                        std::vector<float> positions,
                        std::vector<float> radius) {
    Layer L;
    L.name = &name;
    L.count = n;
    L.pos = std::move(positions);
    L.radius = std::move(radius);
    L.alive.assign((size_t)n, 1);
    L.alive_count = n;
    L.sel.resize(n);

    // The median distance from the median point, as the viewer frames a model
    // by: a trained scene has floaters, so a bounding box puts every default
    // radius a kilometre out.
    float c[3] = {0, 0, 0};
    if (n > 0) {
        const int64_t step = std::max<int64_t>(1, n / (1 << 20));
        std::vector<float> tmp;
        tmp.reserve((size_t)(n / step + 1));
        for (int d = 0; d < 3; d++) {
            tmp.clear();
            for (int64_t i = 0; i < n; i += step) tmp.push_back(L.pos[(size_t)i * 3 + d]);
            std::nth_element(tmp.begin(), tmp.begin() + tmp.size() / 2, tmp.end());
            c[d] = tmp[tmp.size() / 2];
        }
        tmp.clear();
        for (int64_t i = 0; i < n; i += step) {
            const float dx = L.pos[(size_t)i * 3 + 0] - c[0];
            const float dy = L.pos[(size_t)i * 3 + 1] - c[1];
            const float dz = L.pos[(size_t)i * 3 + 2] - c[2];
            tmp.push_back(dx * dx + dy * dy + dz * dz);
        }
        std::nth_element(tmp.begin(), tmp.begin() + tmp.size() / 2, tmp.end());
        L.extent = 2.0f * std::sqrt(std::max(tmp[tmp.size() / 2], 1e-24f));
    }
    L.extent = std::max(L.extent, 1e-6f);
    for (int d = 0; d < 3; d++) L.middle[d] = c[d];
    const double side = 2.0 * (double)L.extent;
    L.radius_hint = (float)(1.5 * side / std::cbrt((double)std::max<int64_t>(n, 1)));
    L.radius_hint = std::clamp(L.radius_hint, L.extent * 1e-4f, L.extent * 0.25f);
    _layers.push_back(std::move(L));
}

void EditDoc::set_layer(int i) {
    if (i >= 0 && i < layer_count()) _cur = i;
}

const spirula::i18n::Msg& EditDoc::layer_name(int i) const {
    return *at(std::clamp(i, 0, layer_count() - 1)).name;
}

void EditDoc::set_alive(int64_t i, bool a) {
    Layer& L = at(_cur);
    uint8_t& v = L.alive[(size_t)i];
    if ((v != 0) == a) return;
    v = a ? 1 : 0;
    L.alive_count += a ? 1 : -1;
}

void EditDoc::set_selection(const std::vector<uint8_t>& w) {
    at(_cur).sel.assign(w);
}

void EditDoc::run(std::unique_ptr<EditOp> op) {
    op->apply(*this);
    _ops.resize((size_t)_head);
    _bytes = 0;
    for (auto& o : _ops) _bytes += o->bytes();
    _bytes += op->bytes();
    _ops.push_back(std::move(op));
    _head = (int)_ops.size();
    while ((int)_ops.size() > kMaxHistoryOps ||
           (_bytes > kMaxHistoryBytes && _ops.size() > 1)) {
        _bytes -= _ops.front()->bytes();
        _ops.erase(_ops.begin());
        _head--;
    }
    _edited = true;
}

void EditDoc::undo() {
    if (!can_undo()) return;
    _ops[(size_t)--_head]->undo(*this);
    _edited = true;
}

void EditDoc::redo() {
    if (!can_redo()) return;
    _ops[(size_t)_head++]->apply(*this);
    _edited = true;
}

void EditDoc::goto_step(int head) {
    head = std::clamp(head, 0, (int)_ops.size());
    while (_head > head) undo();
    while (_head < head) redo();
}

std::string EditDoc::undo_name() const {
    return can_undo() ? _ops[(size_t)_head - 1]->label() : std::string();
}

std::string EditDoc::redo_name() const {
    return can_redo() ? _ops[(size_t)_head]->label() : std::string();
}

std::shared_ptr<const SelectRecipe> EditDoc::current_recipe() const {
    return _head > 0 ? _ops[(size_t)_head - 1]->recipe()
                     : std::shared_ptr<const SelectRecipe>();
}

// The current layer's live positions, which are already in the frame the
// viewport navigates.
bool EditDoc::live_centers(dsparse::CenterTable& out) const {
    const Layer& L = at(_cur);
    if (L.alive_count <= 0) return false;
    std::vector<float> live;
    live.reserve((size_t)L.alive_count * 3);
    for (int64_t i = 0; i < L.count; i++) {
        if (!L.alive[(size_t)i]) continue;
        live.insert(live.end(), L.pos.begin() + (ptrdiff_t)(i * 3),
                    L.pos.begin() + (ptrdiff_t)(i * 3 + 3));
    }
    out = dsparse::scene_centers(nullptr, 0, live.data(),
                                 (int64_t)live.size() / 3, 3, nullptr);
    return true;
}

bool EditDoc::publish() {
    if (!_geom_dirty && !_display_dirty) return false;
    const bool geom = _geom_dirty;
    _geom_dirty = false;
    _display_dirty = false;
    publish_impl(geom);
    return true;
}


// ---------------------------------------------------------------------------
// Ops
// ---------------------------------------------------------------------------

namespace {

// Deletes are soft: the element is flagged, not removed, and compaction
// happens at save. Undo is then the same index list back the other way, which
// is why nothing here carries a copy of the data.
class HideOp : public EditOp {
public:
    HideOp(int layer, std::vector<int32_t> idx, std::vector<uint8_t> was,
           bool invert)
        : _layer(layer), _idx(std::move(idx)), _was(std::move(was)),
          _invert(invert) {}

    void apply(EditDoc& doc) override {
        LayerScope scope(doc, _layer);
        std::vector<uint8_t> w = doc.sel().weights();
        for (size_t k = 0; k < _idx.size(); k++) {
            doc.set_alive(_idx[k], false);
            w[(size_t)_idx[k]] = 0;
        }
        // What was deleted is no longer there to be selected, and a count
        // that goes on naming it is the first thing anyone queries.
        if (!_invert) doc.set_selection(w);
        doc.mark_geometry_dirty();
    }
    void undo(EditDoc& doc) override {
        LayerScope scope(doc, _layer);
        std::vector<uint8_t> w = doc.sel().weights();
        for (size_t k = 0; k < _idx.size(); k++) {
            doc.set_alive(_idx[k], true);
            w[(size_t)_idx[k]] = _was[k];
        }
        if (!_invert) doc.set_selection(w);
        doc.mark_geometry_dirty();
    }
    std::string label() const override {
        return (_invert ? msg::op_isolate : msg::op_delete).get();
    }
    size_t bytes() const override {
        return _idx.size() * (sizeof(int32_t) + 1) + 32;
    }

private:
    int _layer;
    std::vector<int32_t> _idx;
    std::vector<uint8_t> _was;
    bool _invert;
};

class RevealOp : public EditOp {
public:
    RevealOp(int layer, std::vector<int32_t> idx)
        : _layer(layer), _idx(std::move(idx)) {}
    void apply(EditDoc& doc) override {
        LayerScope scope(doc, _layer);
        for (int32_t i : _idx) doc.set_alive(i, true);
        doc.mark_geometry_dirty();
    }
    void undo(EditDoc& doc) override {
        LayerScope scope(doc, _layer);
        for (int32_t i : _idx) doc.set_alive(i, false);
        doc.mark_geometry_dirty();
    }
    std::string label() const override { return msg::op_restore.get(); }
    size_t bytes() const override { return _idx.size() * sizeof(int32_t) + 32; }

private:
    int _layer;
    std::vector<int32_t> _idx;
};

class SelectOp : public EditOp {
public:
    SelectOp(int layer, std::vector<uint8_t> prev, std::vector<uint8_t> next,
             std::string label, std::shared_ptr<const SelectRecipe> recipe)
        : _layer(layer), _prev(rle_encode(prev)), _next(rle_encode(next)),
          _n(prev.size()), _label(std::move(label)),
          _recipe(std::move(recipe)) {}

    void apply(EditDoc& doc) override { put(doc, _next); }
    void undo(EditDoc& doc) override { put(doc, _prev); }
    std::string label() const override { return _label; }
    size_t bytes() const override { return _prev.size() + _next.size() + 48; }
    std::shared_ptr<const SelectRecipe> recipe() const override {
        return _recipe;
    }

private:
    void put(EditDoc& doc, const std::vector<uint8_t>& rle) {
        LayerScope scope(doc, _layer);
        std::vector<uint8_t> w(_n, 0);
        rle_decode(rle, w);
        doc.set_selection(w);
        doc.mark_display_dirty();
    }
    int _layer;
    std::vector<uint8_t> _prev, _next;
    size_t _n;
    std::string _label;
    std::shared_ptr<const SelectRecipe> _recipe;
};

}  // namespace


std::unique_ptr<EditOp> make_hide_op(EditDoc& doc, bool invert) {
    std::vector<int32_t> idx;
    std::vector<uint8_t> was;
    const uint8_t* alive = doc.alive();
    const Selection& s = doc.sel();
    for (int64_t i = 0; i < doc.count(); i++) {
        if (!alive[i]) continue;
        if (s.selected(i) == invert) continue;
        idx.push_back((int32_t)i);
        was.push_back(s.weight(i));
    }
    return std::make_unique<HideOp>(doc.layer(), std::move(idx), std::move(was),
                                    invert);
}

std::unique_ptr<EditOp> make_reveal_op(EditDoc& doc) {
    std::vector<int32_t> idx;
    const uint8_t* alive = doc.alive();
    for (int64_t i = 0; i < doc.count(); i++)
        if (!alive[i]) idx.push_back((int32_t)i);
    return std::make_unique<RevealOp>(doc.layer(), std::move(idx));
}

namespace {

// A setting change. It touches no element, so undo is the same call the other
// way and the history costs nothing to carry it.
class SettingOp : public EditOp {
public:
    SettingOp(std::function<void(bool)> apply, std::string label,
              std::shared_ptr<const SelectRecipe> recipe)
        : _apply(std::move(apply)), _label(std::move(label)),
          _recipe(std::move(recipe)) {}
    void apply(EditDoc& doc) override {
        _apply(true);
        doc.mark_display_dirty();
    }
    void undo(EditDoc& doc) override {
        _apply(false);
        doc.mark_display_dirty();
    }
    std::string label() const override { return _label; }
    size_t bytes() const override { return _label.size() + 64; }
    // The selection this step re-derives, so a second setting changed on top
    // of it re-derives the same one.
    std::shared_ptr<const SelectRecipe> recipe() const override {
        return _recipe;
    }

private:
    std::function<void(bool)> _apply;
    std::string _label;
    std::shared_ptr<const SelectRecipe> _recipe;
};

}  // namespace

namespace {

class PlacementOp : public EditOp {
public:
    PlacementOp(const spirula::Sim3& was, const spirula::Sim3& next,
                std::string label, bool carried)
        : _was(was), _next(next), _label(std::move(label)), _carried(carried) {}
    bool carries_view() const override { return _carried; }
    void apply(EditDoc& doc) override { doc.set_placement(_next); }
    void undo(EditDoc& doc) override { doc.set_placement(_was); }
    std::string label() const override { return _label; }
    size_t bytes() const override { return sizeof *this + _label.size(); }

private:
    spirula::Sim3 _was, _next;
    std::string _label;
    bool _carried;
};

}  // namespace

std::unique_ptr<EditOp> make_placement_op(EditDoc& doc, const spirula::Sim3& next,
                                          std::string label, bool carries_view) {
    return std::make_unique<PlacementOp>(doc.placement(), next, std::move(label),
                                         carries_view);
}

std::unique_ptr<EditOp> make_setting_op(std::function<void(bool)> apply,
                                        std::string label,
                                        std::shared_ptr<const SelectRecipe> recipe) {
    return std::make_unique<SettingOp>(std::move(apply), std::move(label),
                                       std::move(recipe));
}

std::unique_ptr<EditOp> make_select_op(EditDoc& doc, std::vector<uint8_t> next,
                                       std::string label,
                                       std::shared_ptr<const SelectRecipe> recipe) {
    return std::make_unique<SelectOp>(doc.layer(), doc.sel().weights(),
                                      std::move(next), std::move(label),
                                      std::move(recipe));
}

}  // namespace gui
