#pragma once

// Numeric controls shared by the editor, preset codec and child command.
#define SS_MESH_NUMERIC_FIELDS(X) \
    X(float, tsdf_voxel, "tsdf-voxel", 0, Surface, Expert, tsdf_voxel_help, tsdf) \
    X(float, tsdf_trunc, "tsdf-trunc", 4, Surface, Expert, tsdf_trunc_help, tsdf) \
    X(float, tsdf_max_depth, "tsdf-max-depth", 0, Surface, Advanced, tsdf_max_depth_help, voxel_grid) \
    X(float, iso, "iso", 0.5, Surface, Advanced, iso_help, true) \
    X(int, bisection_iters, "bisection-iters", 3, Surface, Expert, bisection_iters_help, !voxel_grid) \
    X(int, samples_per_gaussian, "samples-per-gaussian", 7, Surface, Advanced, samples_per_gaussian_help, !voxel_grid) \
    /* X(int, max_grid_res, "max-grid-res", 512, Runtime, Expert, legacy_grid_help, false) */ \
    /* X(float, grid_cell_factor, "grid-cell-factor", 2, Runtime, Expert, legacy_grid_help, false) */ \
    X(int, num_threads, "num-threads", 0, Runtime, Advanced, num_threads_help, true) \
    X(float, merge_flat_px, "merge-flat-px", 1, Merge, Expert, merge_flat_px_help, merging) \
    X(float, merge_thin, "merge-thin", 0.1f, Merge, Expert, merge_thin_help, merging && job.use_data) \
    X(float, merge_max_edge_px, "merge-max-edge-px", 8, Merge, Expert, merge_max_edge_px_help, merging) \
    X(float, merge_color, "merge-color", 0.05f, Merge, Expert, merge_color_help, merging) \
    X(float, merge_max_flip_deg, "merge-max-flip-deg", 60, Merge, Expert, merge_max_flip_deg_help, merging) \
    X(float, fill_hole_ratio, "fill-hole-ratio", 0.05f, Cleanup, Expert, fill_hole_ratio_help, true) \
    X(int, fill_hole_max_edges, "fill-hole-max-edges", 20, Cleanup, Advanced, fill_hole_max_edges_help, true) \
    X(float, degenerate_angle_deg, "degenerate-angle-deg", 15, Cleanup, Expert, degenerate_angle_deg_help, true) \
    X(int, quality_iters, "quality-iters", 3, Cleanup, Advanced, quality_iters_help, true) \
    X(int, tex_gutter_px, "tex-gutter-px", 4, Texture, Expert, tex_gutter_px_help, textured) \
    X(float, chart_angle_deg, "chart-angle-deg", 60, Texture, Expert, chart_angle_deg_help, textured) \
    X(int, check, "check", 0, Runtime, Expert, check_help, true)
