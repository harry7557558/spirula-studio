#pragma once

// The correction layer on disk: per edited frame a byte copy of the mask the
// run wrote (.base.png), a forced-drop and a forced-keep stencil, composed as
// keep ? 255 : drop ? 0 : base into masks/. An index of FNV-1a fingerprints
// tells a regenerated mask from the composite written last time. In memory
// polarity is the app's, 255 = keep; on disk it is the mask folder's, which
// LayerIndex::mask_flipped records. No ImGui, no GL.
// Design: docs/notes/mask-editor.md.

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace gui {
namespace mask {

inline constexpr const char* kLayerDirName = "mask_edits";
inline constexpr const char* kIndexFileName = "index.json";

uint64_t fnv1a64(const uint8_t* p, size_t n);
std::string fnv_hex(uint64_t h);
bool fnv_parse(const std::string& s, uint64_t& out);

bool read_file(const std::string& path, std::vector<uint8_t>& out);

// `dir` lexically normalized, without a trailing separator: what the frame
// keys and the skip test are computed against.
std::string normalize_dir(const std::string& dir);

// "<rel_dir>/<stem>" of `file` under `image_root` ("cam0/00023", or "00023"
// at the root itself). Lexical, as group_frames_by_camera keys it.
std::string frame_key(const std::string& image_root, const std::string& file);

using MaskPaths = std::map<std::string, std::string>;   // frame key -> path under mask_root
std::string mask_file(const std::string& mask_root, const std::string& key,
                      const MaskPaths& paths = {});

enum class Layer { Base, Drop, Keep };
std::string layer_file(const std::string& layer_root, const std::string& key,
                       Layer l);

// In place, over the 0/255 load_stencil emits: the mask folder's convention
// to the app's, and back. Its own inverse.
void flip_polarity(uint8_t* px, size_t n);

// final = keep ? 255 : drop ? 0 : base. Either layer may be null.
void composite(const uint8_t* base, const uint8_t* drop, const uint8_t* keep,
               size_t n, uint8_t* out);

bool encode_gray_png(const uint8_t* px, int w, int h, std::vector<uint8_t>& png);
bool fingerprint_file(const std::string& path, uint64_t& out);
std::string utc_now_iso();

struct IndexEntry {
    uint64_t base_fp = 0;        // of .base.png; 0 while no base exists
    uint64_t composite_fp = 0;   // of the mask last written; 0 if never
    float kept = 0.0f;
    std::string saved_at;        // UTC, "2026-09-21T10:00:00Z"
};

struct LayerIndex {
    std::string mask_root;
    // Does mask_root hold 255 = REMOVE (TrainConfig::flip_mask)? Every read of
    // a file under mask_root, and every write into it, goes through it.
    bool mask_flipped = false;
    std::map<std::string, IndexEntry> frames;
    MaskPaths mask_paths;
    // save() writes nothing: one worker's copy of a frame's entry, merged later.
    bool in_memory = false;
    // A missing file is an empty index and succeeds; a corrupt one fails.
    bool load(const std::string& layer_root, std::string& error);
    bool save(const std::string& layer_root, std::string& error) const;
};

// Unedited: no entry. Unchanged: the mask is the composite written last.
// Regenerated: something else wrote it since. Missing: no mask on disk.
enum class BaseState { Unedited, Unchanged, Regenerated, Missing };
BaseState base_state(const std::string& mask_root, const std::string& key,
                     const LayerIndex& idx);

struct FrameLayers {
    int w = 0, h = 0;
    std::vector<uint8_t> drop, keep;   // w*h each, 0/255
};
// Absent layer files read as all zero. A file of another size reads as zero
// too, names itself in `warning`, and makes this return false.
bool read_layers(const std::string& layer_root, const std::string& key, int w,
                 int h, FrameLayers& out, std::string& warning);   // warning: ", "-joined paths

// The first save preserves the original mask's bytes, including its polarity.
bool save_frame(const std::string& layer_root, const std::string& mask_root,
                const std::string& key, int w, int h, const uint8_t* base,
                const uint8_t* drop, const uint8_t* keep, bool write_composite,
                LayerIndex& idx, std::string& error);

// A regenerated mask becomes the new base and the layers are re-applied
// over it. `found` says what was there; only Regenerated writes anything.
bool recomposite_frame(const std::string& layer_root, const std::string& mask_root,
                       const std::string& key, LayerIndex& idx, BaseState& found,
                       std::string& error);
// Over every entry of the index under `layer_root`. A failing frame does not
// stop the rest: returns the rebased count, with any per-frame failures
// named in `error`. -1 only if the index itself could not be loaded.
int recomposite_all(const std::string& layer_root, std::string& error);

// Without a base, only the layers and the index entry are removed.
bool revert_frame(const std::string& layer_root, const std::string& mask_root,
                  const std::string& key, LayerIndex& idx, std::string& error);
// A failing frame does not stop the rest: returns the count reverted, with
// any per-frame failures named in `error`. -1 only if the index would not
// load.
int revert_all(const std::string& layer_root, std::string& error);

// ---- propagate -------------------------------------------------------------

// The size a frame's correction has to be: its .base.png, else its mask,
// else its image, which is the order MaskDoc::load resolves the base in.
// `from` is the file the size came from, or the one that failed.
bool frame_size(const std::string& layer_root, const std::string& mask_root,
                const std::string& key, const std::string& image_file, int& w, int& h,
                std::string& from, const MaskPaths& paths = {});

// What a target's layer files and entry were before a propagate, so it can
// be put back. Absent files are recorded as absent.
struct LayerSnapshot {
    std::string key;
    bool had_entry = false;
    IndexEntry entry;
    bool had_drop = false, had_keep = false;
    std::vector<uint8_t> drop_png, keep_png;
    size_t bytes() const { return drop_png.size() + keep_png.size(); }
};
// False with `error` set to the layer file whose read failed.
bool snapshot_layers(const std::string& layer_root, const std::string& key,
                     const LayerIndex& idx, LayerSnapshot& out, std::string& error);
// Writes the snapshot's layers back (or removes them), re-derives the composite
// over the frame's base, restores the entry; a frame with no entry is reverted.
// A failure leaves the mask agreeing with the layers and entry, bar a second one.
bool restore_layers(const std::string& layer_root, const std::string& mask_root,
                    const LayerSnapshot& snap, LayerIndex& idx, std::string& error);

// ---- kept fractions -------------------------------------------------

inline constexpr const char* kKeptFileName = "kept.json";

// The kept fraction of every mask seen, keyed by the mask file's fingerprint
// so a second open decodes nothing. Kept apart from index.json: an entry
// there means "corrected", and these frames are not.
struct KeptEntry {
    uint64_t fp = 0;
    bool flipped = false;   // the convention `kept` was counted under
    float kept = 0.0f;      // in the app's convention, 255 = keep
};
struct KeptCache {
    std::map<std::string, KeptEntry> frames;
    bool dirty = false;
    // Both refuse a relative or empty root rather than use the working folder.
    bool load(const std::string& layer_root, std::string& error);
    // Writes only when dirty, so an unchanged dataset is not rewritten.
    bool save(const std::string& layer_root, std::string& error) const;
};
// From the cache when the fingerprint and `flipped` (the folder's 255 is
// drop) match, else decoded and cached. False with kept = -1 when the mask is
// absent, unreadable or not a regular file.
bool kept_fraction_of(const std::string& mask_root, const std::string& key, bool flipped,
                      KeptCache& cache, float& kept, const MaskPaths& paths = {});

}  // namespace mask
}  // namespace gui
