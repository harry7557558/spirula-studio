#pragma once


#include <core/Tensor.h>

enum class RawPPISPRegLossIndex {
    SumExposure,
    SumVignettingCrSquared,
    SumVignettingAlpha0Relu,
    SumVignettingAlpha1Relu,
    SumVignettingAlpha2Relu,
    SumVignettingCxChannelVariance,
    SumVignettingCyChannelVariance,
    SumVignettingAlpha0ChannelVariance,
    SumVignettingAlpha1ChannelVariance,
    SumVignettingAlpha2ChannelVariance,
    SumColorBx,
    SumColorBy,
    SumColorRx,
    SumColorRy,
    SumColorGx,
    SumColorGy,
    SumColorNx,
    SumColorNy,
    SumCRFToeChannelVariance,
    SumCRFShoulderChannelVariance,
    SumCRFGammaChannelVariance,
    SumCRFCenterChannelVariance,
    length
};

enum class RawPPISPRegLossIndexNoCRF {
    SumExposure,
    SumVignettingCrSquared,
    SumVignettingAlpha0Relu,
    SumVignettingAlpha1Relu,
    SumVignettingAlpha2Relu,
    SumVignettingCxChannelVariance,
    SumVignettingCyChannelVariance,
    SumVignettingAlpha0ChannelVariance,
    SumVignettingAlpha1ChannelVariance,
    SumVignettingAlpha2ChannelVariance,
    SumColorBx,
    SumColorBy,
    SumColorRx,
    SumColorRy,
    SumColorGx,
    SumColorGy,
    SumColorNx,
    SumColorNy,
    length
};

enum class RawPPISPRegLossIndexRQS {
    SumExposure,
    SumVignettingCrSquared,
    SumVignettingAlpha0Relu,
    SumVignettingAlpha1Relu,
    SumVignettingAlpha2Relu,
    SumVignettingCxChannelVariance,
    SumVignettingCyChannelVariance,
    SumVignettingAlpha0ChannelVariance,
    SumVignettingAlpha1ChannelVariance,
    SumVignettingAlpha2ChannelVariance,
    SumColorBx,
    SumColorBy,
    SumColorRx,
    SumColorRy,
    SumColorGx,
    SumColorGy,
    SumColorNx,
    SumColorNy,
    SumCRFG0ChannelVariance,
    SumCRFG1ChannelVariance,
    SumCRFX0ChannelVariance,
    SumCRFY0ChannelVariance,
    SumCRFGcChannelVariance,
    length
};

enum class PPISPRegLossIndex {
    ExposureMean,
    VignettingCenter,
    VignettingNonPositivity,
    VignettingChannelVariance,
    ColorMean,
    CRFChannelVariance,
    length
};


/* == AUTO HEADER GENERATOR - DO NOT EDIT THIS LINE OR ANYTHING BELOW THIS LINE == */



void uint8_image_to_float_tensor(
    DeviceTensor3D<uint8_t> img_in,  // [B, H, W, C] (C packed as uint8_t)
    DeviceTensor3D<float> img_out    // [B, H, W, C]
);


void uint16_image_to_float_tensor(
    DeviceTensor3D<uint16_t> img_in,  // [B, H, W, C] (C packed as uint16_t)
    DeviceTensor3D<float> img_out     // [B, H, W, C]
);


void rendered_depth_to_expected_depth_forward(
    TorchTensorView depth,  // [B, H, W, 1]
    TorchTensorView transmittance,  // [B, H, W, 1]
    TorchTensorView out_depth  // [B, H, W, 1]
);


void rendered_depth_to_expected_depth_backward(
    TorchTensorView depth,  // [B, H, W, 1]
    TorchTensorView transmittance,  // [B, H, W, 1]
    TorchTensorView v_out_depth,  // [B, H, W, 1]
    TorchTensorView v_depth,  // [B, H, W, 1]
    TorchTensorView v_transmittance  // [B, H, W, 1]
);


void blend_background_forward(
    DeviceTensor3D<float3> rgb,           // [B, H, W, 3]
    DeviceTensor3D<float>  transmittance, // [B, H, W, 1]
    DeviceTensor3D<float3> background,    // [B, H, W, 3]
    DeviceTensor3D<float3> out_rgb        // [B, H, W, 3]
);


