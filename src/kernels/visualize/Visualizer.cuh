#include "backend/api/BackendTypes.h"
#include <cstdint>


#include <core/Common.cuh>


/* == AUTO HEADER GENERATOR - DO NOT EDIT THIS LINE OR ANYTHING BELOW THIS LINE == */



void engine_blit_view(
    std::string     buffer_key,
    TorchTensorView render_buffer,
    TorchTensorView render_depth,
    TorchTensorView render_alpha,
    bool            alpha_is_transmittance,
    int             view_camera_model,
    std::string     distortion,
    TorchTensorView view_intrins,
    TorchTensorView view_viewmat,
    TorchTensorView view_dist_coeffs,
    bool            show_training_cameras,
    bool            show_overlay,
    float           grid_dist,
    float           grid_target_x,
    float           grid_target_y,
    float           grid_target_z,
    TorchTensorView out_rgb);


void blit_train_cameras_tensor(
    TorchTensorView render_rgbs,      // [H, W, C] float32
    TorchTensorView render_depths,    // [H, W, 1] float32
    TorchTensorView render_alphas,    // [H, W, 1] float32
    const int view_camera_model,
    std::string distortion,
    TorchTensorView view_intrins,     // [1, 4] or [4] float32
    TorchTensorView view_viewmat,     // [4, 4] float32
    TorchTensorView view_dist_coeffs,
    TorchTensorView intrins,          // [N, 4] float32
    TorchTensorView widths,           // [N] int32
    TorchTensorView heights,          // [N] int32
    TorchTensorView camera_models,    // [N] int32
    TorchTensorView dist_coeffs,
    TorchTensorView distortions,      // [N] int32 CameraDistortionType
    TorchTensorView camera_to_worlds, // [N, 3, 4] float32
    TorchTensorView thumbnails,       // [B, H, W, 4] uint8
    float camera_size,
    bool show_training_cameras,
    TorchTensorView out_rgb           // [H, W, 3] uint8, pre-allocated
);
