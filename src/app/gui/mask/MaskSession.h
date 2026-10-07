#pragma once

// The mask editor's session: the frames of a prepared dataset, the frame
// that is open with its document and picture, a worker that loads, saves and
// reverts, the view, and the stroke commit. Drawing and every GL call live in
// MaskPanel.cpp so this file links into mask_doc_test.
// Design: docs/notes/mask-editor.md.

#include "app/gui/GlLoader.h"
#include "app/gui/edit/EditTool.h"
#include "app/gui/mask/MaskAdd.h"
#include "app/gui/mask/MaskDoc.h"
#include "app/gui/mask/MaskSlideshow.h"
#include "app/gui/mask/MaskWindow.h"
#include "app/gui/mask/Livewire.h"
#include "app/gui/mask/PathTool.h"
#include "app/gui/mask/PenTool.h"

#include <cstddef>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace gui {

struct MaskSettings;

namespace mask {

struct FrameRef {
    std::string file;      // the image, absolute
    std::string key;       // frame_key under the image root
    std::string camera;    // the camera folder, "" for the image root
};

class MaskSam;
struct SamResult;

// How close() and the retiring slot see a MaskSam, so a test can stand in for
// a job that is still running. `release` is MaskSam::release: it joins.
struct SamOps {
    std::function<bool(MaskSam&)> busy;
    std::function<bool(MaskSam&)> release;
};

// What a left click on the canvas does. One value rather than a flag per tool,
// so no two can be on at once, whichever picker forgets what.
enum class CanvasMode { Shape, Eraser, Path, Pen, Sam };

// The continuous inverse of to_stored (MaskDoc.h): a stored point to the
// displayed frame.
void to_displayed(const sfm::ExifTransform& t, int W, int H, float sx, float sy,
                  float& dx, float& dy);

// What Tab is showing: nothing, the bare photo (Tab), the bare mask (Shift+Tab).
enum class Peek { None, Photo, Mask };
enum class ViewMode { Overlay, MaskOnly, SideBySide };
enum class PropagateScope { Next, Range, Camera };
// Indices of the frames a propagate from `src` reaches: frames sharing
// its camera key, never `src` itself. `lo`/`hi` are 0-based inclusive and
// only read for Range.
std::vector<int> propagate_targets(const std::vector<FrameRef>& frames, int src,
                                   PropagateScope scope, int lo, int hi);
// Frames a propagate works on at once: about 1 GB of w x h planes in flight
// (six a frame), never more than the cores minus one, never more than 8.
inline int propagate_threads(int w, int h, unsigned hardware_threads) {
    const int cap = std::clamp((int)hardware_threads - 1, 1, 8);
    const size_t one = w > 0 && h > 0 ? (size_t)w * (size_t)h * 6u : 0;
    if (one == 0) return 1;
    return std::clamp((int)std::min<size_t>(((size_t)1 << 30) / one, 8), 1, cap);
}
struct PropagateReport {
    int done = 0, refused = 0, failed = 0;
    int unrestored = 0;                 // failed targets that could not be put back
    std::string refused_key;            // the first refusal
    int refused_w = 0, refused_h = 0;   // its size
    std::string failed_key, failed_path;   // the first failure and the file that failed
    std::string unrestored_key, unrestored_path;   // the first failure not put back
    bool failed_stray = false;          // that failure is a .base.png with no index entry
    int w = 0, h = 0;                   // the source's size
    int skipped = 0;                    // targets a cancel left untouched
    bool undoable = false;
    size_t bytes = 0;                   // of the undo record
};

// What the scan knows about a frame. `kept` is -1 until scanned or when
// the mask is absent.
struct FrameHealth {
    bool scanned = false;
    bool missing_mask = false;
    float kept = -1.0f;
};
// Missing: no mask file, or a kept fraction outside [lo, hi]. Unscanned is
// not missing.
inline bool is_missing(const FrameHealth& h, float lo, float hi) {
    return h.scanned && (h.missing_mask || h.kept < lo || h.kept > hi);
}
// The first missing index strictly after `from` in direction `dir` (+1 or
// -1), or -1.
int next_missing(const std::vector<FrameHealth>& v, int from, int dir, float lo, float hi);
// A band field's edit in whole percent: the edited end is held inside [0, 100]
// and on its own side of the other end, which never moves with it.
void band_edit(int& lo_pct, int& hi_pct, bool lo_edited);

// What pane 0 (left) or 1 (right, side by side only) shows under a peek.
Style pane_style_for(Peek peek, ViewMode view, int pane);
struct PaneDerive { bool left = false, right = false; };
// Which pane windows to derive for `want`, bringing the caches up to date.
// The right pane is forgotten off screen: upload_rect keeps it current only there.
PaneDerive plan_derive(bool dirty, const Window& want, ViewMode view, Peek peek, Window& win0,
                       Style& style0, Window& win1, Style& style1);
// The pane of `panes`, pane_w wide and gap apart, holding canvas x; -1 in a gap or off the end.
int pane_at(float x, int panes, float pane_w, float gap);
inline float pane_left(int pane, float pane_w, float gap) { return (float)pane * (pane_w + gap); }
// `want`, unless a shape is mid-stroke: its points are pane pixels, and the pane
// width is part of the mapping. The pen keeps frame pixels, so it may switch.
ViewMode switch_view(ViewMode cur, ViewMode want, bool shape_in_progress);

class MaskSession {
public:
    MaskSession();
    ~MaskSession();
    MaskSession(const MaskSession&) = delete;
    MaskSession& operator=(const MaskSession&) = delete;

