// PostSplit.cpp -- bake_post_split: the POST-split camera arrays the engine
// consumes, one pinhole face per K (camhost::plan_split_faces), from the
// PER-INPUT cameras a parser returns.

#include "data/DatasetParser.h"

#include "core/CameraModel.h"
#include "core/Env.h"
#include "data/CameraMath.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>

namespace {

constexpr double kPi = 3.14159265358979323846;   // MSVC has no M_PI by default

camhost::Camera host_camera(const ParsedDataset& ds, int64_t i) {
    camhost::Camera c;
    c.model  = ds.camera_models[(size_t)i];
    c.tier   = ds.camera_distortions[(size_t)i];
    c.width  = ds.widths[(size_t)i];
    c.height = ds.heights[(size_t)i];
    c.fx = ds.intrins[(size_t)i * 4 + 0];
    c.fy = ds.intrins[(size_t)i * 4 + 1];
    c.cx = ds.intrins[(size_t)i * 4 + 2];
    c.cy = ds.intrins[(size_t)i * 4 + 3];
    for (int k = 0; k < kCameraDistortionParams; ++k)
        c.dist[k] = ds.dist_coeffs[(size_t)i * kCameraDistortionParams + k];
    if (!ds.redistort.empty() && ds.redistort[(size_t)i].source_model >= 0) {
        c.source_model = ds.redistort[(size_t)i].source_model;
        std::copy(std::begin(ds.redistort[(size_t)i].params),
                  std::end(ds.redistort[(size_t)i].params),
                  std::begin(c.source_params));
    }
    return c;
}

// Byte-identical cameras share one plan. Per-image intrinsics can make every
// camera distinct, which is why the planning below runs in parallel.
std::string camera_key(const camhost::Camera& c) {
    std::string k;
    k.reserve(160);
    auto num = [&](double v) { k += std::to_string(v); k += ','; };
    num(c.model); num(c.tier); num(c.width); num(c.height);
    num(c.fx); num(c.fy); num(c.cx); num(c.cy);
    for (float d : c.dist) num(d);
    num(c.source_model);
    if (c.source_model >= 0) for (float p : c.source_params) num(p);
    return k;
}

}  // namespace

