# Experimental global SfM mapper

`spirula sfm auto IMAGES/ -o WORKSPACE/ --mapper global` selects the global
initialization. `spirula sfm map` accepts the same option for an existing
feature and match database. The usual finishing, gauge and export stages follow.

The mapper bootstraps camera groups using the existing calibration machinery,
then fits unknown pinhole focal lengths jointly over fundamental matrices
re-estimated from verified correspondences. Fixed focal priors are held constant.
Relative poses are re-estimated using those intrinsics. Rotation averaging
initializes from a spanning forest ranked by triangle consistency and inlier
count, then solves L1 regression through ADMM followed by Geman--McClure IRLS over coupled
geodesic updates and rejects inconsistent edges. Components are solved
independently.

Accepted matches must also pass cheirality and a minimum pairwise parallax test.
They are joined into multi-view tracks. Nearby features from the same image are
reduced to the one supported by the most matches; more distant conflicts are
split through a quality-ranked forest. After positioning, observations inconsistent with the
estimated rays and tracks with insufficient triangulation angle are removed.
The angular threshold is 1 degree with a focal prior and 2 degrees without one,
matching the corresponding GLOMAP filtering stages.

The Vulkan positioning solver minimizes a robust camera-ray cost over camera
centres, points and a positive scale per observation. It uses LM and an implicit
Jacobi-preconditioned conjugate-gradient solve, with a fixed frame centre until
absolute centre factors free the translation gauge. An observation scale is
fixed when neither calibrated rig baselines nor metric centre factors set it.
After an initial visual solve, GPS and absolute sensor factors are fitted in
that model gauge and solved as robust, anisotropic centre constraints; the CPU
reference and Vulkan implementation use the same factors.
A Schur reduction eliminates observation scales;
the remaining system includes camera centres and points. Per-observation and
vector operations run on the GPU in fp32; camera contributions are reduced
cooperatively, while each point is accumulated by one thread. Conjugate-gradient
reductions and convergence checks run on the GPU; indirect dispatch suppresses
post-convergence PCG kernels, while the host controls the outer LM iterations. A
double-precision CPU reference is used in tests. The resulting `Reconstruction`
is passed to the existing bundle adjustment, assembly, recovery and export.
Each initial refinement round runs BA with frame rotations fixed, followed by
joint BA; both stages retain the available pose priors.

This is an experimental implementation, not a port or a validated equivalent
of GLOMAP. Its current limits matter when choosing a test capture:

- Positioning has not been profiled on large scenes or validated on a physical
  GPU. The reduced system still includes points, and fp32 arithmetic may limit
  precision.
- View-graph calibration only adjusts focal length for pinhole-family cameras;
  it has not been validated on real captures or against GLOMAP. Planar and
  panoramic pairs use rotation-only RANSAC first, then calibrated homography
  decomposition and cheirality to recover a translation when rotation-only
  support is insufficient.
- Rotation averaging currently runs on the CPU and does not yet use gravity
  constraints.
- Wrong bridges without cycles cannot be detected by rotation cycle checks;
  repeated structure and long, weakly connected sequences need real-data tests.
- A track with distant conflicts is split by a quality-ranked forest that keeps
  one feature per image. The union-find state is compacted to matched features;
  temporary endpoints and links still scale with all verified inliers.
- Fixed, supplied rig extrinsics constrain rotation averaging and positioning;
  known baselines set scale in the supplied extrinsics' units. IMU relative
  rotations can add missing rotation-graph edges, while absolute centre factors
  from GPS or another sensor constrain a second global-positioning solve.
  Refinable rig extrinsics and real GPS/IMU captures have not been validated
  end to end.

The `sfm_global_*`, `sfm_rotation_average_test` and
`sfm_relative_view_graph_test` tests exercise the stages with synthetic data,
including outliers and disconnected graphs. Synthetic success is not evidence
of parity with GLOMAP or of quality on real image collections.
`sfm_fixed_rotation_ba_test`, also with `--vulkan`, checks dense and CG BA
with pose priors, fixed rotations and a subsequent free solve.
