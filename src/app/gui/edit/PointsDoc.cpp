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

    // One flag per camera of the DISPLAY dataset, which is the live subset in
    // its own order -- what the frusta are drawn from.
    _cam_highlight.assign((size_t)_display.num_cameras, 0);
    if (ck.empty()) return;
    const Selection& csel = sel_of(kCameras);
    size_t live = 0;
    for (size_t i = 0; i < ck.size() && live < _cam_highlight.size(); i++) {
        if (!ck[i]) continue;
        _cam_highlight[live++] = csel.weight((int64_t)i) ? 1 : 0;
    }
}

void PointsDoc::publish_impl(bool geometry) {
    if (!_show) return;
    const int64_t live_cams =
        layer_count() > kCameras ? alive_count_of(kCameras) : 0;
    const bool cameras_changed = geometry && live_cams != _live_cameras;
    _live_cameras = live_cams;
    rebuild_display(cameras_changed);
    _show(_display, _post_display,
          _cam_highlight.empty() ? nullptr : _cam_highlight.data());
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

void PointsDoc::revert_display() {
    if (_show) _show(_ds, _post, nullptr);
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

void PointsDoc::save(int target, const std::string& path,
                     std::atomic<int>* progress) {
    const std::vector<SaveTarget> t = save_targets();
    if (target < 0 || target >= (int)t.size()) return;
    if (t[(size_t)target].folder) {
        spirula::SparseKeep keep;
        keep.points = alive_of(kPoints);
        const std::vector<uint8_t>& ck = alive_of(kCameras);
        for (size_t i = 0; i < ck.size(); i++)
            if (!ck[i] && i < _ds.image_filenames.size())
                keep.drop_images.push_back(_ds.image_filenames[i]);
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
