# `spirula geometry`

Writes per-image normal maps (and optionally depth maps) for a dataset with a
monocular geometry model, MoGe-2 by default or Metric3D v2. The trainer reads
them through `--load-normals` and `--load-depths`. Vulkan builds with
`SS_BUILD_SAM=ON` only.

```text
spirula geometry <dataset> [options]
spirula geometry --check
```

`<dataset>` is a folder with a `transforms.json`, a COLMAP `sparse/` model or
a Metashape export. Normals go to `normals/` and depth to `depths/` inside it,
where the dataset readers look; `transforms.json` is not rewritten. A run
continues where the last one stopped unless `--overwrite`.

| flag | default | meaning |
|---|---|---|
| `--image-dir <dir>` | `images` | The images, relative to the dataset or absolute. |
| `--model <id\|file>` | `moge2-vitb` | `moge2-vits`, `moge2-vitb`, `moge2-vitl`, `metric3d-vit-small`, `metric3d-vit-large`, `metric3d-vit-giant2`, or a path to an `.onnx` file. A known id is downloaded and cached. |
| `--max-size <n>` | 1064 | Longest side of the maps written, and of one image the network runs. |
| `--num-tokens <n>` | 3600 | Patches MoGe's transformer runs (its range is 1200 to 3600); sets its cost. Metric3D ignores it. |
| `--depth` | off | Also write depth maps. |
| `--no-normal` | | Do not write normal maps. |
| `--normal-format png\|jpg` | `png` | File type for the normal maps; `jpg` is about a fifth the size. |
| `--jpeg-quality <n>` | 95 | JPEG quality when the format is `jpg`. |
| `--depth-units relative\|mm` | `relative` | `relative` fills the 16 bits with the scene, scaled by its 99.9th percentile; `mm` stores millimetres (up to 65.5 m). |
| `--ray-depth auto\|yes\|no` | `auto` | Store distance along the ray rather than z. `auto` does so exactly when the frame was split into pinhole faces. |
| `--split auto\|yes\|no` | `auto` | Split a wide frame into several pinhole faces instead of undistorting it to one. `auto` splits a panorama always, and a fisheye when one pinhole would keep less than three quarters of it. |
| `--face-res output\|source` | `output` | How finely a split samples the frame: `output` sizes each face to the map it fills, `source` to the frame's own resolution (up to `--max-size`). |
| `--overwrite` | off | Recompute maps already on disk. |
| `--image-gamut <name>` | the file's own | Colour primaries of the images; they convert to sRGB before inference. |
| `--image-linear` / `--no-image-linear` | the file's own | Treat the images as linear light, or as display-encoded. |
| `--image-exposure auto\|<stops>` | | Brighten what the model sees, in linear light, without changing the files. |
| `--device <index\|name\|auto\|uuid:hex>` | `auto` | Vulkan device; wins over `SS_VK_DEVICE`. |
| `--check` | | Run the camera round-trip self-test and exit. |
| `-h`, `--help` | | Show help and exit. |

Exit status is 0 on success, 1 when the run fails and 2 for a usage error.
