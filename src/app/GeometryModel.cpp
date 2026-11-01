#include "app/GeometryModel.h"

#include "app/GeometryWarp.h"
#include "marigold/Marigold.h"
#include "marigold/model/Install.h"
#include "metric3d/Metric3D.h"
#include "metric3d/model/Fetch.h"
#include "moge/Moge.h"
#include "moge/model/Fetch.h"
#include "nn/Device.h"
#include "nn/core/Error.h"
#include "nn/io/Onnx.h"

#include <algorithm>
#include <cmath>

namespace app {
namespace {

// What anchors Marigold v2 and decides its sky: MoGe-2's base model, which is
// metric and quick beside it.
constexpr const char* kAnchorModel = "moge2-vitb";

bool is_marigold(const std::string& id) { return id == marigold::kModelId; }

// Marigold's depth is log-affine, log z = a + b * rel; MoGe-2's metric depth
// fixes a and b by least squares in log space, refitted on the 90% it fits best
// (MoGe's frayed edges drop out). MoGe's sky stays 0, "no ground truth here".
std::vector<float> anchor_depth(const std::vector<float>& rel, const std::vector<float>& metric) {
    std::vector<float> out(rel.size(), 0.0f);
    std::vector<size_t> idx;
    for (size_t i = 0; i < rel.size(); ++i)
        if (metric[i] > 0.0f) idx.push_back(i);
    if (idx.size() < 256) return out;
    double a = 0.0, b = 0.0;
    for (int pass = 0; pass < 2; ++pass) {
        double sx = 0, sy = 0, sxx = 0, sxy = 0;
        for (size_t i : idx) {
            const double x = rel[i], y = std::log((double)metric[i]);
            sx += x;
            sy += y;
            sxx += x * x;
            sxy += x * y;
        }
        const double n = (double)idx.size(), det = n * sxx - sx * sx;
        if (det <= 1e-12 * n * n) return out;
        b = (n * sxy - sx * sy) / det;
        a = (sy - b * sx) / n;
        if (pass == 1) break;
        std::vector<std::pair<double, size_t>> r;
        r.reserve(idx.size());
        for (size_t i : idx)
            r.emplace_back(std::fabs(std::log((double)metric[i]) - a - b * rel[i]), i);
        const size_t keep = idx.size() * 9 / 10;
        std::nth_element(r.begin(), r.begin() + (long)keep, r.end());
        idx.clear();
        for (size_t k = 0; k < keep; ++k) idx.push_back(r[k].second);
    }
    // Near is -1: a fit with b <= 0 has the scene inside out, and MoGe's own
    // depth is the better answer.
    if (b <= 0.0) return metric;
    for (size_t i = 0; i < rel.size(); ++i)
        if (metric[i] > 0.0f) out[i] = (float)std::exp(a + b * rel[i]);
    return out;
}

// The family of a file the user pointed at. The two graphs share no module
// path, and `read_onnx_structure` reads node names without the payload. Only
// MoGe-3 ships as a torch .pt.
bool file_is_moge(const std::string& path) {
    if (path.size() > 3 && path.compare(path.size() - 3, 3, ".pt") == 0) return true;
    const nn::OnnxFile g = nn::read_onnx_structure(path);
    for (const nn::OnnxNode& n : g.nodes) {
        if (n.name.find("/points_head/") != std::string::npos) return true;
        if (n.name.find("/depth_model/") != std::string::npos) return false;
    }
    nn::fail("'%s' is neither a MoGe nor a Metric3D export (known ids: %s)",
             path.c_str(), geometry_model_ids().c_str());
}

}  // namespace

std::string geometry_model_ids() {
    return moge::model_id_list() + ", " + metric3d::model_id_list() + ", " +
           marigold::kModelId;
}

GeometryInstall geometry_install_needs(const std::string& id, bool want_depth,
                                       bool want_normal) {
    GeometryInstall g;
    if (!is_marigold(id)) return g;
    const marigold::InstallNeeds n = marigold::install_needs(want_depth, want_normal);
    g.download = n.download;
    g.disk = n.disk;
    g.free = n.free;
    g.spare = marigold::kDiskSpare;
    g.where = marigold::install_dir();
    return g;
}

void geometry_install(const std::string& id, bool want_depth, bool want_normal,
                      const GeometryInstallProgress& progress,
                      const std::atomic<bool>* cancel) {
    if (is_marigold(id)) marigold::install(want_depth, want_normal, progress, cancel);
}

GeometryRequest face_request(const GeometryWarp& warp, int k, int num_tokens) {
    GeometryRequest r;
    r.num_tokens = num_tokens;
    r.width = warp.faceWidth(k);
    r.height = warp.faceHeight(k);
    r.fx = warp.faceFocal(k);
    r.fy = warp.faceFocalY(k);
    r.cx = warp.faceCx(k);
    r.cy = warp.faceCy(k);
    return r;
}

GeometryRequest turn_request(const GeometryRequest& r, const sfm::ExifTransform& t) {
    GeometryRequest out = r;
    if (t.identity()) return out;
    double w = r.width, h = r.height, fx = r.fx, fy = r.fy, cx = r.cx, cy = r.cy;
    const bool centred = cx >= 0.0 && cy >= 0.0;
    for (int q = 0; q < (t.turns_cw & 3); q++) {
        // One clockwise quarter turn, matching core/ImageOrient.h's mapping of
        // the pixel grid: (x, y) -> (h - 1 - y, x).
        const double ncx = (h - 1.0) - cy;
        cy = cx;
        cx = ncx;
        std::swap(fx, fy);
        std::swap(w, h);
    }
    if (t.mirror) cx = (w - 1.0) - cx;
    out.width = (int)w;
    out.height = (int)h;
    out.fx = fx;
    out.fy = fy;
    if (centred) {
        out.cx = cx;
        out.cy = cy;
    }
    return out;
}

struct GeometryModel::Impl {
    moge::Predictor      moge;
    metric3d::Predictor  metric3d;
    marigold::Predictor  marigold;
    bool is_moge = false;
    bool is_marigold = false;
    bool want_depth = true, want_normal = true;
    int anchor_tokens = 0;