void blend_background_backward(
    DeviceTensor3D<float3> rgb,              // [B, H, W, 3]
    DeviceTensor3D<float>  transmittance,    // [B, H, W, 1]
    DeviceTensor3D<float3> background,       // [B, H, W, 3]
    DeviceTensor3D<float3> v_out_rgb,        // [B, H, W, 3]
    DeviceTensor3D<float3> v_rgb,            // [B, H, W, 3]
    DeviceTensor3D<float>  v_transmittance,  // [B, H, W, 1]
    DeviceTensor3D<float3> v_background      // [B, H, W, 3]
);


void blend_background_noise_forward(
    bool is_linear,
    bool blocky,                          // tiled RGB corners instead of U[0,1)
    DeviceTensor3D<float3> rgb,           // [B, H, W, 3]
    DeviceTensor3D<float>  transmittance, // [B, H, W, 1]
    float randomize_weight,
    uint32_t seed,
    DeviceTensor3D<float3> out_rgb        // [B, H, W, 3]
);


void blend_background_noise_backward(
    bool is_linear,
    bool blocky,                          // tiled RGB corners instead of U[0,1)
    DeviceTensor3D<float3> rgb,              // [B, H, W, 3]
    DeviceTensor3D<float>  transmittance,    // [B, H, W, 1]
    float randomize_weight,
    uint32_t seed,
    DeviceTensor3D<float3> v_out_rgb,        // [B, H, W, 3]
    DeviceTensor3D<float3> v_rgb,            // [B, H, W, 3]
    DeviceTensor3D<float>  v_transmittance   // [B, H, W, 1]
);


void rgb_to_srgb_forward(
    bool is_input_linear,
    DeviceTensor3D<float3> rgb,          // [B, H, W, 3]
    DeviceTensor2D<float3> color_matrix, // [3, 3] stored as 3 float3
    DeviceTensor3D<float3> out_rgb       // [B, H, W, 3]
);


void rgb_to_srgb_backward(
    bool is_input_linear,
    DeviceTensor3D<float3> rgb,          // [B, H, W, 3]
    DeviceTensor2D<float3> color_matrix, // [3, 3] stored as 3 float3
    DeviceTensor3D<float3> v_out_rgb,    // [B, H, W, 3]
    DeviceTensor3D<float3> v_rgb         // [B, H, W, 3]
);


void overexposure_grad_add(
    DeviceTensor3D<float3> rgb,    // [B, H, W, 3]
    float weight,                  // L = weight * mean(max(-x, x-1, 0)^2)
    DeviceTensor3D<float3> v_rgb   // [B, H, W, 3], in/out
);


void depth_to_points_forward(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,            // [B, 4]
    TorchTensorView dist_coeffs,        // [B, 8]
    bool is_ray_depth,
    DeviceTensor3D<float>  depths,      // [B, H, W, 1]
    DeviceTensor3D<float3> out_points   // [B, H, W, 3]
);


void depth_to_points_backward(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,            // [B, 4]
    TorchTensorView dist_coeffs,        // [B, 8]
    bool is_ray_depth,
    DeviceTensor3D<float>  in_depths,   // [B, H, W, 1]
    DeviceTensor3D<float3> v_out_points,// [B, H, W, 3]
    DeviceTensor3D<float>  v_in_depths  // [B, H, W, 1]
);


void depth_to_normal_forward(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,            // [B, 4]
    TorchTensorView dist_coeffs,        // [B, 8]
    bool is_ray_depth,
    DeviceTensor3D<float>  depths,      // [B, H, W, 1]
    DeviceTensor3D<float3> normals      // [B, H, W, 3]
);


void depth_to_normal_backward(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,            // [B, 4]
    TorchTensorView dist_coeffs,        // [B, 8]
    bool is_ray_depth,
    DeviceTensor3D<float>  depths,      // [B, H, W, 1]
    DeviceTensor3D<float3> v_normals,   // [B, H, W, 3]
    DeviceTensor3D<float>  v_depths     // [B, H, W, 1] (accumulated in-place)
);


