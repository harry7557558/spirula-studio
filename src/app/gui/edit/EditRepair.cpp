// EditRepair.cpp -- the editing session's Repair tab: cameras the SfM put in
// the wrong place or left out, fixed against the rest of the model
// (sfm/Repair.h) and kept or discarded as a whole.

#include "app/gui/edit/EditSession.h"

#include "app/gui/GlLoader.h"
#include "app/gui/Ui.h"
#include "app/gui/ViewportPanel.h"
#include "core/CameraModel.h"
#include "data/DatasetParser.h"
#include "data/SparseEdit.h"
#include "i18n/catalog/Edit.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <atomic>
#include <chrono>
#include <mutex>
#include <filesystem>
#include <set>
#include <system_error>

namespace fs = std::filesystem;
namespace msg = spirula::i18n::msg::edit;
using spirula::Sim3;
using spirula::i18n::Msg;

namespace gui {

namespace {

// The files a repair writes, which are what Keep replaces.
// Long edge of the photo panel's copy: a sidebar is a few hundred pixels.
constexpr int kPhotoSide = 1024;

constexpr const char* kModelFiles[] = {"cameras.bin", "images.bin", "points3D.bin", "rigs.txt",
                                       "gauge.txt"};

fs::path fresh_stage() {
    std::error_code ec;
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path p = fs::temp_directory_path(ec) / ("spirula-repair-" + std::to_string(now));
    fs::remove_all(p, ec);
    fs::create_directories(p, ec);
    return p;
}

// OpenGL camera-to-world (raw file frame) moved by `m`, as COLMAP's world-to-camera.
void to_colmap(const std::array<double, 12>& c2w, const Sim3& m, RepairRequest::Hint& h) {
    double R[9], C[3] = {c2w[3], c2w[7], c2w[11]}, Cm[3];
    for (int r = 0; r < 3; r++)
        for (int k = 0; k < 3; k++) {
            double v = 0;
            for (int j = 0; j < 3; j++) v += m.R[r * 3 + j] * c2w[(size_t)(j * 4 + k)];
            R[r * 3 + k] = k == 0 ? v : -v;  // OpenGL -> OpenCV: y and z flip
        }
    m.apply(C, Cm);
    for (int r = 0; r < 3; r++)
        for (int k = 0; k < 3; k++) h.R[r * 3 + k] = R[k * 3 + r];
    for (int r = 0; r < 3; r++)
        h.t[r] = -(h.R[r * 3 + 0] * Cm[0] + h.R[r * 3 + 1] * Cm[1] + h.R[r * 3 + 2] * Cm[2]);
}

using Pose12 = std::array<double, 12>;
using Quat = std::array<double, 4>;  // w, x, y, z

// COLMAP's world-to-camera as an OpenGL camera-to-world, moved by `m`.
Pose12 from_colmap(const ColmapImage& im, const Sim3& m) {
    const double w = im.qvec[0], x = im.qvec[1], y = im.qvec[2], z = im.qvec[3];
    const double R[9] = {1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y),
                         2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x),
                         2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y)};
    double C[3], Cm[3];
    for (int k = 0; k < 3; k++)
        C[k] = -(R[0 * 3 + k] * im.tvec[0] + R[1 * 3 + k] * im.tvec[1] + R[2 * 3 + k] * im.tvec[2]);
    m.apply(C, Cm);
    Pose12 c;
    for (int r = 0; r < 3; r++) {
        for (int k = 0; k < 3; k++) {
            double v = 0;  // m.R * R^T, then OpenCV -> OpenGL
            for (int j = 0; j < 3; j++) v += m.R[r * 3 + j] * R[k * 3 + j];
            c[(size_t)(r * 4 + k)] = k == 0 ? v : -v;
        }
        c[(size_t)(r * 4 + 3)] = Cm[r];
    }
    return c;
}

// A camera-to-world carried by `m`: turned and moved, never scaled.
Pose12 carried(const Sim3& m, const Pose12& c) {
    Pose12 n;
    for (int r = 0; r < 3; r++)
        for (int k = 0; k < 3; k++) {
            double v = 0;
            for (int j = 0; j < 3; j++) v += m.R[r * 3 + j] * c[(size_t)(j * 4 + k)];
            n[(size_t)(r * 4 + k)] = v;
        }
    const double C[3] = {c[3], c[7], c[11]};
    double Cn[3];
    m.apply(C, Cn);
    for (int r = 0; r < 3; r++) n[(size_t)(r * 4 + 3)] = Cn[r];
    return n;
}

Quat quat_of(const double* m, int stride) {
    auto at = [&](int r, int k) { return m[r * stride + k]; };
    const double tr = at(0, 0) + at(1, 1) + at(2, 2);
    Quat q;
    if (tr > 0) {
        const double s = std::sqrt(tr + 1) * 2;
        q = {0.25 * s, (at(2, 1) - at(1, 2)) / s, (at(0, 2) - at(2, 0)) / s, (at(1, 0) - at(0, 1)) / s};
    } else if (at(0, 0) > at(1, 1) && at(0, 0) > at(2, 2)) {
        const double s = std::sqrt(1 + at(0, 0) - at(1, 1) - at(2, 2)) * 2;
        q = {(at(2, 1) - at(1, 2)) / s, 0.25 * s, (at(0, 1) + at(1, 0)) / s, (at(0, 2) + at(2, 0)) / s};
    } else if (at(1, 1) > at(2, 2)) {
        const double s = std::sqrt(1 + at(1, 1) - at(0, 0) - at(2, 2)) * 2;
        q = {(at(0, 2) - at(2, 0)) / s, (at(0, 1) + at(1, 0)) / s, 0.25 * s, (at(1, 2) + at(2, 1)) / s};
    } else {
        const double s = std::sqrt(1 + at(2, 2) - at(0, 0) - at(1, 1)) * 2;
        q = {(at(1, 0) - at(0, 1)) / s, (at(0, 2) + at(2, 0)) / s, (at(1, 2) + at(2, 1)) / s, 0.25 * s};
    }
    return q;
}

