#pragma once

// PoolSlots -- the single source of truth for every DevicePool buffer.
//
// WHY THIS FILE EXISTS
// --------------------
// Device buffers used to be keyed by ad-hoc std::string literals scattered
// across ~20 translation units. Three things silently broke when a new buffer
// was added and someone forgot to update a matching site:
//   1. checkpoint save (a buffer omitted from the hand-written dump),
//   2. the training-time VRAM breakdown (a buffer mis-bucketed by string
//      prefix-matching), and
//   3. any future save/load-for-resume path.
//
// Every buffer is now one row in POOL_SLOT_TABLE below. A row carries all the
// metadata the rest of the system needs, so adding a buffer is impossible
// WITHOUT supplying that metadata -- the X-macro makes each column mandatory,
// and the build fails otherwise. That is the whole point: a future programmer
// (or agent) cannot regress checkpointing or the VRAM report by forgetting a
// step, because there is only one step.
//
// HOW TO ADD A BUFFER
// -------------------
// Add ONE X(...) row to POOL_SLOT_TABLE with four fields:
//   X(Enumerator, "pool.name", <VramCategory>, <SaveClass>)
//     Enumerator   -- unique CamelCase name -> PoolSlot::Enumerator
//     "pool.name"  -- unique string shown in the VRAM breakdown / checkpoint
//     VramCategory -- which bucket it counts toward (see enum below)
//     SaveClass    -- Never / Resume / Always (see enum below)
// Then allocate it via the pool using PoolSlot::Enumerator. Nothing else.
//
// GRANULARITY
// -----------
// One row == one logical buffer. A codec that internally needs two backing
// allocations (QuantizedTensor* = packed bytes + per-block bounds) is still a
// SINGLE row; the two allocations are distinguished by a SUB-INDEX (see
// pool_key / kSubPacked / kSubBounds). Rows tagged `+.q/.qb` below use this.
//
// Buffers whose key is built from genuinely runtime-variable content (a
// per-pyramid-scale loop index + a per-buffer name, e.g. PerPixelLoss.cu's
// "ppl.s{n}.{name}") do NOT get a row. They are Never-saved scratch and go
// through acquire_dynamic(category, name, n) -- an escape hatch that attaches a
// VramCategory without a compile-time identity. Keep that list tiny.

#include <cstdint>
#include <cstddef>


// VRAM report buckets: splat / splat x img / image / appearance / viewer /
// other.
enum class VramCategory : uint8_t {
    Splat = 0,   // per-splat params, gradients, optimizer state, densify aux
    // Scratch sized by (camera, gaussian), visible or splat-tile PAIRS, so it
    // grows with the batch too. A buffer sized [N] is Splat however late it is
    // allocated -- docs/notes/vram-splat-x-img.md.
    SplatXImg,
    Image,       // per-image buffers: render outputs, GT, loss maps, img grads
    Appearance,  // bilagrid / background-SH / PPISP / color-space / color-shift
    Viewer,      // interactive viewer + sync visualizer caches and scratch
    Other,       // camera tables, data-manager staging, misc scratch
    Count
};

// Checkpoint disposition.
//   Never  -- transient (per-step scratch, gradients); never serialized.
//   Resume -- needed ONLY to resume training bit-exactly (optimizer moments,
//             densify aux, raw world buffers). Written when the resume flag
//             (--save_full_checkpoint) is on. NOTE: the Gaussians themselves
//             are Resume, not Always, because the PLY already captures them for
//             inference/meshing; the raw fp32/quant buffers here exist only to
//             continue optimizing.
//   Always -- appearance/correction params needed for inference and NOT already
//             in the PLY (bilagrid grids, PPISP params, background SH, ...).
//             Written in every checkpoint.
enum class SaveClass : uint8_t {
    Never = 0,
    Resume,
    Always,
    Count
};