    // `workspace` is where mask_edits/ goes; `mask_flipped` is the convention
    // `mask_dir` is in (TrainConfig::flip_mask). False with `error` (a
    // sentence) on any of the five refusals in open().
    bool open(const std::string& workspace, const std::string& image_dir,
              const std::string& mask_dir, bool mask_flipped, std::string& error);
    // Where a failure that lands after the editor is gone goes -- a save that
    // fails while closing has no status strip left to reach.
    void set_log(std::function<void(const std::string&)> log) { _log = std::move(log); }
    bool is_open() const { return _open; }
    // Saves a dirty frame, hands the SAM checkpoint back (or, mid-job, cancels it
    // into the retiring slot), then joins the worker.
    void close();
    void destroy_gl();
    void draw();

    // ---- read by the panel and the tests ----
    int frame_count() const { return (int)_frames.size(); }
    int frame_index() const { return _idx; }
    const std::vector<FrameRef>& frames() const { return _frames; }
    const std::string& layer_root() const { return _layer_root; }
    const std::string& mask_root() const { return _mask_root; }
    MaskDoc* doc() { return _doc.get(); }
    const MaskDoc* doc() const { return _doc.get(); }
    int shown_width() const { return _dw; }
    int shown_height() const { return _dh; }
    const sfm::ExifTransform& turn() const { return _turn; }
    bool idle() const { return _pending.load() == 0; }
    int corrected_count() const;
    std::string status() const;
    std::string error() const;
    double last_commit_ms() const { return _last_commit_ms; }
    CanvasMode mode() const { return _mode; }
    void set_mode(CanvasMode m) { _mode = m; }
    bool erasing() const { return _mode == CanvasMode::Eraser; }
    bool path_mode() const { return _mode == CanvasMode::Path; }
    bool pen_mode() const { return _mode == CanvasMode::Pen; }
    bool sam_mode() const { return _mode == CanvasMode::Sam; }
    // Off returns to the shapes, which is where every other picker leaves it.
    void set_erasing(bool on) { _mode = on ? CanvasMode::Eraser : CanvasMode::Shape; }
    // Subtract swaps drop and keep for every tool, as the eraser does for the brush.
    bool subtracting() const { return _subtract; }
    void set_subtract(bool on) { _subtract = on; }
    // ONE radius, shared by the brush and the eraser: the operator wants the
    // size to carry when they switch tools mid-correction. A second copy is
    // the defect to avoid here, not a feature to add.
    float radius() const { return _brush; }
    void set_radius(float r) { _brush = clamp_brush(r); }
    View& view() { return _view; }
    WindowSource window_source() const;
    Peek peek() const { return _peek; }
    int peek_total() const { return _peek_total; }
    ViewMode view_mode() const { return _view_mode; }
    void set_view_mode(ViewMode v) { _view_mode = v; }
    // Pane px <-> frame px: the view, the EXIF turn, the mask-to-frame scale.
    PathSpace path_space(const Mapping& m) const;
    // The layout the last drawn frame used: pane origin in screen px and, side
    // by side, the pane count, width and gap. A click maps through the pane it
    // lands in, and nowhere from a gap. False until one is drawn.
    void note_shown(const Mapping& m, float origin_x, float origin_y, int panes = 1,
                    float pane_w = 0.0f, float gap = 0.0f);
    bool shown_to_frame(float screen_x, float screen_y, float& fx, float& fy) const;
    // The pane the pointer maps through this frame: a held button keeps the pane it
    // pressed in, so a drag never jumps the gap; else the pane under it (`hover`, -1
    // for none). Both panes show one view, so a pane pixel is a frame pixel either way.
    int bind_pane(int hover, bool pressed, bool down, int panes);
    // The open frame's pixels, co-owned: a holder keeps them past a frame change.
    std::shared_ptr<const std::vector<uint8_t>> frame_pixels() const { return _rgb; }

