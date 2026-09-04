#pragma once

// A distorted camera into a monocular geometry model and back: the frame is
// resampled into pinhole faces on the way in and the prediction gathered into
// the source camera's own frame on the way out, planned once per camera.
//
// The faces are laid out from what the lens actually holds -- a front face
// plus a ring of upright square faces reaching the visible boundary -- and
// OVERLAP so `gather` can cross-fade them, because each is a separate call
// into a network that re-guesses its depth scale and its horizon every time.
// src/metric3d/README.md, "The camera side", is the rest of it.

#include "core/CameraModel.h"

#include <cstdint>
#include <string>
#include <vector>

namespace app {

// [sh, sw, channels] bytes -> [dh, dw, 3] floats in 0..1, what sampleFace
// takes. Area average, not bilinear: at a 2x downscale bilinear reads two of
// every three source pixels and aliases the structure the normals are made of.
std::vector<float> resize_area(const uint8_t* src, int sw, int sh, int channels,
                               int dw, int dh);

// One camera, as data/DatasetParser.h describes it.
struct GeometryCamera {
    int   model = 0;        // CameraModelType
    int   distortion = 0;   // CameraDistortionType
    int   width = 0, height = 0;
    float fx = 0, fy = 0, cx = 0, cy = 0;
    float dist[kCameraDistortionParams] = {};
    // A camera whose lens no tier represents exactly; the parser fitted it and
    // recorded the true projection. -1 when the tier is exact.
    int   source_model = -1;
    float source_params[16] = {};
};

class GeometryWarp {
public:
    // A split face is sized at the lens's own pixel density where it points,
    // NOT from out_w x out_h, then capped at `max_face`; sides land on a
    // multiple of `patch`. Throws std::runtime_error when nothing is visible.
    void plan(const GeometryCamera& cam, int out_w, int out_h, bool split, int patch,
              int max_face);

    bool split() const { return faces_.size() > 1; }
    int  faces() const { return (int)faces_.size(); }
    int  faceWidth(int k) const { return faces_[(size_t)k].w; }
    int  faceHeight(int k) const { return faces_[(size_t)k].h; }
    int  outWidth() const { return out_w_; }
    int  outHeight() const { return out_h_; }
    // A split keeps the wide capture's ray semantics; one face keeps z.
    bool defaultRayDepth() const { return split(); }

    // [faces(), 3, 3], or null for one face. Rows 0 and 1 span the face and
    // carry its half-extent in their LENGTH; row 2 is the unit optical axis.
    const double* faceAxes() const { return axes_.empty() ? nullptr : axes_.data(); }

    // The pinhole face k IS, in its own pixels. Metric3D's depth is canonical
    // to a focal of 1000, so metres are the prediction times faceFocal(k) over
    // 1000; MoGe needs all four to resolve its point map's z shift.
    double faceFocal(int k) const { return faces_[(size_t)k].fx; }
    double faceFocalY(int k) const { return faces_[(size_t)k].fy; }
    double faceCx(int k) const { return faces_[(size_t)k].cx; }
    double faceCy(int k) const { return faces_[(size_t)k].cy; }

    // The resolution to hand `sampleFace`: the source's, downscaled only as
    // far as the densest face is, and NOT the output resolution.
    int sampleWidth() const { return sw_; }
    int sampleHeight() const { return sh_; }

    // [sampleHeight(), sampleWidth(), 3] in 0..1 -> [faceHeight(k),
    // faceWidth(k), 3]. What the camera cannot see is filled mid-grey; a black
    // border reads as an edge to the network.
    void sampleFace(int k, const float* src, std::vector<float>& dst) const;

    // `depth[k]` is [fh*fw] depth in ONE unit across faces, `normal[k]` is
    // [fh*fw*3] unit normals in face k's frame; either list may be empty.
    // Pixels no face covers come back 0, the trainer's "no ground truth here".
    void gather(const std::vector<std::vector<float>>& depth,
                const std::vector<std::vector<float>>& normal, bool ray_depth,
                std::vector<float>* out_depth, std::vector<float>* out_normal) const;

private:
    struct Face {
        int    w = 0, h = 0;
        double fx = 0, fy = 0, cx = 0, cy = 0;
        // [fh*fw*2] coordinates in the sampleWidth() grid, NaN where the
        // camera has no ray at all, and [fh*fw] of how much is in frame.
        std::vector<float> to_src;
        std::vector<float> valid;
    };

    // One face's claim on one output pixel.
    struct Contrib {
        int32_t face;
        float   px, py;    // where in that face, in its own pixel coordinates
        float   w;         // cross-fade weight, zero at the face's outer edge
        float   sz, sr;    // face z-depth -> this camera's z / distance
    };

    std::vector<double> alignFaces(const std::vector<std::vector<float>>& depth) const;

    std::vector<Face>   faces_;
    std::vector<double> axes_;   // [faces, 3, 3], empty for one face
    int out_w_ = 0, out_h_ = 0, sw_ = 0, sh_ = 0;
    // Which faces reach each output pixel. Row offsets rather than a fixed
    // slot per face: only the overlap names more than one.
    std::vector<int64_t> contrib_off_;
    std::vector<Contrib> contrib_;
};

}  // namespace app
