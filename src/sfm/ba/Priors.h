// Camera-side priors for the bundle adjustment: residuals on the poses alone
// -- a relative rotation, a gravity direction, a linear constraint on camera
// centres -- that a sensor states and no observation does. There are
// O(frames) of them against O(observations) reprojections, so they are
// evaluated on the host and handed to either solver as sparse 6x6 frame
// blocks (docs/notes/sensor-priors.md has the algebra).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

#include "sfm/ba/Problem.h"
#include "sfm/core/Pose.h"

namespace sfm {

// R_j ~ R_ji R_i between two images' camera frames (world -> camera).
struct PriorRotation {
    uint32_t i = 0, j = 0;
    Mat3 R_ji = mat3Identity();
    double sigma = 0.01;   // radians
};

// R_i up_w ~ u: the world up axis seen from image i's camera frame.
struct PriorUp {
    uint32_t i = 0;
    Vec3 u{0, -1, 0};
    double sigma = 0.05;   // radians
};

// sum_k A_k c_{img_k} ~ b on camera centres, each axis weighted by 1/sigma
// (0 drops the axis). One term is an absolute position, two a displacement,
// three the velocity-free inertial triple.
struct PriorCentre {
    int n = 0;
    uint32_t img[3] = {0, 0, 0};
    Mat3 A[3] = {mat3Identity(), mat3Identity(), mat3Identity()};
    Vec3 b;
    Vec3 sigma{1, 1, 1};
};

// Image indices are whatever the holder says: the mapper fills them with
// reconstruction image ids, buildBundle remaps them to BA indices.
struct PosePriors {
    Vec3 up_w{0, 0, 1};    // the world up every PriorUp is measured against
    double huber = 1.345;  // in sigmas, per factor
    std::vector<PriorRotation> rotations;
    std::vector<PriorUp> ups;
    std::vector<PriorCentre> centres;
    bool empty() const { return rotations.empty() && ups.empty() && centres.empty(); }
    size_t size() const { return rotations.size() + ups.size() + centres.size(); }
};

// The priors' normal equations as a CSR over frames of 6x6 blocks, both
// orderings of a pair listed, plus the gradient over pose_dim: block element
// [6a + b] takes x[6 col + b] into y[6 row + a].
class PriorAssembler {
public:
    void init(const BAProblem& P) {
        const PosePriors* pr = P.priors;
        nfact_ = 0;
        fact_.clear();
        rows_.clear();
        cols_.clear();
        erow_.clear();
        blk_.clear();
        g_.clear();
        nframes_ = P.num_frames;
        if (!pr || pr->empty()) return;
        auto frameOf = [&](uint32_t img) { return P.image_frame[img]; };
        std::map<std::pair<uint32_t, uint32_t>, uint32_t> id;
        auto note = [&](Fact& f) {
            for (int a = 0; a < f.nf; a++)
                for (int b = 0; b < f.nf; b++)
                    id.emplace(std::make_pair(f.frame[a], f.frame[b]), 0u);
        };
        for (const PriorRotation& r : pr->rotations) {
            if (r.i >= P.num_images || r.j >= P.num_images || !(r.sigma > 0)) continue;
            Fact f;
            f.kind = 0;
            f.index = (uint32_t)(&r - pr->rotations.data());
            f.addFrame(frameOf(r.i));
            f.addFrame(frameOf(r.j));
            note(f);
            fact_.push_back(f);
        }
        for (const PriorUp& u : pr->ups) {
            if (u.i >= P.num_images || !(u.sigma > 0)) continue;
            Fact f;
            f.kind = 1;
            f.index = (uint32_t)(&u - pr->ups.data());
            f.addFrame(frameOf(u.i));
            note(f);
            fact_.push_back(f);
        }
        for (const PriorCentre& c : pr->centres) {
            bool ok = c.n >= 1 && c.n <= 3;
            for (int k = 0; ok && k < c.n; k++) ok = c.img[k] < P.num_images;
            if (!ok) continue;
            Fact f;
            f.kind = 2;
            f.index = (uint32_t)(&c - pr->centres.data());
            for (int k = 0; k < c.n; k++) f.addFrame(frameOf(c.img[k]));
            note(f);
            fact_.push_back(f);
        }
        nfact_ = fact_.size();
        // CSR in (row, col) order: std::map iterates that way.
        rows_.assign(nframes_ + 1, 0);
        uint32_t e = 0;
        for (auto& kv : id) {
            kv.second = e++;
            rows_[kv.first.first + 1]++;
            cols_.push_back(kv.first.second);
            erow_.push_back(kv.first.first);
        }
        for (uint32_t f = 0; f < nframes_; f++) rows_[f + 1] += rows_[f];
        for (Fact& f : fact_)
            for (int a = 0; a < f.nf; a++)
                for (int b = 0; b < f.nf; b++)
                    f.entry[a][b] = id.at({f.frame[a], f.frame[b]});
        blk_.assign(36 * (size_t)e, 0.0);
        g_.assign(6 * (size_t)nframes_, 0.0);
    }