void rot_of(Quat q, double* m, int stride) {
    const double n = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    const double w = q[0] / n, x = q[1] / n, y = q[2] / n, z = q[3] / n;
    const double R[9] = {1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y),
                         2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x),
                         2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y)};
    for (int r = 0; r < 3; r++)
        for (int k = 0; k < 3; k++) m[r * stride + k] = R[r * 3 + k];
}

Quat slerp(const Quat& a, Quat b, double t) {
    double d = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
    if (d < 0) {
        for (double& v : b) v = -v;
        d = -d;
    }
    double wa = 1 - t, wb = t;
    if (d < 0.9995) {
        const double th = std::acos(d), s = std::sin(th);
        wa = std::sin((1 - t) * th) / s;
        wb = std::sin(t * th) / s;
    }
    return {wa * a[0] + wb * b[0], wa * a[1] + wb * b[1], wa * a[2] + wb * b[2], wa * a[3] + wb * b[3]};
}

double angle_deg(const Quat& a, const Quat& b) {
    const double d = std::fabs(a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]);
    return 2 * std::acos(std::min(1.0, d)) * 180.0 / M_PI;
}

// The similarity taking `src` onto `dst`: rotation from the orientations (a
// straight path leaves the centres no roll), scale and shift from the centres,
// refitted without the pairs that disagree.
bool fit_poses(const std::vector<Pose12>& src, const std::vector<Pose12>& dst, Sim3& out) {
    std::vector<size_t> use(src.size());
    for (size_t k = 0; k < use.size(); k++) use[k] = k;
    std::vector<Quat> rel(src.size());
    for (size_t k = 0; k < src.size(); k++) {
        double M[9];  // dst * src^T
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++) {
                double v = 0;
                for (int j = 0; j < 3; j++) v += dst[k][(size_t)(r * 4 + j)] * src[k][(size_t)(c * 4 + j)];
                M[r * 3 + c] = v;
            }
        rel[k] = quat_of(M, 3);
    }
    for (int pass = 0; pass < 3; pass++) {
        if (use.size() < 3) return false;
        Quat sum{0, 0, 0, 0};
        const Quat& first = rel[use[0]];
        for (size_t k : use) {
            const double d = first[0] * rel[k][0] + first[1] * rel[k][1] + first[2] * rel[k][2] +
                             first[3] * rel[k][3];
            for (size_t i = 0; i < 4; i++) sum[i] += d < 0 ? -rel[k][i] : rel[k][i];
        }
        rot_of(sum, out.R, 3);
        double ms[3] = {0, 0, 0}, md[3] = {0, 0, 0};
        for (size_t k : use)
            for (int i = 0; i < 3; i++) {
                ms[i] += src[k][(size_t)(i * 4 + 3)] / (double)use.size();
                md[i] += dst[k][(size_t)(i * 4 + 3)] / (double)use.size();
            }
        double num = 0, den = 0, spread = 0;
        for (size_t k : use) {
            const double a[3] = {src[k][3] - ms[0], src[k][7] - ms[1], src[k][11] - ms[2]};
            double ra[3];
            out.rotate(a, ra);
            for (int i = 0; i < 3; i++) {
                const double b = dst[k][(size_t)(i * 4 + 3)] - md[i];
                num += ra[i] * b;
                den += a[i] * a[i];
                spread += b * b;
            }
        }
        if (!(den > 0) || !(num > 0)) return false;
        out.s = num / den;
        double rms[3];
        out.rotate(ms, rms);
        for (int i = 0; i < 3; i++) out.t[i] = md[i] - out.s * rms[i];
        spread = std::sqrt(spread / (double)use.size()) + 1e-12;

        const Quat fq = quat_of(out.R, 3);
        std::vector<double> rot(src.size()), pos(src.size());
        for (size_t k = 0; k < src.size(); k++) {
            rot[k] = angle_deg(rel[k], fq);
            const Pose12 m = carried(out, src[k]);
            pos[k] = std::sqrt(std::pow(m[3] - dst[k][3], 2) + std::pow(m[7] - dst[k][7], 2) +
                               std::pow(m[11] - dst[k][11], 2)) / spread;
        }
        auto median = [&](const std::vector<double>& v) {
            std::vector<double> s;
            for (size_t k : use) s.push_back(v[k]);
            std::nth_element(s.begin(), s.begin() + (ptrdiff_t)(s.size() / 2), s.end());
            return s[s.size() / 2];
        };
        const double rg = std::max(2.0, 3 * median(rot)), pg = std::max(0.02, 3 * median(pos));
        std::vector<size_t> keep;
        for (size_t k = 0; k < src.size(); k++)
            if (rot[k] <= rg && pos[k] <= pg) keep.push_back(k);
        if (keep == use) break;
        use.swap(keep);
    }
    return use.size() >= 3;
}

// A dataset path as the model names it: relative to the image folder.
std::string under(const fs::path& images, const std::string& file) {
    const std::string s = fs::path(file).lexically_relative(images).generic_string();
    return s.empty() || s.rfind("..", 0) == 0 ? fs::path(file).filename().generic_string() : s;
}

