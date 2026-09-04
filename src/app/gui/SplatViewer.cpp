// SplatViewer.cpp -- see SplatViewer.h.

#include "app/gui/SplatViewer.h"

#include "checkpoint/Resume.h"
#include "checkpoint/SplatPly.h"
#include "config/TrainConfig.h"
#include "data/DatasetParser.h"
#include "engine/Engine.h"
#include "i18n/catalog/Gui.h"
#include "app/TrainerCore.h"
#include "mesh/MeshImport.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace fs = std::filesystem;
namespace msg = spirula::i18n::msg::gui;
using spirula::i18n::format;

namespace gui {

namespace {

TorchTensorView tv(std::vector<float>& v, std::vector<int64_t> shape) {
    return {(uint64_t)(uintptr_t)v.data(), (uint32_t)sizeof(float), std::move(shape)};
}

// Where the model is and how big it is, robustly -- a port of the standalone
// web viewer's ssv_fit_sphere + fitModel (viewer/src/viewer.cpp,
// viewer/js/main.js), so a file framed here and the same file framed there
// open on the same view.
//
// A trained scene has floaters: a handful of splats parked far outside
// anything a camera saw. A bounding box is therefore useless -- it puts the
// viewer a kilometre away looking at a dot -- and the median is what is left.
// `radius` comes back as the web viewer's model radius, twice the MEDIAN
// distance from the centre.
void scene_extent(const std::vector<float>& xyz, int64_t n,
                  float center[3], float& radius) {
    center[0] = center[1] = center[2] = 0.0f;
    radius = 1.0f;
    if (n <= 0) return;
    // Subsampled above a million points, as the web viewer does: a median does
    // not get more accurate for being taken over more of the same cloud.
    const int64_t step = std::max<int64_t>(1, n / (1 << 20));
    std::vector<float> tmp;
    tmp.reserve((size_t)(n / step + 1));
    for (int d = 0; d < 3; d++) {
        tmp.clear();
        for (int64_t i = 0; i < n; i += step) tmp.push_back(xyz[(size_t)i * 3 + d]);
        std::nth_element(tmp.begin(), tmp.begin() + tmp.size() / 2, tmp.end());
        center[d] = tmp[tmp.size() / 2];
    }
    tmp.clear();
    for (int64_t i = 0; i < n; i += step) {
        const float dx = xyz[(size_t)i * 3 + 0] - center[0];
        const float dy = xyz[(size_t)i * 3 + 1] - center[1];
        const float dz = xyz[(size_t)i * 3 + 2] - center[2];
        tmp.push_back(dx * dx + dy * dy + dz * dz);
    }
    std::nth_element(tmp.begin(), tmp.begin() + tmp.size() / 2, tmp.end());
    radius = 2.0f * std::sqrt(std::max(tmp[tmp.size() / 2], 1e-24f));
    radius = std::max(radius, 1e-6f);
}

// One client unit, in model units: the distance the web viewer's fitSphere
// puts the camera at for that radius (r / tan(fov/2) * 1.25, at the 90-degree
// field of view this viewport opens with). The client navigates at distance 1
// from the origin, exactly as it does over a dataset's normalized frame, so
// this scalar IS the framing.
float view_distance_for(float radius) { return 1.25f * radius; }

}  // namespace


SplatViewer::~SplatViewer() {
    if (_worker.joinable()) _worker.join();
    close();
}

void SplatViewer::open(const std::string& path, int scene_slot,
                       std::mutex* engine_mutex) {
    close();
    {
        std::lock_guard<std::mutex> lk(_mu);
        _error.clear();
        _path = path;
        _file.clear();
    }
    _num_splats = 0;
    _sh_degree = 0;
    _engine_mutex = engine_mutex;
    _pending_slot = scene_slot;
    _state = State::Loading;
    _worker = std::thread([this, path] { run(path); });
}

void SplatViewer::close() {
    if (_worker.joinable()) _worker.join();
    const int slot = _scene_slot.exchange(-1);
    if (slot >= 0) {
        std::lock_guard<std::mutex> lk(*_engine_mutex);
        engine_scene_free(slot);
    }
    _state = State::Idle;
    _num_splats = 0;
    _num_faces = 0;
    std::lock_guard<std::mutex> lk(_mu);
    _path.clear();
    _file.clear();
    // A baked 4096^2 atlas is 48 MB; do not hold it until the next open.
    _mesh = meshing::MeshData();
    _points = ParsedDataset();
}

std::string SplatViewer::error() {
    std::lock_guard<std::mutex> lk(_mu);
    return _error;
}
std::string SplatViewer::path() {
    std::lock_guard<std::mutex> lk(_mu);
    return _path;
}
std::string SplatViewer::file() {
    std::lock_guard<std::mutex> lk(_mu);
    return _file;
}
ViewerRenderConfig SplatViewer::render_config() {
    std::lock_guard<std::mutex> lk(_mu);
    return _cfg;
}
std::string SplatViewer::scene_key() {
    std::lock_guard<std::mutex> lk(_mu);
    return _file + ":" + std::to_string(_num_splats.load());
}

std::string SplatViewer::gamut() const {
    std::lock_guard<std::mutex> lk(const_cast<std::mutex&>(_mu));
    return _gamut;
}
int SplatViewer::transfer() const {
    std::lock_guard<std::mutex> lk(const_cast<std::mutex&>(_mu));
    return _transfer;
}
bool SplatViewer::linear_color() const {
    std::lock_guard<std::mutex> lk(const_cast<std::mutex&>(_mu));
    return _linear;
}

void SplatViewer::set_color_space(const char* gamut, int transfer, bool linear) {
    const int slot = _scene_slot.load();
    if (slot < 0) return;
    const std::string g = gamut ? gamut : "";
    const bool on = transfer != 0 || linear || !g.empty();
    {
        std::lock_guard<std::mutex> lk(*_engine_mutex);
        // Splat side only: there is no GT image to convert here.
        auto vec = [](const spirula::Mat3f& m) {
            return std::vector<float>(m.begin(), m.end());
        };
        engine_scene_set_color_space(
            slot, on, transfer, linear,
            on ? vec(spirula::gamut_to_rec709(g)) : std::vector<float>{});
    }
    std::lock_guard<std::mutex> lk(_mu);
    _gamut = g;
    _transfer = transfer;
    _linear = linear;
}

void SplatViewer::release_screen_buffers() {
    if (_scene_slot.load() < 0) return;
    std::lock_guard<std::mutex> lk(*_engine_mutex);
    engine_release_screen_buffers();
}

ViewerHooks SplatViewer::make_hooks() {
    ViewerHooks hooks;
    hooks.engine_mutex = _engine_mutex;
    // No training step to warm the SH degree up against: every band the file
    // carries is drawn from the first frame.
    hooks.current_step = [] { return 1 << 20; };
    hooks.set_render_pending = [](bool) {};
    return hooks;
}

void SplatViewer::log(const std::string& s) {
    std::lock_guard<std::mutex> lk(_mu);
    _log.push_back(s);
    if (_log.size() > 2000) _log.erase(_log.begin(), _log.begin() + 500);
}

std::vector<std::string> SplatViewer::drain_log() {
    std::lock_guard<std::mutex> lk(_mu);
    std::vector<std::string> out;
    out.swap(_log);
    return out;
}

void SplatViewer::run(std::string path) {
    auto fail = [&](const std::string& why) {
        std::lock_guard<std::mutex> lk(_mu);
        _error = why;
        _state = State::Failed;
    };
    auto set_frame = [&](const float center[3], float unit) {
        std::lock_guard<std::mutex> lk(_mu);
        for (int i = 0; i < 3; i++) _center[i] = center[i];
        _unit = unit;
    };
    try {
        // A mesh is not a checkpoint, so it is resolved before
        // find_splat_ply (which looks for step-*.ckpt directories) gets a
        // chance to misread the path. A .ply only counts as a mesh when its
        // header declares faces -- otherwise it is a splat or point cloud.
        if (meshing::is_mesh_path(path) &&
            (fs::path(path).extension() != ".ply" ||
             meshing::ply_is_mesh(path))) {
            {
                std::lock_guard<std::mutex> lk(_mu);
                _file = path;
            }
            log(format(msg::viewer_reading, {path}));
            meshing::MeshData m;
            std::string err;
            if (!meshing::read_mesh(path, m, err)) return fail(err);
            meshing::mesh_compute_normals(m);

            // The mesh lives in the model's own coordinates; the viewport
            // navigates a normalized frame centred on it and one unit across,
            // exactly as for a splat file.
            std::vector<float> xyz(m.V.size() * 3);
            for (size_t i = 0; i < m.V.size(); i++)
                for (int a = 0; a < 3; a++) xyz[3 * i + a] = m.V[i][a];
            float center[3], radius;
            scene_extent(xyz, (int64_t)m.V.size(), center, radius);
            const float unit = view_distance_for(radius);
            const float inv = unit > 1e-20f ? 1.0f / unit : 1.0f;
            const int64_t nv = (int64_t)m.V.size();
            const int64_t nf = (int64_t)m.F.size();
            set_frame(center, unit);
            {
                std::lock_guard<std::mutex> lk(_mu);
                _mesh = std::move(m);
                // model -> normalized: subtract the centre, divide by the
                // unit (the inverse of the train_to_normalized similarity
                // the splat path builds).
                const float t[12] = {inv, 0, 0, -inv * center[0],
                                     0, inv, 0, -inv * center[1],
                                     0, 0, inv, -inv * center[2]};
                for (int i = 0; i < 12; i++) _mesh_t2n[i] = t[i];
            }
            _kind = Kind::Mesh;
            _num_splats = nv;
            _num_faces = nf;
            _sh_degree = 0;
            log(format(msg::viewer_loaded_mesh,
                       {(long long)nv, (long long)nf}));
            _state = State::Ready;
            return;
        }

        auto [ply, run_dir] = spirula::find_splat_ply(path);
        {
            std::lock_guard<std::mutex> lk(_mu);
            _file = ply;
        }
        // A .ply holding points rather than Gaussians -- an SfM cloud, a
        // Metashape export -- is a thing a user reasonably drops here, and
        // the GL preview already draws exactly that.
        if (!spirula::is_splat_ply(ply)) {
            log(format(msg::viewer_reading, {ply}));
            ColmapPoints3D pts = read_ply_points(ply);
            if (pts.num() <= 0) return fail(format(msg::viewer_not_a_splat_file, {ply}));
            float center[3], radius;
            scene_extent(pts.xyz, pts.num(), center, radius);
            const float unit = view_distance_for(radius);
            ParsedDataset ds;
            ds.points = std::move(pts);
            ds.num_cameras = 0;
            ds.train_frame_scale = unit;
            ds.train_to_normalized = {unit, 0, 0, center[0],
                                      0, unit, 0, center[1],
                                      0, 0, unit, center[2],
                                      0, 0, 0, 1};
            const int64_t n = ds.points.num();
            set_frame(center, unit);
            {
                std::lock_guard<std::mutex> lk(_mu);
                _points = std::move(ds);
                _post = PostSplitCameras();
            }
            _kind = Kind::Points;
            _num_splats = n;
            _sh_degree = 0;
            log(format(msg::viewer_loaded_points, {(long long)n}));
            _state = State::Ready;
            return;
        }
        _kind = Kind::Splats;
        log(format(msg::viewer_reading, {ply}));
        spirula::SplatCloud c = spirula::read_splat_ply(ply);
        if (c.num <= 0) return fail(msg::viewer_no_splats.get());

        // The run this file came out of, when it is still next to it: what
        // primitive its splats are, and how they are rasterized. A PLY says
        // nothing about either, and rendering a Mip or 3DGUT model as vanilla
        // 3DGS is wrong in a way that looks like a bad reconstruction.
        TrainConfig cfg;
        const fs::path cfg_path = fs::path(run_dir) / "config.json";
        std::error_code ec;
        if (fs::is_regular_file(cfg_path, ec)) {
            try {
                cfg = ckpt::config_from_json(cfg_path);
                log(format(msg::viewer_using_run_config, {cfg_path.string()}));
            } catch (const std::exception& e) {
                log(format(msg::viewer_run_config_unreadable, {e.what()}));
            }
        }

        // The run's own color space, so the model opens looking the way it
        // was trained rather than the way an untagged PLY defaults to.
        const spirula::ColorResolution color = spirula::resolve_color(cfg);
        {
            std::lock_guard<std::mutex> lk(_mu);
            _gamut = color.splat_gamut;
            _transfer = (int)color.splat_transfer;
            _linear = color.splat_linear;
        }

        float center[3], radius;
        scene_extent(c.means, c.num, center, radius);
        const float unit = view_distance_for(radius);
        set_frame(center, unit);

        ViewerRenderConfig vc;
        vc.primitive = cfg.primitive;
        vc.packed = cfg.packed || cfg.use_bvh;
        vc.sh_degree_warmup_every = 1;
        // Median and distortion buffers are training diagnostics computed from
        // regularizers this file has none of, so the viewer does not offer
        // them. rgb / depth / alpha / normals all come from the same render.
        vc.output_median = false;
        vc.distortion_reg_on = false;
        // The client navigates a normalized frame centred on the model, at
        // one unit; train_to_normalized maps that frame back onto the file's
        // own coordinates, which is precisely what it does for a dataset.
        vc.train_frame_scale = unit;
        vc.train_to_normalized = {unit, 0, 0, center[0],
                                  0, unit, 0, center[1],
                                  0, 0, unit, center[2],
                                  0, 0, 0, 1};
        vc.base_camera_size = 0.0f;   // no cameras to draw
        vc.scene_slot = _pending_slot;

        {
            std::lock_guard<std::mutex> lk(*_engine_mutex);
            const int64_t K = c.dim_sh() - 1;
            // Claimed before the upload, not after: an upload that throws part
            // way still leaves buffers for close() to free.
            _scene_slot = _pending_slot;
            engine_scene_set_data_3dgs(
                          _pending_slot, c.num,
                          tv(c.means,       {c.num, 3}),
                          tv(c.quats,       {c.num, 4}),
                          tv(c.scales,      {c.num, 3}),
                          tv(c.opacities,   {c.num, 1}),
                          tv(c.features_dc, {c.num, 3}),
                          tv(c.features_sh, {c.num, K, 3}));
            const bool cs_on = color.splat_on();
            auto vec = [](const spirula::Mat3f& m) {
                return std::vector<float>(m.begin(), m.end());
            };
            engine_scene_set_color_space(
                _pending_slot, cs_on, (int)color.splat_transfer,
                color.splat_linear,
                cs_on ? vec(spirula::gamut_to_rec709(color.splat_gamut))
                      : std::vector<float>{});
        }

        _num_splats = c.num;
        _sh_degree = c.sh_degree;
        {
            std::lock_guard<std::mutex> lk(_mu);
            _cfg = vc;
        }
        log(format(msg::viewer_loaded, {(long long)c.num, (long long)c.sh_degree}));
        _state = State::Ready;
    } catch (const std::exception& e) {
        fail(e.what());
    }
}

}  // namespace gui