    // ---- SAM assist (MaskSam.h); every call is safe with no checkpoint ----
    // The SAM half, created on first use.
    MaskSam& sam();
    bool sam_available() const;
    // The process-wide inference pool, MiB -- readable with the editor closed.
    static double sam_pool_mib();
    // Checkpoint loads in this process (MaskSam::load_count).
    static int sam_loads();
    // GuiApp's checkpoint, every frame; "" = not cached. A NEW path drops the
    // warm session (released once any job stops) and keeps the clicks.
    void set_sam_model(const std::string& path, bool text_prompts);
    const std::string& sam_model_path() const;
    int sam_model_changes() const { return _sam_model_changes; }
    // Draws GuiApp's model picker into the SAM strip; GuiApp owns the state.
    void set_model_picker(std::function<void()> draw) { _model_picker = std::move(draw); }
    bool sam_has_model() const { return !sam_model_path().empty(); }
    bool sam_text_supported() const;
    bool sam_busy() const;
    void sam_cancel();
    // The editor's own prompt state, created on first use; never the dataset's.
    MaskSettings& sam_prompt();
    int sam_click_count() const;
    int sam_object_count() const;
    // Frame pixels of the open frame. False when nothing started.
    bool sam_prompt_point(float frame_x, float frame_y, Paint mode, bool positive = true);
    bool sam_prompt_text(const std::string& phrases);
    // What Enter in the phrase field and the Find button both call.
    bool sam_submit_text();
    // Why Find is disabled, translated; "" when it can run. Blank = only `;` and spaces.
    static bool sam_phrases_blank(const std::string& phrases);
    static std::string sam_text_refusal(bool has_model, bool text, const std::string& blocker,
                                        bool busy, const std::string& phrases);
    std::string sam_text_refused() const;
    // Paints a finished result onto the open frame; the DISPLAYED rect changed.
    Rect sam_pump();
    // The paint half of sam_pump(): a re-prompt of the object whose add is still
    // this frame's newest edit replaces it; anything else adds. -1 = text.
    Rect apply_sam_add(SamResult res, int object);
    // The mode of the current object's add while it is on top; `fallback` otherwise.
    Paint sam_refine_mode(Paint fallback) const;
    // A click, either button: Shift or Ctrl held says the mode outright; a bare
    // click on the object on top keeps its add's mode, else it drops.
    Paint sam_click_mode(bool shift, bool ctrl) const;
    // The margin slider was released: once no job runs, sam_pump() rebuilds the
    // last drop at the editor's margin on the job thread and lands it in place,
    // unless something was edited meanwhile.
    bool sam_margin_reapplies() const;
    void sam_margin_changed() { _sam_margin_pending = true; }
    // The object list was edited (Clear, Clear all): its numbers now name other
    // objects, so no later click may replace the last add.
    void sam_objects_edited();
    // What sam_prompt_point() records once its job starts; public for the stub.
    void sam_prompt_started(float frame_x, float frame_y, bool positive);
    // UI-thread ms of the frame a margin job started and the one it landed on,
    // and the job's own ms.
    double sam_margin_start_ms() const { return _sam_margin_start_ms; }
    double sam_reapply_ms() const { return _sam_reapply_ms; }
    double sam_reapply_job_ms() const { return _sam_reapply_job_ms; }
    // What the held detections take, bytes; 0 once they cannot re-apply.
    size_t sam_held_bytes() const;
    std::string sam_status() const;
    std::string sam_error() const;
    double sam_vram_mib() const;
    // Why another inference user bars SAM here, "" when none does: every
    // sam::Session shares the pool's slots and one unsynchronised stream.
    static std::string sam_blocker(bool mask_preview, bool depth_preview, bool run_active);
    // GuiApp's sam_blocker() answer, every frame before draw(); a blocked
    // prompt is refused with it in sam_error().
    void set_sam_blocker(const std::string& reason);
    // GuiApp's freeze_native_device(), asked before every prompt so SAM loads on
    // the device the app chose: false refuses the prompt with `error`.
    using DeviceGate = std::function<bool(std::string& device, std::string& error)>;
    void set_sam_device_gate(DeviceGate gate) { _sam_device_gate = std::move(gate); }
    // Cancels, joins and unloads, before another inference user starts; drains
    // the retiring slot too. Keeps the clicks. Returns the milliseconds joined.
    double sam_yield();
    // A job still running when the editor closed waits here, cancelled, so the
    // UI thread never joins it: poll() releases it once idle, every frame;
    // drain() releases it now, before anything else touches the device.
    bool sam_retiring() const { return (bool)_sam_retiring; }
    void sam_poll_retiring();
    void sam_drain_retiring();
    double sam_retire_ms() const { return _sam_retire_ms; }
    void set_sam_ops(SamOps ops) { _sam_ops = std::move(ops); }
    // What a job is stamped with and a result must still match: the frame's
    // key AND the document generation, since a revert reopens the same key.
    std::string sam_frame_stamp() const;
    // `last_ms` is prompt -> painted, stamped on this thread; `last_job_ms` is
    // the job's own time. Both cover a result that painted nothing.
    int sam_results() const { return _sam_results; }
    int sam_dropped() const { return _sam_dropped; }
    double sam_last_ms() const { return _sam_last_ms; }
    double sam_last_job_ms() const { return _sam_last_job_ms; }
    float sam_last_score() const { return _sam_last_score; }
    // The stencil's pixel count, NOT the pixels that changed: a paint over
    // pixels already dropped reports its full area and changes nothing.
    int64_t sam_last_area() const { return _sam_last_area; }
    int sam_last_detections() const { return _sam_last_detections; }
    // Why the last result painted nothing, for the strip; null when it painted.
    const spirula::i18n::Msg* sam_empty_note() const;
    // UI-thread ms the last close() spent on SAM; 0 when it had none.
    double sam_close_ms() const { return _sam_close_ms; }
    // UI-thread ms of the last result's paint and upload; the frame pixel the
    // last point prompt was sent at; the canvas height drawn last frame.
    double sam_ui_ms() const { return _sam_ui_ms; }
    float sam_click_x() const { return _sam_click_x; }
    float sam_click_y() const { return _sam_click_y; }
    float canvas_height() const { return _canvas_h; }
    int path_anchors() const { return _path.anchor_count(); }
    int pen_anchors() const { return _pen.anchor_count(); }
    // The editor's own drop margin (its MaskSettings), -1 before SAM was used.
    float sam_margin() const;

