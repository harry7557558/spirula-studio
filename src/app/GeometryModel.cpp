#include "app/GeometryModel.h"

#include "app/GeometryWarp.h"
#include "metric3d/Metric3D.h"
#include "metric3d/model/Fetch.h"
#include "moge/Moge.h"
#include "moge/model/Fetch.h"
#include "nn/core/Error.h"
#include "nn/io/Onnx.h"

namespace app {
namespace {

// Which family a checkpoint belongs to when the user pointed at a file rather
// than naming an id. The two graphs share no module path, and the node names
// survive `read_onnx_structure`, which reads no payload.
bool file_is_moge(const std::string& path) {
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
    return moge::model_id_list() + ", " + metric3d::model_id_list();
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

struct GeometryModel::Impl {
    moge::Predictor      moge;
    metric3d::Predictor  metric3d;
    bool is_moge = false;
};

GeometryModel::GeometryModel() : impl_(new Impl) {}
GeometryModel::~GeometryModel() { delete impl_; }

void GeometryModel::load(const std::string& id_or_path) {
    if (moge::find_model_source(id_or_path)) impl_->is_moge = true;
    else if (metric3d::find_model_source(id_or_path)) impl_->is_moge = false;
    else impl_->is_moge = file_is_moge(id_or_path);

    if (impl_->is_moge) impl_->moge.load(id_or_path);
    else impl_->metric3d.load(id_or_path);
}

int GeometryModel::sizeGranularity() const {
    return impl_->is_moge ? 1 : metric3d::Predictor::sizeGranularity();
}

double GeometryModel::depthToMillimetres(double face_focal_px) const {
    return impl_->is_moge ? 1000.0 : face_focal_px;
}

GeometryPrediction GeometryModel::predict(const float* rgb, const GeometryRequest& req) {
    GeometryPrediction out;
    if (impl_->is_moge) {
        moge::PredictOptions po;
        po.want_depth = req.want_depth;
        po.want_normal = req.want_normal;
        po.num_tokens = req.num_tokens;
        po.fx = (float)req.fx;
        po.fy = (float)req.fy;
        po.cx = (float)req.cx;
        po.cy = (float)req.cy;
        moge::Prediction p = impl_->moge.predict(rgb, req.width, req.height, po);
        out.width = p.width;
        out.height = p.height;
        out.depth = std::move(p.depth);
        out.normal = std::move(p.normal);
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
