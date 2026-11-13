#pragma once

// Per-image smooth warp of the ground truth onto the render, absorbing a
// camera's calibration residual (docs/notes/calib-warp.md). The table holds,
// per image, an n x n grid of control points spanning the image corner to
// corner, each D floats: a displacement in milliradians (x fx/1000 -> pixels),
// then with D = 4 a parallax term that is divided by the rendered depth.
// The gradient and Hessian tables are per image of the batch, not per table row.
// calib_warp_aux_forward moves a uint8 mask (nearest), a depth or a normal map
// (bilinear over valid taps) through the same table at its own size;
// cut_by_source keeps a mask inside its unwarped self, for skipped tiles.

#include <core/Tensor.h>

#include <stdexcept>
#include <string>

// Floats of lumped Gauss-Newton Hessian per control point: (A, B, C) of the
// 2x2 pixel-space block, times 1, e, e^2 for the parallax term e = 1/depth.
inline int calib_warp_hess_floats(int D) { return D == 4 ? 9 : 3; }


/* == AUTO HEADER GENERATOR - DO NOT EDIT THIS LINE OR ANYTHING BELOW THIS LINE == */



void calib_warp_gt_forward(
    TorchTensorView gt_rgb, TorchTensorView render_rgb, TorchTensorView render_depth,
    TorchTensorView render_Ts, TorchTensorView params, TorchTensorView intrins,
    TorchTensorView cam_indices, int grid_n, int D, TorchTensorView out_rgb
);


void calib_warp_aux_forward(
    TorchTensorView src, TorchTensorView render_depth, TorchTensorView render_Ts,
    TorchTensorView params, TorchTensorView intrins, TorchTensorView cam_indices,
    int grid_n, int D, int render_H, int render_W, bool cut_by_source, TorchTensorView out
);


void calib_warp_accumulate(
    TorchTensorView gt_rgb, TorchTensorView render_rgb, TorchTensorView render_depth,
    TorchTensorView render_Ts, TorchTensorView v_render_rgb, float v_scale,
    TorchTensorView params, TorchTensorView intrins, TorchTensorView cam_indices,
    TorchTensorView mask, int grid_n, int D, float robust_eps, float f_scale, bool gt_is_warped,
    TorchTensorView grad, TorchTensorView hess
);


void calib_warp_update(
    TorchTensorView params, TorchTensorView grad, TorchTensorView hess,
    TorchTensorView cam_indices, TorchTensorView intrins, float f_scale, int batch, int grid_n,
    int D, float step, float damping, float smooth, float prior, float max_step
);


void calib_warp_downsample2(TorchTensorView in_rgb, TorchTensorView out_rgb);
