#pragma once

// A document open for editing: what was loaded, what has been done to it, and
// what is selected. Design: docs/notes/gui-editing-plan.md.
//
// An edit is an ENTRY IN A LIST, not a mutation of the loaded data. Replaying
// the list from the original is how undo works and how "save the edits, not
// the result" will work; a delete is soft (a flag, not a removal) and
// compaction happens at save.
//
// A document has one or more LAYERS -- a sparse reconstruction has its points
// and its cameras -- and every tool works on the one that is current.

#include "app/gui/edit/Selection.h"
#include "core/Similarity.h"
#include "data/SceneCenter.h"
#include "i18n/Message.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace spirula { struct SplatCloud; struct SparseStats; }
namespace meshing { struct MeshData; }
struct ParsedDataset;

namespace gui {

class EditDoc;

// What produced a selection, carried along with the step that recorded it so
// that changing a setting after walking the history re-runs the right thing.
// Opaque here: the editing session is what knows the shape of one.
struct SelectRecipe;

// The only thing allowed to change a document.
struct EditOp {
    virtual ~EditOp() = default;
    virtual void apply(EditDoc& doc) = 0;
    virtual void undo(EditDoc& doc) = 0;
    // As it appears in the history list. Already translated and already
    // formatted, because a step naming a setting and its new value is a
    // sentence with a {0} in it rather than one fixed string.
    virtual std::string label() const = 0;
    virtual size_t bytes() const = 0;
    // Set on the steps that are a selection, null on the rest.
    virtual std::shared_ptr<const SelectRecipe> recipe() const { return {}; }
    // A placement made with the view taken along (an alignment): walking the
    // history across it has to take the view back the same way.
    virtual bool carries_view() const { return false; }
};

// What "Save a copy" can write this document as. `ext` is the extension a
// file target expects ("" for a target that writes a folder). `copy` is false
// for a target that can only be written back over its source.
struct SaveTarget {
    const spirula::i18n::Msg* label = nullptr;
    std::string ext;
    bool folder = false;
    bool copy = true;
};

class EditDoc {
public:
    enum class Kind { Splats, Points, Mesh };

    virtual ~EditDoc() = default;

    virtual Kind kind() const = 0;

    // ---- layers ----
    int layer_count() const { return (int)_layers.size(); }
    int layer() const { return _cur; }
    void set_layer(int i);
    const spirula::i18n::Msg& layer_name(int i) const;
    // What one element of a layer is called, for the counts on screen.
    const spirula::i18n::Msg& element_name() const { return layer_name(_cur); }

    // ---- the current layer ----
    int64_t count() const { return at(_cur).count; }
    int64_t alive_count() const { return at(_cur).alive_count; }
    const uint8_t* alive() const { return at(_cur).alive.data(); }
    Selection& sel() { return at(_cur).sel; }
    const Selection& sel() const { return at(_cur).sel; }
    // Element centres in the frame the viewport navigates, 3 floats each.
    const float* positions() const { return at(_cur).pos.data(); }
    // Per-element world radius, or null when the layer has none -- only a
    // Gaussian has an extent worth testing a stencil against.
    const float* radii() const {
        return at(_cur).radius.empty() ? nullptr : at(_cur).radius.data();
    }
    // The layer's extent and middle, by the median rather than the bounding
    // box: a trained model has floaters parked a kilometre out, and anything
    // sized off a box that holds them is sized wrong for everything else.
    float extent() const { return at(_cur).extent; }
    const float* middle() const { return at(_cur).middle; }
    // A neighbour distance that suits this cloud, from its density.
    float suggested_radius() const { return at(_cur).radius_hint; }

    // Where the LIVE elements are, for the viewport's centring menu: it
    // follows the edit rather than the file. False when there is nothing to
    // centre on.
    virtual bool live_centers(dsparse::CenterTable& out) const;

    // The element under a pixel, where the document can answer exactly -- a
    // mesh intersects its own faces. -1 leaves it to the nearest projection.
    virtual int64_t pick(const struct ViewProjection&, float, float) const {
        return -1;
    }

    // Element pairs connected by construction rather than by distance -- a
    // mesh's faces. Null when the layer has no topology of its own, which is
    // every layer that is a cloud.
    virtual const int32_t* topology(int64_t& pairs) const {
        pairs = 0;
        return nullptr;
    }