int64_t first_selected_camera(const EditDoc& d) {
    const std::vector<uint8_t>& alive = d.alive_of(1);
    const Selection& sel = d.sel_of(1);
    for (int64_t i = 0; i < (int64_t)alive.size(); i++)
        if (alive[(size_t)i] && sel.selected(i)) return i;
    return -1;
}

const Msg& outcome_label(const std::string& o) {
    if (o == "moved") return msg::outcome_moved;
    if (o == "kept") return msg::outcome_kept;
    if (o == "added") return msg::outcome_added;
    if (o == "removed") return msg::outcome_removed;
    return msg::outcome_failed;
}

}  // namespace


bool EditSession::repair_available() {
    if (_repair_avail >= 0) return _repair_avail == 1;
    _repair_avail = 0;
    if (!_doc || _doc->kind() != EditDoc::Kind::Points || _doc->layer_count() < 2) return false;
    const auto& pd = static_cast<const PointsDoc&>(*_doc);
    if (pd.format() != spirula::SparseFormat::Colmap || pd.dataset_dir().empty()) return false;
    std::error_code ec;
    const fs::path ds(pd.dataset_dir());
    _repair_model = find_colmap_model(pd.dataset_dir(), "");
    if (_repair_model.empty() || !fs::exists(fs::path(_repair_model) / "images.bin", ec) ||
        !fs::exists(ds / "matches.bin", ec) || !fs::is_directory(ds / "features", ec))
        return false;
    _repair_avail = 1;
    return true;
}

bool EditSession::camera_mode() const {
    return _tab == 2 && _doc && _doc->kind() == EditDoc::Kind::Points && _doc->layer() == 1 &&
           !_doc->sel().empty();
}

void EditSession::begin_camera_move() {
    _cam_xform = camera_mode();
    if (!_cam_xform) return;
    const auto& pd = static_cast<const PointsDoc&>(*_doc);
    _cam_from = pd.moved_cameras();
    _cam_start.clear();
    const uint8_t* alive = _doc->alive();
    for (int64_t i = 0; i < _doc->count(); i++)
        if (alive[i] && _doc->sel().selected(i)) _cam_start[i] = pd.camera_pose(i);
}

// shared = base * placement * positions, positions = view_frame * raw.
PointsDoc::Poses EditSession::moved_by(const Sim3& shared_step) const {
    const Sim3 bp = base_frame() * _doc->placement();
    const Sim3 vf = _doc->view_frame();
    const Sim3 step = vf.inverse() * bp.inverse() * shared_step * bp * vf;
    PointsDoc::Poses out = _cam_from;
    for (const auto& kv : _cam_start) {
        const std::array<double, 12>& c = kv.second;
        std::array<double, 12> n;
        for (int r = 0; r < 3; r++)
            for (int k = 0; k < 3; k++) {
                double v = 0;
                for (int j = 0; j < 3; j++) v += step.R[r * 3 + j] * c[(size_t)(j * 4 + k)];
                n[(size_t)(r * 4 + k)] = v;
            }
        const double C[3] = {c[3], c[7], c[11]};
        double Cn[3];
        step.apply(C, Cn);
        for (int r = 0; r < 3; r++) n[(size_t)(r * 4 + 3)] = Cn[r];
        out[kv.first] = n;
    }
    return out;
}


// ---------------------------------------------------------------------------
// Running
// ---------------------------------------------------------------------------

void EditSession::start_repair(RepairKind kind) {
    if (!_doc || busy() || !repair_available()) return;
    auto& pd = static_cast<PointsDoc&>(*_doc);
    discard_repair();

    RepairRequest rq;
    rq.workspace = pd.dataset_dir();
    rq.match = _repair_match;
    std::error_code ec;
    if (fs::is_directory(fs::path(rq.workspace) / "images", ec))
        rq.image_dir = (fs::path(rq.workspace) / "images").string();
    const spirula::SparseKeep keep = pd.sparse_keep();
    rq.exclude = keep.drop_images;
    const uint8_t* alive = pd.alive_of(1).data();
    const Selection& sel = pd.sel_of(1);
    const Sim3 moved = _doc->file_placement();
    switch (kind) {
        case RepairKind::Replace:
            for (int64_t i = 0; i < (int64_t)pd.alive_of(1).size(); i++)
                if (alive[i] && sel.selected(i)) rq.replace.push_back(pd.image_file(i));
            break;
        case RepairKind::Snap:
            for (const auto& kv : pd.moved_cameras()) {
                if (!alive[kv.first]) continue;
                RepairRequest::Hint h;
                h.name = pd.image_file(kv.first);
                to_colmap(kv.second, moved, h);
                rq.hints.push_back(std::move(h));
            }
            break;
        case RepairKind::Missing:
            rq.add_missing = true;
            for (const auto& kv : pd.placed()) {
                RepairRequest::Hint h;
                h.name = (fs::path(rq.workspace) / "images" / kv.first).string();
                to_colmap(kv.second.pose, moved, h);
                rq.hints.push_back(std::move(h));
            }
            break;
        case RepairKind::Audit: rq.audit_all = true; break;
    }
    if (rq.replace.empty() && rq.hints.empty() && !rq.add_missing && !rq.audit_all) return;

    // The repair starts from what is on screen: deletions and the placement
    // are written into a copy first, exactly as Save a copy would write them.
    const bool edited = !moved.is_identity() || !keep.drop_images.empty() ||
                        std::find(keep.points.begin(), keep.points.end(), 0) != keep.points.end();
    const fs::path stage = fresh_stage();
    _repair_stage = stage.string();
    _repair_moved = moved;
    rq.output_dir = (stage / "out").string();
    _repair_busy = true;
    _repair_cancel = false;
    _repair_error.clear();
    _status = msg::repair_running.get();
    _status_err = false;
    const std::string dataset = pd.dataset_dir();
    _repair_worker = std::thread([this, rq, keep, moved, edited, stage, dataset]() mutable {
        struct Done {
            std::atomic<bool>& f;
            ~Done() { f = false; }
        } done{_repair_busy};
        try {
            if (edited) {
                spirula::sparse_write_copy(dataset, (stage / "in").string(), keep, &moved);
                rq.model_dir = (stage / "in" / "sparse" / "0").string();
            } else {
                rq.model_dir = _repair_model;
            }
            std::vector<RepairResultLine> report;
            _repair_result = run_repair_in_process(
                rq,
                [this](const std::string& line) {
                    std::lock_guard<std::mutex> lk(_repair_log_mtx);
                    _repair_log.push_back(line);
                },
                _repair_cancel, &report);
            _repair_report = std::move(report);
        } catch (const std::exception& e) {
            _repair_result = InProcessResult{};
            _repair_result.exit_code = 2;
            _repair_result.error = e.what();
        }
    });
}

