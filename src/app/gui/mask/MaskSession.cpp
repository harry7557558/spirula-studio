// MaskSession.cpp -- see MaskSession.h.

#include "app/gui/mask/MaskSession.h"
#include "app/gui/mask/MaskSam.h"
#include "app/gui/MaskSettings.h"

#include "app/FrameLook.h"
#include "app/FrameMask.h"
#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/MaskEdit.h"

#include <algorithm>
#include <iterator>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <map>

namespace fs = std::filesystem;
namespace msg = spirula::i18n::msg::maskedit;

namespace gui {
namespace mask {

namespace {

// Lexical, as DatasetPrep's own test is: does `p` sit inside `root`?
bool inside(const fs::path& p, const fs::path& root) {
    const fs::path rel = p.lexically_relative(root);
    return !rel.empty() && *rel.begin() != "..";
}

// Undoes read_layers' own ", "-joined `warning` (MaskLayer.h) so each
// mismatched file can be reported with ITS OWN size, not the first one's.
std::vector<std::string> split_paths(const std::string& joined) {
    std::vector<std::string> out;
    size_t start = 0;
    for (;;) {
        const size_t sep = joined.find(", ", start);
        out.push_back(joined.substr(start, sep - start));
        if (sep == std::string::npos) return out;
        start = sep + 2;
    }
}

// One sentence per mismatched file, each with its own size: a joined path list
// with one file's dimensions used to name every file it is not is worse than
// either a single file or an honest omission.
std::string size_mismatch_text(const std::string& joined, const MaskDoc& doc) {
    std::string text;
    for (const std::string& path : split_paths(joined)) {
        int lw = 0, lh = 0;
        app::image_size(path, lw, lh);
        if (!text.empty()) text += " ";
        text += spirula::i18n::format(msg::err_size_mismatch,
                                      {path, lw, lh, doc.width(), doc.height()});
    }
    return text;
}

// fn(i) once for each i in [0, n), lowest first, on up to `threads` threads;
// none starts after `stop` is set.
void run_pool(int n, int threads, const std::atomic<bool>& stop,
              const std::function<void(int)>& fn) {
    std::atomic<int> next{0};
    auto body = [&] {
        while (!stop.load()) {
            const int i = next.fetch_add(1);
            if (i >= n) return;
            fn(i);
        }
    };
    std::vector<std::thread> pool;
    for (int t = 1; t < std::min(threads, n); t++) pool.emplace_back(body);
    body();
    for (std::thread& t : pool) t.join();
}

// An index a pool shares: each frame works on a copy holding only its own
// entry, merged back under the lock; the file is written at most once a second.
struct SharedIndex {
    LayerIndex& idx;
    const std::string& root;
    std::mutex mu;
    std::chrono::steady_clock::time_point saved = std::chrono::steady_clock::now();