    // Another layer's state, for a document that publishes or saves all of
    // them at once.
    const std::vector<uint8_t>& alive_of(int layer) const {
        return at(layer).alive;
    }
    const Selection& sel_of(int layer) const { return at(layer).sel; }
    const float* positions_of(int layer) const { return at(layer).pos.data(); }
    int64_t alive_count_of(int layer) const { return at(layer).alive_count; }

    // ---- placement ----

    // One similarity of the positions() frame for every layer: the scene is
    // rigid. The VIEWER applies it; nothing here moves until a save bakes it.
    const spirula::Sim3& placement() const { return _placement; }
    void set_placement(const spirula::Sim3& p) {
        _placement = p;
        _rev++;
        _placement_rev++;
    }
    uint64_t placement_revision() const { return _placement_rev; }
    // File coordinates into the frame positions() are in.
    virtual spirula::Sim3 view_frame() const = 0;
    // The placement as the file's own coordinates see it: what a save writes.
    spirula::Sim3 file_placement() const {
        const spirula::Sim3 n = view_frame();
        return n.inverse() * _placement * n;
    }
    // A unit normal and a weight per element of layer 0, in the positions()
    // frame, where the document has them: a flat Gaussian, a mesh vertex.
    virtual bool normals(std::vector<float>& n, std::vector<float>& w) const {
        (void)n; (void)w;
        return false;
    }
    // ---- raw material for the attribute table (Attributes.h) ----
    // A display-referred, UNCLAMPED colour per element of the current layer.
    virtual bool colours(std::vector<float>& rgb) const {
        (void)rgb;
        return false;
    }
    // The same question without the answer, for a panel deciding what to show.
    virtual bool colours_available() const { return false; }
    // The Gaussians themselves, when that is what the elements are.
    virtual const spirula::SplatCloud* splats() const { return nullptr; }
    // The triangles, the parsed reconstruction and what its files say about
    // its quality, for the documents that are those things.
    virtual const meshing::MeshData* mesh() const { return nullptr; }
    virtual const ParsedDataset* dataset() const { return nullptr; }
    virtual const spirula::SparseStats* sparse_stats() const { return nullptr; }
    // Whether that last one is worth asking for: the asking reads files.
    virtual bool has_sparse_stats() const { return false; }
    // Camera centres in the positions() frame, [n, 3]; empty without cameras.
    virtual std::vector<float> camera_centres() const { return {}; }

    // How much each element of layer 0 is part of a SURFACE, 0..1, or null
    // when they all are. A trained model is full of faint, oversized haze
    // that no floor should be fitted through.
    virtual const float* solidity() const { return nullptr; }

    // Which way the people who took the photos thought was up, same frame:
    // the mean of the cameras' own up axes. False without cameras.
    virtual bool up_hint(float up[3]) const {
        (void)up;
        return false;
    }

    // ---- history ----
    // Runs `op` and puts it on the stack; drops the oldest entries when the
    // history is over its byte or count budget.
    void run(std::unique_ptr<EditOp> op);
    bool can_undo() const { return _head > 0; }
    bool can_redo() const { return _head < (int)_ops.size(); }
    void undo();
    void redo();
    // Walk to a point in the history, which is what the list in the panel is
    // for: reaching a state ten steps back without ten keystrokes.
    void goto_step(int head);
    std::string undo_name() const;
    std::string redo_name() const;
    // The recipe of the step the history currently stands on, which is what a
    // setting changed here should re-run. Null when that step is not one.
    std::shared_ptr<const SelectRecipe> current_recipe() const;
    size_t history_bytes() const { return _bytes; }
    const std::vector<std::unique_ptr<EditOp>>& history() const { return _ops; }
    int history_head() const { return _head; }

    // ---- what the ops write through ----
    void set_alive(int64_t i, bool a);
    void set_selection(const std::vector<uint8_t>& w);
    void mark_geometry_dirty() { _geom_dirty = true; _display_dirty = true; _rev++; }
    void mark_display_dirty() { _display_dirty = true; _rev++; }
    // Bumped by every change to what is live or selected: what a cache of
    // anything derived from either is keyed on.
    uint64_t revision() const { return _rev; }

    bool dirty() const { return _edited; }
    void mark_saved() { _edited = false; }

