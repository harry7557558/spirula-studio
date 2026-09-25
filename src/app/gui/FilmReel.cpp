// FilmReel.cpp -- see FilmReel.h.

#include "app/gui/FilmReel.h"

#include "app/gui/FeatureOverlay.h"
#include "app/gui/Layout.h"
#include "app/gui/Ui.h"
#include "i18n/catalog/Dataset.h"

#include <algorithm>

namespace dmsg = spirula::i18n::msg::dataset;
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
                   const uint8_t* mask, FramePoints points) {
    Picture pic;
    std::vector<KeyPoint2D> pts;
    if (rgb) {
        int target = 0;
        {
            std::lock_guard<std::mutex> lk(_mu);
            target = _target;
        }
        make_picture(rgb, w, h, mask, target, pic);
        pts = thin(points.pts, points.count, w, h);
    }
    append(f, std::move(pic), std::move(pts));
}

void FilmReel::add_loaded(const FilmFrame& f) {
    int target = 0;
    {
        std::lock_guard<std::mutex> lk(_mu);
        target = _target;
    }
    Picture pic;
    if (!f.panels.empty()) load_picture_row(f.panels, target, pic);
    else load_picture(f.image_path, f.mask_path, target, pic);
    append(f, std::move(pic), {});
}

void FilmReel::append(const FilmFrame& f, Picture&& pic,
                      std::vector<KeyPoint2D>&& pts) {
    std::lock_guard<std::mutex> lk(_mu);
    const int index = (int)_frames.size();
    _frames.push_back(f);
    if (!pic.empty()) put(index, std::move(pic), std::move(pts));
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
        {
            std::unique_lock<std::mutex> lk(_mu);
            _cv.wait(lk, [this] {
                return _stop || (_want >= 0 && _want < (int)_frames.size());
            });
            if (_stop) return;
            want = _want;
            target = _target;
            f = _frames[(size_t)want];
        }
        Picture pic;
        std::vector<KeyPoint2D> pts;
        // A decode that throws would be std::terminate off a worker thread.
        // An empty picture is the path a file that is not there already takes.
        try {
            if (!f.panels.empty()) {
                load_picture_row(f.panels, target, pic);
            } else if (load_picture(f.image_path, f.mask_path, target, pic) &&
                       !f.points_path.empty()) {
                std::vector<KeyPoint2D> kp;
                if (read_keypoints_file(f.points_path, pic.src_w, pic.src_h, kp))
                    pts = thin(kp.data(), kp.size(), pic.src_w, pic.src_h);
            }
        } catch (...) {
            pic = Picture();
        }
        std::unique_lock<std::mutex> lk(_mu);
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
// The screen
// ---------------------------------------------------------------------------

bool FilmReel::draw(float height) {
    std::unique_lock<std::mutex> lk(_mu);
    const int n = (int)_frames.size();
    if (!n) return false;

    // Following means the newest picture that is actually here, not the newest
    // frame: the producer registers every frame and only offers a picture for
    // as many as the screen can keep up with, and chasing the rest to disk
    // would read the same file the producer is still writing.
    int newest = -1;
    for (const Slot& s : _cache) newest = std::max(newest, s.index);
    if (_follow) _shown = newest >= 0 ? newest : n - 1;
    _shown = std::clamp(_shown, 0, n - 1);

    ImGui::BeginChild("##reel", ImVec2(0, height));

    ImGui::BeginDisabled(n <= 1);
    if (ui::ButtonRaw("<") && _shown > 0) {
        _shown--;
        _follow = false;
    }
    ImGui::SameLine();
    if (ui::ButtonRaw(">") && _shown < n - 1) {
        _shown++;
        _follow = _shown == n - 1;
    }
    ImGui::SameLine();
    // "12 / 240" is two numbers and a separator: the same in every language.
    char fmt[32];
    std::snprintf(fmt, sizeof fmt, "%%d / %d", n);
    int one_based = _shown + 1;
    ImGui::SetNextItemWidth(px(190.0f));
    if (ui::SliderIntRaw("##reelidx", &one_based, 1, n, fmt)) {
        _shown = std::clamp(one_based - 1, 0, n - 1);
        _follow = _shown == n - 1;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ui::Checkbox(dmsg::reel_follow, &_follow) && _follow) _shown = n - 1;
    ui::help_on_hover(dmsg::reel_follow_help);

    // What the pane can show, rounded up so that dragging a splitter does not
    // reload the picture on every frame of the drag.
    const ImVec2 box = ImGui::GetContentRegionAvail();
    const int want_side =
        std::clamp((((int)std::max(box.x, box.y) + 255) / 256) * 256, 256, 4096);
    _target = want_side;

    Slot* slot = find(_shown);
    if (slot && !slot->pic.empty()) {
        slot->used = ++_clock;
        // Also when the same frame came back at another size, which is what a
        // resized pane asks the loader for below.
        if (_tex_index != slot->index || _tex_w != slot->pic.w ||
            _tex_h != slot->pic.h) {
            if (!_tex) glGenTextures(1, &_tex);
            glBindTexture(GL_TEXTURE_2D, _tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, slot->pic.w, slot->pic.h, 0,
                         GL_RGB, GL_UNSIGNED_BYTE, slot->pic.rgb.data());
            _tex_index = slot->index;
            _tex_w = slot->pic.w;
            _tex_h = slot->pic.h;
            _tex_pts = slot->pts;
        }
    }
    if (!slot || !slot->pic.fits(want_side)) {
        _want = _shown;
        start_loader();
        _cv.notify_all();
    }

    if (_tex && _tex_w > 0 && _tex_h > 0) {
        const std::string& name = _frames[(size_t)_tex_index].name;
        if (!name.empty()) {
            ImGui::SameLine();
            ui::TextDisabledRaw(
                elide_middle(name, ImGui::GetContentRegionAvail().x));
        }
        const float s = std::min(box.x / (float)_tex_w, box.y / (float)_tex_h);
        if (s > 0.0f) {
            const ImVec2 size(_tex_w * s, _tex_h * s);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (box.x - size.x) * 0.5f);
            const ImVec2 at = ImGui::GetCursorScreenPos();
            ImGui::Image((ImTextureID)(intptr_t)_tex, size);
            // Features drawn over the picture rather than baked into it: the
            // same picture is shown before extraction reaches it.
            draw_keypoints(ImGui::GetWindowDrawList(), _tex_pts, at, size,
                           IM_COL32(120, 255, 140, 220));
        }
    }

    ImGui::EndChild();
    return true;
}

void FilmReel::destroy_gl() {
    std::lock_guard<std::mutex> lk(_mu);
    if (_tex) glDeleteTextures(1, &_tex);
    _tex = 0;
    _tex_index = -1;
    _tex_w = _tex_h = 0;
    _tex_pts.clear();
}

// ---------------------------------------------------------------------------
// OutputWatch
// ---------------------------------------------------------------------------

OutputWatch::OutputWatch(FilmReel* reel, fs::path dir, fs::path images,
                         fs::path depths, bool fresh_only)
    : _reel(reel), _dir(std::move(dir)), _images(std::move(images)),
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
        if (it->is_regular_file(ec)) out.push_back(it->path().string());
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

    if (!flush) {
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
