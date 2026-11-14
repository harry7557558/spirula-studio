#include "app/AnchorRefine.h"

#include "i18n/Message.h"
#include "i18n/catalog/Lidar.h"
#include "sfm/core/Features.h"
#include "sfm/core/Matches.h"
#include "sfm/core/Rig.h"
#include "sfm/map/Bundle.h"
#include "sfm/map/KnownPoses.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <map>
#include <set>
#include <stdexcept>

namespace app::lidar {
namespace {

namespace fs = std::filesystem;
namespace lmsg = spirula::i18n::msg::lidar;
using sfm::Mat3;
using sfm::Pose;
using sfm::Vec3;

constexpr int kColmapOpenCV = 4;
constexpr int kColmapOpenCVFisheye = 5;
constexpr double kPi = 3.14159265358979323846;

Pose pose_of(const Anchor& a) {
    const Mat3 Rw2c = sfm::transpose(a.R);
    const Vec3 t = sfm::mul(Rw2c, a.centre);
    return {Rw2c, {-t.x, -t.y, -t.z}};
}

double median(std::vector<double> v) {
    if (v.empty()) return 0;
    std::nth_element(v.begin(), v.begin() + (std::ptrdiff_t)(v.size() / 2), v.end());
    return v[v.size() / 2];
}

double angle_deg(const Mat3& a, const Mat3& b) {
    const Mat3 d = sfm::mul(sfm::transpose(a), b);
    const double c = std::max(-1.0, std::min(1.0, 0.5 * (d[0] + d[4] + d[8] - 1.0)));
    return std::acos(c) * 180.0 / kPi;
}

// The photographs of one capture: <capture>/<camera>/<instant>.<ext>.
struct Placed {
    size_t anchor;
    std::string folder, capture, instant;
};

}  // namespace

RefineResult refine_anchors(std::vector<Anchor>& anchors, const sfm::MatchesDatabase& db,
                            const std::vector<sfm::FeatureSet>& features,
                            const RefineOptions& opt) {
    RefineResult res;
    std::map<std::string, uint32_t> db_index;
    for (uint32_t i = 0; i < db.images.size(); i++) db_index[db.images[i].name] = i;

    std::vector<Placed> placed;
    for (size_t i = 0; i < anchors.size(); i++) {
        const Anchor& a = anchors[i];
        if ((a.camera_model != kColmapOpenCV && a.camera_model != kColmapOpenCVFisheye) ||
            a.params.size() != 8 || a.width <= 0 || a.height <= 0)
            continue;
        const fs::path p(a.name);
        placed.push_back({i, p.parent_path().generic_string(),
                          p.parent_path().parent_path().generic_string(), p.stem().string()});
    }
    res.photos = (int64_t)placed.size();
    if (placed.empty()) return res;

    // ---- the model: the device's poses, one camera per camera folder ----
    sfm::Reconstruction rec;
    std::map<std::string, uint32_t> camera_of;
    std::vector<uint32_t> image_of(db.images.size(), sfm::kNotInModel);
    std::vector<std::string> names(placed.size() + 1);
    for (size_t k = 0; k < placed.size(); k++) {
        const Anchor& a = anchors[placed[k].anchor];
        const uint32_t id = (uint32_t)k + 1;
        auto cam = camera_of.find(placed[k].folder);
        if (cam == camera_of.end()) {
            sfm::Camera c;
            c.id = (uint32_t)camera_of.size() + 1;
            c.width = (int)a.width;
            c.height = (int)a.height;
            c.model = a.camera_model == kColmapOpenCV ? sfm::CamModel::OpenCV
                                                      : sfm::CamModel::OpenCVFisheye;
            sfm::unpackColmap(c, a.params.data());
            rec.cameras[c.id] = c;
            cam = camera_of.emplace(placed[k].folder, c.id).first;
        }
        sfm::Image im;
        im.id = id;
        im.camera_id = cam->second;
        im.name = a.name;
        im.pose = pose_of(a);
        im.registered = true;
        rec.images[id] = im;
        names[id] = a.name;
        const auto d = db_index.find(fs::path(a.name).replace_extension().generic_string());
        if (d != db_index.end() && d->second < features.size() &&
            features[d->second].keypoints.size() == db.images[d->second].num_features) {
            image_of[d->second] = id;
            res.matched++;
            sfm::Camera& c = rec.cameras[cam->second];
            if (c.pixel_scale == 1.0) c.pixel_scale = features[d->second].pixelScale();
        }
    }
    if (res.matched == 0) return res;

    // ---- one rig per capture, keyed by instant ----
    std::map<std::string, std::vector<std::string>> folders_of;
    std::map<std::pair<std::string, std::string>, std::vector<size_t>> instants;
    for (size_t k = 0; k < placed.size(); k++) {
        std::vector<std::string>& f = folders_of[placed[k].capture];
        if (std::find(f.begin(), f.end(), placed[k].folder) == f.end()) f.push_back(placed[k].folder);
        instants[{placed[k].capture, placed[k].instant}].push_back(k);
    }
    std::vector<sfm::RigDef> defs;
    for (auto& [capture, folders] : folders_of) {
        std::sort(folders.begin(), folders.end());
        if (folders.size() < 2) continue;
        sfm::RigDef d;
        d.name = capture.empty() ? std::string("capture") : capture;
        for (const std::string& f : folders) {
            sfm::RigMemberDef md;
            md.prefix = f;
            md.ext_fixed = false;
            md.dof = f == folders.front() ? (uint8_t)sfm::kRigDofNone : (uint8_t)sfm::kRigDofRotation;
            d.members.push_back(md);
        }
        // The device states the rig exactly: any instant holding both cameras gives it.
        for (const auto& [key, ks] : instants) {
            if (key.first != capture) continue;
            const Placed* lead = nullptr;
            for (size_t k : ks)
                if (placed[k].folder == folders.front()) lead = &placed[k];
            if (!lead) continue;
            const Pose ref = pose_of(anchors[lead->anchor]);
            for (size_t k : ks) {
                const size_t m = (size_t)(std::find(folders.begin(), folders.end(), placed[k].folder) -
                                          folders.begin());
                if (d.members[m].has_ext) continue;
                d.members[m].has_ext = true;
                d.members[m].ext = sfm::composePose(pose_of(anchors[placed[k].anchor]),
                                                    sfm::invertPose(ref));
            }
        }
        defs.push_back(std::move(d));
    }
    const sfm::RigTable table = sfm::buildRigTable(names, defs);
    rec.rigs.assign(table.rigs.size(), sfm::RigCalib{});
    for (size_t r = 0; r < table.rigs.size(); r++) {
        sfm::RigCalib& c = rec.rigs[r];
        const std::vector<sfm::RigMemberDef>& members = table.rigs[r].members;
        c.resize(members.size());
        c.ref = 0;
        for (size_t m = 0; m < members.size(); m++) {
            if (!members[m].has_ext) continue;
            c.cam_from_rig[m] = members[m].ext;
            c.established[m] = 1;
        }
        c.fixed[0] = 1;
    }

    // ---- the device's poses as priors: centre and attitude of each instant ----
    sfm::PosePriors priors;
    priors.up_w = {0, 0, 1};
    const double sigma_r = opt.rotation_sigma_deg * kPi / 180.0;
    for (const auto& [key, ks] : instants) {
        size_t best = ks.front();
        for (size_t k : ks)
            if (placed[k].folder < placed[best].folder) best = k;
        const uint32_t id = (uint32_t)best + 1;
        const Anchor& a = anchors[placed[best].anchor];
        sfm::PriorCentre c;
        c.n = 1;
        c.img[0] = id;
        c.b = a.centre;
        c.sigma = {opt.centre_sigma_m, opt.centre_sigma_m, opt.centre_sigma_m};
        priors.centres.push_back(c);
        // Up and a horizontal axis: tilt alone left the heading to drift, 2.4 deg
        // over the first 30 s of a capture where its photographs match poorly.
        for (const Vec3 w : {priors.up_w, Vec3{1, 0, 0}}) {
            sfm::PriorUp u;
            u.i = id;
            u.w = w;
            u.u = sfm::mul(sfm::transpose(a.R), w);
            u.sigma = sigma_r;
            priors.ups.push_back(u);
        }
    }
    res.moments = (int64_t)instants.size();

    // ---- triangulate, adjust, and again from the adjusted poses ----
    double scale = 0;
    for (const auto& [id, c] : rec.cameras) scale += c.pixel_scale;
    scale /= (double)rec.cameras.size();
    sfm::BundleOptions bo;
    bo.max_iters = 50;
    bo.loss_param = (float)(2.0 * scale);
    // Freed against BundleOptions' advice for several lenses: a principal point's drift turns the pose
    // with it, and every pose leaves with its own lens, so the rays stay where the photos put them.
    // 300 s PortalCam capture: 1.57 / 5.0 px held, 1.19 / 3.7 freed, on tracks of a free model.
    bo.refine_principal_point = true;
    bo.refine_extra_params = true;
    bo.rigs = &table;
    bo.priors = &priors;
    const sfm::KnownPoseOptions ko{opt.max_error_px, 1.5};
    sfm::KnownPoseStats st;
    for (int round = 0; round < 2; round++) {
        rec.points3D.clear();
        st = sfm::triangulateKnownPoses(rec, db, features, image_of, ko);
        if (st.points == 0) return res;
        sfm::runGlobalBA(rec, bo);
    }
    rec.points3D.clear();
    st = sfm::triangulateKnownPoses(rec, db, features, image_of, ko);
    res.points = (int64_t)st.points;
    std::vector<double> errors, moved, turned;
    for (const auto& [id, p] : rec.points3D) errors.push_back(p.error);
    res.error_px = median(errors);

    // An instant no photo of which sees a point sat on its priors, at the device's frame: a refined
    // principal point would turn its photos off the device's poses, so they keep the factory lens.
    std::set<std::pair<std::string, std::string>> observed;
    for (size_t k = 0; k < placed.size(); k++)
        if (rec.images.at((uint32_t)k + 1).numPoint3D() > 0) observed.insert({placed[k].capture, placed[k].instant});
    for (size_t k = 0; k < placed.size(); k++) {
        if (!observed.count({placed[k].capture, placed[k].instant})) continue;
        Anchor& a = anchors[placed[k].anchor];
        const sfm::Image& im = rec.images.at((uint32_t)k + 1);
        const Mat3 R = sfm::transpose(im.pose.R);
        const Vec3 c = sfm::cameraCenter(im.pose);
        moved.push_back((c - a.centre).norm());
        turned.push_back(angle_deg(R, a.R));
        a.R = R;
        a.centre = c;
        sfm::packColmap(rec.cameras.at(im.camera_id), a.params.data());
    }
    res.moved_m = median(moved);
    res.turned_deg = median(turned);
    return res;
}

RefineResult refine_anchors_in(std::vector<Anchor>& anchors, const std::string& workspace,
                               const RefineOptions& opt) {
    const fs::path ws(workspace);
    std::error_code ec;
    if (!fs::exists(ws / "matches.bin", ec))
        throw std::runtime_error(
            spirula::i18n::format(lmsg::err_refine_no_matches, {ws.string()}));
    const sfm::MatchesDatabase db = sfm::readMatches((ws / "matches.bin").string());
    std::map<std::string, bool> wanted;
    for (const Anchor& a : anchors) wanted[fs::path(a.name).replace_extension().generic_string()] = true;
    std::vector<sfm::FeatureSet> features(db.images.size());
    for (size_t i = 0; i < db.images.size(); i++) {
        if (!wanted.count(db.images[i].name)) continue;
        const fs::path f = ws / "features" / (db.images[i].name + ".bin");
        if (fs::exists(f, ec)) features[i] = sfm::readFeatures(f.string(), false);
    }
    return refine_anchors(anchors, db, features, opt);
}

}  // namespace app::lidar
