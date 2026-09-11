// Camera rigs: images whose relative poses are fixed. A rig has members (one
// per lens) and frames (one image per member, taken at one instant); the
// bundle adjustment optimizes a pose per FRAME plus one cam_from_rig per member
// (map/Bundle.h). docs/notes/sfm-rig-constraints.md has the design.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "sfm/core/Pose.h"

namespace sfm {

constexpr uint32_t kNoImage = UINT32_MAX;
constexpr uint32_t kNoRig = UINT32_MAX;

// One member as the user describes it: a path prefix under the image
// directory, and optionally the extrinsic it is known to have.
struct RigMemberDef {
    std::string prefix;
    bool has_ext = false;
    Pose ext;             // cam_from_rig, in the user's units
    bool ext_fixed = true;
};

// Members are path prefixes; with `captures` they are relative to each of
// those prefixes in turn, so one physical rig behind several videos is one
// definition and a frame is keyed by its capture as well as its name.
struct RigDef {
    std::string name;
    std::vector<RigMemberDef> members;
    std::vector<std::string> captures;  // "*" = every top-level folder
};

// The rig resolved against a database: image ids per frame.
struct RigSpec {
    std::string name;
    std::vector<RigMemberDef> members;
    std::vector<std::vector<uint32_t>> frames;  // frames[f][m], kNoImage where absent
    std::vector<std::string> frame_keys;        // what the images of a frame share
    bool anyKnownExt() const {
        for (const RigMemberDef& m : members)
            if (m.has_ext) return true;
        return false;
    }
};

struct RigSlot {
    uint32_t rig = kNoRig, frame = 0, member = 0;
    bool valid() const { return rig != kNoRig; }
};

// Every rig of a run, with the inverse lookup the mapper needs per image.
struct RigTable {
    std::vector<RigSpec> rigs;
    std::vector<RigSlot> of_image;  // by image id

    bool empty() const { return rigs.empty(); }
    RigSlot slot(uint32_t img) const {
        return img < of_image.size() ? of_image[img] : RigSlot{};
    }
    const std::vector<uint32_t>& frameOf(RigSlot s) const { return rigs[s.rig].frames[s.frame]; }

    // Fill of_image from the frames; throws on an image claimed twice.
    void index(size_t num_images) {
        of_image.assign(num_images, RigSlot{});
        for (uint32_t r = 0; r < rigs.size(); r++) {
            const RigSpec& rig = rigs[r];
            for (uint32_t f = 0; f < rig.frames.size(); f++) {
                if (rig.frames[f].size() != rig.members.size())
                    throw std::runtime_error("rig " + rig.name + ": frame " + std::to_string(f) +
                                             " does not have one slot per member");
                for (uint32_t m = 0; m < rig.frames[f].size(); m++) {
                    const uint32_t img = rig.frames[f][m];
                    if (img == kNoImage) continue;
                    if (img >= num_images)
                        throw std::runtime_error("rig " + rig.name + ": image id out of range");
                    if (of_image[img].valid()) {
                        const RigSlot& o = of_image[img];
                        throw std::runtime_error(
                            "image " + std::to_string(img) + " is in two rig frames (rig " +
                            rigs[o.rig].name + " frame " + std::to_string(o.frame) + " and rig " +
                            rig.name + " frame " + std::to_string(f) + "): conflicting rigs");
                    }
                    of_image[img] = {r, f, m};
                }
            }
        }
    }

    // The same rigs over a sub-database (sfm/map/Atoms.h): `local[g]` is the
    // local id of database image g, or kNoImage when the atom lacks it.
    RigTable subset(const std::vector<uint32_t>& local, size_t num_local) const {
        RigTable out;
        for (const RigSpec& rig : rigs) {
            RigSpec s;
            s.name = rig.name;
            s.members = rig.members;
            for (size_t f = 0; f < rig.frames.size(); f++) {
                std::vector<uint32_t> fr(rig.members.size(), kNoImage);
                size_t n = 0;
                for (size_t m = 0; m < fr.size(); m++) {
                    const uint32_t g = rig.frames[f][m];
                    if (g == kNoImage || g >= local.size() || local[g] == kNoImage) continue;
                    fr[m] = local[g];
                    n++;
                }
                if (!n) continue;
                s.frames.push_back(std::move(fr));
                if (f < rig.frame_keys.size()) s.frame_keys.push_back(rig.frame_keys[f]);
            }
            out.rigs.push_back(std::move(s));
        }
        out.index(num_local);
        return out;
    }