    // ---- actions ----
    void go_to(int i);
    void pump();
    // A finished stroke in pane pixels under `m`: mapped, rasterised, painted.
    // Returns the DISPLAYED rectangle that changed; empty when nothing did.
    Rect commit_stroke(const ShapeStroke& pane_stroke, Paint mode, const Mapping& m);
    // The mode a stroke commits with, from the modifiers on the frame it
    // completes and the tool it was drawn with.
    static Paint paint_for(bool shift, bool ctrl, bool erasing);
    Paint paint_now(bool shift, bool ctrl) const {
        return paint_for(shift, ctrl, erasing() != _subtract);
    }
    // The radius arithmetic, all of it, in mask pixels. clamp_brush is the
    // one place [kMinBrush, kMaxBrush] is enforced -- and it folds NaN to the
    // minimum, which std::clamp would propagate instead.
    static constexpr float kMinBrush = 1.0f;
    static constexpr float kMaxBrush = 4096.0f;
    static float clamp_brush(float r);
    static float scale_brush(float r, float factor);
    // `[`/`]`: `grow` true widens by 1.18x, false narrows by 0.85x.
    static float step_brush(float r, bool grow);
    // Alt+wheel, one notch per `]`, reciprocal so a notch back undoes it.
    static float wheel_brush(float r, float wheel);
    Rect undo();
    Rect redo();
    // SAM mode's undo: the object just clicked loses its last click and is
    // prompted again with the rest, in place; its only click goes with the add.
    // False when the top step is not that object's add.
    bool sam_undo_click(Rect& changed);
    Rect undo_step();
    void save();
    void revert_open_frame();
    void revert_every_frame();