void EditSession::poll_repair() {
    if (_repair_busy.load() || !_repair_worker.joinable()) return;
    _repair_worker.join();
    if (_repair_result.cancelled) {
        discard_repair();
        _status.clear();
        return;
    }
    if (_repair_result.exit_code != 0) {
        const std::string why = _repair_result.error.empty()
                                    ? std::to_string(_repair_result.exit_code)
                                    : _repair_result.error;
        _status = spirula::i18n::format(msg::repair_failed, {why});
        _status_err = true;
        note(_status);
        discard_repair();
        return;
    }
    long long n[5] = {0, 0, 0, 0, 0};
    for (const RepairResultLine& l : _repair_report)
        n[l.outcome == "moved" ? 0 : l.outcome == "added" ? 1 : l.outcome == "kept" ? 2
          : l.outcome == "removed" ? 4 : 3]++;
    _status = spirula::i18n::format(msg::repair_result, {n[0], n[1], n[2], n[3], n[4]});
    _status_err = false;
    note(_status);
    _repair_shown = true;
    show_repair_preview();
}

// The repaired poses drawn over the model, each moved camera beside a ghost of
// where it was. The result was written in the staged frame: placement undone.
void EditSession::show_repair_preview() {
    if (!_doc || _doc->kind() != EditDoc::Kind::Points) return;
    auto& pd = static_cast<PointsDoc&>(*_doc);
    std::map<int32_t, ColmapImage> imgs;
    try {
        imgs = read_images_binary((fs::path(_repair_stage) / "out").string());
    } catch (const std::exception&) {
        return;
    }
    std::map<std::string, const ColmapImage*> by_name;
    for (const auto& kv : imgs) by_name[kv.second.name] = &kv.second;
    const Sim3 back = _repair_moved.inverse();
    auto pose_of = [&](const ColmapImage& im) { return from_colmap(im, back); };
    // Report names are the model's, relative to the image folder; the dataset's are paths.
    auto camera_of = [&](const std::string& name) -> int64_t {
        std::string want = name;
        std::replace(want.begin(), want.end(), '\\', '/');
        const std::vector<uint8_t>& alive = pd.alive_of(1);
        for (int64_t i = 0; i < (int64_t)alive.size(); i++) {
            std::string f = pd.image_file(i);
            std::replace(f.begin(), f.end(), '\\', '/');
            if (f.size() >= want.size() &&
                f.compare(f.size() - want.size(), want.size(), want) == 0 &&
                (f.size() == want.size() || f[f.size() - want.size() - 1] == '/'))
                return i;
        }
        return -1;
    };
    PointsDoc::RepairPreview pv;
    for (const RepairResultLine& l : _repair_report) {
        const auto it = by_name.find(l.name);
        const int64_t cam = camera_of(l.name);
        if (l.outcome == "failed" || l.outcome == "removed") {
            if (cam >= 0) pv.marks[cam] = PointsDoc::Mark::Failed;
        } else if (it != by_name.end() && cam >= 0 && l.outcome == "moved") {
            pv.poses[cam] = pose_of(*it->second);
            pv.marks[cam] = PointsDoc::Mark::Moved;
        } else if (it != by_name.end() && cam < 0 && l.outcome == "added") {
            pv.added.push_back({l.name, like_of(l.name), pose_of(*it->second)});
        }
    }
    pd.set_repair_preview(std::move(pv));
}

void EditSession::keep_repair() {
    if (!_repair_shown || _repair_model.empty()) return;
    const fs::path out = fs::path(_repair_stage) / "out";
    const fs::path dst(_repair_model);
    std::error_code ec;
    for (const char* f : kModelFiles) {
        if (!fs::exists(out / f, ec)) continue;
        const fs::path orig = dst / (std::string(f) + ".orig");
        if (fs::exists(dst / f, ec) && !fs::exists(orig, ec)) fs::copy_file(dst / f, orig, ec);
        fs::copy_file(out / f, dst / f, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            _status = spirula::i18n::format(msg::repair_failed, {ec.message()});
            _status_err = true;
            return;
        }
    }
    _status = spirula::i18n::format(msg::repair_wrote, {_repair_model});
    _status_err = false;
    note(_status);
    auto& pd = static_cast<PointsDoc&>(*_doc);
    _placed_carry = pd.placed();
    for (const RepairResultLine& l : _repair_report)
        if (l.outcome == "added") {
            std::string n = l.name;
            std::replace(n.begin(), n.end(), '\\', '/');
            _placed_carry.erase(n);
        }
    _placed_carry_dir = pd.dataset_dir();
    discard_repair();
    _saved_over_source = true;
    _doc->mark_saved();
    if (_on_model_replaced) _on_model_replaced();
}