    // Image ids shifted by `offset`, for a problem that stacks several models
    // (map/Bundle.h runJointBA).
    RigTable shifted(uint32_t offset, size_t num_images) const {
        RigTable out = *this;
        for (RigSpec& rig : out.rigs)
            for (auto& fr : rig.frames)
                for (uint32_t& img : fr)
                    if (img != kNoImage) img += offset;
        out.index(num_images);
        return out;
    }
};

// `--rig [CAPTURES:]MEMBERS`, both comma-separated prefixes: `cam0,cam1`,
// `vid1,vid2:cam0,cam1`, `*:cam0,cam1`. "" on success.
inline std::string parseRigArg(const std::string& v, RigDef& out) {
    out = RigDef{};
    auto split = [](const std::string& s, std::vector<std::string>& into) {
        for (size_t i = 0;;) {
            size_t c = s.find(',', i);
            if (c == std::string::npos) c = s.size();
            std::string p = s.substr(i, c - i);
            while (!p.empty() && (p.back() == '/' || p.back() == '\\')) p.pop_back();
            if (p.empty()) return false;
            into.push_back(p);
            if (c == s.size()) break;
            i = c + 1;
        }
        return true;
    };
    const size_t colon = v.find(':');
    std::vector<std::string> members;
    if (colon != std::string::npos && !split(v.substr(0, colon), out.captures))
        return "--rig '" + v + "': empty capture prefix";
    if (!split(colon == std::string::npos ? v : v.substr(colon + 1), members))
        return "--rig '" + v + "': empty member prefix";
    for (const std::string& p : members) {
        RigMemberDef m;
        m.prefix = p;
        out.members.push_back(m);
    }
    if (out.members.size() < 2) return "--rig '" + v + "': a rig needs at least two members";
    return {};
}

// ---- resolving definitions against image names ----------------------------

namespace rig_detail {

inline bool prefixMatches(const std::string& name, const std::string& prefix) {
    if (prefix.empty()) return true;
    if (name.size() < prefix.size() || name.compare(0, prefix.size(), prefix) != 0) return false;
    return name.size() == prefix.size() || name[prefix.size()] == '/';
}

// The part of a name that identifies its frame: the path under the member's
// prefix, as given -- database names carry no extension, and a timestamp
// name such as 1756371636.290711 keeps its fraction.
inline std::string frameKey(const std::string& name, const std::string& prefix) {
    std::string rest = prefix.empty() ? name : name.substr(prefix.size());
    if (!rest.empty() && rest[0] == '/') rest.erase(0, 1);
    return rest;
}

}  // namespace rig_detail

// Group `names` (image names, by id) into the frames each definition
// describes. The longest member prefix wins within a rig; an image claimed by
// two rigs, or a member no image matches, is an error.
inline RigTable buildRigTable(const std::vector<std::string>& names,
                              const std::vector<RigDef>& defs) {
    RigTable out;
    for (const RigDef& d : defs) {
        if (d.members.size() < 2)
            throw std::runtime_error("rig " + d.name + ": a rig needs at least two members");
        size_t known = 0;
        for (const RigMemberDef& m : d.members) known += m.has_ext ? 1 : 0;
        if (known && known != d.members.size())
            throw std::runtime_error("rig " + d.name +
                                     ": extrinsics were given for some members but not all");
        RigSpec s;
        s.name = d.name.empty() ? "rig" + std::to_string(out.rigs.size()) : d.name;
        s.members = d.members;
        // The captures the members are relative to: the ones named, every
        // top-level folder for "*", or the one empty capture (absolute members).
        std::vector<std::string> captures = d.captures;
        if (captures.size() == 1 && captures[0] == "*") {
            std::set<std::string> tops;
            for (const std::string& n : names) {
                const size_t slash = n.find('/');
                if (slash != std::string::npos) tops.insert(n.substr(0, slash));
            }
            captures.assign(tops.begin(), tops.end());
            if (captures.empty())
                throw std::runtime_error("rig " + s.name + ": '*' found no folders to be captures");
        }
        if (captures.empty()) captures.push_back("");
        std::map<std::string, size_t> key_to_frame;
        std::vector<size_t> used(d.members.size(), 0);
        for (uint32_t img = 0; img < names.size(); img++) {
            int best = -1, best_cap = -1;
            size_t best_len = 0;
            for (size_t ci = 0; ci < captures.size(); ci++)
                for (size_t m = 0; m < d.members.size(); m++) {
                    const std::string pre = captures[ci].empty()
                                                ? d.members[m].prefix
                                                : captures[ci] + "/" + d.members[m].prefix;
                    if (!rig_detail::prefixMatches(names[img], pre)) continue;
                    if (best < 0 || pre.size() > best_len) {
                        best = (int)m;
                        best_cap = (int)ci;
                        best_len = pre.size();
                    }
                }
            if (best < 0) continue;
            const std::string pre = captures[best_cap].empty()
                                        ? d.members[best].prefix
                                        : captures[best_cap] + "/" + d.members[best].prefix;
            std::string key = rig_detail::frameKey(names[img], pre);
            if (!captures[best_cap].empty()) key = std::to_string(best_cap) + "/" + key;
            auto it = key_to_frame.find(key);
            if (it == key_to_frame.end()) {
                it = key_to_frame.emplace(key, s.frames.size()).first;
                s.frames.emplace_back(d.members.size(), kNoImage);
                s.frame_keys.push_back(key);
            }
            std::vector<uint32_t>& fr = s.frames[it->second];
            if (fr[best] != kNoImage)
                throw std::runtime_error("rig " + s.name + ": images " + names[fr[best]] +
                                         " and " + names[img] + " both fill member " +
                                         d.members[best].prefix + " of frame " + key);
            fr[best] = img;
            used[best]++;
        }
        for (size_t m = 0; m < d.members.size(); m++)
            if (!used[m])
                throw std::runtime_error("rig " + s.name + ": no image matches member '" +
                                         d.members[m].prefix + "'");
        out.rigs.push_back(std::move(s));
    }
    out.index(names.size());
    return out;
}

// ---- the calibration a model carries -------------------------------------

inline Pose invertPose(const Pose& p) {
    Mat3 Rt = transpose(p.R);
    Vec3 t = mul(Rt, p.t);
    return {Rt, {-t.x, -t.y, -t.z}};
}

// A -> B given world -> A and world -> B.
inline Pose relativePose(const Pose& a, const Pose& b) {
    Mat3 R = mul(b.R, transpose(a.R));
    return {R, b.t - mul(R, a.t)};
}

inline double rotationAngleDeg(const Mat3& R) {
    const double tr = std::max(-1.0, std::min(1.0, (R[0] + R[4] + R[8] - 1.0) * 0.5));
    return std::acos(tr) * 180.0 / M_PI;
}

// One rig's extrinsics in a model's gauge. The rig frame is the reference
// member's camera frame (cam_from_rig[ref] = identity); translations are in
// model units and scale with the model (transformRigs).
struct RigCalib {
    int ref = -1;
    std::vector<Pose> cam_from_rig;
    std::vector<uint8_t> established;  // per member: usable as a constraint
    std::vector<uint8_t> fixed;        // per member: held by the bundle adjustment
    std::vector<uint32_t> support;     // frames the estimate rests on
    std::vector<double> spread_deg;    // median angular deviation over them
    std::vector<uint32_t> declined_at; // frames when last reported unsynchronized
    double user_scale = 0;             // model units per user unit, 0 = unknown