    // ---- propagate ----
    // Saves the open frame, then copies its two layers onto the targets on
    // the worker. `lo`/`hi` are 0-based inclusive and only read for Range.
    void propagate(PropagateScope scope, int lo, int hi);
    // Offered while the open frame is still the propagate's source.
    bool can_undo_propagate() const;
    void undo_propagate();
    PropagateReport last_propagate() const;
    // Frames handled and in all by the propagate (or undo) now running; total 0 when none is.
    int propagate_done() const { return _prop_done.load(); }
    int propagate_total() const { return _prop_total.load(); }
    // Stops a running propagate or undo after the frames in flight; what was
    // done stays done and undoable.
    void cancel_propagate() { _prop_cancel = true; }
    bool propagate_cancelling() const { return _prop_cancel.load() && _prop_total.load() > 0; }

    // ---- find missing ----
    float band_lo() const { return _band_lo; }
    float band_hi() const { return _band_hi; }
    // Ordered here, not only in the panel: lo above hi makes is_missing
    // true for every scanned frame, and the API has other callers.
    void set_band(float lo, float hi) {
        _band_lo = lo < hi ? lo : hi;
        _band_hi = lo < hi ? hi : lo;
    }
    FrameHealth health(int i) const;
    int scanned_count() const;
    int missing_count() const;
    // A scan thread was started and has not been joined. is_open() cannot
    // answer this: close() clears _open whether or not it stopped the scan.
    bool scan_running() const;
    // Wall time of the last completed scan, ms; -1 before one completes.
    double scan_ms() const;
    // Loads the next (+1) or previous (-1) missing frame; false, with a
    // status line, when there is none that way.
    bool go_to_missing(int dir);
    // Test-only, set before open() and read without a lock. The scan's runs
    // before a frame's read (false) and between read and publication (true);
    // the worker's inside a job, before (false) and after (true) it runs.
    void set_scan_hook_for_test(std::function<void(int, bool)> hook) { _scan_hook = std::move(hook); }
    void set_worker_hook_for_test(std::function<void(bool)> hook) { _worker_hook = std::move(hook); }
    // Test-only, as MaskDoc::set_history_byte_cap_for_test is:
    // a record of never-edited targets is 0 bytes, so 256 MB is unreachable.
    void set_propagate_byte_cap_for_test(size_t bytes) { _prop_byte_cap = bytes; }
    PathTool& path_for_test() { return _path; }
    PenTool& pen_for_test() { return _pen; }
    // A SAM job runs, its result waits for sam_pump(), or a margin re-apply
    // waits to start: propagate and Play wait for it, since Play
    // drops a waiting result. The job is asked through SamOps, as close() does.
    bool sam_work_pending() const;