// ============================================================================
// THE TABLE. See "HOW TO ADD A BUFFER" above. Columns:
//   X(Enumerator, "pool.name", VramCategory, SaveClass)
// Rows marked `+.q/.qb` allocate two sub-slots (packed + bounds) under one
// logical slot via the sub-index; the metadata applies to both.
// ============================================================================
#define POOL_SLOT_TABLE(X) \
  /* ---- world params ---- */ \
  X(WorldMeans                     , "world.means",                       Splat    , Resume) \
  X(WorldQuats                     , "world.quats",                       Splat    , Resume) \
  X(WorldScales                    , "world.scales",                      Splat    , Resume) \
  X(WorldOpacities                 , "world.opacities",                   Splat    , Resume) \
  X(WorldFeaturesDc                , "world.features_dc",                 Splat    , Resume) \
  X(WorldFeaturesSh                , "world.features_sh",                 Splat    , Resume) \
  /* ---- Adam moments (fp32) ---- */ \
  X(EngG1Means                     , "eng.g1_means",                      Splat    , Resume) \
  X(EngG1Quats                     , "eng.g1_quats",                      Splat    , Resume) \
  X(EngG1Scales                    , "eng.g1_scales",                     Splat    , Resume) \
  X(EngG1Opacities                 , "eng.g1_opacities",                  Splat    , Resume) \
  X(EngG1FeaturesDc                , "eng.g1_features_dc",                Splat    , Resume) \
  X(EngG1FeaturesSh                , "eng.g1_features_sh",                Splat    , Resume) \
  X(EngG2Means                     , "eng.g2_means",                      Splat    , Resume) \
  X(EngG2Quats                     , "eng.g2_quats",                      Splat    , Resume) \
  X(EngG2Scales                    , "eng.g2_scales",                     Splat    , Resume) \
  X(EngG2Opacities                 , "eng.g2_opacities",                  Splat    , Resume) \
  X(EngG2FeaturesDc                , "eng.g2_features_dc",                Splat    , Resume) \
  X(EngG2FeaturesSh                , "eng.g2_features_sh",                Splat    , Resume) \
  /* ---- Adam moments (FPBO quant) ---- */ \
  X(EngMeansQfpbo                  , "eng.means_qfpbo",                   Splat    , Resume)  /* +.q/.qb */ \
  X(EngQuatsQfpbo                  , "eng.quats_qfpbo",                   Splat    , Resume)  /* +.q/.qb */ \
  X(EngScalesQfpbo                 , "eng.scales_qfpbo",                  Splat    , Resume)  /* +.q/.qb */ \
  X(EngOpacitiesQfpbo              , "eng.opacities_qfpbo",               Splat    , Resume)  /* +.q/.qb */ \
  X(EngFeaturesDcQfpbo             , "eng.features_dc_qfpbo",             Splat    , Resume)  /* +.q/.qb */ \
  /* ---- SH Adam-state quant ---- */ \
  X(EngShQuant                     , "eng.sh_quant",                      Splat    , Resume)  /* +.q/.qb */ \
  X(EngShQuantFpbo                 , "eng.sh_quant_fpbo",                 Splat    , Resume)  /* +.q/.qb */ \
  /* ---- SH value quant ---- */ \
  X(EngWorldShVq8                  , "eng.world.sh_vq8",                  Splat    , Resume)  /* +.q/.qb */ \
  X(EngWorldShVq8Fpbo              , "eng.world.sh_vq8_fpbo",             Splat    , Resume)  /* +.q/.qb */ \
  X(EngWorldShVq16                 , "eng.world.sh_vq16",                 Splat    , Resume)  /* +.q/.qb */ \
  X(EngWorldShVq16Fpbo             , "eng.world.sh_vq16_fpbo",            Splat    , Resume)  /* +.q/.qb */ \
  /* ---- densify aux ---- */ \
  X(EngRadii                       , "eng.radii",                         Splat    , Resume) \
  X(EngAccumBuffer                 , "eng.accum_buffer",                  Splat    , Resume) \
  X(EngBiasCorrectionSteps         , "eng.bias_correction_steps",         Splat    , Resume) \
  X(EngDensifyWorldGradScore       , "eng.densify.world_grad_score",      Splat    , Never) \
  X(EngDensifySampleScore          , "eng.densify.sample_score",          Splat    , Never) \
  X(EngDensifyOversize             , "eng.densify.oversize",              Splat    , Never) \
  X(EngDensifyOversizeWeight       , "eng.densify.oversize_weight",       Splat    , Never) \
  /* ---- sub-batch scratch ---- */ \
  X(EngSubbatchAccumWeightSum      , "eng.subbatch.accum_weight_sum",     Splat    , Never) \
  /* ---- gradients ---- */ \
  X(EngVMeans                      , "eng.v_means",                       Splat    , Never) \
  X(EngVQuats                      , "eng.v_quats",                       Splat    , Never) \
  X(EngVScales                     , "eng.v_scales",                      Splat    , Never) \
  X(EngVOpacities                  , "eng.v_opacities",                   Splat    , Never) \
  X(EngVFeaturesDc                 , "eng.v_features_dc",                 Splat    , Never) \
  X(EngVFeaturesSh                 , "eng.v_features_sh",                 Splat    , Never) \
  /* ---- gradients (block-wise quantized; non-FPBO grad-quant path) ---- */ \
  X(EngVMeansQ                     , "eng.v_means_q",                     Splat    , Never)  /* +.q/.qb */ \
  X(EngVQuatsQ                     , "eng.v_quats_q",                     Splat    , Never)  /* +.q/.qb */ \
  X(EngVScalesQ                    , "eng.v_scales_q",                    Splat    , Never)  /* +.q/.qb */ \
  X(EngVOpacitiesQ                 , "eng.v_opacities_q",                 Splat    , Never)  /* +.q/.qb */ \
  X(EngVFeaturesDcQ                , "eng.v_features_dc_q",               Splat    , Never)  /* +.q/.qb */ \
  X(EngVFeaturesShQ                , "eng.v_features_sh_q",               Splat    , Never)  /* +.q/.qb */ \
  X(EngVRgb                        , "eng.v_rgb",                         Image    , Never) \
  X(EngVDepth                      , "eng.v_depth",                       Image    , Never) \
  X(EngVTs                         , "eng.v_Ts",                          Image    , Never) \
  X(EngVDepthNormal                , "eng.v_depth_normal",                Image    , Never) \
  X(EngVRefDepth                   , "eng.v_ref_depth",                   Image    , Never) \
  X(EngVRefNormal                  , "eng.v_ref_normal",                  Image    , Never) \
  X(EngVRgbDist                    , "eng.v_rgb_dist",                    Image    , Never) \
  X(EngVDepthDist                  , "eng.v_depth_dist",                  Image    , Never) \
  X(EngVNormalDist                 , "eng.v_normal_dist",                 Image    , Never) \
  X(EngVMedianDepth                , "eng.v_median_depth",                Image    , Never) \
  X(EngVMedianNormal               , "eng.v_median_normal",               Image    , Never) \
  X(EngVLosses                     , "eng.v_losses",                      Image    , Never) \
  /* ---- render scratch ---- */ \
  X(EngDepthNormal                 , "eng.depth_normal",                  Image    , Never) \
  X(EngMedianNormal                , "eng.median_normal",                 Image    , Never) \
  X(EngLossMap                     , "eng.loss_map",                      Image    , Never) \
  /* ---- projection scratch ---- */ \
  X(ProjAabb                       , "proj.aabb",                         SplatXImg, Never) \
  X(ProjDepths                     , "proj.depths",                       SplatXImg, Never) \
  X(ProjMask                       , "proj.mask",                         SplatXImg, Never) \
  X(ProjBlockCount                 , "proj.block_count",                  SplatXImg, Never) \
  X(ProjScan                       , "proj.scan",                         SplatXImg, Never) \
  X(ProjCameraIds                  , "proj.camera_ids",                   SplatXImg, Never) \
  X(ProjGaussianIds                , "proj.gaussian_ids",                 SplatXImg, Never) \
  X(ProjScreen                     , "proj.screen",                       SplatXImg, Never) \
  /* ---- tile intersect scratch ---- */ \
  X(IsectTilesPerSplat             , "isect.tiles_per_splat",             SplatXImg, Never) \
  X(IsectCumTiles                  , "isect.cum_tiles",                   SplatXImg, Never) \
  X(IsectTotal                     , "isect.total",                       SplatXImg, Never) \
  X(IsectOffsets                   , "isect.offsets",                     SplatXImg, Never) \
  X(IsectIdsA                      , "isect.ids_a",                       SplatXImg, Never) \
  X(IsectIdsB                      , "isect.ids_b",                       SplatXImg, Never) \
  X(IsectFlatA                     , "isect.flat_a",                      SplatXImg, Never) \
  X(IsectFlatB                     , "isect.flat_b",                      SplatXImg, Never) \
  X(IsectTileActive                , "isect.tile_active",                 Image    , Never) \
  /* ---- render outputs ---- */ \
  X(RenderTs                       , "render.Ts",                         Image    , Never) \
  X(RenderLastIds                  , "render.last_ids",                   Image    , Never) \
  X(RenderMedian                   , "render.median",                     Image    , Never) \
  /* render/distortion channel tuples: sub-buffers ".rgb"/".depth"/".normal" */ \
  X(Renders                        , "renders",                           Image    , Never) \
  X(Distortions                    , "distortions",                       Image    , Never) \
  /* ---- raster backward scratch ---- */ \
  X(RasterBwdAccumWeight           , "raster_bwd.accum_weight",           Splat    , Never) \
  X(RasterBwdVViewmats             , "raster_bwd.v_viewmats",             Other    , Never) \
  X(RasterBwdVScreen               , "raster_bwd.v_screen",               SplatXImg, Never) \
  X(RasterBwdVWorld                , "raster_bwd.v_world",                Splat    , Never) \
  /* ---- fused proj-bwd scratch ---- */ \
  X(FusedProjBwdGaussSorted        , "fused_proj_bwd.gauss_sorted",       SplatXImg, Never) \
  X(FusedProjBwdPerm               , "fused_proj_bwd.perm",               SplatXImg, Never) \
  X(FusedProjBwdPermSorted         , "fused_proj_bwd.perm_sorted",        SplatXImg, Never) \
  X(FusedProjBwdCamBounds          , "fused_proj_bwd.cam_bounds",         Splat    , Never) \
  /* ---- splat-tile intersector scratch ---- */ \
  X(StiBruteCount                  , "sti.brute.count",                   SplatXImg, Never) \
  X(StiBruteCountMap               , "sti.brute.count_map",               SplatXImg, Never) \
  X(StiBruteIds                    , "sti.brute.ids",                     SplatXImg, Never) \
  X(StiLbvhSplatAabb               , "sti.lbvh.splat_aabb",               SplatXImg, Never) \
  X(StiLbvhRootAabb                , "sti.lbvh.root_aabb",                SplatXImg, Never) \
  X(StiLbvhMorton                  , "sti.lbvh.morton",                   SplatXImg, Never) \
  X(StiLbvhArgsortIn               , "sti.lbvh.argsort_in",               SplatXImg, Never) \
  X(StiLbvhSortedMorton            , "sti.lbvh.sorted_morton",            SplatXImg, Never) \
  X(StiLbvhArgsort                 , "sti.lbvh.argsort",                  SplatXImg, Never) \
  X(StiLbvhTreeRanges              , "sti.lbvh.tree_ranges",              SplatXImg, Never) \
  X(StiLbvhInternalNodes           , "sti.lbvh.internal_nodes",           SplatXImg, Never) \
  X(StiLbvhParentNodes             , "sti.lbvh.parent_nodes",             SplatXImg, Never) \
  X(StiLbvhTreeAabb                , "sti.lbvh.tree_aabb",                SplatXImg, Never) \
  X(StiLbvhCount                   , "sti.lbvh.count",                    SplatXImg, Never) \
  X(StiLbvhCountMap                , "sti.lbvh.count_map",                SplatXImg, Never) \
  X(StiLbvhIds                     , "sti.lbvh.ids",                      SplatXImg, Never) \
  /* ---- meshing scratch ---- */ \
  X(MeshingRenderRadii             , "meshing.render.radii",              Splat    , Never) \
  /* ---- per-pixel-loss scratch ---- */ \
  X(PplLosses                      , "ppl.losses",                        Image    , Never) \
  X(PplLossMapScale                , "ppl.loss_map_scale",                Image    , Never) \
  X(PplRawLosses                   , "ppl.raw_losses",                    Image    , Never) \
  X(PplTotalLosses                 , "ppl.total_losses",                  Image    , Never) \
  X(PplVRawLosses                  , "ppl.v_raw_losses",                  Image    , Never) \
  /* ---- bilagrid rgb ---- */ \
  X(EngBgRgbGrids                  , "eng.bg.rgb.grids",                  Appearance, Always) \
  /* ---- bilagrid depth ---- */ \
  X(EngBgDepthGrids                , "eng.bg.depth.grids",                Appearance, Always) \
  /* ---- bilagrid normal ---- */ \
  X(EngBgNormalGrids               , "eng.bg.normal.grids",               Appearance, Always) \
  /* ---- bilagrid value-quant grids ---- */ \
  X(EngBgRgbGridsQ                 , "eng.bg.rgb.grids_q",                Appearance, Always)  /* +.q/.qb */ \
  X(EngBgDepthGridsQ               , "eng.bg.depth.grids_q",              Appearance, Always)  /* +.q/.qb */ \
  X(EngBgNormalGridsQ              , "eng.bg.normal.grids_q",             Appearance, Always)  /* +.q/.qb */ \
  /* ---- bilagrid rgb ---- */ \
  X(EngBgRgbPost                   , "eng.bg.rgb.post",                   Appearance, Never) \
  /* ---- bilagrid depth ---- */ \
  X(EngBgDepthPost                 , "eng.bg.depth.post",                 Appearance, Never) \
  /* ---- bilagrid normal ---- */ \
  X(EngBgNormalPost                , "eng.bg.normal.post",                Appearance, Never) \
  /* ---- bilagrid rgb ---- */ \
  X(EngBgRgbImageGrad              , "eng.bg.rgb.image_grad",             Appearance, Never) \
  /* ---- bilagrid depth ---- */ \
  X(EngBgDepthImageGrad            , "eng.bg.depth.image_grad",           Appearance, Never) \
  /* ---- bilagrid normal ---- */ \
  X(EngBgNormalImageGrad           , "eng.bg.normal.image_grad",          Appearance, Never) \
  /* ---- bilagrid depth ---- */ \
  X(EngBgDepthScalars              , "eng.bg.depth.scalars",              Appearance, Always) \
  X(EngBgDepthTmpScalars           , "eng.bg.depth.tmp_scalars",          Appearance, Never) \
  /* ---- bilagrid shared ---- */ \
  X(EngBgCamIndices                , "eng.bg.cam_indices",                Appearance, Never) \
  X(EngBgTvReadout                 , "eng.bg.tv_readout",                 Appearance, Never) \
  /* ---- background-SH ---- */ \
  X(EngBgSkyShCoeffs               , "eng.bg_sky.sh_coeffs",              Appearance, Always) \
  X(EngBgSkyG1                     , "eng.bg_sky.g1",                     Appearance, Resume) \
  X(EngBgSkyG2                     , "eng.bg_sky.g2",                     Appearance, Resume) \
  X(EngBgSkyImage                  , "eng.bg_sky.image",                  Appearance, Never) \
  X(EngBgSkyRgbPost                , "eng.bg_sky.rgb_post",               Appearance, Never) \
  X(EngBgSkyVTsScratch             , "eng.bg_sky.v_Ts_scratch",           Appearance, Never) \
  X(EngBgSkyVBg                    , "eng.bg_sky.v_bg",                   Appearance, Never) \
  X(EngBgSkyVSh                    , "eng.bg_sky.v_sh",                   Appearance, Never) \
  /* ---- PPISP ---- */ \
  X(EngPpispParams                 , "eng.ppisp.params",                  Appearance, Always) \
  X(EngPpispGrads                  , "eng.ppisp.grads",                   Appearance, Never) \
  X(EngPpispG1                     , "eng.ppisp.g1",                      Appearance, Resume) \
  X(EngPpispG2                     , "eng.ppisp.g2",                      Appearance, Resume) \
  X(EngPpispAccum                  , "eng.ppisp.accum",                   Appearance, Resume) \
  X(EngPpispRgbPost                , "eng.ppisp.rgb_post",                Appearance, Never) \
  X(EngPpispRegLosses              , "eng.ppisp.reg_losses",              Appearance, Never) \
  X(EngPpispRegRawLosses           , "eng.ppisp.reg_raw_losses",          Appearance, Never) \
  X(EngPpispVRegLosses             , "eng.ppisp.v_reg_losses",            Appearance, Never) \
  X(EngPpispVRegParams             , "eng.ppisp.v_reg_params",            Appearance, Never) \
  /* ---- color-shift reg ---- */ \
  X(EngColorShiftRegEma            , "eng.color_shift_reg.ema",           Appearance, Always) \
  X(EngColorShiftRegBatchSum       , "eng.color_shift_reg.batch_sum",     Appearance, Never) \
  /* ---- color space ---- */ \
  X(ColorSpaceSplatMatrix          , "color_space.splat_matrix",          Appearance, Always) \
  X(ColorSpaceImageMatrix          , "color_space.image_matrix",          Appearance, Always) \
  X(ColorSpaceFwdPost              , "color_space.fwd_post",              Image    , Never) \
  X(ColorSpaceBgSrgb               , "color_space.bg_srgb",               Image    , Never) \
  /* ---- ground-truth staging ---- */ \
  X(GtRgb                          , "gt.rgb",                            Image    , Never) \
  X(GtDepth                        , "gt.depth",                          Image    , Never) \
  X(GtNormal                       , "gt.normal",                         Image    , Never) \
  X(GtAlpha                        , "gt.alpha",                          Image    , Never) \
  X(GtStagingU8                    , "gt.staging_u8",                     Image    , Never) \
  X(GtStagingU16                   , "gt.staging_u16",                    Image    , Never) \
  /* ---- camera table ---- */ \
  X(CamViewmats                    , "cam.viewmats",                      Other    , Never) \
  X(CamIntrins                     , "cam.intrins",                       Other    , Never) \
  X(CamDistCoeffs                  , "cam.dist_coeffs",                   Other    , Never) \
  /* ---- warped-setup staging ---- */ \
  X(WarpInputIntrins               , "warp.input_intrins",                Other    , Never) \
  X(WarpInputDistCoeffs            , "warp.input_dist_coeffs",            Other    , Never) \
  X(WarpSourceModels               , "warp.source_models",                Other    , Never) \
  X(WarpSourceParams               , "warp.source_params",                Other    , Never) \
  X(WarpFaceAxes                   , "warp.face_axes",                    Other    , Never) \
  /* ---- debug ---- */ \
  X(DebugFeaturesDc                , "debug.features_dc",                 Other    , Never) \
  /* ---- bilagrid misc ---- */ \
  X(BilagridQuantileTemp           , "bilagrid.quantile_temp",            Other    , Never) \
  /* ---- misc scratch ---- */ \
  X(PpispVRawLosses                , "ppisp_v_raw_losses",                Other    , Never) \
  X(RdtedMaxDepth                  , "rdted_max_depth",                   Other    , Never) \
  X(SemiOffloadedAdamBuf           , "semi_offloaded_adam_buf",           Other    , Never) \
  X(SsimScalar                     , "ssim_scalar",                       Other    , Never) \
  X(SsimMaskWeight                 , "ssim.mask_weight",                  Image    , Never) \
  X(SsimMaskWeightTmp              , "ssim.mask_weight_tmp",              Image    , Never) \
  /* ---- densify scratch ---- */ \
  X(DensifyQuantileTemp            , "densify_quantile_temp",             Other    , Never) \
  X(DensifyMapNorm                 , "densify_map_norm",                  Other    , Never) \
  X(DensifyScoreGather             , "densify_score_gather",              Other    , Never) \
  X(DensifyScoreClip               , "densify_score_clip",                Other    , Never) \
  X(DensifyClipScale               , "densify_clip_scale",                Other    , Never) \
  X(DensifyUpdateWeight            , "densify_update_weight",             Other    , Never) \
  X(DensifyWswrSortingValues       , "densify_wswr_sorting_values",       Other    , Never) \
  X(DensifyWswrOutIdx              , "densify_wswr_out_idx",              Other    , Never) \
  X(DensifyWswrKeysOut             , "densify_wswr_keys_out",             Other    , Never) \
  X(DensifyWswrIndicesIn           , "densify_wswr_indices_in",           Other    , Never) \
  X(DensifyWswrIndicesOut          , "densify_wswr_indices_out",          Other    , Never) \
  X(DensifyRelocMask               , "densify_reloc_mask",                Other    , Never) \
  X(DensifyRelocCount              , "densify_reloc_count",               Other    , Never) \
  X(DensifyRelocDstIndices         , "densify_reloc_dst_indices",         Other    , Never) \
  X(DensifyMcmcSampleProbs         , "densify_mcmc_sample_probs",         Other    , Never) \
  X(DensifyMcmcSampleProbsCumsum   , "densify_mcmc_sample_probs_cumsum",  Other    , Never) \
  X(DensifyMcmcIndexMap            , "densify_mcmc_index_map",            Other    , Never) \
  X(DensifyMcmcNIdxBuffer          , "densify_mcmc_n_idx_buffer",         Other    , Never) \
  X(DensifyMcmcAddSampleProbs      , "densify_mcmc_add_sample_probs",     Other    , Never) \
  X(DensifyMcmcAddSampleProbsCumsum, "densify_mcmc_add_sample_probs_cumsum",Other    , Never) \
  X(DensifyMcmcAddIndexMap         , "densify_mcmc_add_index_map",        Other    , Never) \
  X(DensifyMcmcAddNIdxBuffer       , "densify_mcmc_add_n_idx_buffer",     Other    , Never) \
  X(DensifyRobustResid             , "densify_robust_resid",              Other    , Never) \
  X(DensifyTukeyC                  , "densify_tukey_c",                   Other    , Never) \
  /* ---- viewer cache/scratch ---- */ \
  X(ViewerThumb                    , "viewer.thumb",                      Viewer   , Never) \
  X(ViewerThumbDone                , "viewer.thumb_done",                 Viewer   , Never) \
  X(ViewerCapIdx                   , "viewer.cap_idx",                    Viewer   , Never) \
  X(ViewerMinMax                   , "viewer.min_max",                    Viewer   , Never) \
  X(ViewerRootAabb                 , "viewer.root_aabb",                  Viewer   , Never) \
  X(ViewerLss                      , "viewer.lss",                        Viewer   , Never) \
  X(ViewerTri                      , "viewer.tri",                        Viewer   , Never) \
  X(ViewerLssBvhNodes              , "viewer.lss_bvh.nodes",              Viewer   , Never) \
  X(ViewerLssBvhAabb               , "viewer.lss_bvh.aabb",               Viewer   , Never) \
  X(ViewerTriBvhNodes              , "viewer.tri_bvh.nodes",              Viewer   , Never) \
  X(ViewerTriBvhAabb               , "viewer.tri_bvh.aabb",               Viewer   , Never) \
  X(ViewerIntrins                  , "viewer.intrins",                    Viewer   , Never) \
  X(ViewerWidths                   , "viewer.widths",                     Viewer   , Never) \
  X(ViewerHeights                  , "viewer.heights",                    Viewer   , Never) \
  X(ViewerCmodels                  , "viewer.cmodels",                    Viewer   , Never) \
  X(ViewerDist                     , "viewer.dist",                       Viewer   , Never) \
  X(ViewerDistTier                 , "viewer.dist_tier",                  Viewer   , Never) \
  X(ViewerC2w                      , "viewer.c2w",                        Viewer   , Never) \
  X(ViewerOverlaySegs              , "viewer.overlay_segs",               Viewer   , Never) \
  X(ViewerOverlayColors            , "viewer.overlay_colors",             Viewer   , Never) \
  /* ---- sync visualizer scratch ---- */ \
  X(VisLss                         , "vis.lss",                           Viewer   , Never) \
  X(VisTri                         , "vis.tri",                           Viewer   , Never) \
  X(VisMinMax                      , "vis.min_max",                       Viewer   , Never) \
  X(VisRootAabb                    , "vis.root_aabb",                     Viewer   , Never) \
  /* ---- bilagrid rgb ---- */ \
  X(EngBgRgbG1                     , "eng.bg.rgb.g1",                     Appearance, Resume) \
  X(EngBgRgbG2                     , "eng.bg.rgb.g2",                     Appearance, Resume) \
  X(EngBgRgbAccum                  , "eng.bg.rgb.accum",                  Appearance, Resume) \
  X(EngBgRgbBgAg                   , "eng.bg.rgb.bg_ag",                  Appearance, Resume)  /* +.q/.qb */ \
  X(EngBgRgbBgQuant                , "eng.bg.rgb.bg_quant",               Appearance, Resume)  /* +.q/.qb */ \
  /* ---- bilagrid depth ---- */ \
  X(EngBgDepthG1                   , "eng.bg.depth.g1",                   Appearance, Resume) \
  X(EngBgDepthG2                   , "eng.bg.depth.g2",                   Appearance, Resume) \
  X(EngBgDepthAccum                , "eng.bg.depth.accum",                Appearance, Resume) \
  X(EngBgDepthBgAg                 , "eng.bg.depth.bg_ag",                Appearance, Resume)  /* +.q/.qb */ \
  X(EngBgDepthBgQuant              , "eng.bg.depth.bg_quant",             Appearance, Resume)  /* +.q/.qb */ \
  /* ---- bilagrid normal ---- */ \
  X(EngBgNormalG1                  , "eng.bg.normal.g1",                  Appearance, Resume) \
  X(EngBgNormalG2                  , "eng.bg.normal.g2",                  Appearance, Resume) \
  X(EngBgNormalAccum               , "eng.bg.normal.accum",               Appearance, Resume) \
  X(EngBgNormalBgAg                , "eng.bg.normal.bg_ag",               Appearance, Resume)  /* +.q/.qb */ \
  X(EngBgNormalBgQuant             , "eng.bg.normal.bg_quant",            Appearance, Resume)  /* +.q/.qb */