void EditSession::discard_repair() {
    if (_doc && _doc->kind() == EditDoc::Kind::Points &&
        !static_cast<PointsDoc&>(*_doc).repair_preview().empty())
        static_cast<PointsDoc&>(*_doc).set_repair_preview({});
    _repair_shown = false;
    _repair_report.clear();
    if (!_repair_stage.empty()) {
        std::error_code ec;
        fs::remove_all(_repair_stage, ec);
        _repair_stage.clear();
    }
}


// ---------------------------------------------------------------------------
// The camera's photograph
// ---------------------------------------------------------------------------

void EditSession::step_camera(int dir) {
    const std::vector<uint8_t>& alive = _doc->alive_of(1);
    const int64_t n = (int64_t)alive.size();
    if (n == 0) return;
    int64_t at = -1;
    for (int64_t i = 0; i < n && at < 0; i++)
        if (alive[(size_t)i] && _doc->sel().selected(i)) at = i;
    for (int64_t k = 1; k <= n; k++) {
        const int64_t i = ((at < 0 ? (dir > 0 ? -1 : 0) : at) + dir * k % n + n) % n;
        if (!alive[(size_t)i]) continue;
        std::vector<uint8_t> w((size_t)n, 0);
        w[(size_t)i] = 255;
        run_select(std::move(w), (dir > 0 ? msg::photo_next : msg::photo_prev).get());
        return;
    }
}

// The navigation camera put where camera `i` stands, with its lens: what the
// model looks like from there, beside its photograph, is what shows it wrong.
void EditSession::look_through(int64_t i) {
    look_from(static_cast<const PointsDoc&>(*_doc).camera_pose(i), i, true);
}

// The view given camera `like`'s lens and, with `move`, the pose `c2w` (raw frame).
void EditSession::look_from(const std::array<double, 12>& c2w, int64_t like, bool move) {
    if (!_panel) return;
    const auto& pd = static_cast<const PointsDoc&>(*_doc);
    const ParsedDataset& ds = pd.parsed();
    if (move) {
        const Pose12 c = carried(base_frame() * _doc->placement() * _doc->view_frame(), c2w);
        float pose[12], target[3];
        for (int k = 0; k < 12; k++) pose[k] = (float)c[(size_t)k];
        const double reach = 0.5 * (double)_doc->extent() * (base_frame() * _doc->placement()).s;
        for (int r = 0; r < 3; r++)
            target[r] = (float)(c[(size_t)(r * 4 + 3)] - c[(size_t)(r * 4 + 2)] * reach);
        _panel->set_nav_pose(pose, target);
    }
    if (like >= 0 && like < ds.num_cameras) {
        const int model = ds.camera_models.empty() ? 0 : (int)ds.camera_models[(size_t)like];
        const double W = ds.widths[(size_t)like], fx = std::max(1e-6f, ds.intrins[(size_t)like * 4]);
        double fov = 0;
        switch ((CameraModelType)model) {
            case CameraModelType::PINHOLE: fov = 2 * std::atan(W / (2 * fx)); break;
            case CameraModelType::FISHEYE: fov = W / fx; break;
            case CameraModelType::EQUISOLID: fov = 4 * std::asin(std::min(1.0, W / (4 * fx))); break;
            case CameraModelType::EQUIRECTANGULAR: fov = 0; break;
        }
        _panel->set_view_lens(model, (float)(fov * 180.0 / M_PI));
    }
    _panel->invalidate();
}

bool EditSession::photo_of(const std::string& path) {
    // One load at a time; the newest request is fetched once it finishes.
    _photo.want = path;
    if (!_photo.busy.load() && _photo.worker.joinable()) _photo.worker.join();
    if (!_photo.busy.load() && _photo.loaded != _photo.want) {
        _photo.busy = true;
        _photo.worker = std::thread([this, path] {
            Picture p;
            const bool ok = load_picture(path, "", kPhotoSide, p);
            {
                std::lock_guard<std::mutex> lk(_photo.mu);
                if (!ok) p = Picture{};
                _photo.pic = std::move(p);
                _photo.loaded = path;
            }
            _photo.busy = false;
        });
    }
    std::lock_guard<std::mutex> lk(_photo.mu);
    if (_photo.shown != _photo.loaded) {
        _photo.shown = _photo.loaded;
        _photo.w = _photo.pic.w;
        _photo.h = _photo.pic.h;
        if (!_photo.pic.empty()) {
            GLuint tex = _photo.tex;
            if (!tex) glGenTextures(1, &tex);
            _photo.tex = tex;
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, _photo.w, _photo.h, 0, GL_RGB,
                         GL_UNSIGNED_BYTE, _photo.pic.rgb.data());
        }
    }
    return _photo.shown == path && _photo.tex && _photo.w > 0;
}