    // ---- slideshow ----
    bool slideshow_playing() const { return _slide_playing; }
    // The press frame is exempt from "any input stops it".
    bool slide_fresh() const { return _slide_fresh; }
    // For GuiApp::animating(): frames must keep coming while it plays.
    bool animating() const { return _slide_playing; }
    float slide_fps() const { return _slide_fps; }
    void set_slide_fps(float f) { _slide_fps = std::clamp(f, 5.0f, 30.0f); }
    // Refused while SAM work, the worker or a pen path is pending (a half-drawn
    // Polygon is the panel's gate: EditTool needs ImGui); otherwise releases
    // the SAM session and the document (saving a dirty frame).
    void start_slideshow();
    // Stops the pool and loads the frame on screen, never one still decoding.
    void stop_slideshow();
    int slide_index() const { return _slide_index; }
    double slide_shown_fps() const;
    double slide_max_gap_ms() const { return _slide_max_gap; }
    // Decodes since play began: one a shown frame is the window's whole point.
    int slide_decoded() const { return _slide.decoded(); }
    bool slide_pool_running() const { return _slide.running(); }
    // The depth the last window was asked for: slide_depth's, never a count.
    int slide_window() const { return _slide_window; }
    // The decoders the last Play started: slide_threads' byte budget.
    int slide_threads() const { return _slide_threads; }
    // How long the last stop blocked the UI thread, ms; -1 before a stop.
    double slide_stop_ms() const { return _slide_stop_ms; }
    // How long the last join of decode threads blocked the UI thread, ms: the
    // one in start_slideshow() (a halted playback's leftovers) or close(). -1 before.
    double slide_join_ms() const { return _slide_join_ms; }
    // The frame slider's value; follows the frame shown while playing.
    int slider_index() const { return _slider_idx; }
    // True when the next frame is due and its picture is in `pic`; an empty
    // picture is an undecodable frame, counted and skipped. `now` is seconds.
    bool slideshow_tick(double now, int target_side, Picture& pic);

private:
    struct Loaded {
        std::unique_ptr<MaskDoc> doc;
        std::vector<uint8_t> rgb;
        int fw = 0, fh = 0;
        sfm::ExifTransform turn;
        int index = -1;
    };
    void enqueue(std::function<void()> job);
    void worker_main();
    void load_frame(int i);
    void post_status(const std::string& s, bool error);
    // `sticky` marks a failed write, which a later successful load must not
    // clear: the work it lost is still lost.
    void post_error(const std::string& s, bool sticky);
    void set_corrected(int n);
    void start_scan();
    void stop_scan();
    void scan_main();
    void refresh_health(const std::vector<std::string>& keys);
    void note_health(int i, const std::string& key);   // one frame just written, any thread
    Rect shown_rect(const Rect& stored) const;
    bool sam_add_on_top(int object) const;
    bool sam_add_redoable() const;
    void close_sam();
    void sam_forget();
    void forget_workflow();          // propagate, find and slideshow state
    void drop_propagate_record();    // guarded by _mu
    // A revert discards the frame's (or, at -1, every frame's) clicks and last add.
    void sam_revert(int frame);
    void sam_forget_clicks(const std::vector<int>& frames);
    Rect sam_land(SamResult res);
    void sam_start_margin();
    bool sam_gate_passes();   // the blocker, then the device gate
    // MaskPanel.cpp
    // How a tool is chosen, so the toolbar and the key handler cannot drift
    // apart over what else a switch cancels. The mode itself is one value.
    void pick_tool(ToolId t);
    void pick_eraser();
    void pick_path();
    void pick_pen();
    void pick_sam();
    void draw_toolbar();
    void draw_canvas();
    void draw_status();
    void draw_sam_status();
    void draw_sam_objects();
    void draw_sam_text();
    void note_sam_ui(const int before[3], double ms);
    void draw_sam_clicks(ImDrawList* dl, const Mapping& m, float ox, float oy);
    void draw_revert_all_modal();
    void handle_keys(const Mapping& m);
    void ensure_window(const Mapping& m, float pane_w, float pane_h);
    void upload_rect(const Rect& shown);
    void draw_workflow_row();                                     // MaskPanel.cpp
    // A shape or pen path is half drawn; changing frames would drop it.
    bool shape_open() const;                                      // MaskPanel.cpp
    void note_row_width();                                        // MaskPanel.cpp
    void upload_window(GLuint& tex, const Window& win, Style style, std::vector<uint8_t>& rgba);
    void upload_rect_to(GLuint tex, const Window& win, Style style, std::vector<uint8_t>& rgba,
                        const Rect& shown);
    // The pen tool (MaskPanel.cpp drives it; this and path_space have no ImGui).
    void ensure_livewire();
    // MaskPanel.cpp: the picture on screen while playing; stops on any input.
    void draw_slideshow(ImDrawList* dl, float ox, float oy, float w, float h);

    bool _open = false;
    // Set once in open() before the worker starts, read by both threads
    // thereafter, cleared only after close()'s join(): safe by ordering,
    // not by exclusivity -- neither field is ever mutable mid-session.
    std::string _workspace, _image_root, _mask_root, _layer_root;
    bool _mask_flipped = false;      // mask_root's 255 is drop
    std::vector<FrameRef> _frames;
    int _idx = -1;