    bool usable(uint32_t m) const { return ref >= 0 && m < established.size() && established[m]; }
    size_t numEstablished() const {
        size_t n = 0;
        for (uint8_t e : established) n += e ? 1 : 0;
        return n;
    }
    void resize(size_t n) {
        cam_from_rig.assign(n, Pose{mat3Identity(), {0, 0, 0}});
        established.assign(n, 0);
        fixed.assign(n, 0);
        support.assign(n, 0);
        spread_deg.assign(n, 0);
        declined_at.assign(n, 0);
    }
    // The frame's pose from one of its images, and the reverse.
    Pose rigFromWorld(uint32_t m, const Pose& cam_from_world) const {
        return composePose(invertPose(cam_from_rig[m]), cam_from_world);
    }
    Pose camFromWorld(uint32_t m, const Pose& rig_from_world) const {
        return composePose(cam_from_rig[m], rig_from_world);
    }
    // Where member `to` sits given member `from`'s pose.
    Pose predict(uint32_t from, uint32_t to, const Pose& cam_from_world) const {
        return camFromWorld(to, rigFromWorld(from, cam_from_world));
    }
};

// A model's calibrations transform with its gauge: rotations are unchanged
// and translations scale (transformPose composes exactly, see Pose.h).
inline void transformRigs(std::vector<RigCalib>& rigs, double scale) {
    for (RigCalib& c : rigs)
        for (Pose& p : c.cam_from_rig) p.t = p.t * scale;
}

struct RigCalibOptions {
    // Frames holding both a member and the reference before the member's
    // extrinsic is trusted, and the share that must agree with it: lenses that
    // did not fire together agree on 55-80% of frames, a rig on nearly all.
    int min_frames = 3;
    double min_inlier_frac = 0.9;
    // A member whose frames' relative poses deviate from their average by more
    // than this (median, degrees) is not rigid. A rig measures 0.2-0.45.
    double max_spread_deg = 1.0;
};

namespace rig_detail {

// The chordal mean of rotations, projected back onto SO(3).
inline Mat3 meanRotation(const std::vector<Mat3>& Rs, const std::vector<char>* mask = nullptr) {
    Mat3 acc{};
    for (size_t i = 0; i < Rs.size(); i++) {
        if (mask && !(*mask)[i]) continue;
        for (int k = 0; k < 9; k++) acc[k] += Rs[i][k];
    }
    Svd3 s = svd3(acc);
    Mat3 D = mat3Identity();
    D[8] = det3(s.U) * det3(s.V) < 0 ? -1.0 : 1.0;
    return mul(mul(s.U, D), transpose(s.V));
}

inline double medianOf(std::vector<double> v) {
    if (v.empty()) return 0;
    std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
    return v[v.size() / 2];
}

}  // namespace rig_detail

// Robust average of cam_ref -> cam_m over the frames both are registered in.
// Returns the inlier count; `spread` the median deviation among them.
inline int averageRelativePoses(const std::vector<Pose>& rel, const RigCalibOptions& opt,
                                Pose& out, double& spread, const Mat3* fixed_R = nullptr) {
    using rig_detail::meanRotation;
    using rig_detail::medianOf;
    const size_t n = rel.size();
    if (!n) return 0;
    std::vector<Mat3> Rs(n);
    for (size_t i = 0; i < n; i++) Rs[i] = rel[i].R;
    Mat3 R0 = fixed_R ? *fixed_R : meanRotation(Rs);
    std::vector<double> dev(n);
    for (size_t i = 0; i < n; i++) dev[i] = rotationAngleDeg(mul(Rs[i], transpose(R0)));
    const double med = medianOf(dev);
    const double thr = std::max(3.0 * opt.max_spread_deg, 3.0 * med);
    std::vector<char> in(n, 0);
    int count = 0;
    for (size_t i = 0; i < n; i++) count += (in[i] = dev[i] <= thr) ? 1 : 0;
    if (!count) return 0;
    if (!fixed_R) {
        R0 = meanRotation(Rs, &in);
        for (size_t i = 0; i < n; i++) dev[i] = rotationAngleDeg(mul(Rs[i], transpose(R0)));
    }
    std::vector<double> tx, ty, tz, dv;
    for (size_t i = 0; i < n; i++) {
        if (!in[i]) continue;
        // Translation expressed against the averaged rotation, so a frame's
        // rotation error does not leak into its baseline vote.
        tx.push_back(rel[i].t.x);
        ty.push_back(rel[i].t.y);
        tz.push_back(rel[i].t.z);
        dv.push_back(dev[i]);
    }
    out.R = R0;
    out.t = {medianOf(tx), medianOf(ty), medianOf(tz)};
    spread = medianOf(dv);
    return count;
}

}  // namespace sfm