void depth_to_normal_forward_tv(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,
    TorchTensorView dist_coeffs,
    bool is_ray_depth,
    TorchTensorView depths,    // [B, H, W, 1] float32, CUDA
    TorchTensorView normals    // [B, H, W, 3] float32, CUDA (pre-allocated output)
);


void depth_to_normal_backward_tv(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,
    TorchTensorView dist_coeffs,
    bool is_ray_depth,
    TorchTensorView depths,    // [B, H, W, 1]
    TorchTensorView v_normals, // [B, H, W, 3]
    TorchTensorView v_depths   // [B, H, W, 1] accumulated in-place
);


void depth_normal_loss_forward(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,            // [B, 4]
    TorchTensorView dist_coeffs,        // [B, 8]
    bool is_ray_depth,
    DeviceTensor3D<float>  depths,      // [B, H, W, 1]
    DeviceTensor3D<float3> gt_normals,  // [B, H, W, 3]
    DeviceTensor3D<float>  losses       // [B, H, W, 1]
);


void depth_normal_loss_backward(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,            // [B, 4]
    TorchTensorView dist_coeffs,        // [B, 8]
    bool is_ray_depth,
    DeviceTensor3D<float>  depths,      // [B, H, W, 1]
    DeviceTensor3D<float3> gt_normals,  // [B, H, W, 3]
    DeviceTensor3D<float>  v_losses,    // [B, H, W, 1]
    DeviceTensor3D<float>  v_depths,    // [B, H, W, 1] (must be pre-zeroed)
    DeviceTensor3D<float3> v_gt_normals // [B, H, W, 3]
);


void ray_depth_to_linear_depth_forward(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,  // [B, 4]
    TorchTensorView dist_coeffs,  // [B, 8]
    TorchTensorView depths,  // [B, H, W, 1]
    TorchTensorView out_depths  // [B, H, W, 1]
);


void ray_depth_to_linear_depth_backward(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,  // [B, 4]
    TorchTensorView dist_coeffs,  // [B, 8]
    TorchTensorView v_out_depths,  // [B, H, W, 1]
    TorchTensorView v_in_depths  // [B, H, W, 1]
);


void linear_depth_to_ray_depth_inplace(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,        // [B, 4] at image resolution
    TorchTensorView dist_coeffs,    // [B, 8]
    int image_width, int image_height,
    DeviceTensor3D<float> depths    // [B, Hd, Wd, 1] in/out
);


void distort_image_tensor(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,            // [B, 4]
    TorchTensorView dist_coeffs,        // [B, 8]
    TorchTensorView in_image,           // [B, H, W, C] float
    TorchTensorView out_image           // [B, H, W, C] float (must be pre-zeroed)
);


void undistort_image_tensor(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,            // [B, 4]
    TorchTensorView dist_coeffs,        // [B, 8]
    TorchTensorView in_image,           // [B, H, W, C] float
    TorchTensorView out_image           // [B, H, W, C] float (must be pre-zeroed)
);


void warp_image_wide_to_pinhole_tensor(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,            // [B, 4]
    TorchTensorView dist_coeffs,        // [B, 8]
    TorchTensorView wide_image,         // [B, H, W, C] (float)
    TorchTensorView axes,               // [K, 3, 3]
    int out_w, int out_h,
    TorchTensorView pinhole_images      // [B, K, H, W, C] (float)
);


void warp_image_equirectangular_to_pinhole_tensor(
    TorchTensorView equirectangular_image,  // [B, H, W, C] (float)
    TorchTensorView axes,                   // [K, 3, 3]
    int out_w, int out_h,
    TorchTensorView pinhole_images          // [B, K, H, W, C] (float)
);


void launch_warp_byte_to_float_wide(
    std::string camera_model,
    std::string distortion,
    const float* d_intrins,                // [B, 4]
    const float* d_dist_coeffs,            // [B, 8] (nullable -> all-zeros)
    const int*   d_source_models,          // [B] (null unless re-distorting)
    const float* d_source_params,          // [B, 16]
    const void* d_byte, bool input_is_u16,
    int B, int Hin, int Win, int C,
    float* d_float_out, int K, int Hout, int Wout,
    const float* d_axes);


