#pragma once

// One frame's correction in memory: the decoded base, the two layers, the
// composite kept current over the rectangle each stroke touched, and an undo
// history of RLE'd rectangles on the EditOp pattern (edit/EditDoc.h). Layers
// are byte planes rather than gui::Selection so a stroke at 8K pays for its
// own rectangle only. No ImGui, no GL. Design: docs/notes/mask-editor.md.

#include "app/gui/edit/SelectShape.h"
#include "app/gui/mask/MaskLayer.h"
#include "core/ImageOrient.h"
#include "sfm/core/Exif.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace spirula { namespace i18n { struct Msg; } }

namespace gui {
namespace mask {

struct Rect {
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;   // half-open
    bool empty() const { return x1 <= x0 || y1 <= y0; }
    int w() const { return x1 - x0; }
    int h() const { return y1 - y0; }
};
Rect clip(const Rect& r, int W, int H);
Rect join(const Rect& a, const Rect& b);

enum class Paint { ForceDrop, ForceKeep, Clear };

// Displayed pixel -> stored pixel under the file's EXIF turn `t`, for a
// W x H STORED image: core/ImageOrient.h's mapping, in both coordinate forms.
void to_stored(const sfm::ExifTransform& t, int W, int H, float dx, float dy,
               float& sx, float& sy);
void to_stored(const sfm::ExifTransform& t, int W, int H, int dx, int dy,
               int& sx, int& sy);
// Stored pixel -> displayed pixel, the inverse of the integer form above.
void to_displayed(const sfm::ExifTransform& t, int W, int H, int sx, int sy,
                  int& dx, int& dy);
ShapeStroke stroke_to_stored(const ShapeStroke& s, const sfm::ExifTransform& t,
                             int W, int H);
Rect rect_to_displayed(const Rect& stored, const sfm::ExifTransform& t, int W, int H);
// The stroke's pixels, brush radius included, clipped to W x H.
Rect stroke_bounds(const ShapeStroke& s, int W, int H);

class MaskDoc;

struct MaskOp {
    virtual ~MaskOp() = default;
    virtual void apply(MaskDoc& doc) = 0;
    virtual void undo(MaskDoc& doc) = 0;
    virtual const spirula::i18n::Msg& label() const = 0;
    virtual size_t bytes() const = 0;
    virtual Rect touched() const = 0;
    // False after the first apply() means it wrote nothing: run() then
    // records no history entry and leaves the document clean.
    virtual bool changed() const { return true; }
};

inline constexpr size_t kMaxHistoryBytes = 256u << 20;
inline constexpr int kMaxHistoryOps = 96;

class MaskDoc {
public:
    // Decodes the base (.base.png, else masks/<key>.png, else all-keep at
    // `w` x `h`) and the layers. A regenerated mask is re-based first. False
    // with `warning` set, not `error`, when a layer file did not fit.
    bool load(const std::string& layer_root, const std::string& mask_root,
              const std::string& key, int w, int h, LayerIndex& idx,
              std::string& error, std::string& warning);

    const std::string& key() const { return _key; }
    int width() const { return _w; }
    int height() const { return _h; }
    BaseState base_state() const { return _state; }
    const std::vector<uint8_t>& base() const { return _base; }
    const std::vector<uint8_t>& drop() const { return _drop; }
    const std::vector<uint8_t>& keep() const { return _keep; }
    const std::vector<uint8_t>& composite() const { return _composite; }
    int64_t kept() const { return _kept; }
    float kept_fraction() const;

    // Every nonzero pixel of `st` inside `bounds`, as one undo step.
    void paint(Paint mode, Stencil st, const Rect& bounds);
    void run(std::unique_ptr<MaskOp> op);
    void undo();
    void redo();
    bool can_undo() const { return _head > 0; }
    bool can_redo() const { return _head < (int)_ops.size(); }
    // The step undo would take back next, as a serial unique to this document;
    // 0 with none. Undo then redo gives it back, where the revision moves on.
    uint64_t top_step() const { return _head > 0 ? _ids[(size_t)_head - 1] : 0; }
    // `step` is on top, or waits on the redo stack: redo alone can bring it back.
    bool redo_reaches(uint64_t step) const;
    void drop_redo();
    size_t history_bytes() const { return _bytes; }
    int history_size() const { return (int)_ops.size(); }
    // Test-only: shrinks the byte budget so eviction is reachable without a
    // 256 MB fixture. Defaults to kMaxHistoryBytes; production never calls it.
    void set_history_byte_cap_for_test(size_t bytes) { _byte_cap = bytes; }
    const spirula::i18n::Msg* last_label() const;
    // What the last paint, undo or redo changed, in stored pixels.
    const Rect& last_change() const { return _last; }

    bool dirty() const { return _revision != _saved; }
    uint64_t revision() const { return _revision; }
    // `composite_written` is what makes a Missing frame's later saves keep
    // writing layers only, and a written one report Unchanged.
    void mark_saved(uint64_t revision, bool composite_written) {
        _saved = revision;
        if (composite_written && _state != BaseState::Missing) _state = BaseState::Unchanged;
    }
    bool save(const std::string& layer_root, const std::string& mask_root,
              LayerIndex& idx, std::string& error);

    // For ops. `drop_r` / `keep_r` are r.w()*r.h(), row-major.
    void read_rect(const Rect& r, std::vector<uint8_t>& drop_r,
                   std::vector<uint8_t>& keep_r) const;
    // Copies verbatim -- no exclusivity check. Both callers hand it an
    // exclusive pair: StrokeOp replays what read_rect captured, propagate_to
    // hands on another document's drop() and keep().
    void write_rect(const Rect& r, const uint8_t* drop_r, const uint8_t* keep_r);
    void paint_rect(Paint mode, const Stencil& st, const Rect& r);

private:
    void recomposite(const Rect& r);

    std::string _key;
    int _w = 0, _h = 0;
    BaseState _state = BaseState::Missing;
    std::vector<uint8_t> _base, _drop, _keep, _composite;
    int64_t _kept = 0;
    Rect _last;
    uint64_t _revision = 0, _saved = 0;
    std::vector<std::unique_ptr<MaskOp>> _ops;
    std::vector<uint64_t> _ids;      // _ops' serials, from _next_id
    uint64_t _next_id = 0;
    int _head = 0;
    size_t _bytes = 0;
    size_t _byte_cap = kMaxHistoryBytes;
};

// ---- propagate -------------------------------------------------------------

struct PropagateRefusal {
    int w = 0, h = 0;   // the target's size, when the sizes differed
};
// Copies `drop` / `keep` (W x H) onto `key` over that frame's OWN base. False
// with `refused` set on a size mismatch, before any write unless the load rebased
// a mask regenerated at a new size; false with `error` (a path) on a failure.
bool propagate_to(const std::string& layer_root, const std::string& mask_root,
                  const std::string& key, const std::string& image_file, int W, int H,
                  const uint8_t* drop, const uint8_t* keep, LayerIndex& idx,
                  PropagateRefusal& refused, std::string& error);

}  // namespace mask
}  // namespace gui