    LayerIndex copy(const std::string& key) {
        LayerIndex x;
        x.in_memory = true;
        std::lock_guard<std::mutex> lk(mu);
        x.mask_root = idx.mask_root;
        x.mask_flipped = idx.mask_flipped;
        const auto it = idx.frames.find(key);
        if (it != idx.frames.end()) x.frames[key] = it->second;
        return x;
    }
    void merge(const std::string& key, const LayerIndex& x) {
        std::lock_guard<std::mutex> lk(mu);
        const auto it = x.frames.find(key);
        if (it != x.frames.end()) idx.frames[key] = it->second;
        else idx.frames.erase(key);
        const auto now = std::chrono::steady_clock::now();
        if (now - saved < std::chrono::seconds(1)) return;
        saved = now;
        std::string ignored;   // the save after the pool reports
        idx.save(root, ignored);
    }
};

}  // namespace

std::vector<int> propagate_targets(const std::vector<FrameRef>& frames, int src,
                                   PropagateScope scope, int lo, int hi) {
    std::vector<int> out;
    const int n = (int)frames.size();
    if (src < 0 || src >= n) return out;
    const std::string& cam = frames[(size_t)src].camera;
    int a = 0, b = n - 1;
    if (scope == PropagateScope::Next) a = b = src + 1;
    if (scope == PropagateScope::Range) {
        a = std::max(lo, 0);
        b = std::min(hi, n - 1);
    }
    for (int i = a; i <= b && i < n; i++)
        if (i != src && i >= 0 && frames[(size_t)i].camera == cam) out.push_back(i);
    return out;
}

int next_missing(const std::vector<FrameHealth>& v, int from, int dir, float lo, float hi) {
    const int n = (int)v.size();
    for (int i = from + dir; i >= 0 && i < n; i += dir)
        if (is_missing(v[(size_t)i], lo, hi)) return i;
    return -1;
}

void band_edit(int& lo_pct, int& hi_pct, bool lo_edited) {
    if (lo_edited) lo_pct = std::clamp(lo_pct, 0, std::clamp(hi_pct, 0, 100));
    else hi_pct = std::clamp(hi_pct, std::clamp(lo_pct, 0, 100), 100);
}

MaskSession::MaskSession()
    : _sam_ops{[](MaskSam& s) { return s.busy(); }, [](MaskSam& s) { return s.release(); }} {}

// stop_scan() again: a close() that skipped it would otherwise end the process
// here on a joinable thread, hiding which check saw that.
MaskSession::~MaskSession() {
    close();
    stop_scan();
    sam_drain_retiring();
}

bool MaskSession::open(const std::string& workspace, const std::string& image_dir,
                       const std::string& mask_dir, bool mask_flipped,
                       std::string& error) {
    close();
    sam_drain_retiring();
    std::error_code ec;
    _workspace = normalize_dir(fs::absolute(workspace, ec).string());
    _image_root = normalize_dir(fs::absolute(image_dir, ec).string());
    _mask_root = normalize_dir(fs::absolute(mask_dir, ec).string());
    _layer_root = (fs::path(_workspace) / kLayerDirName).string();
    if (_workspace == _image_root || inside(fs::path(_workspace), fs::path(_image_root))) {
        error = msg::err_workspace_inside_images.get();
        return false;
    }
    _frames.clear();
    const auto groups = app::group_frames_by_camera(_image_root, _mask_root);
    for (const auto& [camera, files] : groups)
        for (const std::string& f : files)
            _frames.push_back({f, frame_key(_image_root, f), camera});
    if (_frames.empty()) {
        error = spirula::i18n::format(msg::err_no_frames, {_image_root});
        return false;
    }
    // On this thread, before the worker: a corrupt index must refuse the
    // session, and the recorded root and convention are what every .base.png
    // means -- adopting different ones reinterprets all of them.
    LayerIndex idx;
    if (!idx.load(_layer_root, error)) {
        error = spirula::i18n::format(msg::err_read, {error});
        return false;
    }
    if (!idx.frames.empty()) {
        if (!idx.mask_root.empty() && idx.mask_root != _mask_root) {
            error = spirula::i18n::format(msg::err_other_mask_root, {idx.mask_root});
            return false;
        }
        if (idx.mask_flipped != mask_flipped) {
            error = msg::err_other_mask_polarity.get();
            return false;
        }
    }
    idx.mask_root = _mask_root;
    idx.mask_flipped = mask_flipped;
    _mask_flipped = mask_flipped;
    _index = std::move(idx);
    _idx = -1;
    _doc.reset();
    _rgb.reset();
    _win_dirty = true;
    _close_requested = false;
    _tool_reset = true;
    {
        std::lock_guard<std::mutex> lk(_mu);
        _loaded = Loaded{};
        _loaded_ready = false;
        _saved_ready = false;
        _status = msg::working.get();
        _error.clear();
        _error_sticky = false;
        _corrected = (int)_index.frames.size();
    }
    forget_workflow();
    {
        std::lock_guard<std::mutex> lk(_mu);
        _health.assign(_frames.size(), FrameHealth{});
        _scanned = 0;
    }
    _mode = CanvasMode::Sam;   // every open starts on click masking
    _quit = false;
    _worker = std::thread([this] { worker_main(); });
    _open = true;
    load_frame(0);
    start_scan();
    return true;
}

void MaskSession::close() {
    if (!_open && !_worker.joinable()) return;
    stop_scan();
    {
        // Also joins the threads a halted slideshow left finishing.
        const auto t0 = std::chrono::steady_clock::now();
        _slide.stop();
        _slide_join_ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    }
    _slide_playing = false;
    _slide_pic = Picture{};
    // A propagate stops after the frames in flight rather than holding the
    // window open for the rest; the log says how far it got.
    const bool propagating = _prop_total.load() > 0;
    _prop_cancel = true;
    if (_doc && _doc->dirty()) save();
    close_sam();   // order vs the worker join is free: the save never touches SAM
    {
        std::lock_guard<std::mutex> lk(_qmu);
        _quit = true;
    }
    _qcv.notify_all();
    if (_worker.joinable()) _worker.join();
    // The status strip is gone by now, so a write that failed on the way out
    // has nowhere else to be seen.
    if (_log) {
        std::string lost, stopped;
        {
            std::lock_guard<std::mutex> lk(_mu);
            if (_error_sticky) lost = _error;
            if (propagating) stopped = _prop_stopped;
        }
        if (!stopped.empty()) _log(stopped);
        if (!lost.empty()) _log(lost);
    }
    _quit = false;
    _open = false;
    _doc.reset();
    _rgb.reset();
    sam_forget();
    forget_workflow();
    _frames.clear();
    _idx = -1;
    _workspace.clear();
    _image_root.clear();
    _mask_root.clear();
    _layer_root.clear();
    _mask_flipped = false;
    std::lock_guard<std::mutex> lk(_mu);
    _loaded = Loaded{};
    _loaded_ready = false;
    _saved_ready = false;
}

// ---------------------------------------------------------------------------
// The worker
// ---------------------------------------------------------------------------

void MaskSession::enqueue(std::function<void()> job) {
    _pending++;
    {
        std::lock_guard<std::mutex> lk(_qmu);
        _queue.push_back(std::move(job));
    }
    _qcv.notify_one();
}

// Drains the queue before quitting, so a save queued by close() lands.
void MaskSession::worker_main() {
    for (;;) {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lk(_qmu);
            _qcv.wait(lk, [&] { return _quit || !_queue.empty(); });
            if (_queue.empty()) return;
            job = std::move(_queue.front());
            _queue.pop_front();
        }
        _job_seq++;
        if (_worker_hook) _worker_hook(false);
        job();
        if (_worker_hook) _worker_hook(true);
        _job_seq++;
        _pending--;
    }
}

void MaskSession::post_status(const std::string& s, bool error) {
    std::lock_guard<std::mutex> lk(_mu);
    if (error) _error = s;
    else _status = s;
}

void MaskSession::post_error(const std::string& s, bool sticky) {
    std::lock_guard<std::mutex> lk(_mu);
    _error = s;
    _error_sticky = _error_sticky || sticky;
}

void MaskSession::set_corrected(int n) {
    std::lock_guard<std::mutex> lk(_mu);
    _corrected = n;
}

int MaskSession::corrected_count() const {
    std::lock_guard<std::mutex> lk(_mu);
    return _corrected;
}

std::string MaskSession::status() const {
    std::lock_guard<std::mutex> lk(_mu);
    return _status;
}

std::string MaskSession::error() const {
    std::lock_guard<std::mutex> lk(_mu);
    return _error;
}

void MaskSession::load_frame(int i) {
    if (i < 0 || i >= frame_count()) return;
    const FrameRef f = _frames[(size_t)i];
    _nav = i;
    post_status(msg::working.get(), false);
    enqueue([this, f, i] {
        Loaded l;
        l.index = i;
        if (!app::load_rgb(f.file, l.fw, l.fh, l.rgb)) {
            post_error(spirula::i18n::format(msg::err_read, {f.file}), false);
            return;
        }
        l.turn = app::photo_turn(f.file);
        l.doc = std::make_unique<MaskDoc>();
        std::string err, warning;
        const auto was = _index.frames.find(f.key);
        const uint64_t composite_was = was == _index.frames.end() ? 0 : was->second.composite_fp;
        if (!l.doc->load(_layer_root, _mask_root, f.key, l.fw, l.fh, _index, err, warning)) {
            post_error(warning.empty() ? spirula::i18n::format(msg::err_read, {err})
                                       : size_mismatch_text(warning, *l.doc),
                       false);
            return;
        }
        set_corrected((int)_index.frames.size());
        // A rebase wrote a new composite into masks/, which the scan never read.
        const auto now = _index.frames.find(f.key);
        if (now != _index.frames.end() && now->second.composite_fp != composite_was)
            refresh_health({f.key});
        std::lock_guard<std::mutex> lk(_mu);
        _loaded = std::move(l);
        _loaded_ready = true;
        _status.clear();
        if (!_error_sticky) _error.clear();
    });
}

void MaskSession::pump() {
    Loaded l;
    bool have_loaded = false, have_saved = false;
    std::string saved_key;
    uint64_t saved_rev = 0;
    bool saved_comp = false;
    {
        std::lock_guard<std::mutex> lk(_mu);
        if (_loaded_ready) {
            l = std::move(_loaded);
            _loaded_ready = false;
            have_loaded = true;
        }
        if (_saved_ready) {
            saved_key = _saved_key;
            saved_rev = _saved_rev;
            saved_comp = _saved_comp;
            _saved_ready = false;
            have_saved = true;
        }
    }
    if (have_saved && _doc && _doc->key() == saved_key) _doc->mark_saved(saved_rev, saved_comp);
    if (!have_loaded) return;
    _doc = std::move(l.doc);
    _doc_gen++;   // by construction: every _doc arrives here
    _shown_valid = false;
    {
        // A target opened for editing would diverge from its snapshot.
        std::lock_guard<std::mutex> lk(_mu);
        for (const LayerSnapshot& t : _prop.targets)
            if (t.key == _doc->key()) {
                _prop = PropagateRecord{};
                _prop_undoable = false;
                break;
            }
    }
    _rgb = std::make_shared<const std::vector<uint8_t>>(std::move(l.rgb));
    _fw = l.fw;
    _fh = l.fh;
    _turn = l.turn;
    _idx = l.index;
    _slider_idx = _idx;
    _dw = _doc->width();
    _dh = _doc->height();
    spirula::oriented_size(_turn.turns_cw, _dw, _dh);
    _view = View{1.0f, 0.5f * (float)_dw, 0.5f * (float)_dh};
    _tool_reset = true;
    _livewire.reset();
    _path.set_livewire(nullptr);
    _path.cancel();
    _pen.cancel();
    _win_dirty = true;
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

void MaskSession::go_to(int i) {
    if (i < 0 || i >= frame_count() || i == _idx) return;
    if (_doc && _doc->dirty()) save();
    _doc.reset();
    _rgb.reset();
    load_frame(i);
}

// A snapshot of the planes goes to the worker, so painting can go on while
// an 8K frame encodes; the revision decides whether the document is still
// dirty when the save lands.
void MaskSession::save() {
    if (!_doc) return;
    struct Snap {
        std::string key;
        int w = 0, h = 0;
        std::vector<uint8_t> base, drop, keep;
        bool comp = false;
        uint64_t rev = 0;
    };
    auto s = std::make_shared<Snap>();
    s->key = _doc->key();
    s->w = _doc->width();
    s->h = _doc->height();
    s->base = _doc->base();
    s->drop = _doc->drop();
    s->keep = _doc->keep();
    s->comp = _doc->base_state() != BaseState::Missing;
    s->rev = _doc->revision();
    enqueue([this, s] {
        std::string err;
        if (!save_frame(_layer_root, _mask_root, s->key, s->w, s->h, s->base.data(),
                        s->drop.data(), s->keep.data(), s->comp, _index, err)) {
            post_error(spirula::i18n::format(msg::err_write, {err}), true);
            return;
        }
        set_corrected((int)_index.frames.size());
        // Here, not in pump(): a save that go_to() or Play made lands after
        // the document is gone.
        refresh_health({s->key});
        std::lock_guard<std::mutex> lk(_mu);
        _saved_key = s->key;
        _saved_rev = s->rev;
        _saved_comp = s->comp;
        _saved_ready = true;
        _status = msg::status_saved.get();
        _error.clear();
        _error_sticky = false;
    });
}

void MaskSession::revert_open_frame() {
    if (!_doc) return;
    const std::string key = _doc->key();
    const int i = _idx;
    _doc.reset();
    _rgb.reset();
    enqueue([this, key] {
        std::string err;
        if (!mask::revert_frame(_layer_root, _mask_root, key, _index, err))
            post_error(spirula::i18n::format(msg::err_write, {err}), true);
        set_corrected((int)_index.frames.size());
        refresh_health({key});
    });
    _idx = -1;
    sam_revert(i);
    load_frame(i);
}

void MaskSession::revert_every_frame() {
    const int i = _idx;
    _doc.reset();
    _rgb.reset();
    drop_propagate_record();
    enqueue([this] {
        std::string err;
        drop_propagate_record();   // one a propagate queued ahead of this may have set
        const bool flipped = _index.mask_flipped;
        std::vector<std::string> reverted;
        for (const auto& [key, e] : _index.frames) reverted.push_back(key);
        if (mask::revert_all(_layer_root, err) < 0)
            post_error(spirula::i18n::format(msg::err_write, {err}), true);
        if (!_index.load(_layer_root, err)) _index = LayerIndex{};
        _index.mask_root = _mask_root;
        _index.mask_flipped = flipped;
        set_corrected((int)_index.frames.size());
        refresh_health(reverted);
    });
    _idx = -1;
    sam_revert(-1);
    if (i >= 0) load_frame(i);
}

// ---------------------------------------------------------------------------
// Propagate
// ---------------------------------------------------------------------------

PropagateReport MaskSession::last_propagate() const {
    std::lock_guard<std::mutex> lk(_mu);
    return _prop_report;
}

bool MaskSession::can_undo_propagate() const {
    if (!_doc) return false;
    std::lock_guard<std::mutex> lk(_mu);
    return _prop_undoable && _prop.source_key == _doc->key();
}

bool MaskSession::sam_work_pending() const {
    return (_sam && (_sam_ops.busy(*_sam) || _sam->has_result())) || _sam_margin_pending;
}

// Frame keys repeat across datasets, so nothing keyed by one may outlive it:
// a record that did would undo this dataset's layers into the next one's.
void MaskSession::forget_workflow() {
    {
        std::lock_guard<std::mutex> lk(_mu);
        _prop = PropagateRecord{};
        _prop_undoable = false;
        _prop_report = PropagateReport{};
        _health.clear();
        _scanned = 0;
        _scan_ms = -1.0;
    }
    _slide_index = _slide_pending = -1;
    _slide_shown = 0;
    _slide_max_gap = 0.0;
    _slide_stop_ms = -1.0;
    _slide_started = _slide_last_shown = _slide_now = 0.0;
}

void MaskSession::drop_propagate_record() {
    std::lock_guard<std::mutex> lk(_mu);
    _prop = PropagateRecord{};
    _prop_undoable = false;
}

void MaskSession::propagate(PropagateScope scope, int lo, int hi) {
    if (!_doc || _idx < 0 || sam_work_pending()) return;
    const std::vector<int> targets = propagate_targets(_frames, _idx, scope, lo, hi);
    {
        std::lock_guard<std::mutex> lk(_mu);
        _prop = PropagateRecord{};
        _prop_undoable = false;
        _prop_report = PropagateReport{};
        _prop_report.w = _doc->width();
        _prop_report.h = _doc->height();
    }
    if (targets.empty()) {
        const bool inverted = scope == PropagateScope::Range && lo > hi;
        post_status((inverted ? msg::prop_range_empty : msg::prop_no_targets).get(), true);
        return;
    }
    sam_forget_clicks(targets);
    // The job saves the source itself: a queued save() would be a separate
    // job whose failure this one could not see.
    struct Job {
        std::string source_key;
        int W = 0, H = 0;
        std::vector<uint8_t> base, drop, keep;
        bool dirty = false, comp = false;
        uint64_t rev = 0;
        size_t byte_cap = 0;
        int threads = 1;
        std::vector<FrameRef> targets;
        std::vector<int> at;   // each target's index in _frames
    };
    auto j = std::make_shared<Job>();
    j->byte_cap = _prop_byte_cap;   // the setter is the UI thread's
    j->source_key = _doc->key();
    j->W = _doc->width();
    j->H = _doc->height();
    j->threads = propagate_threads(j->W, j->H, std::thread::hardware_concurrency());
    j->base = _doc->base();
    j->drop = _doc->drop();
    j->keep = _doc->keep();
    j->dirty = _doc->dirty();
    j->comp = _doc->base_state() != BaseState::Missing;
    j->rev = _doc->revision();
    for (int t : targets) {
        j->targets.push_back(_frames[(size_t)t]);
        j->at.push_back(t);
    }
    post_status(msg::prop_working.get(), false);
    _prop_cancel = false;
    _prop_done = 0;
    _prop_total = (int)j->targets.size();
    enqueue([this, j] {
        struct Progress {
            std::atomic<int>& total;
            ~Progress() { total = 0; }
        } progress{_prop_total};
        std::string err;
        if (j->dirty) {
            if (!save_frame(_layer_root, _mask_root, j->source_key, j->W, j->H, j->base.data(),
                            j->drop.data(), j->keep.data(), j->comp, _index, err)) {
                std::lock_guard<std::mutex> lk(_mu);
                _status.clear();
                _error = spirula::i18n::format(msg::err_write, {err});
                _error_sticky = true;
                return;
            }
            std::lock_guard<std::mutex> lk(_mu);
            _saved_key = j->source_key;
            _saved_rev = j->rev;
            _saved_comp = j->comp;
            _saved_ready = true;
        }
        struct Outcome {
            enum What { NotRun, NoSnapshot, Stray, Refused, Done, Failed } what = NotRun;
            LayerSnapshot snap;
            std::string err;
            PropagateRefusal refused;
            bool unrestored = false;
        };
        std::vector<Outcome> out(j->targets.size());
        SharedIndex shared{_index, _layer_root};
        run_pool((int)out.size(), j->threads, _prop_cancel, [&](int i) {
            const FrameRef& f = j->targets[(size_t)i];
            Outcome& o = out[(size_t)i];
            LayerIndex x = shared.copy(f.key);
            // Without a snapshot nothing could be put back: skipped, counted.
            if (!snapshot_layers(_layer_root, f.key, x, o.snap, o.err)) {
                o.what = Outcome::NoSnapshot;
                _prop_done++;
                return;
            }
            // Undo would revert through a base no entry vouches for, copying
            // it over the mask on disk, whatever wrote that since.
            const std::string stray = layer_file(_layer_root, f.key, Layer::Base);
            std::error_code ec;
            if (!o.snap.had_entry && fs::exists(stray, ec)) {
                o.what = Outcome::Stray;
                o.err = stray;
                _prop_done++;
                return;
            }
            if (propagate_to(_layer_root, _mask_root, f.key, f.file, j->W, j->H, j->drop.data(),
                             j->keep.data(), x, o.refused, o.err)) {
                o.what = Outcome::Done;
            } else if (o.err.empty()) {
                o.what = Outcome::Refused;
            } else {
                // Written, or partly: the snapshot is what puts it back.
                o.what = Outcome::Failed;
                std::string rerr;
                o.unrestored = !restore_layers(_layer_root, _mask_root, o.snap, x, rerr);
            }
            shared.merge(f.key, x);
            if (o.what != Outcome::Refused) note_health(j->at[(size_t)i], f.key);
            _prop_done++;
        });
        std::string index_err;
        const bool index_saved = shared.idx.save(_layer_root, index_err);

        // In target order, so the first failure named is the first frame's.
        PropagateRecord rec;
        rec.source_key = j->source_key;
        PropagateReport rep;
        rep.w = j->W;
        rep.h = j->H;
        auto fail = [&rep](const std::string& key, const std::string& path, bool stray) {
            rep.failed++;
            if (!rep.failed_key.empty()) return;
            rep.failed_key = key;
            rep.failed_path = path;
            rep.failed_stray = stray;
        };
        for (size_t i = 0; i < out.size(); i++) {
            Outcome& o = out[i];
            const std::string& key = j->targets[i].key;
            switch (o.what) {
            case Outcome::NotRun: rep.skipped++; break;
            case Outcome::NoSnapshot: fail(key, o.err, false); break;
            case Outcome::Stray: fail(key, o.err, true); break;
            case Outcome::Refused:
                rep.refused++;
                if (rep.refused_key.empty()) {
                    rep.refused_key = key;
                    rep.refused_w = o.refused.w;
                    rep.refused_h = o.refused.h;
                }
                break;
            case Outcome::Done:
            case Outcome::Failed:
                rec.bytes += o.snap.bytes();
                if (o.what == Outcome::Done) {
                    rep.done++;
                } else {
                    fail(key, o.err, false);
                    if (o.unrestored && !rep.unrestored++) {
                        rep.unrestored_key = key;
                        rep.unrestored_path = o.err;
                    }
                }
                rec.targets.push_back(std::move(o.snap));
                break;
            }
        }
        rep.bytes = rec.bytes;
        rep.undoable = !rec.targets.empty() && rec.bytes <= j->byte_cap;
        set_corrected((int)_index.frames.size());
        if (j->dirty) refresh_health({j->source_key});   // saved above
        std::lock_guard<std::mutex> lk(_mu);
        _prop_report = rep;
        _prop_undoable = rep.undoable;
        _prop = rep.undoable ? std::move(rec) : PropagateRecord{};
        _status = rep.skipped
                      ? spirula::i18n::format(msg::prop_stopped,
                                              {rep.done, rep.skipped, rep.refused, rep.failed})
                      : spirula::i18n::format(msg::prop_done, {rep.done, rep.refused, rep.failed});
        _prop_stopped = rep.skipped ? _status : std::string();
        // By priority, so a failure named in the loop is never clobbered, and
        // a frame left not put back is named before any that was.
        if (!index_saved) {
            _error = spirula::i18n::format(msg::err_write, {index_err});
            _error_sticky = true;
        } else if (rep.unrestored)
            _error = spirula::i18n::format(rep.undoable ? msg::prop_failed_not_restored
                                                        : msg::prop_failed_not_restored_final,
                                           {rep.unrestored_key, rep.unrestored_path, rep.unrestored});
        else if (rep.failed)
            _error = spirula::i18n::format(rep.failed_stray ? msg::prop_failed_stray_base
                                                            : msg::prop_failed_restored,
                                           {rep.failed_key, rep.failed_path});
        else if (rep.refused)
            _error = spirula::i18n::format(msg::prop_refused_size,
                                           {rep.refused_key, rep.refused_w, rep.refused_h, rep.w, rep.h});
        else if (rep.done > 0 && !rep.undoable)
            _error = spirula::i18n::format(msg::prop_not_undoable, {(int)(rep.bytes >> 20)});
        else
            _error.clear();
    });
}

// A target that would not go back stays in the record, so Undo retries it;
// restore_layers leaves its mask agreeing with its layers, bar a second fault.
void MaskSession::undo_propagate() {
    if (!can_undo_propagate()) return;
    auto rec = std::make_shared<PropagateRecord>();
    {
        std::lock_guard<std::mutex> lk(_mu);
        *rec = std::move(_prop);
        _prop = PropagateRecord{};
        _prop_undoable = false;
    }
    std::map<std::string, int> at;
    for (size_t i = 0; i < _frames.size(); i++) at[_frames[i].key] = (int)i;
    std::vector<int> frames, slot;   // slot: each target's frame, -1 if gone
    for (const LayerSnapshot& t : rec->targets) {
        const auto it = at.find(t.key);
        slot.push_back(it == at.end() ? -1 : it->second);
        if (it != at.end()) frames.push_back(it->second);
    }
    sam_forget_clicks(frames);
    const int threads = propagate_threads(_doc->width(), _doc->height(),
                                          std::thread::hardware_concurrency());
    _prop_cancel = false;
    _prop_done = 0;
    _prop_total = (int)rec->targets.size();
    enqueue([this, rec, threads, slot] {
        struct Progress {
            std::atomic<int>& total;
            ~Progress() { total = 0; }
        } progress{_prop_total};
        const size_t n = rec->targets.size();
        std::vector<int> result(n, 0);   // 0 not reached, 1 restored, 2 failed
        std::vector<std::string> errs(n);
        SharedIndex shared{_index, _layer_root};
        run_pool((int)n, threads, _prop_cancel, [&](int i) {
            const LayerSnapshot& t = rec->targets[(size_t)i];
            LayerIndex x = shared.copy(t.key);
            result[(size_t)i] = restore_layers(_layer_root, _mask_root, t, x, errs[(size_t)i]) ? 1 : 2;
            shared.merge(t.key, x);
            note_health(slot[(size_t)i], t.key);
            _prop_done++;
        });
        std::string index_err;
        const bool index_saved = shared.idx.save(_layer_root, index_err);
        // Failed and not reached, in record order; with no index written,
        // every frame, so Undo can try again.
        PropagateRecord left;
        left.source_key = rec->source_key;
        std::string failed_key, failed_path;
        int restored = 0, skipped = 0;
        for (size_t i = 0; i < n; i++) {
            LayerSnapshot& t = rec->targets[i];
            if (result[i] == 1 && index_saved) {
                restored++;
                continue;
            }
            if (result[i] == 0) skipped++;
            // The last in record order, which the undo used to meet first.
            if (result[i] == 2) {
                failed_key = t.key;
                failed_path = errs[i];
            }
            left.bytes += t.bytes();
            left.targets.push_back(std::move(t));
        }
        set_corrected((int)_index.frames.size());
        std::lock_guard<std::mutex> lk(_mu);
        _prop_report = PropagateReport{};
        _status = skipped ? spirula::i18n::format(msg::prop_undo_stopped, {restored, skipped})
                          : spirula::i18n::format(msg::prop_undone, {restored});
        _prop_stopped = skipped ? _status : std::string();
        if (!index_saved) {
            _error = spirula::i18n::format(msg::err_write, {index_err});
            _error_sticky = true;
        } else if (failed_key.empty()) {
            _error.clear();
        } else {
            _error = spirula::i18n::format(msg::prop_undo_failed, {failed_key, failed_path});
        }
        if (left.targets.empty()) return;
        _prop = std::move(left);
        _prop_undoable = true;
    });
}

// ---------------------------------------------------------------------------
// Find missing
// ---------------------------------------------------------------------------

FrameHealth MaskSession::health(int i) const {
    std::lock_guard<std::mutex> lk(_mu);
    return i >= 0 && (size_t)i < _health.size() ? _health[(size_t)i] : FrameHealth{};
}

int MaskSession::scanned_count() const {
    std::lock_guard<std::mutex> lk(_mu);
    return _scanned;
}

bool MaskSession::scan_running() const {
    std::lock_guard<std::mutex> lk(_mu);
    return _scan_running;
}

double MaskSession::scan_ms() const {
    std::lock_guard<std::mutex> lk(_mu);
    return _scan_ms;
}

int MaskSession::missing_count() const {
    std::lock_guard<std::mutex> lk(_mu);
    int n = 0;
    for (const FrameHealth& h : _health) n += is_missing(h, _band_lo, _band_hi) ? 1 : 0;
    return n;
}

// From the frame last asked for, not _idx: a load that failed leaves _idx on
// the frame before it, and searching from there would land on it again.
bool MaskSession::go_to_missing(int dir) {
    std::vector<FrameHealth> v;
    bool scanning = false;
    {
        std::lock_guard<std::mutex> lk(_mu);
        v = _health;
        scanning = _scanned < (int)_frames.size();
    }
    const int j = next_missing(v, _nav >= 0 ? _nav : _idx, dir, _band_lo, _band_hi);
    if (j < 0) {
        post_status((scanning ? msg::find_none_scanning : msg::find_none).get(), true);
        return false;
    }
    if (j == _idx && !_doc) load_frame(j);   // go_to() skips its own index
    else go_to(j);
    return true;
}

void MaskSession::start_scan() {
    stop_scan();
    _scan_stop = false;
    _scan = std::thread([this] { scan_main(); });
    std::lock_guard<std::mutex> lk(_mu);
    _scan_running = true;
}

void MaskSession::stop_scan() {
    _scan_stop = true;
    if (_scan.joinable()) _scan.join();
    std::lock_guard<std::mutex> lk(_mu);
    _scan_running = false;
}

// One decode per mask the cache does not know by fingerprint, off the worker
// so navigation stays live. A read a worker job overlapped is thrown away,
// cache entry and all, and redone: that job may have rewritten the mask.
void MaskSession::scan_main() {
    const auto t0 = std::chrono::steady_clock::now();
    KeptCache cache;
    std::string err;
    cache.load(_layer_root, err);
    const int n = (int)_frames.size();
    int since_save = 0;
    bool save_failed = false;
    for (int i = 0; i < n && !_scan_stop.load();) {
        if (_scan_hook) _scan_hook(i, false);
        const uint64_t seq = _job_seq.load();
        if (seq & 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }
        const std::string& key = _frames[(size_t)i].key;
        FrameHealth h;
        h.scanned = true;
        h.missing_mask = !kept_fraction_of(_mask_root, key, _mask_flipped, cache, h.kept);
        if (_scan_hook) _scan_hook(i, true);
        {
            std::lock_guard<std::mutex> lk(_mu);
            if (_job_seq.load() != seq) {
                cache.dirty = cache.frames.erase(key) > 0 || cache.dirty;
                continue;
            }
            if ((size_t)i < _health.size()) _health[(size_t)i] = h;
            _scanned = ++i;
        }
        // One failure ends the periodic writes, so a failing write is never
        // retried on every remaining frame of a thousand-frame scan.
        if (++since_save >= 64 && !save_failed) {
            since_save = 0;
            if (cache.save(_layer_root, err)) cache.dirty = false;
            else save_failed = true;
        }
    }
    if (!save_failed && !cache.save(_layer_root, err)) save_failed = true;
    if (save_failed) post_status(spirula::i18n::format(msg::err_write, {err}), true);
    else cache.dirty = false;
    if (_scan_stop.load()) return;
    std::lock_guard<std::mutex> lk(_mu);
    _scan_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

// After a job rewrote masks. Worker thread, so no mask changes under the
// read. The cache is read and never written back: kept.json has one writer.
void MaskSession::refresh_health(const std::vector<std::string>& keys) {
    if (keys.empty()) return;
    std::map<std::string, int> at;
    for (size_t i = 0; i < _frames.size(); i++) at[_frames[i].key] = (int)i;
    KeptCache cache;
    std::string err;
    cache.load(_layer_root, err);
    for (const std::string& key : keys) {
        const auto it = at.find(key);
        if (it == at.end()) continue;
        FrameHealth h;
        h.scanned = true;
        h.missing_mask = !kept_fraction_of(_mask_root, key, _mask_flipped, cache, h.kept);
        std::lock_guard<std::mutex> lk(_mu);
        if ((size_t)it->second < _health.size()) _health[(size_t)it->second] = h;
    }
}

void MaskSession::note_health(int i, const std::string& key) {
    FrameHealth h;
    h.scanned = true;
    KeptCache none;   // the mask was just written, so no cached fraction fits
    h.missing_mask = !kept_fraction_of(_mask_root, key, _mask_flipped, none, h.kept);
    std::lock_guard<std::mutex> lk(_mu);
    if (i >= 0 && (size_t)i < _health.size()) _health[(size_t)i] = h;
}

Rect MaskSession::shown_rect(const Rect& stored) const {
    if (!_doc) return {};
    return rect_to_displayed(stored, _turn, _doc->width(), _doc->height());
}

Rect MaskSession::commit_stroke(const ShapeStroke& pane_stroke, Paint mode, const Mapping& m) {
    if (!_doc) return {};
    ShapeStroke shown = pane_stroke;
    for (size_t i = 0; i + 1 < shown.pts.size(); i += 2) {
        shown.pts[i] = m.to_mask_x(pane_stroke.pts[i]);
        shown.pts[i + 1] = m.to_mask_y(pane_stroke.pts[i + 1]);
    }
    shown.brush_radius = pane_stroke.brush_radius / m.scale;
    const int W = _doc->width(), H = _doc->height();
    const ShapeStroke stored = stroke_to_stored(shown, _turn, W, H);
    const Rect r = stroke_bounds(stored, W, H);
    if (r.empty()) return {};
    Stencil st;
    rasterize_shape(stored, W, H, st);
    _doc->paint(mode, std::move(st), r);
    return shown_rect(_doc->last_change());
}

// Plain and Shift drop, Ctrl keeps, both held clears back to the base
// (Intersect in the 3D editor, meaningless on a layer). The eraser starts
// from keep instead, so Ctrl still means "the other one".
Paint MaskSession::paint_for(bool shift, bool ctrl, bool erasing) {
    if (shift && ctrl) return Paint::Clear;
    const bool keep = ctrl != erasing;
    return keep ? Paint::ForceKeep : Paint::ForceDrop;
}

// Written as a rejection test rather than std::clamp so that a NaN lands on
// the minimum: std::clamp returns it, and a NaN radius rasterizes nothing
// while the slider and the status strip still read a number.
float MaskSession::clamp_brush(float r) {
    if (!(r > kMinBrush)) return kMinBrush;
    return r < kMaxBrush ? r : kMaxBrush;
}

float MaskSession::scale_brush(float r, float factor) { return clamp_brush(r * factor); }

float MaskSession::step_brush(float r, bool grow) {
    return scale_brush(r, grow ? 1.18f : 0.85f);
}

float MaskSession::wheel_brush(float r, float wheel) {
    return scale_brush(r, std::pow(1.18f, wheel));
}

Rect MaskSession::undo() {
    if (!_doc || !_doc->can_undo()) return {};
    _doc->undo();
    return shown_rect(_doc->last_change());
}

bool MaskSession::sam_undo_click(Rect& changed) {
    changed = {};
    if (!_sam || !_doc || _idx < 0 || sam_busy() || _sam_release_pending) return false;
    const int obj = _sam_add_object;
    if (!sam_add_on_top(obj)) return false;
    MaskSettings& p = _sam->prompt();
    const std::string& camera = _frames[(size_t)_idx].camera;
    auto mine = [&](const MaskClick& c) {
        return c.source.empty() && c.object == obj && c.frame == _idx && c.camera == camera;
    };
    const auto last = std::find_if(p.clicks.rbegin(), p.clicks.rend(), mine);
    if (last == p.clicks.rend()) return false;
    const MaskClick popped = *last;
    p.clicks.erase(std::next(last).base());
    if (std::none_of(p.clicks.begin(), p.clicks.end(), mine)) {
        changed = undo();
        _sam_held.clear();
        return true;
    }
    p.current_object = obj;
    if (!sam_gate_passes() ||
        !_sam->start_points(sam_frame_stamp(), _rgb, _fw, _fh, _doc->width(), _doc->height(),
                            _sam->object_points(_idx, camera), _sam_add_mode, p.dilate_ratio)) {
        p.clicks.push_back(popped);
        return false;
    }
    _sam_job_object = obj;
    _sam_t0 = std::chrono::steady_clock::now();
    return true;
}

Rect MaskSession::undo_step() {
    Rect r;
    if (sam_mode() && sam_undo_click(r)) return r;
    return undo();
}

Rect MaskSession::redo() {
    if (!_doc || !_doc->can_redo()) return {};
    _doc->redo();
    return shown_rect(_doc->last_change());
}

WindowSource MaskSession::window_source() const {
    WindowSource s;
    if (!_doc) return s;
    s.rgb = _rgb ? _rgb->data() : nullptr;
    s.fw = _fw;
    s.fh = _fh;
    s.composite = _doc->composite().data();
    s.drop = _doc->drop().data();
    s.keep = _doc->keep().data();
    s.W = _doc->width();
    s.H = _doc->height();
    s.turn = _turn;
    return s;
}

// Derived by inverting to_stored's switch, not through inverse_turn: the
// mirror is applied before the turn in one and after it in the other.
void to_displayed(const sfm::ExifTransform& t, int W, int H, float sx, float sy,
                  float& dx, float& dy) {
    int dw = W, dh = H;
    spirula::oriented_size(t.turns_cw, dw, dh);
    float mx;
    switch (t.turns_cw & 3) {
        case 1:  dy = sx;            mx = (float)H - sy; break;
        case 2:  mx = (float)W - sx; dy = (float)H - sy; break;
        case 3:  dy = (float)W - sx; mx = sy;            break;
        default: mx = sx;            dy = sy;            break;
    }
    dx = t.mirror ? (float)dw - mx : mx;
}

void MaskSession::ensure_livewire() {
    if (_livewire || !_doc || !_rgb || _rgb->empty()) return;
    const auto t0 = std::chrono::steady_clock::now();
    auto lw = std::make_unique<Livewire>();
    lw->build(_rgb->data(), _fw, _fh);
    _livewire_ms = std::chrono::duration<double, std::milli>(
                       std::chrono::steady_clock::now() - t0).count();
    _livewire = std::move(lw);
    _path.set_livewire(_livewire.get());
    char ms[32];
    std::snprintf(ms, sizeof ms, "%.0f", _livewire_ms);
    post_status(spirula::i18n::format(msg::path_edge_map,
                                      {_livewire->width(), _livewire->height(),
                                       _livewire->step(), std::string(ms)}),
                false);
}

Style pane_style_for(Peek peek, ViewMode view, int pane) {
    if (view == ViewMode::SideBySide) {
        // Both bare views are on screen already, so the peek turns ONE pane
        // into the overlay and leaves the other as the reference.
        if (peek == Peek::Photo) return pane == 1 ? Style::Overlay : Style::Photo;
        if (peek == Peek::Mask) return pane == 0 ? Style::Overlay : Style::MaskOnly;
        return pane == 0 ? Style::Photo : Style::MaskOnly;
    }
    if (peek == Peek::Photo) return Style::Photo;
    if (peek == Peek::Mask) return Style::MaskOnly;
    if (view == ViewMode::MaskOnly) return Style::MaskOnly;
    return Style::Overlay;
}

PaneDerive plan_derive(bool dirty, const Window& want, ViewMode view, Peek peek, Window& win0,
                       Style& style0, Window& win1, Style& style1) {
    const bool two = view == ViewMode::SideBySide;
    const Style s0 = pane_style_for(peek, view, 0), s1 = pane_style_for(peek, view, 1);
    if (!two) win1 = Window{};
    PaneDerive d;
    d.left = dirty || !same_window(want, win0) || s0 != style0;
    d.right = two && (dirty || !same_window(want, win1) || s1 != style1);
    if (d.left) {
        win0 = want;
        style0 = s0;
    }
    if (d.right) {
        win1 = want;
        style1 = s1;
    }
    return d;
}

int pane_at(float x, int panes, float pane_w, float gap) {
    const float stride = pane_w + gap;
    if (panes < 1 || x < 0.0f || stride <= 0.0f) return -1;
    const int p = (int)std::floor(x / stride);
    if (p >= panes || x - (float)p * stride >= pane_w) return -1;
    return p;
}

ViewMode switch_view(ViewMode cur, ViewMode want, bool shape_in_progress) {
    return shape_in_progress ? cur : want;
}

// Cleared after the release frame is answered: that frame ends the drag.
int MaskSession::bind_pane(int hover, bool pressed, bool down, int panes) {
    if (_held_pane >= panes) _held_pane = -1;
    if (pressed && hover >= 0) _held_pane = hover;
    const int p = _held_pane >= 0 ? _held_pane : std::max(0, hover);
    if (!down) _held_pane = -1;
    return p;
}

// Pane px -> displayed mask px (the mapping) -> stored mask px (the EXIF
// turn) -> stored frame px (the mask-to-frame scale), and back.
void MaskSession::note_shown(const Mapping& m, float origin_x, float origin_y, int panes,
                             float pane_w, float gap) {
    _shown = m;
    _shown_x = origin_x;
    _shown_y = origin_y;
    _shown_panes = std::max(1, panes);
    _shown_pane_w = pane_w;
    _shown_gap = gap;
    _shown_valid = true;
}

bool MaskSession::shown_to_frame(float screen_x, float screen_y, float& fx, float& fy) const {
    if (!_shown_valid || !_doc) return false;
    float x = screen_x - _shown_x;
    if (_shown_panes > 1) {
        const int p = pane_at(x, _shown_panes, _shown_pane_w, _shown_gap);
        if (p < 0) return false;
        x -= pane_left(p, _shown_pane_w, _shown_gap);
    }
    path_space(_shown).to_frame(x, screen_y - _shown_y, fx, fy);
    return true;
}

PathSpace MaskSession::path_space(const Mapping& m) const {
    PathSpace s;
    const int W = _doc ? _doc->width() : 1, H = _doc ? _doc->height() : 1;
    const float kx = (float)_fw / (float)std::max(1, W), ky = (float)_fh / (float)std::max(1, H);
    const sfm::ExifTransform turn = _turn;
    s.to_frame = [m, turn, W, H, kx, ky](float x, float y, float& fx, float& fy) {
        float sx, sy;
        to_stored(turn, W, H, m.to_mask_x(x), m.to_mask_y(y), sx, sy);
        fx = sx * kx;
        fy = sy * ky;
    };
    s.from_frame = [m, turn, W, H, kx, ky](float fx, float fy, float& x, float& y) {
        float dx, dy;
        to_displayed(turn, W, H, fx / kx, fy / ky, dx, dy);
        x = m.to_screen_x(dx);
        y = m.to_screen_y(dy);
    };
    return s;
}

// ---------------------------------------------------------------------------
// SAM assist
// ---------------------------------------------------------------------------

bool MaskSession::sam_available() const { return MaskSam::available(); }

double MaskSession::sam_pool_mib() { return MaskSam::pool_mib(); }

int MaskSession::sam_loads() { return MaskSam::load_count(); }

// Idempotent: GuiApp calls it every frame. A changed path cancels the job and
// defers the release to sam_pump(), so the UI thread never joins an encode.
void MaskSession::set_sam_model(const std::string& path, bool text_prompts) {
    _sam_text_hint = text_prompts;
    if (path != _sam_model) {
        _sam_model = path;
        _sam_model_changes++;
        if (_sam) {
            _sam->cancel();
            _sam_release_pending = true;
        }
    }
    if (_sam) _sam->set_model(path, text_prompts);
}

MaskSam& MaskSession::sam() {
    if (!_sam) {
        _sam = std::make_unique<MaskSam>();
        _sam->set_model(_sam_model, _sam_text_hint);
    }
    return *_sam;
}

bool MaskSession::sam_text_supported() const {
    return _sam ? _sam->text_supported() : MaskSam::available() && _sam_text_hint;
}

bool MaskSession::sam_busy() const { return _sam && _sam->busy(); }

void MaskSession::sam_cancel() {
    if (_sam) _sam->cancel();
}

MaskSettings& MaskSession::sam_prompt() { return sam().prompt(); }

int MaskSession::sam_click_count() const {
    return _sam ? (int)_sam->prompt().clicks.size() : 0;
}

int MaskSession::sam_object_count() const { return _sam ? _sam->prompt().object_count : 0; }

std::string MaskSession::sam_status() const { return _sam ? _sam->status() : std::string(); }

std::string MaskSession::sam_error() const { return _sam ? _sam->error() : std::string(); }

std::string MaskSession::sam_blocker(bool mask_preview, bool depth_preview, bool run_active) {
    if (run_active) return msg::sam_blocked_run.get();
    if (mask_preview || depth_preview) return msg::sam_blocked_preview.get();
    return {};
}

// Records why SAM is paused; never joins or yields (stop_inference_users()
// does that). A lifted pause takes its own message with it and nothing else.
void MaskSession::set_sam_blocker(const std::string& reason) {
    if (reason == _sam_blocker) return;
    if (_sam && !_sam_blocker.empty()) _sam->clear_error_if(_sam_blocker);
    _sam_blocker = reason;
}

// Everything but the picker, which draws GuiApp's own state, and the counter
// of model changes. The clicks go too: their frame indices name this session.
void MaskSession::sam_forget() {
    _sam.reset();
    _sam_model.clear();
    _sam_text_hint = false;
    _sam_release_pending = false;
    _sam_results = _sam_dropped = _sam_last_detections = 0;
    _sam_last_ms = _sam_last_job_ms = 0.0;
    _sam_last_score = 0.0f;
    _sam_last_area = 0;
    _sam_last_vetoed = false;
    _sam_ui_ms = 0.0;
    _sam_click_x = _sam_click_y = -1.0f;
    _sam_job_object = -1;
    _sam_add_key.clear();
    _sam_add_step = 0;
    _sam_add_object = -1;
    _sam_add_mode = Paint::ForceDrop;
    _sam_held.clear();
    _sam_reapply_ms = _sam_reapply_job_ms = _sam_margin_start_ms = 0.0;
    _sam_reapplies = _sam_margin_starts = 0;
    _sam_margin_pending = _sam_margin_moved = false;
    _sam_blocker.clear();
}

// An idle session is released here; a running job is cancelled and parked, as
// joining it froze the UI for the whole stage, 2887 ms mid-encode, 7805 mid-load.
void MaskSession::close_sam() {
    _sam_close_ms = 0.0;
    if (!_sam) return;
    const auto t0 = std::chrono::steady_clock::now();
    if (_sam_ops.busy(*_sam)) {
        _sam->cancel();
        sam_drain_retiring();
        _sam_retiring = std::move(_sam);
    } else {
        sam_yield();
    }
    _sam_close_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

void MaskSession::sam_poll_retiring() {
    if (!_sam_retiring || _sam_ops.busy(*_sam_retiring)) return;
    const auto t0 = std::chrono::steady_clock::now();
    sam_drain_retiring();
    _sam_retire_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

void MaskSession::sam_drain_retiring() {
    if (!_sam_retiring) return;
    _sam_retiring->cancel();
    _sam_ops.release(*_sam_retiring);
    _sam_retiring.reset();
}

double MaskSession::sam_yield() {
    const auto t0 = std::chrono::steady_clock::now();
    sam_drain_retiring();
    if (_sam) {
        _sam->cancel();
        if (_sam_ops.release(*_sam)) _sam_dropped++;
    }
    _sam_release_pending = false;
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
        .count();
}

std::string MaskSession::sam_frame_stamp() const {
    return _doc ? std::to_string(_doc_gen) + "|" + _doc->key() : std::string();
}

const std::string& MaskSession::sam_model_path() const {
    return _sam ? _sam->model_path() : _sam_model;
}


float MaskSession::sam_margin() const { return _sam ? _sam->prompt().dilate_ratio : -1.0f; }

const spirula::i18n::Msg* MaskSession::sam_empty_note() const {
    if (_sam_results == 0 || _sam_last_area != 0) return nullptr;
    return _sam_last_vetoed ? &msg::sam_vetoed_all : &msg::sam_empty;
}

double MaskSession::sam_vram_mib() const { return _sam ? _sam->vram_mib() : -1.0; }

// A click joins the current object on this frame, and the prompt is every click
// that object has here, "not this" ones included. It is recorded only once its
// job starts. A point off the frame is ignored quietly: SAM answers it with speckle.
bool MaskSession::sam_prompt_point(float frame_x, float frame_y, Paint mode, bool positive) {
    if (!_doc || !_rgb || _idx < 0 || !sam_has_model() || _sam_release_pending) return false;
    if (!(frame_x >= 0.0f && frame_y >= 0.0f && frame_x < (float)_fw && frame_y < (float)_fh))
        return false;
    if (!sam_gate_passes()) return false;
    _sam_click_x = frame_x;
    _sam_click_y = frame_y;
    MaskSam& sam = this->sam();
    const std::string& camera = _frames[(size_t)_idx].camera;
    std::vector<SamPoint> points =
        sam.prompt_points(_idx, camera, SamPoint{frame_x, frame_y, positive});
    if (!sam.start_points(sam_frame_stamp(), _rgb, _fw, _fh, _doc->width(), _doc->height(),
                          std::move(points), mode, sam_prompt().dilate_ratio))
        return false;
    sam_prompt_started(frame_x, frame_y, positive);
    return true;
}

// A refusal lands in sam_error(); the device is frozen only once no blocker
// stands, so a paused prompt never commits the app to a device.
bool MaskSession::sam_gate_passes() {
    if (!_sam_blocker.empty()) {
        sam().refuse(_sam_blocker);
        return false;
    }
    if (!_sam_device_gate) return true;
    std::string device, error;
    if (!_sam_device_gate(device, error)) {
        sam().refuse(error);
        return false;
    }
    sam().set_device(device);
    return true;
}

void MaskSession::sam_prompt_started(float frame_x, float frame_y, bool positive) {
    MaskSam& sam = this->sam();
    sam.add_click(_idx, _frames[(size_t)_idx].camera, frame_x, frame_y, positive);
    _sam_job_object = sam.prompt().current_object;
    _sam_t0 = std::chrono::steady_clock::now();
}

// A list with no phrase in it (sam::split_phrases would drop every entry) is
// refused here: a job would pay a ~1.5 s encode to find nothing.
bool MaskSession::sam_prompt_text(const std::string& phrases) {
    if (!_doc || !_rgb || _idx < 0 || !sam_has_model() || _sam_release_pending) return false;
    if (sam_phrases_blank(phrases)) return false;
    if (!sam_gate_passes()) return false;
    if (!sam().start_text(sam_frame_stamp(), _rgb, _fw, _fh, _doc->width(), _doc->height(),
                          phrases, sam_prompt().dilate_ratio))
        return false;
    _sam_job_object = -1;
    _sam_t0 = std::chrono::steady_clock::now();
    return true;
}

bool MaskSession::sam_phrases_blank(const std::string& phrases) {
    return std::all_of(phrases.begin(), phrases.end(),
                       [](char c) { return c == ';' || c == ' ' || c == '\t'; });
}

bool MaskSession::sam_submit_text() {
    if (sam_busy() || sam_phrases_blank(sam_prompt().prompt)) return false;
    // Refused before the device gate, which would freeze a GPU for nothing.
    if (sam_has_model() && !sam_text_supported()) {
        sam().refuse(msg::sam_text_unsupported.get());
        return false;
    }
    return sam_prompt_text(sam_prompt().prompt);
}

std::string MaskSession::sam_text_refusal(bool has_model, bool text, const std::string& blocker,
                                          bool busy, const std::string& phrases) {
    if (!has_model) return spirula::i18n::msg::dataset::mask_model_first.get();
    if (!text) return msg::sam_text_unsupported.get();
    if (!blocker.empty()) return blocker;
    if (busy) return msg::sam_working.get();
    if (sam_phrases_blank(phrases)) return msg::sam_text_empty.get();
    return {};
}

std::string MaskSession::sam_text_refused() const {
    return sam_text_refusal(sam_has_model(), sam_text_supported(), _sam_blocker,
                            sam_busy() || _sam_release_pending || _sam_margin_pending,
                            _sam ? _sam->prompt().prompt : std::string());
}

Rect MaskSession::sam_pump() {
    if (!_sam) return {};
    // Held while the add is on top or a redo away; after that it never returns.
    if (!_sam_held.empty() && !sam_add_redoable()) _sam_held.clear();
    SamResult res;
    // A model change: the old model's answer, if any, is counted and dropped.
    if (_sam_release_pending) {
        if (_sam->busy()) return {};
        if (_sam->take_result(res)) _sam_dropped++;
        _sam->release();
        _sam_release_pending = false;
        return {};
    }
    if (!_doc) return {};
    // Read before the take: a job publishes before it stops, so a job idle here
    // has had its result taken below, and a margin job cannot overwrite it.
    const bool idle = !_sam->busy();
    Rect shown;
    if (_sam->take_result(res)) shown = sam_land(std::move(res));
    if (_sam_margin_pending && idle) sam_start_margin();
    return shown;
}

// A result that outlived its document -- a frame change, a revert, another
// dataset -- is counted and dropped, never painted onto what is open now; so
// is a margin that lands after another edit, rather than stacking on it.
Rect MaskSession::sam_land(SamResult res) {
    if (res.frame_key != sam_frame_stamp() || (res.margin_job && !sam_add_on_top(_sam_add_object))) {
        _sam_dropped++;
        // An undone add a redo can still bring back gets its detections back too.
        if (res.margin_job && res.frame_key == sam_frame_stamp() && sam_add_redoable())
            _sam_held = std::move(res.held);
        return {};
    }
    if (res.margin_job) {
        _sam_reapplies++;
        _sam_reapply_job_ms = res.ms;
        return apply_sam_add(std::move(res), _sam_add_object);
    }
    _sam_results++;
    _sam_last_job_ms = res.ms;
    _sam_last_score = res.score;
    _sam_last_detections = res.detections;
    const Rect shown = apply_sam_add(std::move(res), _sam_job_object);
    _sam_last_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - _sam_t0)
            .count();
    return shown;
}

void MaskSession::sam_start_margin() {
    _sam_margin_pending = false;
    if (!sam_margin_reapplies()) return;
    if (_sam->start_margin(sam_frame_stamp(), std::move(_sam_held), _doc->width(), _doc->height(),
                           _sam_add_mode, sam_prompt().dilate_ratio))
        _sam_margin_starts++;
}

// A job in flight lands as a plain add: the list's numbers now name other
// objects, so nothing may replace it (and drop its redo tail) later.
void MaskSession::sam_objects_edited() {
    _sam_add_object = -1;
    _sam_job_object = -1;
    _sam_held.clear();
}

// Kept, a reverted frame's clicks would re-prompt its next click with the
// correction just discarded. The phrase and exceptions are not per frame.
void MaskSession::sam_revert(int frame) {
    sam_objects_edited();
    _sam_add_key.clear();
    _sam_margin_pending = false;
    if (!_sam) return;
    MaskSettings& p = _sam->prompt();
    if (frame < 0) {
        const MaskSettings fresh;
        p.clicks.clear();
        p.object_count = fresh.object_count;
        p.current_object = fresh.current_object;
        return;
    }
    sam_forget_clicks({frame});
}

// sam_revert's per-frame half: only those frames' clicks, the object list kept.
void MaskSession::sam_forget_clicks(const std::vector<int>& frames) {
    if (!_sam) return;
    MaskSettings& p = _sam->prompt();
    p.clicks.erase(std::remove_if(p.clicks.begin(), p.clicks.end(),
                                  [&](const MaskClick& c) {
                                      return std::find(frames.begin(), frames.end(), c.frame) !=
                                             frames.end();
                                  }),
                   p.clicks.end());
}

// The stamp names the document (a reload moves it) and the step names the add
// itself, so "on top" means undo would take back exactly that add next.
bool MaskSession::sam_add_redoable() const {
    return _sam_add_object >= 0 && _doc && sam_frame_stamp() == _sam_add_key &&
           _doc->redo_reaches(_sam_add_step);
}

bool MaskSession::sam_add_on_top(int object) const {
    return object >= 0 && object == _sam_add_object && _doc && _doc->can_undo() &&
           sam_frame_stamp() == _sam_add_key && _doc->top_step() == _sam_add_step;
}

Rect MaskSession::apply_sam_add(SamResult res, int object) {
    _sam_last_area = 0;
    _sam_last_vetoed = res.vetoed_all;
    if (!_doc || !res.landed) return {};
    Rect changed;
    const bool replacing = sam_add_on_top(object);
    if (replacing) {
        _doc->undo();
        changed = _doc->last_change();
    }
    const uint64_t before = _doc->revision();
    _doc->paint(res.mode, std::move(res.stencil), res.bounds);
    // A paint that changed nothing records no step, so nothing of ours is on top,
    // and the add it replaced must not wait on the redo stack either.
    const bool painted = _doc->revision() != before;
    if (painted) changed = join(changed, _doc->last_change());
    else if (replacing) _doc->drop_redo();
    _sam_add_key = painted ? sam_frame_stamp() : std::string();
    _sam_add_step = _doc->top_step();
    _sam_add_object = painted ? object : -1;
    _sam_add_mode = res.mode;
    _sam_held = painted && object >= 0 ? std::move(res.held) : std::vector<HeldRegion>();
    _sam_last_area = res.set_px;
    return shown_rect(changed);
}

Paint MaskSession::sam_refine_mode(Paint fallback) const {
    return _sam && sam_add_on_top(_sam->prompt().current_object) ? _sam_add_mode : fallback;
}

Paint MaskSession::sam_click_mode(bool shift, bool ctrl) const {
    const Paint held = paint_now(shift, ctrl);
    return shift || ctrl ? held : sam_refine_mode(held);
}

bool MaskSession::sam_margin_reapplies() const {
    return !_sam_held.empty() && sam_add_on_top(_sam_add_object);
}

size_t MaskSession::sam_held_bytes() const {
    size_t n = 0;
    for (const HeldRegion& h : _sam_held) n += h.mask.size();
    return n;
}

// ---------------------------------------------------------------------------
// Slideshow
// ---------------------------------------------------------------------------

void MaskSession::start_slideshow() {
    if (_slide_playing || frame_count() < 2 || _idx < 0 || !idle() || sam_work_pending() ||
        _pen.in_progress() || _path.in_progress())
        return;
    // Both want the same memory and never need it at once.
    sam_yield();
    if (_doc && _doc->dirty()) save();
    _slide_index = _slide_pending = _idx;
    std::vector<SlideFrame> frames;
    for (const FrameRef& f : _frames)
        frames.push_back({f.file, mask_file(_mask_root, f.key), _mask_flipped});
    // The largest frame sets the budget: one header per camera, whose frames share a size.
    int w = 0, h = 0;
    std::vector<std::string> cameras;
    for (const FrameRef& f : _frames) {
        if (std::find(cameras.begin(), cameras.end(), f.camera) != cameras.end()) continue;
        cameras.push_back(f.camera);
        int fw = 0, fh = 0;
        if (app::image_size(f.file, fw, fh) && (long long)fw * fh > (long long)w * h) {
            w = fw;
            h = fh;
        }
    }
    _slide_threads = mask::slide_threads(w, h, std::thread::hardware_concurrency());
    const auto t0 = std::chrono::steady_clock::now();
    _slide.start(std::move(frames), _slide_threads);   // joins a halted playback's leftovers
    _slide_join_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    _slide_playing = true;
    _slide_fresh = true;
    _slide_need = false;
    _slide_started = _slide_last_shown = _slide_now = 0.0;
    _slide_first = true;
    _slide_max_gap = 0.0;
    _slide_stop_ms = -1.0;
    _slide_shown = 0;
    _slide_window = 0;
    _slide_src_w = 0;
    _slide_src_h = 0;
    _slide_tex_w = 0;   // the last playback's picture is not this one's first
    // One frame's buffers are not held while pictures stream.
    _doc.reset();
    _rgb.reset();
    std::vector<uint8_t>().swap(_rgba);
    std::vector<uint8_t>().swap(_rgba2);
    _slide_pic = Picture{};
    _win_dirty = true;
}

// halt(), not stop(): this runs inside an ImGui frame, and joining waited out
// an 8K decode in flight, 321 to 536 ms in the app.
void MaskSession::stop_slideshow() {
    if (!_slide_playing) return;
    const auto t0 = std::chrono::steady_clock::now();
    _slide.halt();
    _slide_pic = Picture{};
    _slide_stop_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    _slide_playing = false;
    const int i = _slide_index;
    _idx = -1;
    load_frame(i);
}

double MaskSession::slide_shown_fps() const {
    const double s = _slide_now - _slide_started;
    return _slide_shown > 1 && s > 0.0 ? (double)(_slide_shown - 1) / s : 0.0;
}

bool MaskSession::slideshow_tick(double now, int target_side, Picture& pic) {
    if (!_slide_playing) return false;
    const int n = frame_count();
    // Bounded by the ring's BYTES, not its slots: at a 4096 target an 8K
    // picture is 22 MB, so 64 MB holds three and a window of eleven thrashes.
    const int depth = slide_depth(slide_picture_bytes(_slide_src_w, _slide_src_h, target_side), n);
    _slide_now = now;
    _slide_window = depth;
    _slide.set_target(target_side);
    _slide_clock.fps = _slide_fps;
    if (_slide_first) {
        // Play queued this frame's save: a decode before it lands plays the
        // composite from before the edit, and holds the file the save renames over.
        if (!idle()) return false;
        _slide_first = false;
        _slide_started = now;
        // The one window that includes the frame shown: nothing is decoded yet.
        _slide.want(_slide_pending, depth);
        _slide_need = true;
    } else if (!_slide_need && _slide_clock.due(now)) {
        _slide_pending = (_slide_pending + 1) % n;
        _slide_need = true;
    }
    if (!_slide_need || !_slide.take(_slide_pending, pic)) return false;
    _slide_need = false;
    // Only after the take: a window moved first would drop this picture unshown.
    _slide.want((_slide_pending + 1) % n, depth);
    if (!pic.empty()) {
        _slide_src_w = pic.src_w;
        _slide_src_h = pic.src_h;
        _slide_index = _slide_pending;
        _slider_idx = _slide_index;
    }
    if (_slide_shown > 0) _slide_max_gap = std::max(_slide_max_gap, 1000.0 * (now - _slide_last_shown));
    _slide_last_shown = now;
    _slide_shown++;
    _slide_clock.start(now);
    return true;
}

}  // namespace mask
}  // namespace gui