    // MoGe's working memory grows on demand and Marigold plans around what is
    // in use, so MoGe runs once at the largest budget before Marigold plans.
    void warm_anchor(int num_tokens) {
        if (anchor_tokens >= num_tokens) return;
        const int side = (int)std::ceil(std::sqrt((double)num_tokens)) * moge::Predictor::patchSize();
        const std::vector<float> grey((size_t)side * side * 3, 0.5f);
        moge::PredictOptions po;
        po.num_tokens = num_tokens;
        po.want_normal = false;
        po.want_mask = true;
        moge.predict(grey.data(), side, side, po);
        anchor_tokens = num_tokens;
    }
};

GeometryModel::GeometryModel() : impl_(new Impl) {}
GeometryModel::~GeometryModel() { delete impl_; }

void GeometryModel::load(const std::string& id_or_path, const std::string& selector,
                         bool want_depth, bool want_normal) {
    // Freeze before model allocation; empty uses SS_VK_DEVICE then Auto.
    if (!selector.empty()) nn::configure_device(selector);

    if (is_marigold(id_or_path)) {
        impl_->is_marigold = true;
        impl_->want_depth = want_depth;
        impl_->want_normal = want_normal;
        if (!impl_->moge.loaded()) impl_->moge.load(kAnchorModel);
        impl_->marigold.load(want_depth, want_normal);
        return;
    }
    impl_->is_marigold = false;
    if (moge::find_model_source(id_or_path)) impl_->is_moge = true;
    else if (metric3d::find_model_source(id_or_path)) impl_->is_moge = false;
    else impl_->is_moge = file_is_moge(id_or_path);

    if (impl_->is_moge) impl_->moge.load(id_or_path);
    else impl_->metric3d.load(id_or_path);
}

int GeometryModel::sizeGranularity() const {
    return impl_->is_moge || impl_->is_marigold ? 1 : metric3d::Predictor::sizeGranularity();
}

// Metric3D publishes no token range and shares MoGe's floor.
int64_t GeometryModel::minFacePixels() {
    const int64_t p = moge::Predictor::patchSize();
    return 1200 * p * p;
}

double GeometryModel::depthToMillimetres(double face_focal_px) const {
    return impl_->is_moge || impl_->is_marigold ? 1000.0 : face_focal_px;
}

bool GeometryModel::cheapWarmup() const { return !impl_->is_marigold; }

void GeometryModel::prepare(const GeometryRequest& req) {
    if (!impl_->is_marigold) return;
    impl_->warm_anchor(req.num_tokens);
    impl_->marigold.prepare(req.width, req.height, req.num_tokens);
}

int GeometryModel::batchCapacity(const GeometryRequest& req) {
    if (!impl_->is_marigold) return 1;
    impl_->warm_anchor(req.num_tokens);
    return impl_->marigold.batchCapacity(req.width, req.height, req.num_tokens);
}

std::vector<GeometryPrediction> GeometryModel::predictBatch(
    const std::vector<const float*>& rgb, const std::vector<GeometryRequest>& reqs) {
    std::vector<GeometryPrediction> out(rgb.size());
    if (!impl_->is_marigold) {
        for (size_t i = 0; i < rgb.size(); ++i) out[i] = predict(rgb[i], reqs[i]);
        return out;
    }
    for (const GeometryRequest& r : reqs) impl_->warm_anchor(r.num_tokens);
    for (size_t i = 0; i < rgb.size(); ++i) {
        const GeometryRequest& r = reqs[i];
        moge::PredictOptions po;
        po.want_depth = r.want_depth;
        po.want_normal = false;
        po.want_mask = true;
        po.num_tokens = r.num_tokens;
        po.fx = (float)r.fx;
        po.fy = (float)r.fy;
        po.cx = (float)r.cx;
        po.cy = (float)r.cy;
        moge::Prediction p = impl_->moge.predict(rgb[i], r.width, r.height, po);
        out[i].width = p.width;
        out[i].height = p.height;
        out[i].depth = std::move(p.depth);
        out[i].mask = std::move(p.mask);
    }
    marigold::PredictOptions mo;
    mo.want_depth = reqs[0].want_depth;
    mo.want_normal = reqs[0].want_normal;
    mo.num_tokens = reqs[0].num_tokens;
    std::vector<marigold::Predictor::Image> imgs;
    for (size_t i = 0; i < rgb.size(); ++i) imgs.push_back({rgb[i], reqs[i].width, reqs[i].height});
    std::vector<marigold::Prediction> mp = impl_->marigold.predict(imgs, mo);
    for (size_t i = 0; i < rgb.size(); ++i) {
        GeometryPrediction& g = out[i];
        if (mo.want_depth) g.depth = anchor_depth(mp[i].depth_log, g.depth);
        if (mo.want_normal) {
            g.normal = std::move(mp[i].normal);
            for (size_t p = 0; p < g.mask.size(); ++p)
                if (g.mask[p] <= 0.5f) g.normal[p * 3] = g.normal[p * 3 + 1] = g.normal[p * 3 + 2] = 0.0f;
        }
    }
    return out;
}

GeometryPrediction GeometryModel::predict(const float* rgb, const GeometryRequest& req) {
    if (impl_->is_marigold) return std::move(predictBatch({rgb}, {req})[0]);
    GeometryPrediction out;
    if (impl_->is_moge) {
        moge::PredictOptions po;
        po.want_depth = req.want_depth;
        po.want_normal = req.want_normal;
        po.want_mask = true;
        po.num_tokens = req.num_tokens;
        po.refine_steps = req.refine_steps;
        po.fx = (float)req.fx;
        po.fy = (float)req.fy;
        po.cx = (float)req.cx;
        po.cy = (float)req.cy;
        moge::Prediction p = impl_->moge.predict(rgb, req.width, req.height, po);
        out.width = p.width;
        out.height = p.height;
        out.depth = std::move(p.depth);
        out.normal = std::move(p.normal);
        out.mask = std::move(p.mask);
        return out;
    }
    metric3d::PredictOptions po;
    po.want_depth = req.want_depth;
    po.want_normal = req.want_normal;
    metric3d::Prediction p = impl_->metric3d.predict(rgb, req.width, req.height, po);
    out.width = p.width;
    out.height = p.height;
    out.depth = std::move(p.depth);
    out.normal = std::move(p.normal);
    return out;
}

}  // namespace app