void launch_warp_byte_to_float_equi(
    const void* d_byte, bool input_is_u16,
    int B, int Hin, int Win, int C,
    float* d_float_out, int K, int Hout, int Wout,
    const float* d_axes);


void launch_warp_mask_wide(
    std::string camera_model,
    std::string distortion,
    const float* d_intrins,                // [B, 4]
    const float* d_dist_coeffs,            // [B, 8] (nullable)
    const int*   d_source_models,          // [B] (null unless re-distorting)
    const float* d_source_params,          // [B, 16]
    const uint8_t* d_byte_mask,
    int B, int Hin, int Win,
    uint8_t* d_byte_out, int K, int Hout, int Wout,
    const float* d_axes);


void launch_warp_mask_equi(
    const uint8_t* d_byte_mask,
    int B, int Hin, int Win,
    uint8_t* d_byte_out, int K, int Hout, int Wout,
    const float* d_axes);


void launch_redistort_byte_to_float(
    std::string camera_model,
    std::string distortion,
    const float* d_intrins,                // [B, 4] fitted
    const float* d_dist_coeffs,            // [B, 8] (nullable -> zeros)
    const int*   d_source_models,          // [B]
    const float* d_source_params,          // [B, 16]
    const void* d_byte, bool input_is_u16,
    int B, int in_H, int in_W, int C,
    float* d_float_out, int out_H, int out_W,
    int ref_H, int ref_W,
    float invalid);


void launch_redistort_depth(
    std::string camera_model,
    std::string distortion,
    const float* d_intrins,
    const float* d_dist_coeffs,
    const int*   d_source_models,
    const float* d_source_params,
    const void* d_in, uint32_t elem_size,   // 2 = uint16 raw counts, 4 = float
    int B, int in_H, int in_W,
    float* d_float_out, int out_H, int out_W,
    int ref_H, int ref_W);


void launch_redistort_mask(
    std::string camera_model,
    std::string distortion,
    const float* d_intrins,
    const float* d_dist_coeffs,
    const int*   d_source_models,
    const float* d_source_params,
    const uint8_t* d_byte_mask,
    int B, int in_H, int in_W,
    uint8_t* d_byte_out, int out_H, int out_W,
    int ref_H, int ref_W);


void launch_redistort_normal(
    std::string camera_model,
    std::string distortion,
    const float* d_intrins,
    const float* d_dist_coeffs,
    const int*   d_source_models,
    const float* d_source_params,
    const void* d_in, bool input_is_float,
    int B, int in_H, int in_W,
    float* d_float_out, int out_H, int out_W,
    int ref_H, int ref_W);


void launch_warp_depth_wide(
    std::string camera_model,
    std::string distortion,
    const float* d_intrins,
    const float* d_dist_coeffs,
    const int*   d_source_models,
    const float* d_source_params,
    const void* d_depth, uint32_t elem_size,
    int B, int Hin, int Win,
    int in_H, int in_W,
    float* d_float_out, int K, int Hout, int Wout,
    const float* d_axes, bool input_is_ray_depth);


void launch_warp_depth_equi(
    const void* d_depth, uint32_t elem_size,
    int B, int Hin, int Win,
    float* d_float_out, int K, int Hout, int Wout,
    const float* d_axes, bool input_is_ray_depth);


void launch_warp_normal_wide(
    std::string camera_model,
    std::string distortion,
    const float* d_intrins,
    const float* d_dist_coeffs,
    const int*   d_source_models,
    const float* d_source_params,
    const void* d_normal, uint32_t elem_size,
    int B, int Hin, int Win,
    int in_H, int in_W,
    float* d_float_out, int K, int Hout, int Wout,
    const float* d_axes);


void launch_warp_normal_equi(
    const void* d_normal, uint32_t elem_size,
    int B, int Hin, int Win,
    float* d_float_out, int K, int Hout, int Wout,
    const float* d_axes);


