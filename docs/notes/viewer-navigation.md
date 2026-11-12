# Viewer navigation defaults

`data/NavigationHint.h` classifies the input OpenGL camera poses for the
native preview, native training viewport, training browser client, and the
standalone WebGL viewer (through its WASM bridge). It chooses the mode and the
orbit centre when a dataset is first attached; both menus remain available.
A model without camera poses keeps the existing orbit default.

The classifier samples at most 2048 camera centres and optical axes, centred
and scaled by camera-position medians, and makes two decisions.

- **Orbit or fly**: `inward`, the mean cosine between each view and the way to
  the camera median, weighted by distance from it up to one capture radius.
  Orbit at 0.5 or more.
- **Which pivot**: the optical axes' intersection is fitted with eight
  iteratively reweighted least-squares passes, downweighting residuals above
  15% of the capture radius. `aimed` is the fraction of views within 40
  degrees of it. At 0.75 or more, with directional coherence below 0.96
  (squared mean direction of the aimed views), the capture orbits about that
  focus, and is an orbit even if `inward` alone would not make it one.
  Otherwise an orbit pivots on the camera median.

Measured on the public sets (inward / aimed):

| capture | inward | aimed | result |
|---|---|---|---|
| Mip-NeRF 360 bicycle, bonsai, counter, garden, kitchen, stump | 0.79-0.97 | 0.95-1.00 | orbit, focus |
| Mip-NeRF 360 room | 0.75 | 0.84 | orbit, focus |
| Tanks and Temples train / truck | 0.91 / 0.91 | 0.95 / 0.98 | orbit, focus |
| Deep Blending playroom | 0.46 | 0.31 | fly (boundary) |
| Deep Blending drjohnson | 0.19 | 0.19 | fly |

Room's views each frame a different part of the room, so they converge
loosely, but its focus lies a full capture radius below the camera median and
explains the views far better. A sculpture walk mixing phone photos with a
dual-fisheye rig scores aimed 0.74, because the rig's lens axes rarely point at
the subject. Its focus lies 0.06 capture radii from the camera median, so that
choice hardly matters. Random 70% subsets of the cameras never move room or train
across a threshold.

Object captures start in turntable (native/training browser) or trackball
(WebGL), centred on the camera focus or median as above. Other captures use first person
in the native GUI and free fly in the browser viewers. Both use wheel translation
that does not shrink as it approaches an orbit pivot.
The manual mode and centre selectors override the initial choice. A native
preview refreshed under the same key, such as the live SfM model, reapplies
the classification on every refresh. It keeps doing so until the user picks a
mode or centre, each separately. A change of centre, or a switch into orbit,
moves the pivot as choosing that centre from the menu would. A live model is
also renormalized on its own cameras at every snapshot and re-gauged at the
end, which moves the subject in the view's frame. When a refresh arrives in a
new frame, the camera and pivot move with the chosen centre, so the pivot stays
on the subject. The scale is not followed: a two-camera snapshot is framed by
its points and the next by its camera baseline, so the view distance would jump.

Orbit navigation displays a yellow focus ring for 450 ms after the pose
changes. These are client overlays, including in the training viewers, so
neither training kernels nor render requests need a focus-marker parameter.
Native saved views appear as purple wire frustums under the cameras toggle;
validation cameras are pink in the preview and training renderer. Saved-view
and training validation outlines are overlays, so they remain visible through
the scene. Evaluation images excluded by the dataset parser are not part of
that camera table.

`navigation_hint_test` covers arcs, hemispheres, outliers, outward cameras,
an inward ring that converges nowhere, scale/translation invariance, and
degenerate captures. Optional dataset path arguments report the mode and both
scores using the real parser without loading images.
