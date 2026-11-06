# Viewer navigation defaults

`data/NavigationHint.h` classifies the input OpenGL camera poses for the
native preview, native training viewport, training browser client, and the
standalone WebGL viewer (through its WASM bridge). It chooses the mode when a
dataset is first attached; the navigation menu remains available afterward.
A model without camera poses keeps the existing orbit default.

The classifier samples at most 2048 optical axes and fits their intersection
with eight iteratively reweighted least-squares passes. Coordinates are
centered and scaled by camera-position medians. Residuals above 15% of the
capture radius are downweighted. An orbit needs 85% of cameras facing the
focus, 80% within approximately 25 degrees of it and half a capture radius
of its axis, and directional coherence below 0.96 (squared mean direction).
This admits quarter arcs and pitched or hemispherical captures while rejecting
parallel trajectories, outward panoramas, and coincident camera centers.
Insufficient or degenerate camera evidence selects non-orbit navigation.

Object captures start in turntable (native/training browser) or trackball
(WebGL), centered on the fitted camera focus. Other captures use first person
in the native GUI and free fly in the browser viewers. Both use wheel translation
that does not shrink as it approaches an orbit pivot.
The fit is a heuristic: a room photographed around a central object can
legitimately choose orbit. The manual mode selector overrides the initial choice.

Orbit navigation displays a yellow focus ring for 450 ms after the pose
changes. These are client overlays, including in the training viewers, so
neither training kernels nor render requests need a focus-marker parameter.
Native saved views appear as purple wire frustums under the cameras toggle;
validation cameras are pink in the preview and training renderer. Saved-view
and training validation outlines are overlays, so they remain visible through
the scene. Evaluation images excluded by the dataset parser are not part of
that camera table.

`navigation_hint_test` covers arcs, hemispheres, outliers, outward cameras,
scale/translation invariance, and degenerate captures. Optional dataset path
arguments report the selected mode using the real parser without loading images.
