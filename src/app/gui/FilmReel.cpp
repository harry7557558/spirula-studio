// FilmReel.cpp -- see FilmReel.h.

#include "app/gui/FilmReel.h"

#include <algorithm>
#include <unordered_map>

namespace fs = std::filesystem;

namespace gui {

FilmReel::~FilmReel() { stop_loader(); }

// ---------------------------------------------------------------------------
// Producer
// ---------------------------------------------------------------------------

bool FilmReel::wants(double min_interval_s) const {
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lk(_mu);
    if (_ticked &&
        std::chrono::duration<double>(now - _last).count() < min_interval_s)
        return false;
    _ticked = true;
    _last = now;
    return true;
}

std::vector<KeyPoint2D> FilmReel::thin(const KeyPoint2D* pts, size_t count,
                                       int w, int h) {
    std::vector<KeyPoint2D> out;
    if (!pts || !count || w <= 0 || h <= 0) return out;
    const size_t step = count / kMaxOverlay + 1;
    const float span = (float)std::max(w, h);
    out.reserve(count / step + 1);
    for (size_t i = 0; i < count; i += step)
        out.push_back({pts[i].x / (float)w, pts[i].y / (float)h,
                       pts[i].r / span});
    return out;
}

void FilmReel::add(const FilmFrame& f, const uint8_t* rgb, int w, int h,
                   const uint8_t* mask, FramePoints points,
                   const uint8_t* feature_mask) {
    Picture pic;
    std::vector<KeyPoint2D> pts;
    if (rgb) {
        int target = 0;
        {
            std::lock_guard<std::mutex> lk(_mu);
            target = _target;
        }
        make_picture(rgb, w, h, mask, target, pic, feature_mask);
        pts = thin(points.pts, points.count, w, h);
    }
    append(f, std::move(pic), std::move(pts));
}

static void read_picture(const FilmFrame& f, int target, Picture& pic) {
    if (!f.panels.empty()) load_picture_row(f.panels, target, pic);
    else load_picture(f.image_path, f.mask_path, target, pic, false, f.feature_mask_path);
}

void FilmReel::add_loaded(const FilmFrame& f) {
    int target = 0;
    {
        std::lock_guard<std::mutex> lk(_mu);
        target = _target;
    }
    Picture pic;
    read_picture(f, target, pic);
    append(f, std::move(pic), {});
}

void FilmReel::append(const FilmFrame& f, Picture&& pic,
                      std::vector<KeyPoint2D>&& pts) {
    std::lock_guard<std::mutex> lk(_mu);
    const int index = (int)_frames.size();
    _frames.push_back(f);
    if (!pic.empty()) put(index, std::move(pic), std::move(pts));
}

void FilmReel::relist(std::vector<FilmFrame> frames) {
    int target = 0;
    bool follow = false;
    {
        std::lock_guard<std::mutex> lk(_mu);
        target = _target;
        follow = _follow;
    }
    // Following shows the newest picture HELD, which in a reordered list need
    // not be the last one.
    Picture last;
    if (follow && !frames.empty()) read_picture(frames.back(), target, last);
    std::unordered_map<std::string, int> at;
    for (size_t i = 0; i < frames.size(); i++) at.emplace(frames[i].image_path, (int)i);
    std::lock_guard<std::mutex> lk(_mu);
    const auto moved = [&](int old) {
        if (old < 0 || old >= (int)_frames.size()) return -1;
        const auto it = at.find(_frames[(size_t)old].image_path);
        return it == at.end() ? -1 : it->second;
    };
    for (Slot& s : _cache) {
        if (s.index < 0) continue;
        s.index = moved(s.index);
        if (s.index >= 0) continue;
        _bytes -= s.pic.bytes();
        s = Slot{};
    }
    _tex_index = moved(_tex_index);
    if (_tex_index < 0) {
        _tex_w = _tex_h = 0;
        _tex_pts.clear();
    }
    if (const int shown = moved(_shown); shown >= 0) _shown = shown;
    _want = -1;
    ++_relisted;
    _frames = std::move(frames);
    if (!last.empty()) put((int)_frames.size() - 1, std::move(last), {});
}

void FilmReel::clear() {
    stop_loader();
    std::lock_guard<std::mutex> lk(_mu);
    _frames.clear();
    for (Slot& s : _cache) s = Slot{};
    _bytes = 0;
    _clock = 0;
    _shown = -1;
    _follow = true;
    _want = -1;
    _tex_index = -1;
    _tex_w = _tex_h = 0;
    _tex_pts.clear();
    _ticked = false;
}

bool FilmReel::has_frames() const {
    std::lock_guard<std::mutex> lk(_mu);
    return !_frames.empty();
}

size_t FilmReel::size() const {
    std::lock_guard<std::mutex> lk(_mu);
    return _frames.size();
}

// ---------------------------------------------------------------------------
// The picture cache
// ---------------------------------------------------------------------------

FilmReel::Slot* FilmReel::find(int index) {
    if (index < 0) return nullptr;
    for (Slot& s : _cache)
        if (s.index == index) return &s;
    return nullptr;
}

void FilmReel::put(int index, Picture&& p, std::vector<KeyPoint2D>&& pts) {
    Slot* pick = find(index);
    if (!pick)
        for (Slot& s : _cache)
            if (s.index < 0) { pick = &s; break; }
    // Evict the least recently drawn until the new picture fits, and to find a
    // slot at all when every one is taken. What is on screen is safe: the
    // texture holds it, not the cache.
    while (!pick || _bytes + p.bytes() > kReelBudget) {
        Slot* lru = nullptr;
        for (Slot& s : _cache)
            if (s.index >= 0 && s.index != _tex_index && &s != pick &&
                (!lru || s.used < lru->used))
                lru = &s;
        if (!lru) break;
        _bytes -= lru->pic.bytes();
        *lru = Slot{};
        if (!pick) pick = lru;
    }
    if (!pick) return;
    _bytes -= pick->pic.bytes();
    pick->index = index;
    pick->pic = std::move(p);
    pick->pts = std::move(pts);
    pick->used = ++_clock;
    _bytes += pick->pic.bytes();
}

void FilmReel::start_loader() {
    if (_loader.joinable()) return;
    _stop = false;
    _loader = std::thread([this] { loader_loop(); });
}

void FilmReel::stop_loader() {
    {
        std::lock_guard<std::mutex> lk(_mu);
        _stop = true;
    }
    _cv.notify_all();
    if (_loader.joinable()) _loader.join();
    _stop = false;
}

void FilmReel::loader_loop() {
    for (;;) {
        FilmFrame f;
        int want = -1, target = 0;
        uint64_t listed = 0;
        {
            std::unique_lock<std::mutex> lk(_mu);
            _cv.wait(lk, [this] {
                return _stop || (_want >= 0 && _want < (int)_frames.size());
            });
            if (_stop) return;
            want = _want;
            target = _target;
            listed = _relisted;
            f = _frames[(size_t)want];
        }
        Picture pic;
        std::vector<KeyPoint2D> pts;
        // A decode that throws would be std::terminate off a worker thread.
        // An empty picture is the path a file that is not there already takes.
        try {
            if (!f.panels.empty()) {
                load_picture_row(f.panels, target, pic);
            } else if (load_picture(f.image_path, f.mask_path, target, pic, false,
                                    f.feature_mask_path) &&
                       !f.points_path.empty()) {
                std::vector<KeyPoint2D> kp;
                if (read_keypoints_file(f.points_path, pic.src_w, pic.src_h, kp))
                    pts = thin(kp.data(), kp.size(), pic.src_w, pic.src_h);
            }
        } catch (...) {
            pic = Picture();
        }
        std::unique_lock<std::mutex> lk(_mu);
        if (listed != _relisted) continue;
        if (pic.empty()) {
            // The producer writes its pictures through a queue, so the file
            // for the newest frame may not be there yet. Waiting beats caching
            // the failure: the next attempt is usually the one that works.
            _cv.wait_for(lk, std::chrono::milliseconds(300),
                         [this] { return _stop; });
            continue;
        }
        put(want, std::move(pic), std::move(pts));
        if (_want == want) _want = -1;
    }
}

// ---------------------------------------------------------------------------
// OutputWatch
// ---------------------------------------------------------------------------

OutputWatch::OutputWatch(FilmReel* reel, fs::path dir, fs::path images,
                         fs::path depths, bool fresh_only, bool atomic_outputs)
    : _reel(reel), _atomic_outputs(atomic_outputs), _dir(std::move(dir)), _images(std::move(images)),
      _depths(std::move(depths)) {
    if (!_reel || !fresh_only) return;
    for (const std::string& f : listing()) {
        std::error_code ec;
        _before[f] = fs::last_write_time(f, ec);
    }
}

std::vector<std::string> OutputWatch::listing() const {
    std::vector<std::string> out;
    std::error_code ec;
    for (fs::recursive_directory_iterator
             it(_dir, fs::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; it.increment(ec))
        if (it->is_regular_file(ec)) {
            const auto extension = it->path().extension().string();
            if (extension == ".png" || extension == ".jpg" || extension == ".jpeg")
                out.push_back(it->path().string());
        }
    return out;
}

void OutputWatch::poll(bool flush) {
    if (!_reel) return;
    const auto now = std::chrono::steady_clock::now();
    if (!flush && now - _last < std::chrono::seconds(1)) return;
    _last = now;

    publish(_ripe);
    _ripe.clear();

    std::vector<std::string> found;
    for (std::string& f : listing()) {
        if (_seen.count(f)) continue;
        const auto was = _before.find(f);
        std::error_code ec;
        if (was != _before.end() && fs::last_write_time(f, ec) == was->second) continue;
        found.push_back(std::move(f));
    }
    std::sort(found.begin(), found.end());

    if (!flush && !_atomic_outputs) {
        _ripe = std::move(found);
        return;
    }
    publish(found);
}

// The reel follows the newest picture it HOLDS, not the newest frame
// registered (FilmReel::draw), so the last of a batch is read here or the
// slider never leaves the first one it managed to load.
void OutputWatch::publish(const std::vector<std::string>& batch) {
    const bool want_pixels = !batch.empty() && _reel->wants();
    for (size_t i = 0; i < batch.size(); i++) {
        _seen.insert(batch[i]);
        FilmFrame f;
        f.name = fs::path(batch[i]).filename().string();
        f.panels = row_for(batch[i]);
        if (want_pixels && i + 1 == batch.size()) _reel->add_loaded(f);
        else                                      _reel->add(f);
    }
}

// The photograph, the normal map and the depth map of one frame. The map
// is what was found; the other two are derived from its path, since the
// three trees mirror each other (spirula geometry writes them that way).
std::vector<PicturePanel> OutputWatch::row_for(const std::string& map) const {
    std::error_code ec;
    fs::path rel = fs::relative(map, _dir, ec);
    if (ec || rel.empty()) rel = fs::path(map).filename();
    std::vector<PicturePanel> out;
    if (!_images.empty()) {
        // The extension is the capture's, not the map's.
        for (const char* ext : {".jpg", ".jpeg", ".png", ".webp", ".tif",
                                ".tiff", ".bmp", ".exr", ".JPG", ".PNG"}) {
            fs::path p = _images / rel;
            p.replace_extension(ext);
            if (!fs::is_regular_file(p, ec)) {
                p = _images / rel.filename();
                p.replace_extension(ext);
            }
            if (fs::is_regular_file(p, ec)) {
                out.push_back({p.string(), false});
                break;
            }
        }
    }
    const bool map_is_depth = _dir.filename() == "depths";
    out.push_back({map, map_is_depth});
    if (!map_is_depth && !_depths.empty()) {
        // Not probed: the writer pool may not have reached it yet, and a
        // panel that fails to load is skipped when the row is read.
        fs::path d = _depths / rel;
        d.replace_extension(".png");
        out.push_back({d.string(), true});
    }
    return out;
}

}  // namespace gui