    // The open frame, UI thread.
    std::unique_ptr<MaskDoc> _doc;
    // Co-owned with any SAM job still reading it, so replacing or dropping it
    // here never frees what a job holds. const: nothing may refill it in place.
    std::shared_ptr<const std::vector<uint8_t>> _rgb;
    int _fw = 0, _fh = 0;
    sfm::ExifTransform _turn;
    int _dw = 0, _dh = 0;
    View _view;
    EditTool _tool;
    // The tool is only ever touched from MaskPanel.cpp (EditTool.cpp needs
    // imgui); a frame change asks it to reset through this flag.
    bool _tool_reset = false;
    float _brush = 24.0f;            // mask pixels, the brush's AND the eraser's
    CanvasMode _mode = CanvasMode::Sam;
    bool _subtract = false;
    bool _panning = false;
    double _last_commit_ms = 0.0;

    PathTool _path;
    PenTool _pen;
    std::unique_ptr<Livewire> _livewire;   // the open frame's edge map, built on first use
    // Created on first use; its session is released by sam_yield() and a model
    // change, and close() releases it and then drops the object, clicks included.
    std::unique_ptr<MaskSam> _sam;
    std::unique_ptr<MaskSam> _sam_retiring;
    SamOps _sam_ops;
    double _sam_retire_ms = 0.0;
    std::string _sam_model;
    bool _sam_text_hint = false;
    bool _sam_release_pending = false;   // a model change waiting for the job to stop
    int _sam_model_changes = 0;
    std::function<void()> _model_picker;
    std::chrono::steady_clock::time_point _sam_t0{};
    int _sam_results = 0, _sam_dropped = 0, _sam_last_detections = 0;
    double _sam_last_ms = 0.0, _sam_last_job_ms = 0.0;
    float _sam_last_score = 0.0f;
    int64_t _sam_last_area = 0;
    bool _sam_last_vetoed = false;
    double _sam_close_ms = 0.0;
    double _sam_ui_ms = 0.0;
    float _sam_click_x = -1.0f, _sam_click_y = -1.0f;
    float _canvas_h = 0.0f;
    // The last SAM add: the document stamp and step it left, its object and
    // mode, and its detections while it is a re-appliable drop.
    int _sam_job_object = -1;        // the running job's object; -1 for text
    std::string _sam_add_key;
    uint64_t _sam_add_step = 0;       // MaskDoc::top_step() after the add
    int _sam_add_object = -1;
    Paint _sam_add_mode = Paint::ForceDrop;
    std::vector<HeldRegion> _sam_held;
    double _sam_reapply_ms = 0.0, _sam_reapply_job_ms = 0.0, _sam_margin_start_ms = 0.0;
    int _sam_reapplies = 0, _sam_margin_starts = 0;
    bool _sam_margin_pending = false;
    bool _sam_margin_moved = false;  // MaskPanel.cpp: re-apply once the slider lets go
    int _sam_objects_drawn = 0;      // MaskPanel.cpp: the object count the list last drew
    int _sam_scroll_frames = 0;      // frames left to hold that list at its end
    Mapping _shown;
    float _shown_x = 0.0f, _shown_y = 0.0f;
    int _shown_panes = 1;
    float _shown_pane_w = 0.0f, _shown_gap = 0.0f;
    bool _shown_valid = false;       // cleared wherever a new document arrives
    StripReserve _strip;
    std::string _sam_blocker;
    DeviceGate _sam_device_gate;
    uint64_t _doc_gen = 0;           // bumped where pump() installs a _doc; never reset
    double _livewire_ms = 0.0;

    // What draw_status() took last frame, so draw_canvas() can reserve it
    // instead of a constant. 0 until the first frame has been drawn.
    float _status_h = 0.0f;
    bool _popup_at_start = false;    // a popup was open when this frame began
    float _toolbar_w = 0.0f;         // the tool row's width last frame, window px