void warp_image_pinhole_to_wide_tensor(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,            // [B, 4]
    TorchTensorView dist_coeffs,        // [B, 8]
    TorchTensorView pinhole_images,     // [B, K, H, W, C]
    TorchTensorView axes,               // [K, 3, 3]
    int out_w, int out_h,
    TorchTensorView wide_image          // [B, H, W, C]
);


void warp_linear_depth_pinhole_to_wide_tensor(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,            // [B, 4]
    TorchTensorView dist_coeffs,        // [B, 8]
    TorchTensorView pinhole_images,     // [B, K, H, W, 1]
    TorchTensorView axes,               // [K, 3, 3]
    int out_w, int out_h,
    TorchTensorView wide_image          // [B, H, W, 1]
);


void warp_ray_depth_pinhole_to_wide_tensor(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,            // [B, 4]
    TorchTensorView dist_coeffs,        // [B, 8]
    TorchTensorView pinhole_images,     // [B, K, H, W, 1]
    TorchTensorView axes,               // [K, 3, 3]
    int out_w, int out_h,
    TorchTensorView wide_image          // [B, H, W, 1]
);


void warp_points_pinhole_to_wide_tensor(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,            // [B, 4]
    TorchTensorView dist_coeffs,        // [B, 8]
    TorchTensorView pinhole_images,     // [B, K, H, W, 3]
    TorchTensorView axes,               // [K, 3, 3]
    int out_w, int out_h,
    TorchTensorView wide_image          // [B, H, W, 3]
);


void warp_depth_pinhole_to_wide_scale_matrix_tensor(
    std::string camera_model,
    std::string distortion,
    TorchTensorView intrins,            // [B, 4]
    TorchTensorView dist_coeffs,        // [B, 8]
    TorchTensorView pinhole_images,     // [B, K, H, W, 1]
    TorchTensorView axes,               // [K, 3, 3]
    int out_w, int out_h,
    TorchTensorView matrix              // [B, K, K] (must be pre-zeroed)
);


void ppisp_forward(
    DeviceTensor3D<float3> in_image,    // [B, H, W, 3]
    TorchTensorView ppisp_params,       // [N_cam or B, PPISP_NUM_PARAMS]
    TorchTensorView intrins,            // [B, 4]
    const float actual_image_width,
    const float actual_image_height,
    std::string param_type,
    TorchTensorView cam_indices,        // [B] int32, or null -> identity (ppisp_params is [B,P])
    DeviceTensor3D<float3> out_image    // [B, H, W, 3]
);


void ppisp_backward(
    DeviceTensor3D<float3> in_image,    // [B, H, W, 3]
    TorchTensorView ppisp_params,       // [N_cam or B, PPISP_NUM_PARAMS]
    TorchTensorView intrins,            // [B, 4]
    const float actual_image_width,
    const float actual_image_height,
    DeviceTensor3D<float3> v_out_image, // [B, H, W, 3]
    std::string param_type,
    TorchTensorView cam_indices,        // [B] int32, or null -> identity
    DeviceTensor3D<float3> v_in_image,  // [B, H, W, 3]
    TorchTensorView v_ppisp_params      // [N_cam or B, PPISP_NUM_PARAMS] (must be pre-zeroed)
);


void compute_ppsip_regularization_forward(
    TorchTensorView ppisp_params,       // [B, PPISP_NUM_PARAMS]
    const std::array<float, (int)PPISPRegLossIndex::length> loss_weights_0,
    std::string param_type,
    TorchTensorView losses,             // [PPISPRegLossIndex::length] (must be pre-zeroed)
    TorchTensorView raw_losses          // [B+1, RawPPISPRegLossIndex::length] (must be pre-zeroed)
);


void compute_ppsip_regularization_backward(
    TorchTensorView ppisp_params,       // [B, PPISP_NUM_PARAMS]
    const std::array<float, (int)PPISPRegLossIndex::length> loss_weights_0,
    TorchTensorView raw_losses,         // [B+1, RawPPISPRegLossIndex::length]
    TorchTensorView v_losses,           // [PPISPRegLossIndex::length]
    std::string param_type,
    TorchTensorView v_ppisp_params      // [B, PPISP_NUM_PARAMS] (must be pre-zeroed)
);
