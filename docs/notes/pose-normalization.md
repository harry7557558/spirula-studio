# Pose normalization: orientation, centering, scale, and train_frame

`orientation_method`, `center_method` and `auto_scale_poses` are accepted by
both trainers but only *partially* implemented natively. This note records
what the C++ does, what it does not, and where the reference implementation
is, so the port does not have to be reverse-engineered from a diff.

## Status

| config | native behaviour |
|---|---|
| `orientation_method` | always `up`; anything else warns and is approximated |
| `center_method` | always `poses`; ditto |
| `auto_scale_poses` | effectively always on — the scale is folded into `train_frame_scale` |
| `train_frame` | only `points`; `normalized` / `camera` are rejected by `check_config()` |

**Where the frame now comes from.** `spirula sfm` applies this same
similarity to the model before writing it (`src/sfm/map/Orient.h`,
`--no-orient` to decline), so for a dataset this repository reconstructed, the
"raw SfM frame" below already *is* the normalized frame: `train_frame_scale`
comes out 1 and, when the model was levelled on the cameras' up,
`train_to_normalized` the identity. A model levelled on the ground
(`gauge.txt` `up ground`) keeps a rotation in it with the scale still 1, so
nothing may treat a unit scale as an identity transform. Everything in this note
still describes what the trainer does — it is simply being handed a scene that
has nothing left to normalize. Datasets from COLMAP, Metashape or nerfstudio
are unchanged and still arrive in whatever gauge their producer chose.

Because `train_frame="points"` keeps splats in the raw SfM frame, the
orientation/centering choice does **not** move the training coordinates. It
only changes `train_frame_scale` (and hence the LR / regularizer rescaling
that hangs off it) and `train_to_normalized`, the similarity the web viewer
uses to place its default camera. That is why the native trainer can warn and
carry on rather than refuse: the approximation is real but bounded.

`src/data/parsers/DatasetCommon.cpp::compute_normalized_transform` is the C++
side. It implements the `up`/`poses` path only.

## Reference implementation

- `reference/python/camera_utils.py` — the algorithms
  (`auto_orient_and_center_poses` for pca/up/vertical/none x poses/focus/none,
  `orient_and_center_poses_gsplat` for the `gsplat` value of both). That module
  is on no code path; it exists for this.
- The call site below — how those outputs become `transform_matrix`,
  `scale_factor`, `train_frame_scale` and `train_to_normalized_transform`.
  This half is at least as fiddly as the algorithms, and it is the part a
  reimplementation is most likely to get subtly wrong.

## The original call site

Verbatim from `spirulae_splat/modules/dataparser.py::_parse_nerfstudio_data`
at commit `af96993`, the last commit before the Python parser was deleted
(`git show af96993:spirulae_splat/modules/dataparser.py` for the full file).
Reproduced rather than paraphrased so it cannot drift from what was actually
verified against the native parser.

