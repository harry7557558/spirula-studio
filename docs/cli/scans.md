# `spirula e57` and `spirula lidar`

Two ways to use a laser scan. `e57` turns an E57 file into a dataset by itself,
with no structure from motion. `lidar` moves an existing reconstruction into a
scan's frame and gives it the scan's geometry. `docs/datasets.md` ("E57 laser
scans") and `docs/notes/lidar-alignment.md` have the details.

## `spirula e57`

```text
spirula e57 <scan.e57> [<dataset folder>] [options]
```

Writes the images the scan carries, their poses and a thinned copy of its
point cloud as a dataset the trainer opens directly: `images/`,
`transforms.json` and `sparse_pc.ply`. The folder defaults to one beside the
scan, named after it.

| flag | default | meaning |
|---|---|---|
| `--points <n>\|all` | 500000 | Thin the point cloud to about this many seed points, one per voxel. `all` keeps every point; 0 writes no cloud. |
| `--no-depth` | | Do not write depth and normal maps from the scan. |
| `--no-pinhole` | | Leave out the pinhole images. |
| `--no-panoramas` | | Leave out the spherical panoramas. |
| `--overwrite` | off | Write into a folder that is not empty. The dataset's own files are replaced; nothing else is removed. |
| `--info` | | List what the file holds and write nothing. |
| `-h`, `--help` | | Show help and exit. |

Exit status: 0 on success, 1 when the run fails or is cancelled, 2 for a
usage error.

## `spirula lidar`

```text
spirula lidar <dataset folder> --cloud <scan> [options]
spirula lidar --extract <scan.e57> <dataset folder> [<subfolder>]
spirula lidar --render-anchors <scan>... <dataset folder>
spirula lidar --info <scan>
```

The first form fits the reconstruction in the dataset folder to the scan
(rotation, translation and scale, from the scan's images where they were part
of the reconstruction, then from its surface) and writes it back as one COLMAP
model whose seed points are the scan's own, each listing the images that see
it. Depth and normal maps are rendered from the scan for every image. The
original reconstruction is kept in `sparse_unaligned/`. A point-cloud path
(`.e57`, `.las`, `.laz`, `.ply`) given without `--cloud` is taken as one too.

| flag | default | meaning |
|---|---|---|
| `--cloud <file>` | required | A point cloud in the scan's frame (`.e57`, `.las`, `.ply`). Repeat for several. |
| `--mode <m>` | `auto` | `auto` fits the anchors, then the surface; `anchors` fits the anchors alone; `keep` leaves a model already in the scan's frame where it is; `refine` fits the surface from where the model is. |
| `--points <n>\|all` | 500000 | Seed points kept from the scan, thinned by voxels; `all` keeps every one, 0 keeps only the model's own points. |
| `--track-cap <n>` | 12 | Most images listed in one scan point's track, 1 to 64. |
| `--no-depth` | | Do not render depth and normal maps. |
| `--no-gap-points` | | Drop the reconstruction's own points, even where the scan has nothing. |
| `--anchors <file>` | `lidar/anchors.json` in the dataset | The anchors file. |
| `--image-dir <dir>` | `images` | Where the images are, relative to the dataset or absolute. |
| `--mask-dir <dir>` | `masks` | Where the masks are, relative to the dataset or absolute. Depth and normals are left blank where a mask removes the image. |
| `--scan-frames auto\|shared\|separate` | `auto` | Whether the clouds share one frame: `shared` (one registration wrote them), `separate` (each is placed through its images in the reconstruction), or `auto`: shared unless two files put their scanner in the same place. |
| `--flip-masks` | off | The dataset's masks are white where an image is not kept. |
| `--overwrite` | off | Align again even when nothing it depends on has changed. |
| `--scanner-poses` | off | Use no reconstruction: place the scan's photographs where the scanner recorded them. |
| `-h`, `--help` | | Show help and exit. |

The other forms do one job and exit:

| flag | meaning |
|---|---|
| `--extract <scan.e57> <dataset> [<subfolder>]` | Write the images an E57 file carries into `<dataset>/images/<subfolder>` (default: the scan's name) and record their poses in `<dataset>/lidar/anchors.json`, so a reconstruction that includes them can be aligned by them. |
| `--render-anchors <scan>... <dataset>` | For a scan with no photographs: render views of it from where the scanner stood into `<dataset>/images/scan_views`, with their poses recorded as anchors. |
| `--info <file>` | Describe a point-cloud file (format, point count, colour, scanner stations). |

Exit status: 0 on success, 1 when the run fails or is cancelled, 2 for a
usage error or a bad value.