// The photo across the view with the lens look_from gave it: a perspective
// lens spans the width, an equirect the whole frame.
void EditSession::draw_photo_overlay(const ViewportOverlay& v) {
    if (_tab != 2 || !_photo_overlay || !_panel || !_photo.tex || _photo.w <= 0 ||
        _photo.shown.empty())
        return;
    float y = v.y, h = v.h;
    if ((CameraModelType)_panel->view_model() != CameraModelType::EQUIRECTANGULAR) {
        h = v.w * (float)_photo.h / (float)_photo.w;
        y = v.y + (v.h - h) * 0.5f;
    }
    v.dl->PushClipRect(ImVec2(v.x, v.y), ImVec2(v.x + v.w, v.y + v.h), true);
    v.dl->AddImage((ImTextureID)(intptr_t)_photo.tex, ImVec2(v.x, y), ImVec2(v.x + v.w, y + h),
                   ImVec2(0, 0), ImVec2(1, 1),
                   IM_COL32(255, 255, 255, (int)(std::clamp(_photo_alpha, 0.0f, 1.0f) * 255)));
    v.dl->PopClipRect();
}

void EditSession::draw_camera_photo(float full) {
    const ImGuiStyle& st = ImGui::GetStyle();
    const float third = (full - st.ItemSpacing.x * 2) / 3.0f;
    auto& pd = static_cast<PointsDoc&>(*_doc);
    const int64_t cam = _doc->layer() == 1 ? first_selected_camera(pd) : -1;

    ui::SeparatorText(msg::sec_camera_photo);
    ui::TextDisabledWrapped(msg::photo_legend);
    if (cam < 0) {
        ui::TextDisabledWrapped(msg::photo_none);
        return;
    }
    const bool ready = photo_of(pd.image_file(cam));

    ui::TextRaw(fs::path(pd.image_file(cam)).filename().string());
    const PointsDoc::RepairPreview& pv = pd.repair_preview();
    const auto mark = pv.marks.find(cam);
    if (mark != pv.marks.end())
        ui::TextColoredWrapped(mark->second == PointsDoc::Mark::Failed ? ImVec4(1, 0.4f, 0.4f, 1)
                                                                       : ImVec4(0.4f, 1, 0.5f, 1),
                               mark->second == PointsDoc::Mark::Failed ? msg::photo_failed
                                                                       : msg::photo_repaired);
    else if (pd.moved_cameras().count(cam))
        ui::TextColoredWrapped(ImVec4(1, 0.9f, 0.2f, 1), msg::photo_hand_moved);

    if (ready)
        ImGui::Image((ImTextureID)(intptr_t)_photo.tex,
                     ImVec2(full, full * (float)_photo.h / (float)_photo.w));
    else
        ui::TextDisabled(msg::photo_loading);
    if (ui::Button(msg::photo_prev, ImVec2(third, 0))) step_camera(-1);
    ImGui::SameLine();
    if (ui::Button(msg::photo_look, ImVec2(third, 0))) look_through(cam);
    ui::help_on_hover(msg::photo_look_help);
    ImGui::SameLine();
    if (ui::Button(msg::photo_next, ImVec2(third, 0))) step_camera(1);
}


// ---------------------------------------------------------------------------
// Images the model left out
// ---------------------------------------------------------------------------