// ---- Generated enum -------------------------------------------------------
enum class PoolSlot : uint16_t {
#define X(name, str, cat, save) name,
    POOL_SLOT_TABLE(X)
#undef X
    Count
};


// ---- Generated metadata array ---------------------------------------------
struct SlotMeta {
    const char*  name;
    VramCategory cat;
    SaveClass    save;
};

inline constexpr SlotMeta kSlotMeta[] = {
#define X(name, str, cat, save) SlotMeta{ str, VramCategory::cat, SaveClass::save },
    POOL_SLOT_TABLE(X)
#undef X
};


// ---- Compile-time invariants ----------------------------------------------
// (1) exactly one metadata row per enumerator.
static_assert(sizeof(kSlotMeta) / sizeof(kSlotMeta[0]) == (size_t)PoolSlot::Count,
              "POOL_SLOT_TABLE: metadata count must match PoolSlot::Count "
              "(did an X-macro expansion diverge?)");

// (2) every pool name string is unique (constexpr O(n^2) scan; n is small).
constexpr bool ce_streq(const char* a, const char* b) {
    while (*a && *b) { if (*a != *b) return false; ++a; ++b; }
    return *a == *b;
}
constexpr bool ce_slot_names_unique() {
    const size_t n = (size_t)PoolSlot::Count;
    for (size_t i = 0; i < n; ++i)
        for (size_t j = i + 1; j < n; ++j)
            if (ce_streq(kSlotMeta[i].name, kSlotMeta[j].name))
                return false;
    return true;
}
static_assert(ce_slot_names_unique(),
              "POOL_SLOT_TABLE: two rows share a pool name string");


