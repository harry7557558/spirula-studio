// `spirula geometry` -- depth and surface normals for a dataset. One
// monocular network per image; all the work around it is cameras, and that
// part is app/GeometryWarp.h.
//
// The output goes to `normals/` and `depths/` beside the images, which both
// dataset readers already probe by name. transforms.json is NOT rewritten: a
// tool that edits a user's dataset description can corrupt it.

#include "app/Tools.h"

#include "app/DepthPng.h"
#include "app/GeometryModel.h"
#include "app/GeometryWarp.h"
#include "app/WriterPool.h"
#include "data/CameraMath.h"
#include "data/DatasetParser.h"
#include "data/ImageProbe.h"
#include "i18n/Locale.h"
#include "i18n/catalog/Geometry.h"
#include "nn/core/Error.h"
#include "nn/core/Log.h"
#include "nn/io/Image.h"
#include "nn/vk/Context.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace G = spirula::i18n::msg::geometry;

using spirula::i18n::format;

namespace {

// ---------------------------------------------------------------------------
// Options
// ---------------------------------------------------------------------------

enum class Tri { Auto, Yes, No };

struct Options {
    std::string dataset;
    // Relative to the dataset, or absolute: a capture reconstructed from
    // photos read where they are keeps its images outside the dataset folder.
    std::string image_dir = "images";
    std::string model = "moge2-vitb";
    // The written maps' longest side, and Metric3D's inference size on top of
    // that (metric3d/README.md). A ceiling on one face, not on the frame.
    int  max_size = 1064;
    // MoGe's ViT budget, which is what sets ITS cost -- the top of the range
    // its own inference offers (moge/README.md). Metric3D ignores it.
    int  num_tokens = 3600;
    bool want_depth = false;
    bool want_normal = true;
    std::string normal_format = "png";
    int  jpeg_quality = 95;
    bool depth_mm = false;
    Tri  ray_depth = Tri::Auto;
    Tri  split = Tri::Auto;
    bool overwrite = false;
    // The dataset's colour space; frames convert to sRGB before inference.
    std::string image_gamut;
    std::optional<bool> image_is_linear;   // unset: an EXR's own header decides
};

void help_row(const char* flags, const spirula::i18n::Msg& m, int col = 26) {
    std::string left = std::string("    ") + flags;
    if (spirula::i18n::display_width(left) >= col + 4) {
        std::fprintf(stderr, "%s\n", left.c_str());
        left.clear();
    }
    left = spirula::i18n::pad_to(left, col + 4);
    for (const std::string& line : spirula::i18n::wrap(m.get(), 86 - col - 4)) {
        std::fprintf(stderr, "%s%s\n", left.c_str(), line.c_str());
        left.assign((size_t)col + 4, ' ');
    }
}

void usage() {
    const std::string prog = app::program_name();
    std::fprintf(stderr, "%s -- %s\n\n", prog.c_str(), G::tagline.get());
    std::fprintf(stderr, "    %s <dataset> [options]\n\n", prog.c_str());
    for (const std::string& l : spirula::i18n::wrap(G::usage_target.get(), 80))
        std::fprintf(stderr, "    %s\n", l.c_str());
    std::fprintf(stderr, "\n%s\n", G::head_options.get());
    help_row("--image-dir <dir>", G::opt_image_dir);
    help_row("--model <id|file>", G::opt_model);
    help_row("--max-size <n>", G::opt_max_size);
    help_row("--num-tokens <n>", G::opt_num_tokens);
    help_row("--depth", G::opt_depth);
    help_row("--normal-format png|jpg", G::opt_normal_format);
    help_row("--jpeg-quality <n>", G::opt_jpeg_quality);
    help_row("--depth-units relative|mm", G::opt_depth_units);
    help_row("--ray-depth auto|yes|no", G::opt_ray_depth);
    help_row("--split auto|yes|no", G::opt_split);
    help_row("--overwrite", G::opt_overwrite);
    help_row("--image-gamut <name>", G::opt_image_gamut);
    help_row("--image-linear / --no-image-linear", G::opt_image_linear);
    // English, like the other deep diagnostics in this repository: what it
    // prints is a table of numerical errors, read by whoever changed the warp.
    std::fprintf(stderr, "    --check                   "
                         "run the camera round-trip self-test and exit\n");
    std::fprintf(stderr, "\n%s --device <index|name>  --lang <code>\n",
                 G::label_common.get());
    std::fprintf(stderr, "%s SS_NN_LOG=0..3  SS_VK_DEVICE  SS_PROFILE=1\n",
                 G::label_environment.get());
    std::fprintf(stderr, "\n    %s\n", app::geometry_model_ids().c_str());
}

bool tri(const char* v, Tri& out) {
    if (!std::strcmp(v, "auto")) out = Tri::Auto;
    else if (!std::strcmp(v, "yes") || !std::strcmp(v, "1")) out = Tri::Yes;
    else if (!std::strcmp(v, "no") || !std::strcmp(v, "0")) out = Tri::No;
    else return false;
    return true;
}

// ---------------------------------------------------------------------------
// Host image handling
// ---------------------------------------------------------------------------

// The scale a depth map is stored against. Not the maximum: a handful of
// horizon pixels a hundred times further than anything else would leave a
// whole room inside 1% of the 16-bit range.
float depth_scale(const std::vector<float>& depth) {
    std::vector<float> v;
    v.reserve(depth.size());
    for (float d : depth)
        if (d > 0.0f) v.push_back(d);
    if (v.empty()) return 1.0f;
    const size_t at = (size_t)((double)(v.size() - 1) * 0.999);
    std::nth_element(v.begin(), v.begin() + (long)at, v.end());
    return std::fmax(v[at], 1e-6f);
}

std::string human_time(double ms) {
    char buf[64];
    if (ms < 60000) std::snprintf(buf, sizeof buf, "%.0fs", ms / 1000.0);
    else if (ms < 3600000) std::snprintf(buf, sizeof buf, "%.0fm", ms / 60000.0);
    else std::snprintf(buf, sizeof buf, "%.1fh", ms / 3600000.0);
    return buf;
}

// ---------------------------------------------------------------------------
// --check: the camera round trip, with no network in it
//
// English, like the other deep diagnostics in this repository. It is the only
// thing that tests the face rotation and the depth conversion: put an analytic
// PLANE in front of the camera, hand each face the depth and normal that plane
// would produce in ITS frame, and check what comes back in the source camera's
// frame is the plane again. A wrong rotation, a wrong z-vs-ray scale or a
// transposed map all fail here, and none of them is visible in a normal map
// that merely looks plausible.
// ---------------------------------------------------------------------------

int self_check() {
    struct Case {
        const char* name;
        int model, tier;
        int w, h;
        float fx, fy, cx, cy;
        float dist[kCameraDistortionParams];
        bool split;
    };
    const Case cases[] = {
        {"pinhole + opencv", (int)CameraModelType::PINHOLE,
         (int)CameraDistortionType::OpenCV, 1600, 1200, 1500, 1500, 800, 600,
         {-0.03f, 0.01f, 0.001f, -0.002f}, false},
        {"fisheye + prism, one face", (int)CameraModelType::FISHEYE,
         (int)CameraDistortionType::ThinPrism, 1000, 1500, 530, 530, 500, 750,
         {0.009f, 0.002f, -0.0006f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}, false},
        {"fisheye + prism, split", (int)CameraModelType::FISHEYE,
         (int)CameraDistortionType::ThinPrism, 1000, 1500, 530, 530, 500, 750,
         {0.009f, 0.002f, -0.0006f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}, true},
        {"equisolid, split", (int)CameraModelType::EQUISOLID,
         (int)CameraDistortionType::None, 1400, 1400, 420, 420, 700, 700, {}, true},
        {"equirectangular, cube map", (int)CameraModelType::EQUIRECTANGULAR,
         (int)CameraDistortionType::None, 1024, 512,
         1024.0f / 6.28318530718f, 1024.0f / 6.28318530718f, 512, 256, {}, true},
    };
    // Three planes, none of them axis-aligned, so a swapped axis cannot pass.
    const double planes[3][3] = {{0.0, 0.0, 1.0}, {0.35, -0.25, 0.90},
                                 {-0.5, 0.4, 0.77}};
    const double dist_to_plane = 4.0;

    int failures = 0;
    for (const Case& c : cases) {
        app::GeometryCamera cam;
        cam.model = c.model;
        cam.distortion = c.tier;
        cam.width = c.w;
        cam.height = c.h;
        cam.fx = c.fx; cam.fy = c.fy; cam.cx = c.cx; cam.cy = c.cy;
        std::copy(std::begin(c.dist), std::end(c.dist), std::begin(cam.dist));

        app::GeometryWarp warp;
        // The coarsest granularity either model asks for. The check is about
        // cameras; this only decides the sizes it runs them at.
        const int patch = 28;
        // Uncapped faces on purpose: what is left after a correct rotation is
        // bilinear interpolation of a curve, and that error falls with the
        // square of the face's pixels.
        warp.plan(cam, c.w / patch * patch, c.h / patch * patch, c.split, patch, 0);

        const double* axes = warp.faceAxes();
        // The plane as each face would see it: `fd` its own z-depth, `fn` its
        // own frame's normal.
        auto face_gt = [&](const double nn3[3], std::vector<std::vector<float>>& fd,
                           std::vector<std::vector<float>>& fn) {
            fd.assign((size_t)warp.faces(), {});
            fn.assign((size_t)warp.faces(), {});
            for (int k = 0; k < warp.faces(); ++k) {
                const int fw = warp.faceWidth(k), fh = warp.faceHeight(k);
                fd[k].assign((size_t)fw * fh, 0.0f);
                fn[k].assign(fd[k].size() * 3, 0.0f);
                for (int y = 0; y < fh; ++y)
                    for (int x = 0; x < fw; ++x) {
                        const double tx = -1.0 + 2.0 * (x + 0.5) / fw;
                        const double ty = -1.0 + 2.0 * (y + 0.5) / fh;
                        double r[3], ax[3], ay[3], az[3];
                        if (axes) {
                            const double* a = axes + (size_t)k * 9;
                            for (int m = 0; m < 3; ++m) {
                                ax[m] = a[m]; ay[m] = a[3+m]; az[m] = a[6+m];
                                r[m] = az[m] + tx * ax[m] + ty * ay[m];
                            }
                            // The extent lives in the axis lengths; the frame
                            // a normal is written in does not.
                            const double lx = std::sqrt(ax[0]*ax[0]+ax[1]*ax[1]+ax[2]*ax[2]);
                            const double ly = std::sqrt(ay[0]*ay[0]+ay[1]*ay[1]+ay[2]*ay[2]);
                            for (int m = 0; m < 3; ++m) { ax[m] /= lx; ay[m] /= ly; }
                        } else {
                            // One face: its frame is the camera's, and tx, ty
                            // are not what indexes it -- the face pixel is the
                            // camera's own normalized coordinate.
                            const double fx = cam.fx * (double)warp.outWidth() / c.w;
                            const double fy = cam.fy * (double)warp.outHeight() / c.h;
                            const double cx = cam.cx * (double)warp.outWidth() / c.w;
                            const double cy = cam.cy * (double)warp.outHeight() / c.h;
                            r[0] = (x + 0.5 - cx) / fx;
                            r[1] = (y + 0.5 - cy) / fy;
                            r[2] = 1.0;
                            ax[0]=1; ax[1]=0; ax[2]=0;
                            ay[0]=0; ay[1]=1; ay[2]=0;
                            az[0]=0; az[1]=0; az[2]=1;
                        }
                        const double dot = nn3[0]*r[0] + nn3[1]*r[1] + nn3[2]*r[2];
                        const size_t i = (size_t)y * fw + x;
                        // The plane at face z-depth t, where the face's own z
                        // is r . az and r was built with r . az == 1; a ray
                        // that grazes or misses it has no depth, as a mask says.
                        fd[k][i] = dot > 0.05 ? (float)(dist_to_plane / dot) : 0.0f;
                        fn[k][i * 3 + 0] =
                            (float)(nn3[0]*ax[0] + nn3[1]*ax[1] + nn3[2]*ax[2]);
                        fn[k][i * 3 + 1] =
                            (float)(nn3[0]*ay[0] + nn3[1]*ay[1] + nn3[2]*ay[2]);
                        fn[k][i * 3 + 2] =
                            (float)(nn3[0]*az[0] + nn3[1]*az[1] + nn3[2]*az[2]);
                    }
            }
        };

        double worst_n = 0, worst_z = 0, worst_r = 0, worst_align = 0;
        for (const double(&n)[3] : planes) {
            const double len = std::sqrt(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
            const double nn3[3] = {n[0]/len, n[1]/len, n[2]/len};

            std::vector<std::vector<float>> fd, fn;
            face_gt(nn3, fd, fn);

            std::vector<float> dz, dr, nrm;
            warp.gather(fd, fn, false, &dz, &nrm);
            warp.gather(fd, fn, true, &dr, nullptr);

            const double sx = (double)warp.outWidth() / c.w;
            const double sy = (double)warp.outHeight() / c.h;
            const double fx = cam.fx * sx, fy = cam.fy * sy;
            const double cx = cam.cx * sx, cy = cam.cy * sy;
            for (int y = 0; y < warp.outHeight(); ++y)
                for (int x = 0; x < warp.outWidth(); ++x) {
                    const size_t i = (size_t)y * warp.outWidth() + x;
                    if (dz[i] == 0.0f) continue;
                    double r[3];
                    if (!camhost::generate_ray((x + 0.5 - cx) / fx, (y + 0.5 - cy) / fy,
                                               cam.model, cam.distortion, cam.dist, r))
                        continue;
                    const double dot = nn3[0]*r[0] + nn3[1]*r[1] + nn3[2]*r[2];
                    if (dot < 0.25) continue;   // grazing: the plane is edge-on
                    const double want_ray = dist_to_plane / dot;
                    const double want_z = want_ray * r[2];
                    worst_r = std::fmax(worst_r, std::fabs(dr[i] - want_ray) / want_ray);
                    worst_z = std::fmax(worst_z, std::fabs(dz[i] - want_z) /
                                                     std::fmax(std::fabs(want_z), 1e-6));
                    const double c_ang = nrm[i*3+0]*nn3[0] + nrm[i*3+1]*nn3[1] +
                                         nrm[i*3+2]*nn3[2];
                    worst_n = std::fmax(worst_n,
                                        std::acos(std::fmin(1.0, std::fmax(-1.0, c_ang))));
                }

            // Hand the faces that same plane at deliberately different
            // scales and the blend should put them back on one. The gauge is
            // free, so this measures the ratio's SPREAD, not the ratio.
            if (!warp.split()) continue;
            for (int k = 0; k < warp.faces(); ++k) {
                const float s = (float)std::exp(0.5 * ((k % 4) - 1.5));
                for (float& v : fd[(size_t)k]) v *= s;
            }
            std::vector<float> misfit;
            warp.gather(fd, {}, true, &dr, nullptr);
            for (int y = 0; y < warp.outHeight(); ++y)
                for (int x = 0; x < warp.outWidth(); ++x) {
                    const size_t i = (size_t)y * warp.outWidth() + x;
                    if (dr[i] == 0.0f) continue;
                    double r[3];
                    if (!camhost::generate_ray((x + 0.5 - cx) / fx, (y + 0.5 - cy) / fy,
                                               cam.model, cam.distortion, cam.dist, r))
                        continue;
                    const double dot = nn3[0]*r[0] + nn3[1]*r[1] + nn3[2]*r[2];
                    if (dot < 0.25) continue;
                    misfit.push_back((float)std::log(dr[i] * dot / dist_to_plane));
                }
            if (misfit.size() > 100) {
                std::sort(misfit.begin(), misfit.end());
                worst_align = std::fmax(
                    worst_align,
                    misfit[misfit.size() * 99 / 100] - misfit[misfit.size() / 100]);
            }
        }
        // Bilinear resampling of a smooth but curved field is what is left; a
        // rotation or a scale error is orders of magnitude above these.
        const bool ok = worst_n < 0.02 && worst_z < 0.02 && worst_r < 0.02 &&
                        worst_align < 0.05;
        if (!ok) ++failures;
        std::printf("  %s %-30s faces %d  normal %.4f deg  z %.2e  ray %.2e"
                    "  realign %.3f\n",
                    ok ? "ok  " : "FAIL", c.name, warp.faces(),
                    worst_n * 57.2957795, worst_z, worst_r, worst_align);
    }

    // The closed form that decided how many faces there were, against
    // counting the pixels it integrates -- of the ideal lens, which is what it
    // claims to measure (data/CameraMath.h).
    std::printf("\n  --split auto / --input-depth-is-ray-depth threshold at 0.75\n");
    for (const Case& c : cases) {
        const float zero[kCameraDistortionParams] = {};
        const int tier = (int)CameraDistortionType::None;
        const double got = camhost::pinhole_coverage(c.model, c.w, c.h, c.fx, c.fy);
        const char* verdict = camhost::splits_to_pinhole_faces(
            c.model, c.w, c.h, c.fx, c.fy) ? "split" : "undistort";
        // Only the two angular models integrate anything; a perspective lens
        // is 1 by definition and a panorama is 0 by fiat.
        if (c.model != (int)CameraModelType::FISHEYE &&
            c.model != (int)CameraModelType::EQUISOLID) {
            std::printf("  ok   %-30s coverage %.4f  (by definition)   %s\n", c.name,
                        got, verdict);
            continue;
        }
        int64_t inside = 0, total = 0;
        for (int y = 0; y < 512; ++y)
            for (int x = 0; x < 512; ++x) {
                const double u = ((x + 0.5) / 512 * c.w - c.w * 0.5) / c.fx;
                const double v = ((y + 0.5) / 512 * c.h - c.h * 0.5) / c.fy;
                double r[3];
                ++total;
                if (!camhost::generate_ray(u, v, c.model, tier, zero, r)) continue;
                if (r[2] <= 0) continue;
                if (std::fabs(r[0] / r[2]) * c.fx <= c.w * 0.5 &&
                    std::fabs(r[1] / r[2]) * c.fy <= c.h * 0.5)
                    ++inside;
            }
        const double want = (double)inside / (double)total;
        const bool ok = std::fabs(got - want) < 0.01;
        if (!ok) ++failures;
        std::printf("  %s %-30s coverage %.4f  (counted %.4f)  %s\n",
                    ok ? "ok  " : "FAIL", c.name, got, want, verdict);
    }

    // The trainer's split: every ray the lens holds must land in a face, and
    // the faces should hold fewer pixels than the uncropped split they replace.
    std::printf("\n  trainer split (camhost::plan_split_faces)\n");
    for (const Case& c : cases) {
        if (c.model == (int)CameraModelType::PINHOLE) continue;
        camhost::Camera hc;
        hc.model = c.model; hc.tier = c.tier;
        hc.width = c.w; hc.height = c.h;
        hc.fx = c.fx; hc.fy = c.fy; hc.cx = c.cx; hc.cy = c.cy;
        std::copy(std::begin(c.dist), std::end(c.dist), std::begin(hc.dist));
        // With the back frame, so a lens seen past 135 degrees is checked for
        // the coverage the whole cube owes it, not the trainer's default five.
        const std::vector<camhost::SplitFace> faces =
            camhost::plan_split_faces(hc, camhost::FaceFit::Uniform, true);
        const bool equi = c.model == (int)CameraModelType::EQUIRECTANGULAR;
        const double* table = equi ? camhost::equirect_face_axes()
                                   : camhost::fisheye_face_axes();
        const int K = equi ? 6 : 5;
        const int S = 2 * (((int)std::ceil(std::sqrt((double)c.w * c.h / K)) + 1) / 2);
        int64_t seen = 0, missed = 0;
        for (int y = 0; y < 256; ++y)
            for (int x = 0; x < 256; ++x) {
                const double px = (x + 0.5) / 256 * c.w, py = (y + 0.5) / 256 * c.h;
                double r[3], back[2];
                const double fx = equi ? c.w / 6.28318530718 : hc.fx;
                const double fy = equi ? fx : hc.fy;
                const double cx = equi ? c.w * 0.5 : hc.cx;
                const double cy = equi ? c.h * 0.5 : hc.cy;
                if (!camhost::generate_ray((px - cx) / fx, (py - cy) / fy,
                                           c.model, c.tier, c.dist, r))
                    continue;
                if (!camhost::ray_in_frame(hc, r, back)) continue;
                ++seen;
                bool hit = false;
                for (const camhost::SplitFace& f : faces) {
                    const double* a = table + 9 * f.face;
                    const double z = a[6]*r[0] + a[7]*r[1] + a[8]*r[2];
                    if (z <= 1e-12) continue;
                    const double u = (a[0]*r[0] + a[1]*r[1] + a[2]*r[2]) / z;
                    const double v = (a[3]*r[0] + a[4]*r[1] + a[5]*r[2]) / z;
                    const double fpx = u * f.fx + f.cx, fpy = v * f.fy + f.cy;
                    if (fpx >= 0 && fpx <= f.width && fpy >= 0 && fpy <= f.height) {
                        hit = true;
                        break;
                    }
                }
                if (!hit) ++missed;
            }
        double px = 0;
        for (const camhost::SplitFace& f : faces) px += (double)f.width * f.height;
        const double uncropped = (double)K * S * S;
        // Rays at the very rim round either way; anything more is a hole. A
        // lens seen past 135 degrees earns a back face, never more than a cube.
        const bool ok = seen > 0 && missed <= seen / 500 + 1 && px <= 6.0 * S * S + 1;
        if (!ok) ++failures;
        std::printf("  %s %-30s faces %d of %dx%d  pixels %.0f%% of uncropped"
                    "  uncovered %lld / %lld\n",
                    ok ? "ok  " : "FAIL", c.name, (int)faces.size(),
                    faces.empty() ? 0 : faces[0].width,
                    faces.empty() ? 0 : faces[0].height,
                    100.0 * px / uncropped, (long long)missed, (long long)seen);
    }

    std::printf("\n%s\n", failures ? "FAILURES" : "camera round trip is exact");
    return failures ? 1 : 0;
}

// Every image whose camera is byte-identical shares one warp plan; a dataset
// usually has one camera and always has few.
std::string camera_key(const ParsedDataset& ds, int64_t i) {
    std::string k;
    k.reserve(96);
    auto num = [&](double v) { k += std::to_string(v) + ","; };
    num(ds.camera_models[(size_t)i]);
    num(ds.camera_distortions[(size_t)i]);
    num(ds.widths[(size_t)i]);
    num(ds.heights[(size_t)i]);
    for (int j = 0; j < 4; ++j) num(ds.intrins[(size_t)i * 4 + j]);
    for (int j = 0; j < kCameraDistortionParams; ++j)
        num(ds.dist_coeffs[(size_t)i * kCameraDistortionParams + j]);
    if (!ds.redistort.empty()) {
        num(ds.redistort[(size_t)i].source_model);
        for (int j = 0; j < 16; ++j) num(ds.redistort[(size_t)i].params[j]);
    }
    return k;
}

}  // namespace

int spirula_geometry_main(int argc, char** argv) {
    app::set_program_name(argc > 0 ? argv[0] : nullptr, "spirula geometry");
    Options o;
    std::string device;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s needs a value\n", a.c_str());
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--help" || a == "-h") { usage(); return 0; }
        else if (a == "--check") return self_check();
        else if (a == "--image-dir") o.image_dir = next();
        else if (a == "--model") o.model = next();
        else if (a == "--max-size") o.max_size = std::atoi(next());
        else if (a == "--num-tokens") o.num_tokens = std::atoi(next());
        else if (a == "--depth") o.want_depth = true;
        else if (a == "--no-normal") o.want_normal = false;
        else if (a == "--normal-format") o.normal_format = next();
        else if (a == "--jpeg-quality") o.jpeg_quality = std::atoi(next());
        else if (a == "--depth-units") o.depth_mm = std::string(next()) == "mm";
        else if (a == "--ray-depth") { if (!tri(next(), o.ray_depth)) { usage(); return 2; } }
        else if (a == "--split") { if (!tri(next(), o.split)) { usage(); return 2; } }
        else if (a == "--overwrite") o.overwrite = true;
        else if (a == "--image-gamut") o.image_gamut = next();
        else if (a == "--image-linear") o.image_is_linear = true;
        else if (a == "--no-image-linear") o.image_is_linear = false;
        else if (a == "--device") device = next();
        else if (!a.empty() && a[0] == '-') {
            std::fprintf(stderr, "unknown option '%s'\n\n", a.c_str());
            usage();
            return 2;
        } else if (o.dataset.empty()) o.dataset = a;
        else { usage(); return 2; }
    }
    if (o.dataset.empty()) {
        std::fprintf(stderr, "%s\n",
                     format(G::err_no_dataset, {app::program_name()}).c_str());
        return 2;
    }
    if (!o.want_depth && !o.want_normal) return 0;
    if (!device.empty()) {
        // Creating the context here fixes the device for the process; the
        // model's own Context::get() then returns this one.
        nn::vk::ContextOptions vo;
        char* end = nullptr;
        const long idx = std::strtol(device.c_str(), &end, 10);
        if (end && *end == '\0') vo.device_index = (int)idx;
        else vo.device_match = device;
        nn::vk::Context::get(vo);
    }