    bool empty() const { return nfact_ == 0; }
    uint32_t numEntries() const { return (uint32_t)cols_.size(); }
    const std::vector<uint32_t>& rows() const { return rows_; }
    const std::vector<uint32_t>& cols() const { return cols_; }
    const std::vector<uint32_t>& entryRow() const { return erow_; }
    const std::vector<double>& blocks() const { return blk_; }
    const std::vector<double>& gradient() const { return g_; }

    // 0.5 sum rho(|r|^2) at the given parameters (poses 6 per frame, exts 6
    // per member).
    double cost(const BAProblem& P, const double* poses, const double* exts) const {
        double c = 0;
        Eval ev;
        for (const Fact& f : fact_) {
            evaluate(P, poses, exts, f, ev);
            c += 0.5 * robustCost(ev.r);
        }
        return c;
    }

    // Blocks and gradient at the given parameters, the diagonal blocks damped
    // by (1 + damping) as the observation kernels damp theirs. Returns the cost.
    double assemble(const BAProblem& P, const double* poses, const double* exts, double damping) {
        std::fill(blk_.begin(), blk_.end(), 0.0);
        std::fill(g_.begin(), g_.end(), 0.0);
        double c = 0;
        Eval ev;
        for (const Fact& f : fact_) {
            evaluate(P, poses, exts, f, ev);
            const double s = ev.r[0] * ev.r[0] + ev.r[1] * ev.r[1] + ev.r[2] * ev.r[2];
            c += 0.5 * robustCost(ev.r);
            const double w = robustWeight(s);
            for (int a = 0; a < f.nf; a++) {
                double* ga = &g_[6 * (size_t)f.frame[a]];
                for (int p = 0; p < 6; p++) {
                    double v = 0;
                    for (int m = 0; m < 3; m++) v += ev.J[a][m][p] * ev.r[m];
                    ga[p] += w * v;
                }
                for (int b = 0; b < f.nf; b++) {
                    double* B = &blk_[36 * (size_t)f.entry[a][b]];
                    for (int p = 0; p < 6; p++)
                        for (int q = 0; q < 6; q++) {
                            double v = 0;
                            for (int m = 0; m < 3; m++) v += ev.J[a][m][p] * ev.J[b][m][q];
                            B[6 * p + q] += w * v;
                        }
                }
            }
        }
        if (damping != 0)
            for (uint32_t e = 0; e < cols_.size(); e++)
                if (cols_[e] == erow_[e])
                    for (int p = 0; p < 6; p++) blk_[36 * (size_t)e + 7 * p] *= 1.0 + damping;
        return c;
    }

    // One factor's residual and Jacobians, for the tests: `frames` are the
    // frame slots the rows of `J` differentiate against.
    int debugFactor(const BAProblem& P, const double* poses, const double* exts, size_t k,
                    double r[3], double J[3][3][6], uint32_t frames[3]) const {
        Eval ev;
        evaluate(P, poses, exts, fact_[k], ev);
        for (int m = 0; m < 3; m++) r[m] = ev.r[m];
        for (int a = 0; a < 3; a++)
            for (int m = 0; m < 3; m++)
                for (int q = 0; q < 6; q++) J[a][m][q] = ev.J[a][m][q];
        for (int a = 0; a < 3; a++) frames[a] = fact_[k].frame[a];
        return fact_[k].nf;
    }
    size_t numFactors() const { return nfact_; }

private:
    struct Fact {
        int kind = 0;      // 0 rotation, 1 up, 2 centre
        uint32_t index = 0;
        int nf = 0;
        uint32_t frame[3] = {0, 0, 0};
        uint32_t entry[3][3] = {};
        void addFrame(uint32_t f) {
            for (int k = 0; k < nf; k++)
                if (frame[k] == f) return;
            frame[nf++] = f;
        }
        int slot(uint32_t f) const {
            for (int k = 0; k < nf; k++)
                if (frame[k] == f) return k;
            return -1;
        }
    };
    struct Eval {
        double r[3];
        double J[3][3][6];   // per frame slot, 3 x [angle-axis 3 | t 3]
    };

    // One image's chain: its frame block and the member on top of it.
    struct Cam {
        uint32_t frame = 0;
        Mat3 Rf, Rm, Rc;   // frame, member, camera = Rm Rf
        Vec3 tf, tm;
        Mat3 Jl;           // left Jacobian of the frame's angle-axis
    };

    static Cam camOf(const BAProblem& P, const double* poses, const double* exts, uint32_t img) {
        Cam c;
        c.frame = P.image_frame[img];
        const double* q = poses + 6 * (size_t)c.frame;
        const Vec3 aa{q[0], q[1], q[2]};
        c.Rf = angleAxisToRotation(aa);
        c.tf = {q[3], q[4], q[5]};
        c.Jl = so3LeftJacobian(aa);
        const uint32_t m = P.image_member[img];
        if (m == kNoMember) {
            c.Rm = mat3Identity();
            c.tm = {0, 0, 0};
            c.Rc = c.Rf;
        } else {
            const double* e = exts + P.members[m].ext_offset;
            c.Rm = angleAxisToRotation({e[0], e[1], e[2]});
            c.tm = {e[3], e[4], e[5]};
            c.Rc = mul(c.Rm, c.Rf);
        }
        return c;
    }