    // MaskPanel.cpp's texture and window.
    GLuint _tex = 0;
    Window _win;
    bool _win_dirty = true;
    std::vector<uint8_t> _rgba;
    Style _win_style = Style::Overlay;   // what _rgba / _tex were derived with
    Peek _peek = Peek::None;
    int _peek_total = 0;
    ViewMode _view_mode = ViewMode::Overlay;
    // The right pane of side by side: its own window and texture over the
    // same view. Same size as the left one, so one Mapping serves both.
    GLuint _tex2 = 0;
    Window _win2;
    Style _win2_style = Style::MaskOnly;
    std::vector<uint8_t> _rgba2;
    int _held_pane = -1;             // the pane a held left button pressed in
    int _prop_scope = 0;             // 0 next, 1 range, 2 camera
    int _prop_from = 1, _prop_to = 1;   // 1-based, as the status strip counts
    int _slider_idx = 0;
    bool _close_requested = false;
    bool _revert_all_ask = false;    // Revert all was clicked; open its confirmation

    SlidePrefetch _slide;
    SlideClock _slide_clock;
    bool _slide_playing = false;
    bool _slide_need = false;        // waiting for the picture of _slide_pending
    int _slide_index = -1;           // the frame on screen: what a stop opens
    int _slide_pending = -1;         // the frame last asked for; ahead while it decodes
    bool _slide_fresh = false;       // MaskPanel.cpp: the frame Play was pressed on
    float _slide_fps = 10.0f;
    double _slide_started = 0.0, _slide_last_shown = 0.0, _slide_now = 0.0;
    bool _slide_first = true;        // no tick has run since start_slideshow
    double _slide_max_gap = 0.0;
    double _slide_stop_ms = -1.0;    // -1 = no stop measured yet
    double _slide_join_ms = -1.0;    // -1 = no join measured yet
    int _slide_shown = 0;
    int _slide_window = 0;
    // The last picture's source size: the window's depth comes from the bytes
    // a picture really costs at the current target.
    int _slide_src_w = 0, _slide_src_h = 0;
    Picture _slide_pic;              // the frame on screen; its buffer is the ring's to swap
    int _slide_threads = 0;
    GLuint _slide_tex = 0;
    int _slide_tex_w = 0, _slide_tex_h = 0;

    // The worker and what it hands back.
    std::thread _worker;
    std::mutex _qmu;
    std::condition_variable _qcv;
    std::deque<std::function<void()>> _queue;
    bool _quit = false;
    std::atomic<int> _pending{0};
    LayerIndex _index;               // worker thread only, after open()
    mutable std::mutex _mu;
    Loaded _loaded;                  // guarded by _mu
    bool _loaded_ready = false;      // guarded by _mu
    std::string _saved_key;          // guarded by _mu
    uint64_t _saved_rev = 0;         // guarded by _mu
    bool _saved_comp = false;        // guarded by _mu
    bool _saved_ready = false;       // guarded by _mu
    struct PropagateRecord {
        std::string source_key;
        std::vector<LayerSnapshot> targets;
        size_t bytes = 0;
    };
    PropagateRecord _prop;             // guarded by _mu
    bool _prop_undoable = false;       // guarded by _mu
    PropagateReport _prop_report;      // guarded by _mu
    std::thread _scan;
    std::atomic<bool> _scan_stop{false};
    bool _scan_running = false;         // guarded by _mu
    std::vector<FrameHealth> _health;   // guarded by _mu
    int _scanned = 0;                   // guarded by _mu
    double _scan_ms = -1.0;             // guarded by _mu
    // UI thread only. The scan never reads the band; missing_count() holds
    // _mu for _health and reads these two outside anything that guards them.
    float _band_lo = 0.05f, _band_hi = 0.98f;
    // Odd while a worker job runs. The worker is the only writer of masks, so a
    // scan read that saw this unchanged and even saw no mask change under it.
    std::atomic<uint64_t> _job_seq{0};
    int _nav = -1;                      // UI thread: the frame last asked for
    std::function<void(int, bool)> _scan_hook;
    std::function<void(bool)> _worker_hook;
    size_t _prop_byte_cap = kMaxHistoryBytes;   // UI thread; copied into the job
    // A running propagate or its undo, for the bar: targets handled, of total.
    std::atomic<int> _prop_done{0}, _prop_total{0};
    std::atomic<bool> _prop_cancel{false};
    std::string _prop_stopped;       // guarded by _mu: the status of a run a cancel cut short
    std::string _status, _error;     // guarded by _mu
    bool _error_sticky = false;      // guarded by _mu
    int _corrected = 0;              // guarded by _mu
    std::function<void(const std::string&)> _log;
};

}  // namespace mask
}  // namespace gui