// ---- Metadata accessors ---------------------------------------------------
constexpr const SlotMeta& slot_meta(PoolSlot s) {
    return kSlotMeta[(size_t)s];
}
constexpr const char*  slot_name(PoolSlot s)     { return slot_meta(s).name; }
constexpr VramCategory slot_category(PoolSlot s) { return slot_meta(s).cat;  }
constexpr SaveClass    slot_save(PoolSlot s)     { return slot_meta(s).save; }

// Human-readable tags (for the VRAM breakdown printout and checkpoint manifest).
constexpr const char* to_string(VramCategory c) {
    switch (c) {
        case VramCategory::Splat:      return "splat";
        case VramCategory::SplatXImg:  return "splat x img";
        case VramCategory::Image:      return "image";
        case VramCategory::Appearance: return "appearance";
        case VramCategory::Viewer:     return "viewer";
        case VramCategory::Other:      return "other";
        default:                       return "?";
    }
}
constexpr const char* to_string(SaveClass s) {
    switch (s) {
        case SaveClass::Never:  return "never";
        case SaveClass::Resume: return "resume";
        case SaveClass::Always: return "always";
        default:                return "?";
    }
}


// ---- Aliasing: phases that share one arena --------------------------------

// A slot listed in POOL_ALIAS_TABLE below owns no storage: every phase carves
// out of ONE arena, so it costs the largest phase and not the sum. Adding a
// row is a lifetime claim -- docs/notes/vram-splat-x-img.md says how to test it.
enum class PoolPhase : uint8_t {
    None = 0,   // owns its allocation -- every slot not listed below
    TileIsect,  // scratch that dies inside do_intersect_tile_generic
    RasterBwd,  // raster backward -> projection backward / fused optim step
    Count
};