    // d r / d delta_cam (rows x 3) into the frame's angle-axis columns:
    // delta_cam = Rm delta_f and delta_f = Jl(a) da.
    static void rotCols(const Mat3& D, const Cam& c, double scale, double J[3][6]) {
        const Mat3 M = mul(mul(D, c.Rm), c.Jl);
        for (int m = 0; m < 3; m++)
            for (int p = 0; p < 3; p++) J[m][p] += scale * M[3 * m + p];
    }

    double robustCost(const double r[3]) const {
        const double s = r[0] * r[0] + r[1] * r[1] + r[2] * r[2];
        const double k2 = huber_ * huber_;
        return s <= k2 ? s : 2.0 * huber_ * std::sqrt(s) - k2;
    }
    double robustWeight(double s) const {
        const double k2 = huber_ * huber_;
        return s <= k2 ? 1.0 : huber_ / std::sqrt(s);
    }

    void evaluate(const BAProblem& P, const double* poses, const double* exts, const Fact& f,
                  Eval& ev) const {
        const PosePriors& pr = *P.priors;
        huber_ = pr.huber;
        for (int a = 0; a < 3; a++)
            for (int m = 0; m < 3; m++)
                for (int p = 0; p < 6; p++) ev.J[a][m][p] = 0;
        if (f.kind == 0) {
            const PriorRotation& q = pr.rotations[f.index];
            const Cam ci = camOf(P, poses, exts, q.i), cj = camOf(P, poses, exts, q.j);
            const Mat3 E = mul(mul(cj.Rc, transpose(ci.Rc)), transpose(q.R_ji));
            const Vec3 e = so3Log(E);
            const double inv = 1.0 / q.sigma;
            ev.r[0] = e.x * inv;
            ev.r[1] = e.y * inv;
            ev.r[2] = e.z * inv;
            // Exp(dj) E = Exp(e + Jl^-1(e) dj); E Exp(-R di) = Exp(e - Jr^-1(e) R di).
            rotCols(so3LeftJacobianInv(e), cj, inv, ev.J[f.slot(cj.frame)]);
            rotCols(mul(so3RightJacobianInv(e), q.R_ji), ci, -inv, ev.J[f.slot(ci.frame)]);
            return;
        }
        if (f.kind == 1) {
            const PriorUp& q = pr.ups[f.index];
            const Cam c = camOf(P, poses, exts, q.i);
            const Vec3 g = mul(c.Rc, pr.up_w);
            const double inv = 1.0 / q.sigma;
            ev.r[0] = (g.x - q.u.x) * inv;
            ev.r[1] = (g.y - q.u.y) * inv;
            ev.r[2] = (g.z - q.u.z) * inv;
            // Exp(d) g = g + d x g = g - [g]x d.
            rotCols(crossMatrix(g), c, -inv, ev.J[f.slot(c.frame)]);
            return;
        }
        const PriorCentre& q = pr.centres[f.index];
        Vec3 sum{0, 0, 0};
        for (int k = 0; k < q.n; k++) {
            const Cam c = camOf(P, poses, exts, q.img[k]);
            // c = -Rf^T a with a = tf + Rm^T tm: dc/dt = -Rf^T, dc/dd = -Rf^T [a]x.
            const Vec3 a = c.tf + mul(transpose(c.Rm), c.tm);
            const Vec3 centre = mul(transpose(c.Rf), a) * -1.0;
            sum = sum + mul(q.A[k], centre);
            const Mat3 dt = mul(q.A[k], transpose(c.Rf));
            const Mat3 dd = mul(mul(dt, crossMatrix(a)), c.Jl);
            double (*J)[6] = ev.J[f.slot(c.frame)];
            for (int m = 0; m < 3; m++) {
                const double sg = (&q.sigma.x)[m];
                const double inv = sg > 0 ? 1.0 / sg : 0.0;
                for (int p = 0; p < 3; p++) {
                    J[m][p] += -inv * dd[3 * m + p];
                    J[m][3 + p] += -inv * dt[3 * m + p];
                }
            }
        }
        const Vec3 d = sum - q.b;
        for (int m = 0; m < 3; m++) {
            const double sg = (&q.sigma.x)[m];
            ev.r[m] = sg > 0 ? (&d.x)[m] / sg : 0.0;
        }
    }

    std::vector<Fact> fact_;
    size_t nfact_ = 0;
    uint32_t nframes_ = 0;
    mutable double huber_ = 1.345;
    std::vector<uint32_t> rows_, cols_, erow_;
    std::vector<double> blk_, g_;
};

}  // namespace sfm