// Every image with features that no camera of the document is: the model's
// own leftovers, not the ones deleted here.
void EditSession::read_missing() {
    _missing_read = true;
    _missing.clear();
    _missing_at = -1;
    const auto& pd = static_cast<const PointsDoc&>(*_doc);
    const fs::path ds(pd.dataset_dir());
    const fs::path images = ds / "images", feats = ds / "features";
    _images_dir = images.string();
    std::set<std::string> have;
    for (size_t i = 0; i < pd.parsed().image_filenames.size(); i++)
        have.insert(under(images, pd.parsed().image_filenames[i]));
    std::error_code ec;
    for (fs::recursive_directory_iterator it(images, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        const std::string rel = it->path().lexically_relative(images).generic_string();
        // Features are named after the image, with or without its extension.
        if (have.count(rel) || (!fs::exists(feats / (rel + ".bin"), ec) &&
                                !fs::exists(feats / fs::path(rel).replace_extension(".bin"), ec)))
            continue;
        _missing.push_back(rel);
    }
    std::sort(_missing.begin(), _missing.end());
}

// The live camera just before `name` in file order (else just after): what an
// image of the same capture is drawn with.
int64_t EditSession::like_of(const std::string& name) const {
    const auto& pd = static_cast<const PointsDoc&>(*_doc);
    const std::vector<uint8_t>& alive = pd.alive_of(1);
    const fs::path images = fs::path(pd.dataset_dir()) / "images";
    std::string n = name;
    std::replace(n.begin(), n.end(), '\\', '/');
    int64_t below = -1, above = -1, any = -1;
    std::string bn, an;
    for (int64_t i = 0; i < (int64_t)alive.size(); i++) {
        if (!alive[(size_t)i]) continue;
        if (any < 0) any = i;
        const std::string f = under(images, pd.image_file(i));
        if (f < n) {
            if (below < 0 || f > bn) below = i, bn = f;
        } else if (above < 0 || f < an) {
            above = i, an = f;
        }
    }
    return below >= 0 ? below : above >= 0 ? above : std::max<int64_t>(any, 0);
}

void EditSession::pick_missing(int k) {
    auto& pd = static_cast<PointsDoc&>(*_doc);
    _missing_at = k;
    _missing_cam = first_selected_camera(pd);
    pd.set_placed_current(k >= 0 ? _missing[(size_t)k] : std::string());
    if (k < 0) return;
    const auto it = pd.placed().find(_missing[(size_t)k]);
    if (it != pd.placed().end())
        look_from(it->second.pose, it->second.like, true);
    else
        look_from({}, like_of(_missing[(size_t)k]), false);
}

// The image put where the view stands, looking the way it looks.
void EditSession::place_at_view() {
    if (!_panel || _missing_at < 0) return;
    auto& pd = static_cast<PointsDoc&>(*_doc);
    float c2w[12], target[3];
    _panel->nav_pose(c2w, target);
    Pose12 c;
    for (int k = 0; k < 12; k++) c[(size_t)k] = c2w[k];
    const Sim3 to_raw = (base_frame() * _doc->placement() * _doc->view_frame()).inverse();
    const std::string& name = _missing[(size_t)_missing_at];
    PointsDoc::PlacedMap next = pd.placed();
    next[name] = {like_of(name), carried(to_raw, c)};
    _doc->run(make_place_missing_op(pd, std::move(next), msg::op_place_missing.get()));
}

// Unplaced images where another model of this dataset has them, lined up on
// the cameras both share; the rest between their neighbours in file order.
void EditSession::guess_missing() {
    auto& pd = static_cast<PointsDoc&>(*_doc);
    const fs::path ds(pd.dataset_dir()), images = ds / "images";
    const std::vector<uint8_t>& alive = pd.alive_of(1);
    std::map<std::string, int64_t> cams;
    for (int64_t i = 0; i < (int64_t)alive.size(); i++)
        if (alive[(size_t)i]) cams[under(images, pd.image_file(i))] = i;
    PointsDoc::PlacedMap next = pd.placed();
    std::vector<std::string> todo;
    for (const std::string& m : _missing)
        if (!next.count(m)) todo.push_back(m);

    long long from_model = 0, between = 0;
    std::error_code ec;
    for (fs::directory_iterator it(ds / "sparse", ec), end; !ec && it != end && !todo.empty();
         it.increment(ec)) {
        const fs::path dir = it->path();
        std::error_code e2;
        if (!fs::exists(dir / "images.bin", e2) || fs::equivalent(dir, _repair_model, e2)) continue;
        std::map<int32_t, ColmapImage> imgs;
        try {
            imgs = read_images_binary(dir.string());
        } catch (const std::exception&) {
            continue;
        }
        std::map<std::string, const ColmapImage*> by_name;
        std::vector<Pose12> src, dst;
        for (const auto& kv : imgs) {
            std::string n = kv.second.name;
            std::replace(n.begin(), n.end(), '\\', '/');
            by_name[n] = &kv.second;
            const auto c = cams.find(n);
            if (c == cams.end()) continue;
            src.push_back(from_colmap(kv.second, Sim3()));
            dst.push_back(pd.camera_pose(c->second));
        }
        Sim3 T;
        if (!fit_poses(src, dst, T)) continue;
        std::vector<std::string> rest;
        for (const std::string& m : todo) {
            const auto b = by_name.find(m);
            if (b == by_name.end()) {
                rest.push_back(m);
                continue;
            }
            next[m] = {like_of(m), from_colmap(*b->second, T)};
            from_model++;
        }
        todo.swap(rest);
    }

    // How far along between its neighbours, counted in images.
    std::vector<std::string> order;
    for (const auto& kv : cams) order.push_back(kv.first);
    order.insert(order.end(), _missing.begin(), _missing.end());
    std::sort(order.begin(), order.end());
    auto rank = [&](const std::string& n) {
        return (double)(std::lower_bound(order.begin(), order.end(), n) - order.begin());
    };
    for (const std::string& m : todo) {
        const auto hi = cams.lower_bound(m);
        const bool has_hi = hi != cams.end(), has_lo = hi != cams.begin();
        if (!has_hi && !has_lo) continue;
        if (!has_hi || !has_lo) {
            const int64_t n = has_hi ? hi->second : std::prev(hi)->second;
            next[m] = {n, pd.camera_pose(n)};
            between++;
            continue;
        }
        const auto lo = std::prev(hi);
        const Pose12 a = pd.camera_pose(lo->second), b = pd.camera_pose(hi->second);
        const double t = (rank(m) - rank(lo->first)) / std::max(1.0, rank(hi->first) - rank(lo->first));
        Pose12 c;
        rot_of(slerp(quat_of(a.data(), 4), quat_of(b.data(), 4), t), c.data(), 4);
        for (int r = 0; r < 3; r++)
            c[(size_t)(r * 4 + 3)] = a[(size_t)(r * 4 + 3)] + t * (b[(size_t)(r * 4 + 3)] - a[(size_t)(r * 4 + 3)]);
        next[m] = {lo->second, c};
        between++;
    }
    if (from_model + between == 0) return;
    _doc->run(make_place_missing_op(pd, std::move(next), msg::op_guess_missing.get()));
    _status = spirula::i18n::format(msg::missing_guessed, {from_model, between});
    _status_err = false;
    note(_status);
}

void EditSession::draw_missing_section(float full) {
    const ImGuiStyle& st = ImGui::GetStyle();
    const float half = (full - st.ItemSpacing.x) * 0.5f;
    auto& pd = static_cast<PointsDoc&>(*_doc);
    if (!_missing_read) read_missing();
    ui::SeparatorText(msg::sec_missing);
    if (_missing.empty()) {
        ui::TextDisabledWrapped(msg::missing_none);
        return;
    }
    // A camera picked in the view takes the photo back.
    if (_missing_at >= 0 && first_selected_camera(pd) != _missing_cam) pick_missing(-1);
    ui::TextDisabledWrapped(msg::missing_help);
    const PointsDoc::PlacedMap& placed = pd.placed();
    ui::Text(msg::missing_count, {(long long)_missing.size(), (long long)placed.size()});
    ImGui::BeginChild("##missing",
                      ImVec2(0, (float)std::clamp((int)_missing.size(), 3, 8) *
                                    ImGui::GetTextLineHeightWithSpacing()),
                      ImGuiChildFlags_Borders);
    int pick = -2;
    ImGuiListClipper clip;
    clip.Begin((int)_missing.size());
    while (clip.Step())
        for (int k = clip.DisplayStart; k < clip.DisplayEnd; k++) {
            const bool on = placed.count(_missing[(size_t)k]) != 0;
            if (on) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.9f, 0.2f, 1));
            if (ui::SelectableRaw(_missing[(size_t)k], k == _missing_at))
                pick = k == _missing_at ? -1 : k;
            if (on) ImGui::PopStyleColor();
        }
    ImGui::EndChild();
    if (pick != -2) pick_missing(pick);
    if (ui::Button(msg::missing_guess, ImVec2(full, 0))) guess_missing();
    ui::help_on_hover(msg::missing_guess_help);
    if (_missing_at < 0) return;

    const std::string name = _missing[(size_t)_missing_at];
    if (ui::Button(msg::missing_place, ImVec2(half, 0))) place_at_view();
    ui::help_on_hover(msg::missing_place_help);
    ImGui::SameLine();
    const bool is_placed = pd.placed().count(name) != 0;
    ImGui::BeginDisabled(!is_placed);
    if (ui::Button(msg::missing_clear, ImVec2(half, 0))) {
        PointsDoc::PlacedMap next = pd.placed();
        next.erase(name);
        _doc->run(make_place_missing_op(pd, std::move(next), msg::missing_clear.get()));
    }
    ImGui::EndDisabled();
    if (pd.placed().count(name))
        ui::TextColoredWrapped(ImVec4(1, 0.9f, 0.2f, 1), msg::missing_placed);
    else
        ui::TextDisabledWrapped(msg::missing_unplaced);
    ui::TextRaw(name);
    if (photo_of((fs::path(_images_dir) / name).string()))
        ImGui::Image((ImTextureID)(intptr_t)_photo.tex,
                     ImVec2(full, full * (float)_photo.h / (float)_photo.w));
    else
        ui::TextDisabled(msg::photo_loading);
}


