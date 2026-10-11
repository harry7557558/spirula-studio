// Model merging: Sim(3) algebra, alignment, track splicing (host only).
//
// Prints PASS/FAIL and returns 0/1. See docs/testing.md.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#include "sfm/core/Model.h"
#include "sfm/map/Assemble.h"
#include "sfm/map/Merge.h"
#include "sfm/tests/TestMain.h"

namespace fs = std::filesystem;
using namespace sfm;

// Camera at C looking at `target`, y-up. Also in sfm_map_test.cpp: a local
// copy beats a shared test-support header for nine lines of scene setup.
static Pose lookAt(const Vec3& C, const Vec3& target) {
    Vec3 f = (target - C).normalized();
    Vec3 up0 = {0, 1, 0};
    Vec3 r = up0.cross(f).normalized();
    Vec3 u = f.cross(r);
    Mat3 R = {r.x, r.y, r.z, u.x, u.y, u.z, f.x, f.y, f.z};
    Vec3 t = mul(R, C);
    return {R, {-t.x, -t.y, -t.z}};
}

// -----------------------------------------------------------------------
// merge-selftest: model merging (host only, no GPU)
// -----------------------------------------------------------------------
//
// The scene is one camera arc split into two overlapping halves, each written
// as its own reconstruction, with the second one moved into a different gauge
// by a known similarity -- exactly what the mapper hands the merger for a
// capture that fragmented (D41/D43). The merge has to recover that similarity
// from the four images the halves share, put the model back together, and
// refuse the cases where it cannot.
int cmdMergeSelftest(int, char**) {
    int fails = 0;
    auto check = [&](bool ok, const char* what) {
        if (!ok) { printf("  FAIL: %s\n", what); fails++; }
        return ok;
    };

    const int W = 1280, H = 960, M = 12, N = 240;
    Camera K = Camera::defaultFor(1, W, H, 1200);
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> ub(-2.5, 2.5);
    std::normal_distribution<double> noise(0.0, 0.25);

    std::vector<Vec3> pts(N);
    for (Vec3& p : pts) p = {ub(rng), ub(rng), ub(rng)};
    std::vector<Pose> gt(M);
    for (int c = 0; c < M; c++) {
        double ang = -1.0 + 2.0 * c / (M - 1);
        gt[c] = lookAt({9 * std::sin(ang), 1.5 * std::sin(0.6 * c), 9 * std::cos(ang)}, {0, 0, 0});
    }
    std::vector<std::vector<Vec2>> obs(M, std::vector<Vec2>(N, {-1000, -1000}));
    std::vector<std::vector<char>> vis(M, std::vector<char>(N, 0));
    for (int c = 0; c < M; c++)
        for (int p = 0; p < N; p++) {
            Vec3 pc = mul(gt[c].R, pts[p]) + gt[c].t;
            Vec2 px = K.project(pc);
            if (pc.z > 0.1 && px.x > 0 && px.x < W && px.y > 0 && px.y < H) {
                obs[c][p] = {px.x + noise(rng), px.y + noise(rng)};
                vis[c][p] = 1;
            }
        }

    // A model over a subset of the cameras: every image carries all N feature
    // slots (so `point2D_idx` means the same keypoint in both halves, which is
    // what merging requires), and every point seen at least twice is triangulated.
    auto buildModel = [&](int first, int last) {
        Reconstruction r;
        r.cameras[1] = K;
        for (int c = first; c <= last; c++) {
            Image im;
            im.id = (uint32_t)c;
            im.camera_id = 1;
            im.name = "cam" + std::to_string(c) + ".jpg";
            im.registered = true;
            im.pose = gt[c];
            im.points2D = obs[c];
            im.point3D_ids.assign(N, kInvalidPoint3D);
            r.images[(uint32_t)c] = std::move(im);
        }
        for (int p = 0; p < N; p++) {
            std::vector<TrackElement> tr;
            for (int c = first; c <= last; c++)
                if (vis[c][p]) tr.push_back({(uint32_t)c, (uint32_t)p});
            if (tr.size() >= 2) r.addPoint3D(pts[p], tr);
        }
        return r;
    };
    auto applySim3 = [](Reconstruction& r, const Sim3& S) {
        for (auto& kv : r.images) kv.second.pose = transformPose(S, kv.second.pose);
        for (auto& kv : r.points3D) kv.second.xyz = transformPoint(S, kv.second.xyz);
    };
    auto meanReproj = [](const Reconstruction& r) {
        double sum = 0;
        size_t n = 0;
        for (const auto& kv : r.points3D)
            for (const TrackElement& e : kv.second.track) {
                const Image& im = r.images.at(e.image_id);
                double v = reprojErrorAt(r.cameras.at(im.camera_id), im.pose,
                                         im.points2D[e.point2D_idx], kv.second.xyz);
                if (v < 1e29) { sum += v; n++; }
            }
        return n ? sum / (double)n : 0.0;
    };

    // ---- 1. Sim3 algebra ----
    // Projection is invariant under a similarity applied to both the world and
    // the cameras: that invariance is the reason a merge is possible at all,
    // and everything below assumes transformPose/transformPoint agree on it.
    Sim3 S;
    S.scale = 2.7;
    S.R = angleAxisToRotation({0.3, -0.7, 0.2});
    S.t = {5, -3, 11};
    {
        double maxPx = 0, maxRt = 0;
        for (int c = 0; c < M; c++) {
            Pose tp = transformPose(S, gt[c]);
            for (int p = 0; p < N; p += 17) {
                Vec2 a = K.project(mul(gt[c].R, pts[p]) + gt[c].t);
                Vec3 X = transformPoint(S, pts[p]);
                Vec2 b = K.project(mul(tp.R, X) + tp.t);
                maxPx = std::max(maxPx, std::hypot(a.x - b.x, a.y - b.y));
                Vec3 back = transformPoint(invertSim3(S), X);
                maxRt = std::max(maxRt, (back - pts[p]).norm());
            }
        }
        printf("merge-selftest: Sim3 invariance %.2e px, inverse round-trip %.2e\n", maxPx, maxRt);
        check(maxPx < 1e-6, "transformPose/transformPoint disagree on projection");
        check(maxRt < 1e-9, "invertSim3 round-trip");

        Sim3 T2;
        T2.scale = 0.4;
        T2.R = angleAxisToRotation({-0.2, 0.5, 0.9});
        T2.t = {-2, 7, 1};
        double maxC = 0;
        for (int p = 0; p < N; p += 17) {
            const Vec3 a = transformPoint(T2, transformPoint(S, pts[p]));
            maxC = std::max(maxC, (a - transformPoint(composeSim3(T2, S), pts[p])).norm());
        }
        check(maxC < 1e-9, "composeSim3 is the two applied in order");
    }

    // ---- 2. Umeyama on exact correspondences ----
    {
        std::vector<Vec3> a, b;
        for (int i = 0; i < 8; i++) {
            Vec3 x = {ub(rng), ub(rng), ub(rng)};
            a.push_back(x);
            b.push_back(transformPoint(S, x));
        }
        Sim3 T;
        bool ok = estimateSim3(a, b, T);
        double err = 0;
        for (size_t i = 0; i < a.size(); i++)
            err = std::max(err, (transformPoint(T, a[i]) - b[i]).norm());
        printf("  estimateSim3: scale %.6f (want %.6f), max residual %.2e\n", T.scale, S.scale,
               err);
        check(ok && err < 1e-9, "estimateSim3 does not reproduce an exact similarity");
        // Degenerate input (all points identical) must be reported, not returned.
        std::vector<Vec3> same(4, Vec3{1, 2, 3});
        Sim3 bad;
        check(!estimateSim3(same, same, bad), "estimateSim3 accepted a degenerate sample");
    }

    // ---- 2b. poses beat centers where captures actually fragment ----
    // Three cameras on a straight line, which is what a corridor or a walked
    // facade gives: their centers leave the rotation about that line free, so
    // the center-only fit is degenerate, while the pose fit is not.
    {
        std::vector<Pose> src, dst;
        for (int i = 0; i < 3; i++) {
            Pose p = lookAt({-2.0 + 2.0 * i, 0, 0}, {-2.0 + 2.0 * i, 0, 6});
            src.push_back(p);
            dst.push_back(transformPose(S, p));
        }
        Sim3 T;
        bool ok = estimateSim3FromPoses(src, dst, T);
        double err = 0;
        for (int p = 0; p < N; p += 13)
            err = std::max(err, (transformPoint(T, pts[p]) - transformPoint(S, pts[p])).norm());
        // The same three centers through Umeyama: it "succeeds" and is wrong.
        std::vector<Vec3> cs, cd;
        for (int i = 0; i < 3; i++) {
            cs.push_back(cameraCenter(src[i]));
            cd.push_back(cameraCenter(dst[i]));
        }
        Sim3 U;
        double uerr = 1e30;
        if (estimateSim3(cs, cd, U)) {
            uerr = 0;
            for (int p = 0; p < N; p += 13)
                uerr = std::max(uerr,
                                (transformPoint(U, pts[p]) - transformPoint(S, pts[p])).norm());
        }
        printf("  collinear shared cameras: pose fit off by %.2e, center fit off by %.2e\n", err,
               uerr);
        check(ok && err < 1e-9, "estimateSim3FromPoses failed on collinear centers");
        check(uerr > 1e-3, "the center-only fit was expected to be degenerate here");
    }

    // ---- 3. alignment from shared images ----
    Reconstruction A = buildModel(0, 7);
    Reconstruction B = buildModel(4, 11);
    applySim3(B, S);  // the second half lives in its own gauge
    {
        MergeOptions limited;
        limited.host_budget_bytes = 1;
        MergeSession session({A, B}, limited);
        bool refused = false;
        try { session.tryMerge(0, 1); }
        catch (const std::runtime_error&) { refused = true; }
        check(refused && session.alive(0) && session.alive(1) &&
                  session.model(0).points3D.size() == A.points3D.size() &&
                  session.model(1).points3D.size() == B.points3D.size(),
              "merge budget failure must preserve both original models");
    }
    {
        AlignmentResult r = alignReconstructions(B, A, MergeOptions());
        double err = 0;
        for (int p = 0; p < N; p += 13)
            err = std::max(err, (transformPoint(r.transform, transformPoint(S, pts[p])) - pts[p])
                                    .norm());
        printf("  alignment: %s, %zu/%zu shared images agree, %.3f px, recovers the gauge to %.2e\n",
               r.success ? "ok" : r.reason.c_str(), r.inliers, r.common_images, r.mean_error, err);
        check(r.success, "alignment failed on four exact shared images");
        check(r.common_images == 4 && r.inliers == 4, "not all shared images were inliers");
        check(err < 1e-2, "recovered similarity is wrong");
    }

    // ---- 4. the merge itself ----
    {
        MergeOptions mo;
        mo.verbose = false;
        MergeSession s({A, B}, mo);
        size_t merges = s.runAuto();
        std::vector<Reconstruction> out = s.take();
        const size_t spliced = s.log().empty() ? 0 : s.log().front().counts.points_spliced;
        double mr = out.empty() ? 0 : meanReproj(out.front());
        printf("  merge: %zu merge(s) -> %zu model(s), %u images, %zu points (%zu spliced), "
               "%.3f px mean\n",
               merges, out.size(), out.empty() ? 0 : out.front().numRegistered(),
               out.empty() ? 0 : out.front().points3D.size(), spliced, mr);
        check(merges == 1 && out.size() == 1, "the two halves did not merge into one model");
        if (!out.empty()) {
            check(out.front().numRegistered() == (uint32_t)M, "merged model lost images");
            // Tracks that meet at a shared image must be spliced, not
            // duplicated: two copies of every point is what a merge that only
            // concatenates would produce.
            check(out.front().points3D.size() <= (size_t)(1.2 * N),
                  "merged model has duplicate points");
            check(spliced > (size_t)(N / 2), "hardly any tracks were spliced");
            check(mr < 1.0, "merged model reprojects badly");
        }
    }

    // ---- 5. too little overlap: refused, models untouched ----
    {
        Reconstruction a2 = buildModel(0, 5), b2 = buildModel(5, 11);
        applySim3(b2, S);
        MergeOptions mo;
        mo.verbose = false;
        MergeSession s({a2, b2}, mo);
        size_t merges = s.runAuto();
        printf("  one shared image: %zu merge(s), %zu candidate(s), models %s\n", merges,
               s.candidates().size(),
               s.alive(0) && s.alive(1) ? "both intact" : "MODIFIED");
        check(merges == 0 && s.candidates().empty(), "merged on a single shared image");
        check(s.alive(0) && s.alive(1) && s.model(0).numRegistered() == 6,
              "a refused merge modified the models");
    }

    // ---- 6. a wrong alignment is detected and undone ----
    // The GUI path (a caller-supplied transform) with a transform that is
    // plausible-looking but wrong: the images it adds land nowhere their
    // observations support, and the merge has to come back off.
    {
        MergeOptions mo;
        mo.verbose = false;
        MergeSession s({A, B}, mo);
        Sim3 bad = invertSim3(S);
        bad.R = mul(angleAxisToRotation({0, 0.5, 0}), bad.R);  // 29 deg off
        bad.scale *= 1.3;
        const size_t points_before = s.model(0).points3D.size();
        const uint32_t imgs_before = s.model(0).numRegistered();
        MergeAttempt a = s.tryMerge(0, 1, &bad);
        printf("  wrong transform: %s (%s), anchor %u images / %zu points -> %u / %zu\n",
               a.merged ? "MERGED" : "refused", a.reason.c_str(), imgs_before, points_before,
               s.model(0).numRegistered(), s.model(0).points3D.size());
        check(!a.merged, "a badly aligned merge was accepted");
        check(s.model(0).numRegistered() == imgs_before &&
                  s.model(0).points3D.size() == points_before,
              "a refused merge left the anchor modified");
        check(s.alive(1), "a refused merge consumed the source model");
    }

    // ---- 7. incompatible models are refused, not merged into garbage ----
    // Same image ids, different keypoints: the assumption that point2D_idx
    // means the same feature in both models is broken, and COLMAP's merge
    // would silently produce nonsense.
    {
        Reconstruction b3 = buildModel(4, 11);
        applySim3(b3, S);
        for (auto& kv : b3.images) kv.second.points2D.resize(N - 1);
        AlignmentResult r = alignReconstructions(b3, A, MergeOptions());
        printf("  mismatched features: %s (%s)\n", r.success ? "ALIGNED" : "refused",
               r.reason.c_str());
        check(!r.success, "aligned two models built from different features");
    }

    // ---- 8. filtering removes what the merge should not keep ----
    {
        Reconstruction f = buildModel(0, 7);
        const size_t before = f.points3D.size();
        f.points3D.begin()->second.xyz = {1000, 1000, 1000};  // nowhere near its track
        size_t robs = 0, rpts = 0;
        filterModel(f, 4.0, 1.5, robs, rpts);
        printf("  filterModel: %zu points -> %zu (%zu obs dropped)\n", before, f.points3D.size(),
               robs);
        check(rpts == 1 && f.points3D.size() == before - 1,
              "filterModel did not drop a point that reprojects nowhere");
    }

    // ---- 9. a fold: two places written on top of each other (D45) ----
    //
    // The failure no other test here can see, because a fold is *self*-
    // consistent. Built literally: a second copy of the arc, its images placed
    // at the first copy's poses and its points at the first copy's points, but
    // with disjoint point ids -- exactly what a wrong merge or a chain of
    // registrations through repeated structure produces. The detector must find
    // co-located, co-oriented image pairs with no structure in common, and the
    // split must put the two copies back in separate models.
    {
        Reconstruction sound = buildModel(0, 11);
        DuplicateOptions dopt;
        DuplicateReport clean = findDuplicateStructure(sound, dopt);
        printf("  fold detector on a sound model: %zu of %zu co-located pairs share nothing\n",
               clean.conflicts, clean.colocated);
        check(!clean.duplicated(dopt), "the fold detector fired on a sound model");

        // The fold. Copy 2's image ids are offset, its poses are copy 1's, and
        // its tracks are its own -- no point is shared with copy 1.
        Reconstruction folded = sound;
        const uint32_t off = 100;
        const uint64_t pid_off = 100000;
        for (int c = 0; c <= 11; c++) {
            Image im = sound.images.at((uint32_t)c);
            im.id = (uint32_t)c + off;
            im.name = "dup" + std::to_string(c) + ".jpg";
            for (uint64_t& p : im.point3D_ids)
                if (p != kInvalidPoint3D) p += pid_off;
            folded.images[im.id] = std::move(im);
        }
        for (const auto& kv : sound.points3D) {
            Point3D p = kv.second;
            for (TrackElement& e : p.track) e.image_id += off;
            folded.points3D[kv.first + pid_off] = std::move(p);
        }
        DuplicateReport rep = findDuplicateStructure(folded, dopt);
        size_t dropped = 0;
        DuplicateCut cut;
        std::vector<Reconstruction> parts =
            splitDuplicateStructure(folded, rep, 4, &dropped, &cut);
        std::vector<uint32_t> sizes;
        for (const Reconstruction& p : parts) sizes.push_back(p.numRegistered());
        printf("  fold detector on two copies at the same poses: %zu of %zu co-located pairs "
               "share nothing -> %zu part(s)", rep.conflicts, rep.colocated, parts.size());
        for (uint32_t n : sizes) printf(" %u", n);
        printf("\n");
        check(rep.duplicated(dopt), "the fold detector missed two copies at identical poses");
        check(parts.size() == 2 && sizes.size() == 2 && sizes[0] == 12 && sizes[1] == 12,
              "the fold split did not separate the two copies");
        // ... and a caller that supplies the correspondence graph must be able
        // to veto: if the two copies *were* matched to each other, the model is
        // right and its tracks are merely split.
        DuplicateReport vetoed = findDuplicateStructure(
            folded, dopt, [](uint32_t, uint32_t) { return true; });
        printf("  ... with every pair reported as matched: %zu conflicts (%zu vetoed)\n",
               vetoed.conflicts, vetoed.unmatched_but_seen);
        check(vetoed.conflicts == 0 && vetoed.unmatched_but_seen == rep.conflicts,
              "the matched-pair veto did not suppress the conflicts");

        // The verdict is the *cut*, not the conflict count (D46). Two copies
        // share nothing, so separating them costs no co-visibility at all --
        // which is what makes the split safe to take by default.
        printf("  ... the split severs %.2f%% of co-visibility (%llu of %llu) -> %s\n",
               100.0 * cut.fraction(), (unsigned long long)cut.severed,
               (unsigned long long)(cut.severed + cut.kept),
               foldSplitAccepted(rep, cut, dopt) ? "folded" : "kept whole");
        check(foldSplitAccepted(rep, cut, dopt), "the fold's split was not accepted");

        // The false positive that the rate alone cannot reject: a sound, densely
        // covisible model with conflicts declared between images that really do
        // share structure. Cutting it has to run through that structure, so the
        // cut is expensive and the split must be refused -- this is drjohnson,
        // which the conflict rate rated *more* folded than the real fold.
        DuplicateReport bogus;
        bogus.colocated = 20;
        bogus.conflicts = 8;
        for (uint32_t i = 0; i + 1 < 12; i += 3) bogus.pairs.push_back({i, i + 1});
        DuplicateCut bcut;
        size_t bdropped = 0;
        std::vector<Reconstruction> bparts =
            splitDuplicateStructure(sound, bogus, 2, &bdropped, &bcut);
        printf("  ... false conflicts inside a sound model: split into %zu, severing %.1f%% "
               "-> %s\n",
               bparts.size(), 100.0 * bcut.fraction(),
               foldSplitAccepted(bogus, bcut, dopt) ? "folded" : "kept whole");
        check(bcut.groups > 1 && !foldSplitAccepted(bogus, bcut, dopt),
              "a split through a sound model's own co-visibility was accepted");

        // The other false positive, which the cut cost cannot reject either:
        // the piece the conflicts cut off has to stand *on top of* something,
        // or it is not a duplicate of anywhere (D67). The same fold with all
        // but three of the second copy's images moved a hundred units away --
        // the conflicts still fire, the cut still costs no co-visibility, and
        // three images out of twelve standing where copy 1 stands is not a
        // fold. This is what tore 358 images off a settled 5500-image model.
        {
            Reconstruction partial = folded;
            for (int c = 3; c <= 11; c++) {
                Pose& p = partial.images.at((uint32_t)c + off).pose;
                p.t = p.t - mul(p.R, Vec3{100, 0, 0});  // centre += (100,0,0)
            }
            DuplicateReport prep = findDuplicateStructure(partial, dopt);
            size_t pdropped = 0;
            DuplicateCut pcut;
            std::vector<Reconstruction> pparts = splitDuplicateStructure(
                partial, prep, 4, &pdropped, &pcut, dopt.min_fold_overlap);
            printf("  ... only 3 of 12 images of the second copy left on top of the first: "
                   "%zu conflicts, %zu part(s), %zu re-attached\n",
                   prep.conflicts, pparts.size(), pcut.reattached);
            check(prep.duplicated(dopt), "the moved copy stopped conflicting at all");
            check(pparts.size() == 1 && pcut.reattached == 1,
                  "a piece standing on top of nothing was written out as a duplicate");

            // ... while the fold itself still splits with the same bar on.
            DuplicateCut fcut;
            size_t fdropped = 0;
            std::vector<Reconstruction> fparts = splitDuplicateStructure(
                folded, rep, 4, &fdropped, &fcut, dopt.min_fold_overlap);
            printf("  ... the real fold under the same bar: %zu part(s), overlap %.0f%%\n",
                   fparts.size(), 100.0 * fcut.min_overlap);
            check(fparts.size() == 2 && fcut.reattached == 0,
                  "the overlap bar rejected a genuine fold");
        }
    }

    printf("%s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}

int main() { return sfmTestMain(0, nullptr, cmdMergeSelftest); }
