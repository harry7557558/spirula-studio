// What a sensor tells the reconstruction about its images, behind one seam:
// a relative rotation between two images, which images were taken next to
// each other, a position in a metric frame, and the factors a bundle
// adjustment may add for a set of posed images. The mapper and the verifier
// see only this; the video's IMU and GPS implement it (map/SensorPriors.h),
// and a LiDAR or wheel odometry would implement it the same way.
// docs/notes/sensor-priors.md.
#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "sfm/ba/Priors.h"
#include "sfm/core/Pose.h"

namespace sfm {

// One registered image as a source sees it: its id, its camera group and its
// pose (world -> camera) in the model's own gauge.
struct PosedImage {
    uint32_t image = 0;
    uint32_t camera = 0;
    Pose pose;
};

class PriorSource {
public:
    virtual ~PriorSource() = default;
    // Whether the source has anything at all to say about an image.
    virtual bool has(uint32_t img) const = 0;
    // R_j <- R_i between two images' camera frames (world -> camera
    // rotations: R_j ~ R_ji R_i), with its 1-sigma in radians.
    virtual bool relativeRotation(uint32_t i, uint32_t j, Mat3& R_ji, double& sigma) const = 0;
    // Images the source relates to `img` directly, nearest first.
    virtual std::vector<uint32_t> neighbours(uint32_t img) const = 0;
    // Everything the source can state about these images, in their own
    // gauge: the source fits whatever it needs for that (an up axis, a scale,
    // a metric frame) from the poses it is handed. Image ids as given.
    virtual PosePriors factors(const std::vector<PosedImage>& imgs) = 0;
    // A position in the source's metric frame (GPS in east-north-up), for
    // pairing images by where they were taken.
    virtual bool position(uint32_t img, Vec3& p) const {
        (void)img;
        (void)p;
        return false;
    }
};

// A source over a database seen through a renumbering (map/Atoms.h): local
// ids in, local ids out, the wrapped source only ever sees global ones.
class RemappedPriorSource : public PriorSource {
public:
    RemappedPriorSource(PriorSource& inner, std::vector<uint32_t> to_global)
        : inner_(inner), to_global_(std::move(to_global)) {
        for (uint32_t i = 0; i < to_global_.size(); i++) to_local_.emplace(to_global_[i], i);
    }
    bool has(uint32_t img) const override {
        return img < to_global_.size() && inner_.has(to_global_[img]);
    }
    bool relativeRotation(uint32_t i, uint32_t j, Mat3& R, double& sigma) const override {
        if (i >= to_global_.size() || j >= to_global_.size()) return false;
        return inner_.relativeRotation(to_global_[i], to_global_[j], R, sigma);
    }
    std::vector<uint32_t> neighbours(uint32_t img) const override {
        std::vector<uint32_t> out;
        if (img >= to_global_.size()) return out;
        for (uint32_t g : inner_.neighbours(to_global_[img])) {
            auto it = to_local_.find(g);
            if (it != to_local_.end()) out.push_back(it->second);
        }
        return out;
    }
    PosePriors factors(const std::vector<PosedImage>& imgs) override {
        std::vector<PosedImage> g = imgs;
        for (PosedImage& p : g) p.image = p.image < to_global_.size() ? to_global_[p.image] : ~0u;
        PosePriors pr = inner_.factors(g);
        auto local = [&](uint32_t& id) {
            auto it = to_local_.find(id);
            if (it == to_local_.end()) return false;
            id = it->second;
            return true;
        };
        PosePriors out;
        out.up_w = pr.up_w;
        out.huber = pr.huber;
        for (PriorRotation r : pr.rotations)
            if (local(r.i) && local(r.j)) out.rotations.push_back(r);
        for (PriorUp u : pr.ups)
            if (local(u.i)) out.ups.push_back(u);
        for (PriorCentre c : pr.centres) {
            bool ok = true;
            for (int k = 0; k < c.n; k++) ok = ok && local(c.img[k]);
            if (ok) out.centres.push_back(c);
        }
        return out;
    }
    bool position(uint32_t img, Vec3& p) const override {
        return img < to_global_.size() && inner_.position(to_global_[img], p);
    }

private:
    PriorSource& inner_;
    std::vector<uint32_t> to_global_;
    std::unordered_map<uint32_t, uint32_t> to_local_;
};

}  // namespace sfm