    // Push whatever changed to whatever is drawing this document, and say
    // whether it did. Once a frame, from the GUI thread.
    bool publish();
    // Put the renderer back the way it was found. A document's effect on what
    // is on screen ends with the document, whether its edits were saved or
    // thrown away -- the file is the only place either outcome is recorded.
    virtual void revert_display() = 0;

    // ---- saving ----
    virtual std::vector<SaveTarget> save_targets() const = 0;
    // Writes the live elements only, off the GUI thread, bumping `progress`
    // once per file it finishes. Throws std::runtime_error.
    virtual void save(int target, const std::string& path,
                      std::atomic<int>* progress = nullptr) = 0;
    // How many files that would be, for the bar.
    virtual int save_steps(int target) const { (void)target; return 1; }
    // How many OTHER files a save would also rewrite, and the switch that
    // says whether to; 0 when the document has no siblings.
    virtual int linked_count() const { return 0; }
    virtual void set_linked(bool on) { (void)on; }
    // What "Save" (as opposed to "Save a copy") would overwrite, "" when the
    // document has no home to write back to.
    virtual std::string default_save_path(int target) const { (void)target; return {}; }

    const std::string& source_path() const { return _source; }

protected:
    struct Layer {
        const spirula::i18n::Msg* name = nullptr;
        int64_t count = 0;
        int64_t alive_count = 0;
        std::vector<float> pos;       // [count, 3], in the navigated frame
        std::vector<float> radius;    // [count] or empty
        std::vector<uint8_t> alive;
        Selection sel;
        float extent = 1.0f;
        float middle[3] = {0, 0, 0};
        float radius_hint = 0.01f;
    };

    // Fills one layer. Call from the concrete document's constructor, in the
    // order the panel should offer them.
    void add_layer(const spirula::i18n::Msg& name, int64_t n,
                   std::vector<float> positions, std::vector<float> radius = {});
    void set_source(std::string s) { _source = std::move(s); }
    void set_position(int layer, int64_t i, const float p[3]) {
        for (int k = 0; k < 3; k++) at(layer).pos[(size_t)i * 3 + k] = p[k];
    }
    // The concrete document's half of publish(): `geometry` says the live set
    // changed, otherwise only the selection did.
    virtual void publish_impl(bool geometry) = 0;

private:
    Layer& at(int i) { return _layers[(size_t)i]; }
    const Layer& at(int i) const { return _layers[(size_t)i]; }

    std::vector<Layer> _layers;
    int _cur = 0;
    std::string _source;
    spirula::Sim3 _placement;

    std::vector<std::unique_ptr<EditOp>> _ops;
    int _head = 0;
    size_t _bytes = 0;
    bool _edited = false;
    bool _geom_dirty = true;
    bool _display_dirty = true;
    uint64_t _rev = 1;
    uint64_t _placement_rev = 1;
};


// ---------------------------------------------------------------------------
// The ops phases 1 and 2 need. Each remembers the layer it was made on, so
// switching layers between an edit and its undo cannot misapply it.
// ---------------------------------------------------------------------------

// Soft delete: the elements are flagged, not removed, and compaction happens
// at save. Undo is then the same list of indices back the other way.
std::unique_ptr<EditOp> make_hide_op(EditDoc& doc, bool invert);
// Every hidden element back.
std::unique_ptr<EditOp> make_reveal_op(EditDoc& doc);
// A selection change, so that a mis-aimed lasso is one Ctrl+Z away like
// everything else. Both sides are run-length encoded.
std::unique_ptr<EditOp> make_select_op(EditDoc& doc, std::vector<uint8_t> next,
                                       std::string label,
                                       std::shared_ptr<const SelectRecipe> recipe);

// An op that is not about the elements at all: a setting the tools read,
// recorded so that changing one is as undoable as anything else. `apply`
// takes true to set the new value and false to put the old one back.
std::unique_ptr<EditOp> make_setting_op(std::function<void(bool)> apply,
                                        std::string label,
                                        std::shared_ptr<const SelectRecipe> recipe);

// A placement change. Both ends are stored rather than the step between
// them, so walking the history back and forth never accumulates rounding.
std::unique_ptr<EditOp> make_placement_op(EditDoc& doc, const spirula::Sim3& next,
                                          std::string label,
                                          bool carries_view = false);

}  // namespace gui