// X(PoolSlot enumerator, PoolPhase enumerator)
#define POOL_ALIAS_TABLE(X) \
  /* The sort keys and tile counts: no caller reads the key array the \
     intersector returns, and the counts never leave it. */ \
  X(IsectIdsA          , TileIsect) \
  X(IsectIdsB          , TileIsect) \
  X(IsectTilesPerSplat , TileIsect) \
  X(IsectCumTiles      , TileIsect) \
  /* Screen-space gradients: written by the raster backward, consumed by the \
     projection backward, or stashed for the fused optim step -- all before \
     the next forward's intersection. forward_3dgs drops the stashed view. */ \
  X(RasterBwdVScreen   , RasterBwd)

struct AliasRow { PoolSlot slot; PoolPhase phase; };
inline constexpr AliasRow kAliasRows[] = {
#define X(name, ph) AliasRow{ PoolSlot::name, PoolPhase::ph },
    POOL_ALIAS_TABLE(X)
#undef X
};

constexpr PoolPhase slot_phase(PoolSlot s) {
    for (const AliasRow& r : kAliasRows)
        if (r.slot == s) return r.phase;
    return PoolPhase::None;
}

constexpr bool ce_alias_rows_unique() {
    const size_t n = sizeof(kAliasRows) / sizeof(kAliasRows[0]);
    for (size_t i = 0; i < n; ++i)
        for (size_t j = i + 1; j < n; ++j)
            if (kAliasRows[i].slot == kAliasRows[j].slot) return false;
    return true;
}
static_assert(ce_alias_rows_unique(),
              "POOL_ALIAS_TABLE: a slot is listed twice");

