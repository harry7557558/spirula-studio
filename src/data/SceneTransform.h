#pragma once

// SceneTransform -- the similarity between a dataset's own frame and the
// frame the splats are trained in, and the scene_transform.json a run leaves
// beside config.json: one transform spelled every common way (matrices,
// quaternions, Euler angles, both directions) so nothing has to be converted
// by hand downstream.

#include <string>

namespace spirula {

// p_train = scale * R @ p_world + t. R row-major.
struct SceneTransform {
    double R[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    double t[3] = {0, 0, 0};
    double scale = 1.0;

    // p_world = R^T (p_train - t) / scale.
    SceneTransform inverse() const;
};

// Unit quaternion (w, x, y, z) of a row-major rotation matrix.
void rotation_to_quaternion(const double R[9], double q[4]);

// Tait-Bryan angles (radians) of R = R_a(x) R_b(y) R_c(z) for an intrinsic
// axis order such as "xyz": the first rotation is about the first letter.
// Extrinsic "abc" is intrinsic "cba" with the angles reversed.
void rotation_to_euler_intrinsic(const double R[9], const char order[3],
                                 double out[3]);
void euler_intrinsic_to_rotation(const char order[3], const double a[3],
                                 double R[9]);

// The JSON text. `center_mode` and `center_world` say how the translation was
// chosen (DatasetParser.h center_mode; the point of the dataset frame that
// became the origin).
std::string scene_transform_json(const SceneTransform& train_from_world,
                                 const std::string& center_mode,
                                 const double center_world[3]);

// The file back: false when it is missing or not one of these.
bool read_scene_transform_json(const std::string& path, SceneTransform& train_from_world);

}  // namespace spirula
