# Marker detection and alignment

The editor's Transform tab has a Fiducial markers section. Select a marker
type (or Auto detect), set detection downsampling, and press Detect markers.
The job runs off the UI thread; its progress bar has a Cancel button.
Cancellation and unsuccessful scans preserve previous results. Reload markers
reads an externally generated `markers.json` without reopening the editor.
Feedback appears inside this section. Marker centres, outlines, IDs and the
proposed +X/+Y/+Z axes are drawn in the viewport while the Transform tab is
open; Show markers and axes toggles the overlay. X is red, Y green, Z blue.

Initial IDs use a wide triangle with its normal facing the scene's up direction.
A valid manual selection survives reloads. The preview follows changes to the
IDs, model placement, alignment and undo/redo. Adjust the suggested IDs to pick
the intended physical origin and ground plane.

For trained PLYs, dataset discovery follows the run's `config.json`, including
relative dataset paths, image directories and reconstruction selection. The
run's `scene_transform.json` maps dataset markers into the trained model's frame.
Nearby reconstruction folders also work. A loose model without an associated
dataset shows an explanation and disables the marker controls.

Each marker needs observations from at least two registered cameras. Photos
outside the reconstruction do not contribute. Detection reports unreadable
photos separately. Downsampling 1 uses the full image; larger values run faster
but can miss small tags. Results with mean reprojection error over four
calibrated pixels or centres behind their observing cameras are rejected.

Supported types are AprilTag `tag36h10` / `tag36h11`, ArUco `4x4`, `5x5`, `6x6`,
`7x7` (the OpenCV `_1000` dictionaries, also containing the `_50`, `_100` and
`_250` subsets), and `aruco_original`. ArUco decoding requires an exact code
match. Auto selects the family with the greatest support from triangulated
markers; it never merges numeric IDs across dictionaries. `markers.json`
records each marker's family; older files without it remain readable.

Choose the origin, +X and XY-plane markers from the available ID lists.
They must be three distinct, non-collinear centres. X distance is the measured
distance between the origin and +X markers in metres; leave it at zero to
preserve the scene's existing scale. Choosing IDs defines the coordinate frame,
so the initial suggestion remains adjustable rather than identifying a physical
origin or floor from the tag IDs alone.

The CLI uses the same detection pipeline:

```bash
./build_vulkan/spirula marker detect /path/to/dataset --family auto
./build_vulkan/spirula marker detect /path/to/dataset --family aruco4x4_1000 --decimate 1
```

For PennCOSYVIO `tango_top/seq_af` dataset, the supplied JSON has 32 IDs,
including 11, 70, 210 and 211, but no 0, 1 or 2. The original GUI also used
“No markers found” for a failed solve with these default IDs. Opening
`sparse/0/points3D.bin` could additionally miss the JSON two directories above.
The editor now resolves that dataset root and reports invalid ID selections
separately. A full-resolution Auto scan of its 116 registered images finds
`tag36h11`; its detected centres can solve the three-marker alignment.

For a private dataset, Auto finds `aruco4x4_1000`. Its 322 registered
images yield two valid triangulated markers, IDs 231 and 412. That confirms
ArUco detection, but this reconstruction cannot yet define the three-marker
coordinate frame. More registered views of a third marker are needed.

The native `marker_detection_test` checks independent OpenCV bit patterns at
all four rotations, automatic family detection, sparse-root lookup, JSON family
round-tripping, unreadable images and cancellation. Optional dataset arguments
run read-only scans and alignment checks:

```bash
./build_vulkan/marker_detection_test /path/to/dataset
```

The vendored ArUco tables and license notices are documented in
`src/external/apriltag/README.spirula.md`. No OpenCV dependency is needed.