constexpr const char* to_string(PoolPhase p) {
    switch (p) {
        case PoolPhase::None:      return "none";
        case PoolPhase::TileIsect: return "tile-isect";
        case PoolPhase::RasterBwd: return "raster-bwd";
        default:                   return "?";
    }
}


// ---- Sub-index scheme -----------------------------------------------------
// A single logical slot may back a bounded number of physical allocations
// (a QuantizedTensor* needs packed bytes + per-block bounds). The pool is keyed
// by a PoolKey = (PoolSlot << kSubBits) | sub, so all sub-allocations of a slot
// share its metadata and sit adjacently in the (future) flat-array pool.
using PoolKey = uint32_t;
static constexpr uint32_t kSubBits = 4;                 // <= 16 sub-slots/slot
static constexpr uint32_t kSubMask = (1u << kSubBits) - 1;

// Conventional sub-indices. kSubPacked/kSubBounds are DISTINCT from kSubMain so
// a codec's two allocations print with the historical ".q"/".qb" suffixes while
// a plain single-allocation slot prints with no suffix -- keeping the
// reconstructed breakdown names byte-identical to the old string keys (so the
// Python VRAM bucketing keeps working until it is switched to categories).
static constexpr uint32_t kSubMain   = 0;   // the (only) allocation of a slot
static constexpr uint32_t kSubPacked = 1;   // QuantizedTensor*: packed bytes
static constexpr uint32_t kSubBounds = 2;   // QuantizedTensor*: per-block bounds

