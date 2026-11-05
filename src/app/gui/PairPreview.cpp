// PairPreview.cpp -- see PairPreview.h.

#include "app/gui/PairPreview.h"

#include "app/gui/FeatureOverlay.h"
#include "app/gui/Layout.h"
#include "app/gui/SfmProgress.h"
#include "app/gui/Ui.h"
#include "i18n/catalog/Dataset.h"

#ifdef SS_TOOL_SFM
#include "sfm/core/Matches.h"
#endif

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;
namespace dmsg = spirula::i18n::msg::dataset;

namespace gui {

namespace {

// Dots per picture and lines per pair. Both are what stays legible rather than
// what exists: eight thousand keypoints on a 400-pixel frame is a grey
// rectangle, and every one of them is a draw command.
constexpr size_t kMaxDots = 500;
constexpr size_t kMaxLines = 160;
constexpr size_t kFewLines = 40;

// The image a feature stem names. The stem carries the folder it came from, so
// only the extension is unknown -- and the run's own extractor found the file
// by walking, which is what makes probing the list here the cheap way back.
std::string image_for_stem(const std::string& dir, const std::string& stem) {
    static const char* const kExt[] = {".jpg", ".JPG", ".jpeg", ".png", ".PNG",
                                       ".webp", ".tif", ".tiff", ".bmp", ".exr"};
    std::error_code ec;
    for (const char* e : kExt) {
        const fs::path p = fs::path(dir) / (stem + e);
        if (fs::exists(p, ec)) return p.string();
    }
    return {};
}

// The image list as the last resort: a finished run deletes its feature files
// and its matches.bin, and those are what name the images. Walking the image
// tree gives the same order matching numbered by (sorted paths), give or take
// an image the extractor could not decode -- worth it, because the pictures
// are the half of this that survives the run.
std::vector<std::string> stems_from_images(const std::string& dir) {
    std::vector<fs::path> files;
    std::error_code ec;
    const fs::path root(dir);
    for (fs::recursive_directory_iterator
             it(root, fs::directory_options::follow_directory_symlink, ec), end;
         !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        std::string e = it->path().extension().string();
        for (char& c : e) c = (char)std::tolower((unsigned char)c);
        if (e == ".jpg" || e == ".jpeg" || e == ".png" || e == ".webp" ||
            e == ".tif" || e == ".tiff" || e == ".bmp" || e == ".exr")
            files.push_back(it->path());
    }
    std::sort(files.begin(), files.end());
    std::vector<std::string> stems;
    stems.reserve(files.size());
    for (const fs::path& f : files) {
        fs::path rel = f.lexically_relative(root);
        if (rel.empty() || *rel.begin() == "..") rel = f.filename();
        stems.push_back((rel.parent_path() / rel.stem()).generic_string());
    }
    return stems;
}

std::vector<KeyPoint2D> thin_points(const std::vector<KeyPoint2D>& pts,
                                    size_t most) {
    if (pts.size() <= most) return pts;
    const size_t step = pts.size() / most + 1;
    std::vector<KeyPoint2D> out;
    out.reserve(most + 1);
    for (size_t i = 0; i < pts.size(); i += step) out.push_back(pts[i]);
    return out;
}

}  // namespace

PairPreview::~PairPreview() { stop(); }

void PairPreview::configure(const std::string& image_dir,
                            const std::string& mask_dir,
                            const std::string& features_dir,
                            const std::string& matches_path,
                            const std::string& live_matches) {
    std::lock_guard<std::mutex> lk(_mu);
    if (_image_dir == image_dir && _mask_dir == mask_dir &&
        _features_dir == features_dir && _matches_path == matches_path &&
        _live_matches == live_matches)
        return;
    _image_dir = image_dir;
    _mask_dir = mask_dir;
    _features_dir = features_dir;
    _matches_path = matches_path;
    _live_matches = live_matches;
}

void PairPreview::show(const PairBlock& block) {
    {
        std::lock_guard<std::mutex> lk(_mu);
        if (_shot.loaded && _shot.block == block) return;
        if (_has_request && _req == block) return;
        if (_loading && _load == block) return;
        _req = block;
        _has_request = true;
    }
    start();
    _cv.notify_all();
}

void PairPreview::show_pair(uint32_t a, uint32_t b) {
    show(PairBlock{a, a + 1, b, b + 1});
}

void PairPreview::reload() {
    std::lock_guard<std::mutex> lk(_mu);
    _shot.loaded = false;
}

bool PairPreview::empty() const {
    std::lock_guard<std::mutex> lk(_mu);
    return !_shot.ready;
}

void PairPreview::clear() {
    stop();
    std::lock_guard<std::mutex> lk(_mu);
    _shot = Shot{};
    _result = Shot{};
    _result_new = false;
    _has_request = false;
    _loading = false;
    _stems.clear();
    _pairs.clear();
    _pairs_mtime = 0;
    _w_features_dir.clear();
    _w_matches_path.clear();
}

void PairPreview::start() {
    std::lock_guard<std::mutex> lk(_mu);
    if (_worker.joinable()) return;
    _stop = false;
    _worker = std::thread([this] { worker_loop(); });
}

void PairPreview::stop() {
    {
        std::lock_guard<std::mutex> lk(_mu);
        _stop = true;
    }
    _cv.notify_all();
    if (_worker.joinable()) _worker.join();
    _stop = false;
}

void PairPreview::worker_loop() {
    for (;;) {
        PairBlock block;
        {
            std::unique_lock<std::mutex> lk(_mu);
            _cv.wait(lk, [this] { return _stop || _has_request; });
            if (_stop) return;
            block = _load = _req;
            _has_request = false;
            _loading = true;
        }
        Shot shot;
        load(block, shot);
        std::lock_guard<std::mutex> lk(_mu);
        _loading = false;
        // A request for another pair that arrived while this one was loading
        // wins: the cursor has moved on and this is not what it is over.
        if (_has_request && _req != block) continue;
        _result = std::move(shot);
        _result_new = true;
    }
}

std::string PairPreview::index_pairs(const std::string& matches_path,
                                     const std::string& live_matches) {
#ifdef SS_TOOL_SFM
    if (matches_path.empty()) return {};
    // While matching runs there is no matches.bin yet, only the file the stage
    // is appending to; hovering a cell then still draws the pair's matches
    // rather than nothing (sfm/core/Progress.h, live_matches.bin).
    std::error_code ec;
    std::string src = matches_path;
    if (!fs::exists(src, ec) && !live_matches.empty() &&
        fs::exists(live_matches, ec))
        src = live_matches;
    const auto stamp = fs::last_write_time(src, ec);
    if (ec) return {};
    if (src != _pairs_src) {
        _pairs.clear();
        _pairs_mtime = 0;
        _pairs_src = src;
    }
    if (_pairs.empty() || stamp.time_since_epoch().count() != _pairs_mtime) {
        sfm::MatchesIndex idx;
        if (!sfm::indexMatches(src, idx)) return {};
        _pairs_mtime = stamp.time_since_epoch().count();
        _pairs.clear();
        _pairs.reserve(idx.pairs.size());
        for (const sfm::MatchesIndex::Entry& e : idx.pairs)
            _pairs.push_back({e.image1, e.image2, e.count, e.offset});
        std::sort(_pairs.begin(), _pairs.end(),
                  [](const PairEntry& x, const PairEntry& y) {
                      return x.a != y.a ? x.a < y.a : x.b < y.b;
                  });
    }
    return src;
#else
    (void)matches_path;
    (void)live_matches;
    return {};
#endif
}

void PairPreview::load(const PairBlock& block, Shot& out) {
    out = Shot{};
    out.block = block;
    out.loaded = true;

    std::string image_dir, mask_dir, features_dir, matches_path, live_matches;
    {
        std::lock_guard<std::mutex> lk(_mu);
        image_dir = _image_dir;
        mask_dir = _mask_dir;
        features_dir = _features_dir;
        matches_path = _matches_path;
        live_matches = _live_matches;
    }
    if (features_dir != _w_features_dir || matches_path != _w_matches_path) {
        _stems.clear();
        _pairs.clear();
        _pairs_mtime = 0;
        _w_features_dir = features_dir;
        _w_matches_path = matches_path;
    }
    // Out of range means the list was read before the extractor had finished
    // writing it, which is the ordinary case when matching has only just
    // started.
    if (_stems.empty() || std::max(block.r1, block.c1) > _stems.size()) {
        _stems = read_image_stems(features_dir, matches_path);
        if (_stems.empty()) _stems = stems_from_images(image_dir);
    }

    // A cell is a block of images once the capture outgrows the map, and its
    // first pair is often not one that matched: the strongest pair is drawn.
    const std::string src = index_pairs(matches_path, live_matches);
    (void)src;
    uint32_t a = block.r0, b = block.c0;
    const PairEntry* entry = nullptr;
    const auto in = [](uint32_t v, uint32_t lo, uint32_t hi) { return v >= lo && v < hi; };
    const uint32_t lo = std::min(block.r0, block.c0);
    const uint32_t hi = std::max(block.r1, block.c1);
    auto it = std::lower_bound(_pairs.begin(), _pairs.end(), lo,
                               [](const PairEntry& x, uint32_t v) { return x.a < v; });
    for (; it != _pairs.end() && it->a < hi; ++it) {
        const bool fwd = in(it->a, block.r0, block.r1) && in(it->b, block.c0, block.c1);
        const bool rev = in(it->b, block.r0, block.r1) && in(it->a, block.c0, block.c1);
        if ((!fwd && !rev) || (entry && it->count <= entry->count)) continue;
        entry = &*it;
        a = fwd ? it->a : it->b;
        b = fwd ? it->b : it->a;
    }
    // On the diagonal an image against itself is not a pair anybody wants to
    // look at -- its neighbour is what the band there is made of.
    if (!entry && a == b && _stems.size() > 1)
        b = std::min((uint32_t)_stems.size() - 1, a + 1);
    out.a = a;
    out.b = b;
    if (a >= _stems.size() || b >= _stems.size()) return;

    std::error_code ec;
    int target = 0;
    {
        std::lock_guard<std::mutex> lk(_mu);
        target = _target;
    }
    // Every keypoint, as fractions of its own picture: the match lines index
    // into this list, so it is thinned only for drawing.
    std::vector<KeyPoint2D> kp[2];
    Side* sides[2] = {&out.left, &out.right};
    const uint32_t ids[2] = {a, b};
    for (int k = 0; k < 2; k++) {
        const std::string& stem = _stems[ids[k]];
        Side& s = *sides[k];
        s.name = stem;
        std::string mask;
        if (!mask_dir.empty()) {
            const fs::path m = fs::path(mask_dir) / (stem + ".png");
            if (fs::exists(m, ec)) mask = m.string();
        }
        if (!load_picture(image_for_stem(image_dir, stem), mask, target, s.pic))
            continue;
        if (features_dir.empty()) continue;
        const std::string feat =
            (fs::path(features_dir) / (stem + ".bin")).string();
        std::vector<KeyPoint2D> px;
        if (!read_keypoints_file(feat, s.pic.src_w, s.pic.src_h, px)) continue;
        const float w = (float)std::max(1, s.pic.src_w);
        const float h = (float)std::max(1, s.pic.src_h);
        const float span = std::max(w, h);
        kp[k].reserve(px.size());
        for (const KeyPoint2D& q : px)
            kp[k].push_back({q.x / w, q.y / h, q.r / span});
        s.pts = thin_points(kp[k], kMaxDots);
    }
    out.ready = !out.left.pic.empty() || !out.right.pic.empty();

#ifdef SS_TOOL_SFM
    if (!entry || kp[0].empty() || kp[1].empty()) return;
    // Verification writes each pair once, in whichever order pairing chose.
    const bool flipped = entry->a != a;
    std::vector<sfm::FeatureMatch> m;
    if (!sfm::readPairMatches(src, {entry->a, entry->b, entry->count, entry->offset}, m))
        return;
    out.matches = m.size();
    const size_t step = m.size() / kMaxLines + 1;
    out.lines.reserve(m.size() / step * 4 + 4);
    for (size_t i = 0; i < m.size(); i += step) {
        // idx1 belongs to the pair's first image, which is not necessarily the
        // one drawn on the left.
        const uint32_t i0 = flipped ? m[i].idx2 : m[i].idx1;
        const uint32_t i1 = flipped ? m[i].idx1 : m[i].idx2;
        if (i0 >= kp[0].size() || i1 >= kp[1].size()) continue;
        out.lines.push_back(kp[0][i0].x);
        out.lines.push_back(kp[0][i0].y);
        out.lines.push_back(kp[1][i1].x);
        out.lines.push_back(kp[1][i1].y);
    }
#endif
}

// ---------------------------------------------------------------------------
// The screen
// ---------------------------------------------------------------------------

void PairPreview::upload(Pane& p, const Picture& t) {
    if (t.empty()) {
        p.w = p.h = 0;
        return;
    }
    if (!p.tex) glGenTextures(1, &p.tex);
    glBindTexture(GL_TEXTURE_2D, p.tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, t.w, t.h, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, t.rgb.data());
    p.w = t.w;
    p.h = t.h;
}

void PairPreview::draw_side(const Pane& p, const Side& s, const ImVec2& min,
                            const ImVec2& size) {
    if (!p.tex || size.x <= 0.0f) return;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddImage((ImTextureID)(intptr_t)p.tex, min,
                 ImVec2(min.x + size.x, min.y + size.y));
    if (_show_keypoints)
        draw_keypoints(dl, s.pts, min, size, IM_COL32(120, 255, 140, 190));
}

void PairPreview::draw(const ImVec2& box) {
    {
        std::lock_guard<std::mutex> lk(_mu);
        if (_result_new) {
            _shot = std::move(_result);
            _result = Shot{};
            _result_new = false;
            upload(_left, _shot.left.pic);
            upload(_right, _shot.right.pic);
        }
    }
    // What the pane can show, rounded up so that dragging a window edge does
    // not reload on every frame of the drag.
    {
        std::lock_guard<std::mutex> lk(_mu);
        _target = std::clamp(
            (((int)std::max(box.x, box.y) + 127) / 128) * 128, 256, 4096);
        // A pane that has outgrown these copies wants them again at the size
        // it is now; the next hover is what asks for them.
        if (_shot.ready && (!_shot.left.pic.fits(_target) ||
                            !_shot.right.pic.fits(_target)))
            _shot.loaded = false;
    }
    if (!_shot.ready) {
        ui::TextDisabledWrapped(dmsg::matrix_pair_hint);
        return;
    }

    const float gap = px(6.0f);
    const float line_h = ImGui::GetTextLineHeightWithSpacing();
    const float iw = (float)std::max(std::max(_left.w, _right.w), 1);
    const float ih = (float)std::max(std::max(_left.h, _right.h), 1);
    // Side by side or one above the other, whichever draws the pair larger:
    // the panel beside a square match map is about as tall as it is wide, and
    // two landscape frames fit it stacked. Each layout leaves room for the
    // names under the pictures and the count under the pair.
    const float across = std::min((box.x - gap) * 0.5f / iw,
                                  (box.y - 2.0f * line_h) / ih);
    const float down = std::min(box.x / iw,
                                (box.y - gap - 3.0f * line_h) / (2.0f * ih));
    const bool side_by_side = across >= down;
    const float scale = std::max(std::max(across, down), 0.0f);

    const ImVec2 org = ImGui::GetCursorScreenPos();
    const ImVec2 size_a((float)_left.w * scale, (float)_left.h * scale);
    const ImVec2 size_b((float)_right.w * scale, (float)_right.h * scale);
    const ImVec2 at_a = org;
    const ImVec2 at_b = side_by_side
                            ? ImVec2(org.x + iw * scale + gap, org.y)
                            : ImVec2(org.x, org.y + ih * scale + gap + line_h);
    ImGui::GetWindowDrawList()->PushClipRect(
        org, ImVec2(org.x + box.x, org.y + box.y), true);
    draw_side(_left, _shot.left, at_a, size_a);
    draw_side(_right, _shot.right, at_b, size_b);

    // The matches themselves, each a line from one picture to the other. What
    // they are for is their SHAPE: a pair that only matched along one edge, or
    // whose lines fan out at every angle, is a pair that will register badly.
    if (_show_matches && !_shot.lines.empty() && _left.tex && _right.tex) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const size_t n = _shot.lines.size() / 4;
        // A few dozen matches is the loop closure worth checking, and faint
        // lines lose it against the pictures: few get a colour each and rings.
        const bool few = n <= kFewLines;
        for (size_t k = 0; k + 3 < _shot.lines.size(); k += 4) {
            const ImVec2 p0(at_a.x + _shot.lines[k] * size_a.x,
                            at_a.y + _shot.lines[k + 1] * size_a.y);
            const ImVec2 p1(at_b.x + _shot.lines[k + 2] * size_b.x,
                            at_b.y + _shot.lines[k + 3] * size_b.y);
            if (!few) {
                dl->AddLine(p0, p1, IM_COL32(255, 210, 90, 110));
                continue;
            }
            float r, g, b;
            ImGui::ColorConvertHSVtoRGB((float)(k / 4) * 0.618034f, 0.75f, 1.0f, r, g, b);
            const ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, 0.9f));
            dl->AddLine(p0, p1, col, px(1.5f));
            dl->AddCircle(p0, px(3.5f), col, 0, px(1.5f));
            dl->AddCircle(p1, px(3.5f), col, 0, px(1.5f));
        }
    }
    ImGui::GetWindowDrawList()->PopClipRect();

    // The names under their own pictures, and the count under the pair. Every
    // move of the cursor is followed by an item, including the last one:
    // ImGui asserts on a SetCursorScreenPos that grows the parent with nothing
    // submitted after it, and a pair with no matches has nothing to say.
    const float name_w = side_by_side ? iw * scale : box.x;
    ImGui::SetCursorScreenPos(ImVec2(at_a.x, at_a.y + ih * scale));
    ui::TextDisabledRaw(elide_middle(_shot.left.name, name_w));
    ImGui::SetCursorScreenPos(ImVec2(at_b.x, at_b.y + ih * scale));
    ui::TextDisabledRaw(elide_middle(_shot.right.name, name_w));
    ImGui::SetCursorScreenPos(ImVec2(org.x, at_b.y + ih * scale + line_h));
    if (_shot.matches)
        ui::TextDisabled(dmsg::matrix_pair_matches, {(long long)_shot.matches});
    else
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
}

void PairPreview::destroy_gl() {
    for (Pane* p : {&_left, &_right}) {
        if (p->tex) glDeleteTextures(1, &p->tex);
        p->tex = 0;
        p->w = p->h = 0;
    }
}

}  // namespace gui