    try {
        // ---- the dataset --------------------------------------------------
        DatasetParserConfig cfg;
        cfg.require_image_files = false;   // the point cloud is not our business
        cfg.image_dir = o.image_dir;
        cfg.probe_image_size = probe_image_size;
        const ParsedDataset ds = parse_dataset(o.dataset, cfg, "");
        const int64_t N = ds.num_cameras;
        NN_CHECK(N > 0, "'%s' holds no images", o.dataset.c_str());

        std::vector<std::string> keys((size_t)N);
        std::vector<int> group((size_t)N, 0);
        std::vector<int64_t> group_first;
        {
            std::vector<std::string> seen;
            for (int64_t i = 0; i < N; ++i) {
                keys[(size_t)i] = camera_key(ds, i);
                auto it = std::find(seen.begin(), seen.end(), keys[(size_t)i]);
                if (it == seen.end()) {
                    group[(size_t)i] = (int)seen.size();
                    seen.push_back(keys[(size_t)i]);
                    group_first.push_back(i);
                } else {
                    group[(size_t)i] = (int)(it - seen.begin());
                }
            }
        }
        std::printf("%s\n", format(G::log_dataset,
                                   {fs::path(o.dataset).filename().string(),
                                    (long long)N, (long long)group_first.size()})
                                .c_str());

        // ---- the model ------------------------------------------------------
        // Before the warps: which input sizes round-trip is the network's, and
        // Metric3D's decoder crops where MoGe resamples its own output.
        app::GeometryModel pred;
        pred.load(o.model);

        // ---- one warp plan per camera --------------------------------------
        const int patch = pred.sizeGranularity();
        std::vector<app::GeometryWarp> warps(group_first.size());
        for (size_t g = 0; g < group_first.size(); ++g) {
            const int64_t i = group_first[g];
            app::GeometryCamera cam;
            cam.model = ds.camera_models[(size_t)i];
            cam.distortion = ds.camera_distortions[(size_t)i];
            cam.width = ds.widths[(size_t)i];
            cam.height = ds.heights[(size_t)i];
            cam.fx = ds.intrins[(size_t)i * 4 + 0];
            cam.fy = ds.intrins[(size_t)i * 4 + 1];
            cam.cx = ds.intrins[(size_t)i * 4 + 2];
            cam.cy = ds.intrins[(size_t)i * 4 + 3];
            for (int j = 0; j < kCameraDistortionParams; ++j)
                cam.dist[j] = ds.dist_coeffs[(size_t)i * kCameraDistortionParams + j];
            if (!ds.redistort.empty() && ds.redistort[(size_t)i].source_model >= 0) {
                cam.source_model = ds.redistort[(size_t)i].source_model;
                std::copy(std::begin(ds.redistort[(size_t)i].params),
                          std::end(ds.redistort[(size_t)i].params),
                          std::begin(cam.source_params));
                NN_LOG_WARN("%s\n",
                            format(G::warn_fitted_camera, {(long long)g}).c_str());
            }

            // The output resolution: the source frame under the longest-side
            // cap, on the patch grid so the undistort path is not cropped.
            double s = 1.0;
            if (o.max_size > 0 && std::max(cam.width, cam.height) > o.max_size)
                s = (double)o.max_size / std::max(cam.width, cam.height);
            const int ow = std::max(patch, (int)(cam.width * s) / patch * patch);
            const int oh = std::max(patch, (int)(cam.height * s) / patch * patch);

            const bool split =
                o.split == Tri::Auto
                    ? camhost::splits_to_pinhole_faces(cam.model, cam.width,
                                                       cam.height, cam.fx, cam.fy)
                    : o.split == Tri::Yes;
            warps[g].plan(cam, ow, oh, split, patch, o.max_size);

            const char* model_name = camera_model_to_string(
                (CameraModelType)cam.model);
            // The faces differ in size; the largest is the one the cost is.
            int fw = 0, fh = 0;
            for (int k = 0; k < warps[g].faces(); ++k)
                if ((int64_t)warps[g].faceWidth(k) * warps[g].faceHeight(k) >
                    (int64_t)fw * fh) {
                    fw = warps[g].faceWidth(k);
                    fh = warps[g].faceHeight(k);
                }
            if (warps[g].split())
                std::printf("  %s\n", format(G::log_camera_split,
                                             {(long long)g, model_name,
                                              (long long)warps[g].faces(),
                                              (long long)fw, (long long)fh})
                                          .c_str());
            else
                std::printf("  %s\n", format(G::log_camera_single,
                                             {(long long)g, model_name,
                                              (long long)fw, (long long)fh})
                                          .c_str());
        }

        // ---- where the output goes -----------------------------------------
        const fs::path root(o.dataset);
        const fs::path image_root = root / cfg.image_dir;
        const fs::path normal_dir = root / cfg.normal_dir;
        const fs::path depth_dir = root / cfg.depth_dir;
        std::error_code ec;
        if (o.want_normal) fs::create_directories(normal_dir, ec);
        if (o.want_depth) fs::create_directories(depth_dir, ec);

        // The output mirrors whatever shape `images/` has, subdirectories and
        // all -- a multi-camera capture is `images/cam0/...`, and a writer
        // that only made the top folder failed every single frame of one.
        auto out_path = [&](const fs::path& dir, int64_t i, const char* ext) {
            fs::path rel = fs::relative(fs::path(ds.image_filenames[(size_t)i]),
                                        image_root, ec);
            if (ec || rel.empty() || rel.native()[0] == '.')
                rel = fs::path(ds.image_filenames[(size_t)i]).filename();
            rel.replace_extension(ext);
            const fs::path out = dir / rel;
            if (out.has_parent_path()) fs::create_directories(out.parent_path(), ec);
            return out;
        };

        app::WriterPool writers;
        const double t_start = nn::now_ms();
        int64_t written = 0, skipped = 0, unreadable = 0;
        double model_ms = 0;

        for (int64_t i = 0; i < N; ++i) {
            const app::GeometryWarp& warp = warps[(size_t)group[(size_t)i]];
            // out_path creates the folder it names, so a map that was not asked
            // for must not have its path built: it would leave an empty depths/
            // in the dataset for a normals-only run.
            fs::path np, dp;
            if (o.want_normal)
                np = out_path(normal_dir, i, o.normal_format == "jpg" ? ".jpg" : ".png");
            if (o.want_depth) dp = out_path(depth_dir, i, ".png");
            const bool need_normal = o.want_normal && (o.overwrite || !fs::exists(np, ec));
            const bool need_depth = o.want_depth && (o.overwrite || !fs::exists(dp, ec));
            if (!need_normal && !need_depth) { ++skipped; continue; }

            const nn::Image img = nn::load_image(ds.image_filenames[(size_t)i],
                                                 o.image_gamut, o.image_is_linear);
            if (img.empty()) {
                NN_LOG_WARN("skipping %s\n", ds.image_filenames[(size_t)i].c_str());
                ++unreadable;
                continue;
            }
            const std::vector<float> src =
                app::resize_area(img.data.data(), img.width, img.height,
                                 img.channels, warp.sampleWidth(),
                                 warp.sampleHeight());

            const double t0 = nn::now_ms();
            std::vector<std::vector<float>> face_depth, face_normal;
            std::vector<float> face_rgb;
            for (int k = 0; k < warp.faces(); ++k) {
                warp.sampleFace(k, src.data(), face_rgb);
                app::GeometryRequest rq = app::face_request(warp, k, o.num_tokens);
                rq.want_depth = need_depth;
                rq.want_normal = need_normal;
                app::GeometryPrediction p = pred.predict(face_rgb.data(), rq);
                NN_CHECK(p.width == warp.faceWidth(k) && p.height == warp.faceHeight(k),
                         "the network returned %dx%d for a %dx%d face", p.width,
                         p.height, warp.faceWidth(k), warp.faceHeight(k));
                // Millimetres on every face before they are blended: Metric3D's
                // depth is canonical to the face's own focal.
                const float mm = (float)pred.depthToMillimetres(warp.faceFocal(k));
                for (float& d : p.depth) d *= mm;
                face_depth.push_back(std::move(p.depth));
                face_normal.push_back(std::move(p.normal));
            }
            model_ms += nn::now_ms() - t0;

            const bool ray = o.ray_depth == Tri::Auto ? warp.defaultRayDepth()
                                                      : o.ray_depth == Tri::Yes;
            std::vector<float> depth, normal;
            warp.gather(face_depth, face_normal, ray, need_depth ? &depth : nullptr,
                        need_normal ? &normal : nullptr);

            if (need_normal) {
                app::WriteJob job;
                job.path = np.string();
                // A quality INSIDE 0..100 is what selects JPEG; the extension
                // is not consulted (nn/io/Image.h). 100 wrote a JPEG called
                // .png, which is lossy right where the mask edge is.
                job.quality = o.normal_format == "jpg" ? o.jpeg_quality : -1;
                job.image.width = warp.outWidth();
                job.image.height = warp.outHeight();
                job.image.channels = 3;
                job.image.data.resize(normal.size());
                // BLACK is the trainer's "no normal here": it decodes
                // byte/127.5 - 1 and masks on a sum <= -2.366 or a length
                // under 0.25 (core/Interpolation.cuh's gt_normal_valid).
                for (size_t p = 0; p * 3 < normal.size(); ++p) {
                    const float* v = &normal[p * 3];
                    uint8_t* out = &job.image.data[p * 3];
                    if (v[0]*v[0] + v[1]*v[1] + v[2]*v[2] < 0.25f) {
                        out[0] = out[1] = out[2] = 0;
                        continue;
                    }
                    for (int c = 0; c < 3; ++c)
                        out[c] = (uint8_t)std::lround(
                            std::fmin(255.0f, std::fmax(0.0f, 127.5f + 127.5f * v[c])));
                }
                writers.submit(std::move(job));
            }
            if (need_depth) {
                // 0 is the trainer's "no ground truth here", so a covered pixel
                // never rounds into it.
                const float inv = o.depth_mm ? 1.0f : 1.0f / depth_scale(depth);
                app::WriteJob job;
                job.path = dp.string();
                job.depth_w = warp.outWidth();
                job.depth_h = warp.outHeight();
                job.depth.resize(depth.size());
                for (size_t j = 0; j < depth.size(); ++j) {
                    if (depth[j] <= 0.0f) { job.depth[j] = 0; continue; }
                    const float v = depth[j] * inv * (o.depth_mm ? 1.0f : 65535.0f);
                    job.depth[j] = (uint16_t)std::lround(
                        std::fmin(65535.0f, std::fmax(1.0f, v)));
                }
                writers.submit(std::move(job));
            }

            // Every image, not every tenth: one costs about a second, and
            // the GUI drives its bar and its preview reel off these lines.
            ++written;
            {
                const double each = model_ms / (double)written;
                std::printf("\r%s   ",
                            format(G::log_progress,
                                   {(long long)(written + skipped), (long long)N,
                                    (long long)std::lround(each),
                                    human_time(each * (double)(N - i - 1))})
                                .c_str());
                std::fflush(stdout);
            }
        }
        writers.finish();
        std::printf("\r%s\n",
                    format(G::log_done, {(long long)written, (long long)skipped,
                                         human_time(nn::now_ms() - t_start)})
                        .c_str());
        // Nothing readable means the wrong image_dir, not an empty dataset:
        // exiting 0 reports a reconstruction that wrote an empty normals/ as
        // a success.
        if (written == 0 && unreadable > 0) {
            std::fprintf(stderr, "%s\n",
                         format(G::err_no_images_read, {image_root.string()})
                             .c_str());
            return 1;
        }
        return writers.failures() ? 1 : 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "\nerror: %s\n", e.what());
        return 1;
    }
}