constexpr PoolKey  pool_key(PoolSlot s, uint32_t sub = kSubMain) {
    return ((PoolKey)s << kSubBits) | (sub & kSubMask);
}
constexpr PoolSlot pool_key_slot(PoolKey k) { return (PoolSlot)(k >> kSubBits); }
constexpr uint32_t pool_key_sub (PoolKey k) { return k & kSubMask; }

// Size of a dense (PoolSlot x sub) index space -- the flat pool array in
// Phase 2 is allocated with this many entries.
static constexpr uint32_t kNumPoolKeys = (uint32_t)PoolSlot::Count << kSubBits;

// Display suffix for a sub-index (appended to slot_name in the VRAM report /
// checkpoint filenames). "" for the main slot, ".q"/".qb" for codec sub-slots.
constexpr const char* sub_suffix(uint32_t sub) {
    return sub == kSubPacked ? ".q" : (sub == kSubBounds ? ".qb" : "");
}


// ---- Escape hatch (defined by the pool, Phase 2) --------------------------
// For Never-saved scratch whose key is built from runtime-variable content and
// therefore cannot have a compile-time PoolSlot. It attaches a VramCategory so
// the buffer still lands in the right bucket. Declared here for discoverability;
// implemented alongside DevicePool.
//   void* pool_acquire_dynamic(VramCategory cat, const char* debug_name,
//                              size_t bytes);