```python
        poses = torch.from_numpy(np.array(poses).astype(np.float32))
        if self.config.center_method == "gsplat" or self.config.orientation_method == "gsplat":
            poses, transform_matrix, scene_scale = camera_utils.orient_and_center_poses_gsplat(
                poses.numpy(), metadata['points3D_xyz'].numpy()
            )
            poses = torch.from_numpy(poses).float()
            transform_matrix = torch.from_numpy(transform_matrix).float()[:3, :]
            scale_factor = 1.0 / scene_scale  # TODO: this messes up scale_reg
        else:
            poses, transform_matrix = camera_utils.auto_orient_and_center_poses(
                poses,
                method=self.config.orientation_method,
                center_method=self.config.center_method,
            )
            scale_factor = 1.0
            if self.config.auto_scale_poses:
                scale_factor /= float(torch.max(torch.abs(poses[:, :3, 3])))

        transform_matrix_scale = np.abs(np.cbrt(np.linalg.det(transform_matrix[:3, :3])))
        transform_matrix /= transform_matrix_scale
        poses[:, :3, 3] /= transform_matrix_scale
        scale_factor *= transform_matrix_scale

        # Normalize all poses to (N, 4, 4) for the train-frame branching below.
        # auto_orient_and_center_poses returns (N, 3, 4); gsplat returns (N, 4, 4).
        if poses.shape[-2] == 3:
            bottom = torch.tensor([[[0, 0, 0, 1]]], dtype=poses.dtype).expand(len(poses), 1, 4)
            poses = torch.cat([poses, bottom], dim=1)

        # Apply transformation to 3D points (orient/center only; scale handled per train_frame)
        metadata['points3D_xyz'] = (
            torch.cat(
                (
                    metadata['points3D_xyz'],
                    torch.ones_like(metadata['points3D_xyz'][..., :1]),
                ),
                -1,
            )
            @ transform_matrix.T
        )

        # Build the similarity that maps a point in the would-be camera-frame
        # into the normalized frame:  p_n = scale_factor * (R_t @ p_c + t_t).
        transform_4x4 = torch.eye(4, dtype=transform_matrix.dtype)
        transform_4x4[:3, :] = transform_matrix
        T_n_from_camera = torch.eye(4, dtype=transform_matrix.dtype)
        T_n_from_camera[:3, :3] = transform_matrix[:3, :3] * scale_factor
        T_n_from_camera[:3, 3] = transform_matrix[:3, 3] * scale_factor

        applied_transform_4x4 = None
        if "applied_transform" in meta:
            at = torch.tensor(meta["applied_transform"], dtype=transform_matrix.dtype)
            applied_transform_4x4 = torch.eye(4, dtype=transform_matrix.dtype)
            applied_transform_4x4[:3, :] = at

        train_frame = self.config.train_frame
        if train_frame == "normalized":
            # Current behavior: poses and points get the scale_factor applied.
            poses[:, :3, 3] *= scale_factor
            metadata['points3D_xyz'] *= scale_factor
            train_frame_scale = 1.0
            train_to_normalized_transform = torch.eye(4, dtype=transform_matrix.dtype)
        elif train_frame in ("camera", "points"):
            # Undo the rigid orient+center on poses and points (already applied
            # by auto_orient/gsplat above) so they're back in the camera frame.
            transform_4x4_inv = torch.linalg.inv(transform_4x4)
            poses = transform_4x4_inv.unsqueeze(0) @ poses
            homog = torch.cat(
                (metadata['points3D_xyz'],
                 torch.ones_like(metadata['points3D_xyz'][..., :1])), -1)
            metadata['points3D_xyz'] = (homog @ transform_4x4_inv.T)[..., :3]
            T_train_from_camera = torch.eye(4, dtype=transform_matrix.dtype)
            if train_frame == "points" and applied_transform_4x4 is not None:
                applied_inv = torch.linalg.inv(applied_transform_4x4)
                poses = applied_inv.unsqueeze(0) @ poses
                homog = torch.cat(
                    (metadata['points3D_xyz'],
                     torch.ones_like(metadata['points3D_xyz'][..., :1])), -1)
                metadata['points3D_xyz'] = (homog @ applied_inv.T)[..., :3]
                T_train_from_camera = applied_inv
            # scale_factor is NOT applied to poses/points in this branch.
            train_frame_scale = 1.0 / scale_factor if scale_factor != 0 else 1.0
            # c2w_train = (T_n)^-1 @ c2w_normalized   for camera mode;
            # c2w_train = (T_n @ applied)^-1 @ c2w_normalized   for points mode.
            T_n_from_train = T_n_from_camera @ torch.linalg.inv(T_train_from_camera)
            train_to_normalized_transform = torch.linalg.inv(T_n_from_train)
        else:
            raise ValueError(f"Unknown train_frame: {train_frame}")

        all_dataparser_outputs = []
        for split_id, indices in enumerate([i_train, i_eval]):

            # Choose image_filenames and poses based on split, but after auto orient and scaling the poses.
            image_filenames_split = [image_filenames[i] for i in indices]
```

## Notes for whoever ports this

- `transform_matrix_scale` (the cube root of the rotation block's determinant)
  is divided out of the transform and folded into `scale_factor`. The gsplat
  path returns a non-unit-determinant matrix; the up/poses path does not, so
  this is a no-op there. Getting it wrong scales the whole scene.
- The `gsplat` branch sets `scale_factor = 1 / scene_scale` and carries a
  `# TODO: this messes up scale_reg` comment from before `train_frame_scale`
  existed. Worth re-checking rather than porting the comment.
- `train_frame="points"` additionally undoes nerfstudio's `applied_transform`
  so poses and points land back in the original SfM frame. The native parser
  does implement this (`NerfstudioParser.cpp`); it is only the
  orientation/centering *choice* that is missing.
- The `train_frame_scale = 1 / scale_factor` relationship is what the engine
  uses to rescale `means_lr`, `scale_reg`, `max_world_size` and `noise_lr`.
  A port that changes `scale_factor` changes effective learning rates.