// ---------------------------------------------------------------------------
// The tab
// ---------------------------------------------------------------------------

void EditSession::draw_repair_tab(float full) {
    const ImGuiStyle& st = ImGui::GetStyle();
    const float half = (full - st.ItemSpacing.x) * 0.5f;
    ui::TextDisabledWrapped(msg::repair_help);
    if (!repair_available()) {
        ui::TextDisabledWrapped(msg::repair_unavailable);
        return;
    }
    EditDoc& d = *_doc;
    auto& pd = static_cast<PointsDoc&>(d);

    if (_repair_shown) {
        ImGui::BeginChild("##repairres",
                          ImVec2(0, (float)std::clamp((int)_repair_report.size() + 1, 3, 10) *
                                        ImGui::GetTextLineHeightWithSpacing()),
                          ImGuiChildFlags_Borders);
        for (const RepairResultLine& l : _repair_report)
            ui::TextRaw(fs::path(l.name).filename().string() + "  " +
                        outcome_label(l.outcome).get());
        ImGui::EndChild();
        if (ui::Button(msg::repair_keep, ImVec2(half, 0))) keep_repair();
        ui::help_on_hover(msg::repair_keep_help, {_repair_model});
        ImGui::SameLine();
        if (ui::Button(msg::repair_discard, ImVec2(half, 0))) discard_repair();
        if (_repair_shown) draw_camera_photo(full);
        return;
    }

    // Selection-driven actions want the camera layer under the tools.
    const bool cams = d.layer() == 1;
    const bool picked = cams && !d.sel().empty();
    ImGui::BeginDisabled(!picked);
    if (ui::Button(msg::repair_replace, ImVec2(full, 0))) start_repair(RepairKind::Replace);
    ImGui::EndDisabled();
    ui::help_on_hover(msg::repair_replace_help);

    ImGui::BeginDisabled(!picked);
    if (ui::Button(msg::repair_move, ImVec2(full, 0))) {
        if (_tool.id() != ToolId::Transform) _xform_return = _tool.id();
        _tool.set_id(ToolId::Transform);
    }
    ImGui::EndDisabled();
    ui::help_on_hover(msg::repair_move_help);
    const size_t moved = pd.moved_cameras().size();
    if (moved) {
        ui::Text(msg::repair_moved_count, {(long long)moved});
        if (ui::Button(msg::repair_snap, ImVec2(half, 0))) start_repair(RepairKind::Snap);
        ui::help_on_hover(msg::repair_snap_help);
        ImGui::SameLine();
        if (ui::Button(msg::repair_forget_moves, ImVec2(half, 0)))
            d.run(make_camera_move_op(pd, {}, msg::repair_forget_moves.get()));
    }

    ImGui::Spacing();
    if (ui::Button(msg::repair_missing, ImVec2(full, 0))) start_repair(RepairKind::Missing);
    ui::help_on_hover(msg::repair_missing_help);
    if (ui::Button(msg::repair_audit, ImVec2(full, 0))) start_repair(RepairKind::Audit);
    ui::help_on_hover(msg::repair_audit_help);
    ui::Checkbox(msg::repair_match, &_repair_match);
    ui::help_on_hover(msg::repair_match_help);
    ui::Checkbox(msg::photo_overlay, &_photo_overlay);
    ui::help_on_hover(msg::photo_overlay_help);
    if (_photo_overlay) {
        ImGui::SetNextItemWidth(full * 0.5f);
        ui::SliderFloat(msg::photo_opacity, &_photo_alpha, 0.1f, 1.0f);
    }
    draw_missing_section(full);
    if (_missing_at < 0) draw_camera_photo(full);
}

}  // namespace gui
