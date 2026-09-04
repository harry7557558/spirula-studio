#pragma once

// The camera models on the CPU: lens distortion both ways, a pixel to a ray,
// a ray back to a pixel, and the pinhole faces a wide camera is split into.
//
// Host mirror of shaders/projection_utils.slang, as data/SourceCamera.h is of
// shaders/camera_source.slang. This is the ONE host copy: the GUI's frustum
// wireframe, `spirula geometry`'s resampling and the trainer's split call it.
//
// Coordinates are NORMALIZED image coordinates ((px - cx) / fx), not pixels,
// and rays are CV camera space (+Z forward, +Y down). The distortion tier
// travels with the coefficients because the slot order is per-tier.

#include <vector>

namespace camhost {

// Any coefficient set to something other than zero. A tier that is nominally
// present but all-zero is not worth a Newton solve.
bool has_distortion(int tier, const float* d);

// Lens distortion alone, in the tier's own slot order. `distort_lens` is
// closed form; `undistort_lens` is Newton, and returns false when the forward
// map folds over itself or the solve does not converge.
void distort_lens(double u, double v, int tier, const float* d, double out[2]);
bool undistort_lens(double u, double v, int tier, const float* d, double out[2]);

// The device's is_valid_distortion: the lens has not folded at this
// undistorted coordinate, by the Jacobian bounds the warp kernels apply.
bool valid_distortion(double u, double v, int tier, const float* d);

// The full model: perspective-normalized coordinates -> the source camera's
// normalized coordinates, i.e. the fisheye or equisolid angular mapping and
// then the lens. EQUIRECTANGULAR passes through -- its coordinates are already
// (lon, lat) and it carries no lens.
void distort_point(double u, double v, int model, int tier, const float* d, double out[2]);

// The inverse: the source camera's normalized coordinates -> perspective
// normalized coordinates. False where the model has no image for that point.
bool undistort_point(double u, double v, int model, int tier, const float* d,
                     double out[2]);

// The source camera's normalized coordinates -> a UNIT ray. False outside the
// model's valid domain, including past the angle where it folds back.
bool generate_ray(double u, double v, int model, int tier, const float* d, double out[3]);

// And back: a ray (need not be normalized) -> the source camera's normalized
// coordinates. False where the camera cannot see, by the GPU warp's own fold
// test; it is the forward projection every wide-to-pinhole resample gathers through.
bool project_ray(const double ray[3], int model, int tier, const float* d, double out[2]);

// What fraction of this frame ONE pinhole undistortion at the same intrinsics
// keeps. Blind to the distortion coefficients and to cx / cy on purpose: a
// threshold that flips under -ffast-math trains differently on two machines.
double pinhole_coverage(int model, int width, int height, double fx, double fy);

// Whether `spirula geometry --split auto` resamples this lens into pinhole
// faces, and so whether the depth map it wrote is RAY depth. The trainer's
// --input-depth-is-ray-depth default asks this rather than re-spelling it.
bool splits_to_pinhole_faces(int model, int width, int height, double fx, double fy);


// ===========================================================================
// Visibility: which rays a frame actually holds
// ===========================================================================

// A camera as the face planners see it: the fitted model plus, when the
// parser had to fit one, the true source lens the kernels project through.
struct Camera {
    int    model = 0, tier = 0;      // CameraModelType / CameraDistortionType
    int    width = 0, height = 0;
    double fx = 0, fy = 0, cx = 0, cy = 0;
    float  dist[8] = {};
    int    source_model = -1;        // data/SourceCamera.h; -1 = exact tier
    float  source_params[16] = {};
};

// Ray (camera frame, any length) -> pixel, true only where the GPU warp
// samples it: a valid projection that lands inside the frame.
bool ray_in_frame(const Camera& cam, const double ray[3], double px[2]);

// Bounding box {x0, x1, y0, y1} of the visible rays `az + x*ax + y*ay` of the
// frame `R` (rows ax, ay, az) within `cell` (same layout); false when empty.
// `fraction`, if given, receives the share of the cell that is visible.
bool visible_bbox(const Camera& cam, const double R[9], const double cell[4],
                  double bbox[4], double* fraction = nullptr);

// The largest visible angle off the optical axis at `samples` azimuths
// (radians; azimuth k is 2*pi*k/samples from +x toward +y), 0 where even the
// axis is not. Star-shaped about the axis, which an inscribed principal point gives.
std::vector<double> visible_boundary(const Camera& cam, int samples);


// ===========================================================================
// The trainer's split
// ===========================================================================

// The six cube frames a wide camera is split into, [6][3][3] rows (ax, ay,
// az): front, +x, +y, -x, -y, back. The side frames of a table share one
// roll, so one size fits every crop; a frame that sees nothing is dropped.
const double* fisheye_face_axes();
const double* equirect_face_axes();

// One rendered face: a pinhole inside frame `face` of the camera's table,
// never more than one per frame. `crop_*` is the extent the lens actually
// fills there, which `width` and `height` cover (see FaceFit).
struct SplitFace {
    int    face = 0;
    int    width = 0, height = 0;
    int    crop_w = 0, crop_h = 0;
    double fx = 0, fy = 0, cx = 0, cy = 0;
};

// How the crops become faces: one common size so a batch renders them all in
// one pass, or each its own -- fewer pixels, one pass per distinct size.
// docs/datasets.md, "The split".
enum class FaceFit { Uniform, PerFace };

// The focal is that of a 90-degree face of ceil(sqrt(W*H/K)) pixels, whatever
// the lens, so the pixel density does not move with the field of view.
// `back_face` admits the frame behind a fisheye; a panorama always takes it.
std::vector<SplitFace> plan_split_faces(const Camera& cam,
                                        FaceFit fit = FaceFit::Uniform,
                                        bool back_face = false);

}  // namespace camhost
