// PointsDoc.cpp -- see PointsDoc.h.

#include "app/gui/edit/PointsDoc.h"

#include "i18n/catalog/Edit.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace fs = std::filesystem;
namespace msg = spirula::i18n::msg::edit;

namespace gui {

namespace {

constexpr uint8_t kTint[3] = {255, 108, 13};
// Frustum colours once any camera is marked; plain and selected are the
// renderer's own (PreviewRenderer::render).
constexpr float kFrustumPlain[3] = {1.0f, 0.62f, 0.25f};
constexpr float kFrustumSelected[3] = {0.25f, 0.92f, 1.0f};
constexpr float kFrustumHand[3] = {1.0f, 0.9f, 0.15f};
constexpr float kFrustumRepaired[3] = {0.35f, 1.0f, 0.45f};
constexpr float kFrustumFailed[3] = {1.0f, 0.3f, 0.3f};
constexpr float kFrustumGhost[3] = {0.45f, 0.45f, 0.45f};

// Keep the rows of every per-camera array a parser filled. A dataset that
// carries none of an optional array keeps carrying none.
template <typename T>
void keep_rows(std::vector<T>& v, int64_t n, int stride, const uint8_t* keep) {
    if ((int64_t)v.size() != n * stride) return;
    std::vector<T> out;
    out.reserve(v.size());
    for (int64_t i = 0; i < n; i++) {
        if (!keep[i]) continue;
        out.insert(out.end(), v.begin() + (ptrdiff_t)(i * stride),
                   v.begin() + (ptrdiff_t)((i + 1) * stride));
    }
    v.swap(out);
}

}  // namespace


PointsDoc::PointsDoc(ParsedDataset ds, PostSplitCameras post,
                     const std::string& source, const std::string& dataset_dir,
                     Show show)
    : _ds(std::move(ds)), _post(std::move(post)), _dataset_dir(dataset_dir),
      _show(std::move(show)) {
    if (!_dataset_dir.empty()) _fmt = spirula::sparse_format_of(_dataset_dir);

    // The preview draws in the normalized frame, so the selection has to
    // project there too: train_to_normalized is stored the other way round.
    double A[16];
    dsparse::train_to_normalized_inverse(_ds, A);
    auto map = [&A](const double* p, float* out) {
        for (int r = 0; r < 3; r++)
            out[r] = (float)(A[r*4+0]*p[0] + A[r*4+1]*p[1] + A[r*4+2]*p[2] +
                             A[r*4+3]);
    };

    const int64_t n = _ds.points.num();
    std::vector<float> pos((size_t)n * 3);
    for (int64_t i = 0; i < n; i++)
        map(&_ds.points.xyz[(size_t)i * 3], &pos[(size_t)i * 3]);
    set_source(source);
    add_layer(msg::elem_point, n, std::move(pos));

    // A camera is its centre: the translation column of its camera-to-world.
    const int64_t nc = _ds.num_cameras;
    if (nc > 0) {
        std::vector<float> cam((size_t)nc * 3);
        for (int64_t i = 0; i < nc; i++) {
            const double c[3] = {_ds.c2w[(size_t)i * 12 + 3],
                                 _ds.c2w[(size_t)i * 12 + 7],
                                 _ds.c2w[(size_t)i * 12 + 11]};
            map(c, &cam[(size_t)i * 3]);
        }
        add_layer(msg::elem_camera, nc, std::move(cam));
    }

    _display = _ds;
    _post_display = _post;
    for (int64_t i = 0; i < _ds.num_cameras; i++) _display_rows.push_back({Row::Live, i});
    rebuild_display(false);
}


// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------

void PointsDoc::rebuild_display(bool cameras_changed) {
    static const std::vector<uint8_t> kNoCameras;
    const std::vector<uint8_t>& pk = alive_of(kPoints);
    const std::vector<uint8_t>& ck =
        layer_count() > kCameras ? alive_of(kCameras) : kNoCameras;

    // Re-baking the split table costs what a dataset's camera count costs, so
    // it happens when that set changes rather than on every point deleted.
    if (cameras_changed && !ck.empty()) {
        _display = _ds;
        const int64_t nc = _ds.num_cameras;
        auto put = [&](int64_t i, const std::array<double, 12>& c) {
            for (int k = 0; k < 12; k++)
                _display.c2w[(size_t)i * 12 + k] =
                    (float)(c[(size_t)k] - (k % 4 == 3 ? _ds.center[(size_t)(k / 4)] : 0.0));
        };
        for (const auto& kv : _moved) put(kv.first, kv.second);
        for (const auto& kv : _preview.poses) put(kv.first, kv.second);
        keep_rows(_display.camera_models, nc, 1, ck.data());
        keep_rows(_display.camera_distortions, nc, 1, ck.data());
        keep_rows(_display.image_filenames, nc, 1, ck.data());
        keep_rows(_display.mask_filenames, nc, 1, ck.data());
        keep_rows(_display.depth_filenames, nc, 1, ck.data());
        keep_rows(_display.normal_filenames, nc, 1, ck.data());
        keep_rows(_display.widths, nc, 1, ck.data());
        keep_rows(_display.heights, nc, 1, ck.data());
        keep_rows(_display.c2w, nc, 12, ck.data());
        keep_rows(_display.intrins, nc, 4, ck.data());
        keep_rows(_display.dist_coeffs, nc, 8, ck.data());
        keep_rows(_display.redistort, nc, 1, ck.data());
        keep_rows(_display.exif_quarter_turns, nc, 1, ck.data());
        _display_rows.clear();
        for (int64_t i = 0; i < nc; i++)
            if (ck[(size_t)i]) _display_rows.push_back({Row::Live, i});
        // A camera that moved keeps a ghost where it was, so the step is visible.
        for (int64_t i = 0; i < nc; i++) {
            if (!ck[(size_t)i]) continue;
            const bool hand = _moved.count(i) != 0;
            const bool repaired = _preview.poses.count(i) != 0;
            if (!hand && !repaired) continue;
            append_camera(i, repaired && hand ? _moved.at(i) : original_pose(i));
            _display_rows.push_back({Row::Ghost, i});
        }
        for (size_t j = 0; j < _preview.added.size(); j++) {
            append_camera(_preview.added[j].like, _preview.added[j].pose);
            _display_rows.push_back({Row::Added, (int64_t)j});
        }
        int64_t nth = 0;
        for (const auto& kv : _placed) {
            append_camera(kv.second.like, kv.second.pose);
            _display_rows.push_back({Row::Placed, nth++});
        }
        _display.num_cameras = (int64_t)_display.widths.size();
        // The split table is derived, so it is rebuilt rather than filtered;
        // its rows are per FACE, which is not one per camera.
        _display.train_indices.resize((size_t)_display.num_cameras);
        for (size_t i = 0; i < _display.train_indices.size(); i++)
            _display.train_indices[i] = (int32_t)i;
        _display.val_indices.clear();
        _post_display = bake_post_split(_display, false, false);
    }

    // The points, filtered and tinted.
    const int64_t n = (int64_t)pk.size();
    ColmapPoints3D& out = _display.points;
    out.xyz.clear();
    out.rgb.clear();
    out.xyz.reserve((size_t)n * 3);
    out.rgb.reserve((size_t)n * 3);
    const uint8_t* psel_w = sel_of(kPoints).data();
    for (int64_t i = 0; i < n; i++) {
        if (!pk[(size_t)i]) continue;
        for (int k = 0; k < 3; k++) out.xyz.push_back(_ds.points.xyz[(size_t)i*3+k]);
        for (int k = 0; k < 3; k++) {
            const uint8_t base = _ds.points.rgb.empty()
                                     ? (uint8_t)200
                                     : _ds.points.rgb[(size_t)i * 3 + k];
            out.rgb.push_back(psel_w && psel_w[i] ? kTint[k] : base);
        }
    }

    // One flag per camera of the DISPLAY dataset -- the live subset in its own
    // order, then ghosts and added cameras -- which is what frusta are drawn from.
    _cam_highlight.assign((size_t)_display.num_cameras, 0);
    _cam_rgb.clear();
    if (ck.empty()) return;
    const Selection& csel = sel_of(kCameras);
    const bool marked = !_moved.empty() || !_preview.empty() || !_placed.empty();
    if (marked) _cam_rgb.resize((size_t)_display.num_cameras * 3);
    const int64_t current =
        _placed.count(_placed_current)
            ? (int64_t)std::distance(_placed.begin(), _placed.find(_placed_current))
            : -1;
    for (size_t r = 0; r < _display_rows.size() && r < _cam_highlight.size(); r++) {
        const DisplayRow& d = _display_rows[r];
        const bool sel = (d.kind == Row::Live && csel.weight(d.index)) ||
                         (d.kind == Row::Placed && d.index == current);
        _cam_highlight[r] = sel ? 1 : 0;
        if (!marked) continue;
        const float* c = kFrustumPlain;
        if (sel) c = kFrustumSelected;
        else if (d.kind == Row::Ghost) c = kFrustumGhost;
        else if (d.kind == Row::Added) c = kFrustumRepaired;
        else if (d.kind == Row::Placed) c = kFrustumHand;
        else if (_preview.marks.count(d.index))
            c = _preview.marks.at(d.index) == Mark::Failed ? kFrustumFailed : kFrustumRepaired;
        else if (_moved.count(d.index)) c = kFrustumHand;
        for (int k = 0; k < 3; k++) _cam_rgb[r * 3 + k] = c[k];
    }
}

std::array<double, 12> PointsDoc::original_pose(int64_t i) const {
    std::array<double, 12> c;
    for (int k = 0; k < 12; k++)
        c[(size_t)k] = (double)_ds.c2w[(size_t)i * 12 + k] +
                       (k % 4 == 3 ? _ds.center[(size_t)(k / 4)] : 0.0);
    return c;
}

// Row `like` of every per-camera array the parser filled, at pose `c2w` (raw frame).
void PointsDoc::append_camera(int64_t like, const std::array<double, 12>& c2w) {
    const int64_t nc = _ds.num_cameras;
    auto row = [&](auto& dst, const auto& src, int stride) {
        if ((int64_t)src.size() != nc * stride) return;
        dst.insert(dst.end(), src.begin() + (ptrdiff_t)(like * stride),
                   src.begin() + (ptrdiff_t)((like + 1) * stride));
    };
    row(_display.camera_models, _ds.camera_models, 1);
    row(_display.camera_distortions, _ds.camera_distortions, 1);
    row(_display.image_filenames, _ds.image_filenames, 1);
    row(_display.mask_filenames, _ds.mask_filenames, 1);
    row(_display.depth_filenames, _ds.depth_filenames, 1);
    row(_display.normal_filenames, _ds.normal_filenames, 1);
    row(_display.widths, _ds.widths, 1);
    row(_display.heights, _ds.heights, 1);
    row(_display.intrins, _ds.intrins, 4);
    row(_display.dist_coeffs, _ds.dist_coeffs, 8);
    row(_display.redistort, _ds.redistort, 1);
    row(_display.exif_quarter_turns, _ds.exif_quarter_turns, 1);
    for (int k = 0; k < 12; k++)
        _display.c2w.push_back(
            (float)(c2w[(size_t)k] - (k % 4 == 3 ? _ds.center[(size_t)(k / 4)] : 0.0)));
}

void PointsDoc::set_repair_preview(RepairPreview p) {
    _preview = std::move(p);
    _poses_dirty = true;
    mark_geometry_dirty();
}

void PointsDoc::set_placed(PlacedMap p) {
    _placed = std::move(p);
    _poses_dirty = true;
    mark_geometry_dirty();
}

void PointsDoc::set_placed_current(const std::string& name) {
    if (name == _placed_current) return;
    _placed_current = name;
    mark_geometry_dirty();
}

void PointsDoc::publish_impl(bool geometry) {
    if (!_show) return;
    const int64_t live_cams =
        layer_count() > kCameras ? alive_count_of(kCameras) : 0;
    const bool cameras_changed = geometry && (live_cams != _live_cameras || _poses_dirty);
    _poses_dirty = false;
    _live_cameras = live_cams;
    rebuild_display(cameras_changed);
    _show(_display, _post_display,
          _cam_highlight.empty() ? nullptr : _cam_highlight.data(),
          _cam_rgb.empty() ? nullptr : _cam_rgb.data());
}

// Points AND cameras, both filtered to what is live and both taken from the
// parsed dataset, which is the one frame they are already in together.
bool PointsDoc::live_centers(dsparse::CenterTable& out) const {
    const std::vector<uint8_t>& pk = alive_of(kPoints);
    std::vector<double> pts;
    for (size_t i = 0; i < pk.size(); i++) {
        if (!pk[i]) continue;
        pts.insert(pts.end(), _ds.points.xyz.begin() + (ptrdiff_t)(i * 3),
                   _ds.points.xyz.begin() + (ptrdiff_t)(i * 3 + 3));
    }
    std::vector<double> c2w;
    if (layer_count() > kCameras) {
        const std::vector<uint8_t>& ck = alive_of(kCameras);
        for (size_t i = 0; i < ck.size(); i++) {
            if (!ck[i]) continue;
            c2w.insert(c2w.end(), _ds.c2w.begin() + (ptrdiff_t)(i * 12),
                       _ds.c2w.begin() + (ptrdiff_t)(i * 12 + 12));
        }
    }
    if (pts.empty() && c2w.empty()) return false;
    double A[16];
    dsparse::train_to_normalized_inverse(_ds, A);
    out = dsparse::scene_centers(c2w.empty() ? nullptr : c2w.data(),
                                 (int64_t)c2w.size() / 12,
                                 pts.empty() ? nullptr : pts.data(),
                                 (int64_t)pts.size() / 3, 3, A);
    return true;
}

// RAW file coordinates -> the normalized frame: the parser's centring shift,
// then the inverse of train_to_normalized.
spirula::Sim3 PointsDoc::view_frame() const {
    double A[16];
    dsparse::train_to_normalized_inverse(_ds, A);
    spirula::Sim3 shift;
    for (int i = 0; i < 3; i++) shift.t[i] = -_ds.center[(size_t)i];
    return spirula::Sim3::from_3x4(A) * shift;
}

bool PointsDoc::up_hint(float up[3]) const {
    if (layer_count() <= kCameras) return false;
    const std::vector<uint8_t>& ck = alive_of(kCameras);
    double acc[3] = {0, 0, 0};
    for (size_t i = 0; i < ck.size(); i++) {
        if (!ck[i]) continue;
        // OpenGL camera-to-world: the second column is the camera's up.
        for (int r = 0; r < 3; r++) acc[r] += _ds.c2w[i * 12 + r * 4 + 1];
    }
    const spirula::Sim3 n = view_frame();
    double v[3];
    n.rotate(acc, v);
    const double len = std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    if (!(len > 1e-9)) return false;
    for (int r = 0; r < 3; r++) up[r] = (float)(v[r] / len);
    return true;
}

const spirula::SparseStats* PointsDoc::sparse_stats() const {
    if (!_stats_read) {
        _stats_read = true;
        try {
            if (!_dataset_dir.empty()) _stats = spirula::read_sparse_stats(_dataset_dir);
        } catch (const std::exception&) {
            _stats = spirula::SparseStats{};
        }
        // A model edited and saved by an earlier session still lines up; one
        // whose point count disagrees with what was parsed does not.
        if ((int64_t)_stats.track_beg.size() != _ds.points.num() + 1)
            _stats = spirula::SparseStats{};
    }
    return _stats.empty() ? nullptr : &_stats;
}

bool PointsDoc::colours(std::vector<float>& rgb) const {
    if (layer() != kPoints || _ds.points.rgb.empty()) return false;
    rgb.resize(_ds.points.rgb.size());
    for (size_t i = 0; i < rgb.size(); i++) rgb[i] = _ds.points.rgb[i] / 255.0f;
    return true;
}

// Live cameras only, read straight off the camera layer.
std::vector<float> PointsDoc::camera_centres() const {
    std::vector<float> out;
    if (layer_count() <= kCameras) return out;
    const std::vector<uint8_t>& ck = alive_of(kCameras);
    const float* p = positions_of(kCameras);
    for (size_t i = 0; i < ck.size(); i++)
        if (ck[i]) out.insert(out.end(), p + i * 3, p + i * 3 + 3);
    return out;
}

std::array<double, 12> PointsDoc::camera_pose(int64_t i) const {
    auto it = _moved.find(i);
    return it != _moved.end() ? it->second : original_pose(i);
}

void PointsDoc::set_moved_cameras(Poses p) {
    if (layer_count() <= kCameras) return;
    std::vector<int64_t> touched;
    for (const auto& kv : _moved) touched.push_back(kv.first);
    for (const auto& kv : p) touched.push_back(kv.first);
    _moved = std::move(p);
    double A[16];
    dsparse::train_to_normalized_inverse(_ds, A);
    for (int64_t i : touched) {
        const std::array<double, 12> c = camera_pose(i);
        const double q[3] = {c[3] - _ds.center[0], c[7] - _ds.center[1],
                             c[11] - _ds.center[2]};
        float pos[3];
        for (int r = 0; r < 3; r++)
            pos[r] = (float)(A[r*4+0]*q[0] + A[r*4+1]*q[1] + A[r*4+2]*q[2] + A[r*4+3]);
        set_position(kCameras, i, pos);
    }
    _poses_dirty = true;
    mark_geometry_dirty();
}

namespace {

class CameraMoveOp : public EditOp {
public:
    CameraMoveOp(PointsDoc::Poses before, PointsDoc::Poses after, std::string label)
        : _before(std::move(before)), _after(std::move(after)), _label(std::move(label)) {}
    void apply(EditDoc& d) override { static_cast<PointsDoc&>(d).set_moved_cameras(_after); }
    void undo(EditDoc& d) override { static_cast<PointsDoc&>(d).set_moved_cameras(_before); }
    std::string label() const override { return _label; }
    size_t bytes() const override {
        return (_before.size() + _after.size()) * (sizeof(int64_t) + 12 * sizeof(double));
    }

private:
    PointsDoc::Poses _before, _after;
    std::string _label;
};

class PlaceMissingOp : public EditOp {
public:
    PlaceMissingOp(PointsDoc::PlacedMap before, PointsDoc::PlacedMap after, std::string label)
        : _before(std::move(before)), _after(std::move(after)), _label(std::move(label)) {}
    void apply(EditDoc& d) override { static_cast<PointsDoc&>(d).set_placed(_after); }
    void undo(EditDoc& d) override { static_cast<PointsDoc&>(d).set_placed(_before); }
    std::string label() const override { return _label; }
    size_t bytes() const override {
        return (_before.size() + _after.size()) * (64 + sizeof(PointsDoc::Placed));
    }

private:
    PointsDoc::PlacedMap _before, _after;
    std::string _label;
};

}  // namespace

std::unique_ptr<EditOp> make_place_missing_op(PointsDoc& doc, PointsDoc::PlacedMap next,
                                              std::string label) {
    return std::make_unique<PlaceMissingOp>(doc.placed(), std::move(next), std::move(label));
}

std::unique_ptr<EditOp> make_camera_move_op(PointsDoc& doc, PointsDoc::Poses next,
                                            std::string label) {
    return std::make_unique<CameraMoveOp>(doc.moved_cameras(), std::move(next),
                                          std::move(label));
}

void PointsDoc::revert_display() {
    if (_show) _show(_ds, _post, nullptr, nullptr);
}


// ---------------------------------------------------------------------------
// Saving
// ---------------------------------------------------------------------------

std::vector<SaveTarget> PointsDoc::save_targets() const {
    std::vector<SaveTarget> t;
    switch (_fmt) {
        case spirula::SparseFormat::Colmap:
            t.push_back({&msg::target_colmap, "", true});
            break;
        case spirula::SparseFormat::Nerfstudio:
        case spirula::SparseFormat::Metashape:
            t.push_back({&msg::target_nerfstudio, "", true, false});
            break;
        default:
            break;
    }
    t.push_back({&msg::target_points_ply, ".ply", false});
    return t;
}

std::string PointsDoc::default_save_path(int target) const {
    const std::vector<SaveTarget> t = save_targets();
    if (target < 0 || target >= (int)t.size()) return {};
    return t[(size_t)target].folder ? _dataset_dir : source_path();
}

spirula::SparseKeep PointsDoc::sparse_keep() const {
    spirula::SparseKeep keep;
    keep.points = alive_of(kPoints);
    if (layer_count() <= kCameras) return keep;
    const std::vector<uint8_t>& ck = alive_of(kCameras);
    for (size_t i = 0; i < ck.size(); i++)
        if (!ck[i] && i < _ds.image_filenames.size())
            keep.drop_images.push_back(_ds.image_filenames[i]);
    return keep;
}

void PointsDoc::save(int target, const std::string& path,
                     std::atomic<int>* progress) {
    const std::vector<SaveTarget> t = save_targets();
    if (target < 0 || target >= (int)t.size()) return;
    if (t[(size_t)target].folder) {
        const spirula::SparseKeep keep = sparse_keep();
        const spirula::Sim3 moved = file_placement();
        std::error_code ec;
        if (fs::equivalent(path, _dataset_dir, ec)) {
            spirula::sparse_write_filtered(_dataset_dir, keep, &moved, &_baseline);
        } else {
            spirula::sparse_write_copy(_dataset_dir, path, keep, &moved, &_baseline);
            // A copy trains from the same pictures. Best effort: where links
            // cannot be made, the trainer is pointed at the source's folders.
            for (const char* sub : {"images", "masks"}) {
                const fs::path from = fs::path(_dataset_dir) / sub;
                if (fs::is_directory(from, ec) && !fs::exists(fs::path(path) / sub, ec))
                    fs::create_directory_symlink(fs::absolute(from, ec), fs::path(path) / sub, ec);
            }
        }
        if (progress) (*progress)++;
        return;
    }
    // A loose PLY is in the parsed frame, which the centring shift left.
    spirula::Sim3 shift;
    for (int i = 0; i < 3; i++) shift.t[i] = _ds.center[(size_t)i];
    const spirula::Sim3 moved = file_placement() * shift;
    spirula::write_ply_points(
        path, _ds.points.xyz.data(),
        _ds.points.rgb.empty() ? nullptr : _ds.points.rgb.data(),
        _ds.points.num(), alive_of(kPoints).data(), &moved);
    if (progress) (*progress)++;
}

}  // namespace gui