PostSplitCameras bake_post_split(const ParsedDataset& ds,
                                 bool warp_to_pinhole,
                                 bool warp_spherical_to_pinhole,
                                 WarpFaceFit fit, bool back_face) {
    const int64_t N = ds.num_cameras;
    const int PINHOLE_V   = (int)camera_model_from_name("PINHOLE");
    const int FISHEYE_V   = (int)camera_model_from_name("FISHEYE");
    const int EQUISOLID_V = (int)camera_model_from_name("EQUISOLID");
    const int EQUIRECT_V  = (int)camera_model_from_name("EQUIRECTANGULAR");

    // Which cameras split, and one plan per distinct camera among them.
    std::vector<int32_t> plan_of((size_t)N, -1);
    std::vector<camhost::Camera> plan_cams;
    std::map<std::string, int32_t> plan_index;
    for (int64_t i = 0; i < N; i++) {
        const int cm = ds.camera_models[(size_t)i];
        const bool split = cm == EQUIRECT_V
            ? warp_spherical_to_pinhole
            : warp_to_pinhole && (cm == FISHEYE_V || cm == EQUISOLID_V);
        if (!split) continue;
        camhost::Camera c = host_camera(ds, i);
        auto it = plan_index.find(camera_key(c));
        if (it == plan_index.end()) {
            it = plan_index.emplace(camera_key(c), (int32_t)plan_cams.size()).first;
            plan_cams.push_back(c);
        }
        plan_of[(size_t)i] = it->second;
    }
    const camhost::FaceFit face_fit = fit == WarpFaceFit::PerFace
        ? camhost::FaceFit::PerFace : camhost::FaceFit::Uniform;
    std::vector<std::vector<camhost::SplitFace>> plans(plan_cams.size());
    #pragma omp parallel for schedule(dynamic)
    for (int p = 0; p < (int)plan_cams.size(); p++)
        plans[(size_t)p] = camhost::plan_split_faces(plan_cams[(size_t)p], face_fit,
                                                    back_face);
    // A lens that fits one face is not wide: the engine renders it as itself,
    // since K == 1 is what tells the DataManager a camera is not warped.
    for (int64_t i = 0; i < N; i++)
        if (plan_of[(size_t)i] >= 0 && plans[(size_t)plan_of[(size_t)i]].size() < 2)
            plan_of[(size_t)i] = -1;
    // SS_SPLIT_LOG=1: the plan per distinct camera, a deep diagnostic in English.
    const char* split_log = spirula::env("SPLIT_LOG");
    if (split_log && *split_log)
        for (size_t p = 0; p < plan_cams.size(); p++) {
            const camhost::Camera& c = plan_cams[p];
            std::fprintf(stderr, "[split] camera %zu: %dx%d f=%.1f ->", p, c.width,
                         c.height, c.fx);
            for (const camhost::SplitFace& f : plans[p])
                std::fprintf(stderr, " [frame %d %dx%d c=(%.0f,%.0f) crop %dx%d]",
                             f.face, f.width, f.height, f.cx, f.cy, f.crop_w,
                             f.crop_h);
            std::fprintf(stderr, "\n");
        }

    PostSplitCameras out;
    out.K_per_camera.resize((size_t)N);
    out.post_offsets.resize((size_t)N);
    int64_t n_post = 0;
    for (int64_t i = 0; i < N; i++) {
        const int cm = ds.camera_models[(size_t)i];
        const int32_t p = plan_of[(size_t)i];
        const int K = p < 0 ? 1 : (int)plans[(size_t)p].size();
        out.K_per_camera[(size_t)i] = K;
        out.post_offsets[(size_t)i] = (int32_t)n_post;
        n_post += K;
        if (p >= 0) {
            out.any_warp = true;
            // A full panorama holds every ray; a fisheye, or a panorama
            // missing its poles, leaves part of a face unseen.
            if (cm != EQUIRECT_V ||
                2 * ds.heights[(size_t)i] + 1 < ds.widths[(size_t)i])
                out.any_fov_mask = true;
        } else if (cm == EQUIRECT_V) {
            out.direct_equirect = true;
        }
    }
    out.n_post = n_post;

    out.viewmats.assign((size_t)n_post * 16, 0.f);
    out.intrins.assign((size_t)n_post * 4, 0.f);
    out.dist_coeffs.assign((size_t)n_post * kCameraDistortionParams, 0.f);
    out.c2w_flip.assign((size_t)n_post * 12, 0.f);
    out.post_widths.assign((size_t)n_post, 0);
    out.post_heights.assign((size_t)n_post, 0);
    out.post_models.assign((size_t)n_post, PINHOLE_V);
    // Faces are canonical pinhole with zero coefficients; only a K == 1
    // camera keeps the tier the parser chose.
    out.post_distortions.assign((size_t)n_post, (int32_t)CameraDistortionType::None);
    out.face_axes.assign((size_t)n_post * 9, 0.f);
    out.post_faces.assign((size_t)n_post, -1);

    // POST-split c2w staging (double).
    std::vector<double> post_c2w((size_t)n_post * 12);
    static const double D[3] = {1.0, -1.0, -1.0};

    for (int64_t i = 0; i < N; i++) {
        const int K   = out.K_per_camera[(size_t)i];
        const int off = out.post_offsets[(size_t)i];
        double ci[3][4];
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 4; c++)
                ci[r][c] = ds.c2w[(size_t)i*12 + r*4 + c];

        if (plan_of[(size_t)i] < 0) {
            std::copy(&ci[0][0], &ci[0][0] + 12, &post_c2w[(size_t)off*12]);
            out.post_widths[(size_t)off]  = ds.widths[(size_t)i];
            out.post_heights[(size_t)off] = ds.heights[(size_t)i];
            out.post_models[(size_t)off]  = ds.camera_models[(size_t)i];
            for (int r = 0; r < 3; r++) out.face_axes[(size_t)off*9 + r*3 + r] = 1.f;
            if (ds.camera_models[(size_t)i] == EQUIRECT_V) {
                // Direct equirectangular: canonical panorama intrinsics
                // matching the equirect projection kernel.
                double f = ds.widths[(size_t)i] / (2.0 * kPi);
                out.intrins[(size_t)off*4 + 0] = (float)f;
                out.intrins[(size_t)off*4 + 1] = (float)f;
                out.intrins[(size_t)off*4 + 2] = ds.widths[(size_t)i]  / 2.0f;
                out.intrins[(size_t)off*4 + 3] = ds.heights[(size_t)i] / 2.0f;
            } else {
                std::copy(&ds.intrins[(size_t)i*4], &ds.intrins[(size_t)i*4] + 4,
                          &out.intrins[(size_t)off*4]);
                std::copy(&ds.dist_coeffs[(size_t)i*kCameraDistortionParams],
                          &ds.dist_coeffs[(size_t)i*kCameraDistortionParams] + kCameraDistortionParams,
                          &out.dist_coeffs[(size_t)off*kCameraDistortionParams]);
                out.post_distortions[(size_t)off] = ds.camera_distortions[(size_t)i];
            }
            continue;
        }

        // Face expansion:
        //   R' = R_c2w * diag(1,-1,-1)   (columns; OpenGL -> OpenCV)
        //   R_post[k] = (R' @ axes[face]^T) * diag(1,-1,-1)
        const std::vector<camhost::SplitFace>& faces = plans[(size_t)plan_of[(size_t)i]];
        const double* table = ds.camera_models[(size_t)i] == EQUIRECT_V
            ? camhost::equirect_face_axes() : camhost::fisheye_face_axes();
        double R[3][3];
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++)
                R[r][c] = ci[r][c] * D[c];
        for (int k = 0; k < K; k++) {
            const camhost::SplitFace& sf = faces[(size_t)k];
            const double* ax = table + 9 * sf.face;
            const size_t j = (size_t)(off + k);
            double* pc = &post_c2w[j * 12];
            for (int r = 0; r < 3; r++) {
                for (int c = 0; c < 3; c++) {
                    // (R @ axes^T)[r][c] = sum_m R[r][m] * axes[c][m]
                    double v = R[r][0]*ax[c*3 + 0] + R[r][1]*ax[c*3 + 1]
                             + R[r][2]*ax[c*3 + 2];
                    pc[r*4 + c] = v * D[c];
                }
                pc[r*4 + 3] = ci[r][3];
            }
            out.intrins[j*4 + 0] = (float)sf.fx;
            out.intrins[j*4 + 1] = (float)sf.fy;
            out.intrins[j*4 + 2] = (float)sf.cx;
            out.intrins[j*4 + 3] = (float)sf.cy;
            out.post_widths[j]  = sf.width;
            out.post_heights[j] = sf.height;
            out.post_faces[j]   = sf.face;
            for (int m = 0; m < 9; m++) out.face_axes[j*9 + m] = (float)ax[m];
        }
    }

    // c2w -> engine viewmat over the POST arrays:
    // R_v = R_post * diag(1,-1,-1) (columns); viewmat = [R_v^T | -R_v^T t].
    // [R_v | t] is also the y/z-flipped c2w form the viewer blit expects.
    for (int64_t j = 0; j < n_post; j++) {
        const double* pc = &post_c2w[(size_t)j*12];
        double Rv[3][3], t[3];
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) Rv[r][c] = pc[r*4 + c] * D[c];
            t[r] = pc[r*4 + 3];
        }
        float* vm = &out.viewmats[(size_t)j*16];
        float* cf = &out.c2w_flip[(size_t)j*12];
        for (int r = 0; r < 3; r++) {
            double ti = 0.0;
            for (int c = 0; c < 3; c++) {
                vm[r*4 + c] = (float)Rv[c][r];
                cf[r*4 + c] = (float)Rv[r][c];
                ti -= Rv[c][r] * t[c];
            }
            vm[r*4 + 3] = (float)ti;
            cf[r*4 + 3] = (float)t[r];
        }
        vm[15] = 1.f;
    }

    // A camera whose lens model had to be fitted needs its images resampled,
    // which runs on the warp path's staging even when K == 1.
    bool any_redistort = false;
    for (const RedistortSource& r : ds.redistort)
        any_redistort |= (r.source_model >= 0);
    if (any_redistort) {
        out.any_warp = true;
        out.redistort_models.assign((size_t)N, -1);
        out.redistort_params.assign((size_t)N * 16, 0.0f);
        for (int64_t i = 0; i < N && i < (int64_t)ds.redistort.size(); i++) {
            out.redistort_models[(size_t)i] = ds.redistort[(size_t)i].source_model;
            for (int k = 0; k < 16; k++)
                out.redistort_params[(size_t)i*16 + k] = ds.redistort[(size_t)i].params[k];
        }
    }

    if (out.any_warp) {
        out.input_intrins     = ds.intrins;
        out.input_dist_coeffs = ds.dist_coeffs;
        out.input_distortions = ds.camera_distortions;
    }
    return out;
}
