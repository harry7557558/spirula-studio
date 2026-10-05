# Rendering a photo or a video

Render mode turns a model on the viewer screen into a picture or a film: a
camera move laid out as keyframes, a lens, the models to show and how the
picture passes from one to the next, and a file at the end. It is phase 6 of
[gui-editing-plan.md](gui-editing-plan.md) and lives in `src/app/gui/render/`;
the GPU video encoder it feeds is `src/video/VideoEncoder.cpp`.

```
RenderProject    the move, the lens, the output, the shots: one JSON file
Trajectory       keys -> a camera at any time
FlightFit        a move flown by hand -> keys
FrameRenderer    one frame: every model drawn by its own renderer, composited
FrameSink        where frames go: image files, a GIF, or an encoder process
GifWriter        animated GIF: a palette per frame, ordered dither, LZW
RenderSession    the mode: viewport interaction, preview, playback, export
RenderPanel.cpp, RenderTimeline.cpp    its panel and its timeline
```

## Getting in

The viewer screen has a **Render photo or video** button on its toolbar and a
**Render** button on every pane; the pane's model becomes the primary one, and
every other open model is a source a shot can show. The meshing preview is a
comparison, not a stage, and does not offer it. A finished training run offers
**Render photo or video** and **Edit the model** under *Train again*: both open
the run's model on the viewer screen and go straight into the mode.

A camera project dropped on the window opens its first model on the viewer
screen, the render on it with the project, and so the other models it names
(`GuiApp::open_render_project`).

Editing and rendering switch into each other from either panel. The editor
stays open behind the render, so what it deleted or moved is what is filmed.

Keys are picked the same way in the viewport as in the list: a click takes
one, Ctrl toggles, Shift takes the run from the last one clicked, and a click
on nothing (or Ctrl+D) takes none. Numeric-pad . frames the selected cameras,
or all of them, as it frames the selection in the editor
(`ViewportInteractor::frame_bounds`, `ViewportPanel::frame_view`: the view
glides to it from the direction it had). A click on the viewport or the panel stops
playback before it lands, and the lens section holds its key while playing,
so what was under the pointer is still there.

## Coordinates

Poses are stored in the **primary model's saved coordinates**: its file's
frame with the editor's placement applied. That is the frame a model trained
from the same dataset shares, so a project made on one run re-renders on the
next. Every source is taken to share it (a dataset, its splats and a mesh made
from them do), which is why the render ignores the comparison view's
per-pane alignment and uses each model's own file frame instead.

A project records the placement it was laid out against, and the keys follow
the model: turn it in the editor and the move turns with it
(`RenderSession::follow_placement`). A save that writes the placement into
the file is remembered, so the model read back from it -- the same place in
the world, now with no placement -- does not move the keys a second time.
Opened against another placement of the same model a project is carried
across the difference, and a model saved moved gets moved copies of its
projects beside it (`copy_moved_projects`) while the originals stay.

*Up* comes from the dataset when there is one (the parsers' levelling guess,
`ParsedDataset::normalized_rotation`, found through the run's `config.json`
and moved by its `scene_transform.json`). A point cloud's is the one its
pane shows: the guess while *auto-level* is on, else the file's own +Z --
off by default for a frame that was measured or saved from the editor. A
model turned in the editor was levelled by hand against the pane, so its up
is the pane's, not the guess turned along with it (`RenderSession::scene_up`).
Until a second key is laid out up follows all of these as they change
(`follow_scene_up`); after that it turns with the keys. A model nothing
levels offers *Take up from the view*. Up decides how an orbit turns, how an aimed camera stays
level and which way the sweep travels. An orbit started from a view steeper
than 60 degrees is tipped to 30, since from overhead it would only spin.

## The motion

Every channel -- position, rotation, look-at target, roll, log focal length --
is a cubic through the keys on one shared parameter, so a stop or an eased
end stops all of them together. The *Curve* decides the slopes at the keys:

- **Smooth spline (C2)**, the default: the interpolating cubic B-spline, one
  tridiagonal solve per channel, so speed *and* acceleration are continuous
  through every key. Free ends have zero second derivative; a key marked
  *Stop here*, and the ends when *Start and stop gently* is on, have zero
  slope and split the solve there.
- **Catmull-Rom**: the local time-weighted slope, scaled by `1 - tension` and
  limited on the whole vector (no faster than three times the slower
  neighbouring chord, easing off as the path doubles back). Only C1; moving
  a key changes less of the path.
- **Straight lines**.

The parameter is key time, unless *Constant speed* is on: then it is the
distance along the keys, so unevenly timed keys cannot make the spline
overshoot, and time is warped by arc length (a 48-samples-per-segment
table). The keys between the ends are then passed when the path says, not at
their own times: `Trajectory::key_times` runs the warp backwards, and the
timeline, a click on a key and the per-key looks all use those times; the
interior keys' own times are shown read-only. *Space evenly* is about times,
so it turns constant speed off (and says so). A **closed loop** adds a knot back at the first key and solves the
spline cyclically (Sherman-Morrison); a rotation that makes a full turn comes
back as -q, which the cyclic system carries as a sign. A loop's video leaves
out its last frame, which would be its first again.

Rotations are the normalized spline of hemisphere-aligned quaternions. Between
two aimed keys the rotation is instead derived from the interpolated position,
target and roll, which keeps an orbit on its subject. The projection and the
distortion tier cannot be blended and change at the key; the zoom glides.

The first key always has a lens of its own; any other may, or takes one in
between (`RenderProject::lens_at`): log focal length and the distortion
coefficients glide in key time between the nearest keys before and after that
have one, held past the last (a loop glides back to the first) and held
across a change of projection.

Editing keys keeps the move: a key added between two others goes where the
camera already passes (`add_key`); a deleted key's own lens passes to the next
key that has none, so the zoom still gets there; R turns one camera about its
middle; with several selected, G and R move and turn them about the pivot --
origin, median or mean -- aim points included, and S spreads their positions
alone, so a widened orbit still looks at its subject (S on one camera is its
zoom). An axis pressed twice with several selected -- R X X, G Z Z -- works on
each in its own frame and about itself: the step the camera clicked last
takes in its frame, carried into every other's (`apply_xform`), so R X X
tilts every camera of an orbit by one pitch and G Z Z dollies them all along
their own view. *Look at a point* and *Pick point* (T) take several keys at
once too. Deleting from the viewport refits the keys left to the old
path (`refit_keys`): Levenberg-Marquardt on a numeric Jacobian over each key's
position and its aim point or a rotation vector, matching position and
rotation at 12 samples per key, a radian weighing one scene unit; the times
and lenses stay. Deleting from the panel or the timeline takes the key out and
nothing else. *Smooth keyframes* makes the motion smoother, moving what
*What* leaves free: the poses at their times, pulled towards their
neighbours' line with Taubin's second outward pass so a loop does not
shrink; the times along the same path, so the speed between keys -- the way
from one to the next, turning included, over its time -- changes gradually
from gap to gap, each run between keys that keep their time (the ends, the
stops, keys not selected) keeping its length (`smooth_key_speeds`); or both.
Evening the gaps instead, as it first did, made a fitted flight jerkier: its
keys crowd where the path bends, so equal times made unequal speeds. A press
is kept only if the camera accelerates less over the whole move
(`motion_energy`, 30 samples a second, a radian weighing the scene's size);
failing that it is tried at half the strength, and at worst changes nothing
and says so.

## Flying a path

*Fly a path* records the viewport's own camera while the user flies it -- any
navigation mode, mouse, keys or a gamepad; the keys and the corner picture
step aside meanwhile -- and Enter turns the flight into keys
(`FlightFit.cpp`), Esc throws it away:

1. The still stretches at either end go: within half a percent of the scene
   (a radian of turn weighs the scene's size) of where it rests.
2. It is resampled evenly at 30 per second and smoothed over a twentieth of
   a second, to the ends, against a hand's jitter and the window's hitches.
3. It is timed anew. A step of recorded length dt that covers ds, turning
   included, takes dt^g (ds / v)^(1-g), v the median pace of the moving part.
   *Timing* g = 1 is the flight as flown, stops and all; g = 0 one steady
   pace, which rushes through a cramped stretch; between, the output pace is
   v_flown^g v^(1-g), so a stop costs nothing and the slow parts stay a
   little slow. *Length* scales the result.
4. Keys are chosen greedily: the two ends, then in every stretch between two
   keys where the spline through them strays furthest past the tolerance --
   *Detail*, 3% of the scene and 4 degrees down to 0.2% and 0.3 degrees -- a
   key at that sample, until none strays. The keys play on a spline with no
   easing, the first with the lens the flight was flown through.

The settings refit on letting go, as long as the keys are still the fit's.

## One frame

`FrameRenderer` draws each model with the renderer the viewer already uses,
at the output size:

- **splats**: the engine through a `RenderWorker` of its own, in the raw mode
  (`ViewRequest::raw`): no overlays, the lens distortion passed through, and
  RGBA premultiplied over nothing, alpha = 1 - transmittance;
- **points and meshes**: `PreviewRenderer` into its own FBO, cleared
  transparent, with the render styles (`PreviewStyle`): square, circle,
  Gaussian or sphere points, lens distortion in the vertex projection, and a
  clip plane for the sweep. Nothing is drawn where the lens folds over itself:
  the shader runs the engine's `is_valid_distortion` test (`lens_valid`), per
  point and per mesh fragment, as the splats already are. A soft point's
  sprite is 2.4 times its size, so that its half-maximum -- how wide it looks
  -- spans the size asked for.

Two layers at most meet in one GLSL compositing pass: a mix through a mask
(crossfade, wipe, iris), one over the other (the 3D ones, where each layer
has moved or cut itself), or -- for a shot leaving its own way -- each layer
on its own, faded, masked or zoomed as its own transition says, the new one
over the old; then two tints (a dip, then the fades) and the background.
Readback is one `glReadPixels` of the result.

A frame is a list of passes, one per layer and look. A `RenderWorker` keeps a
single latest-wins request slot, so a second request to the same worker
before the first came back replaced it and the frame waited forever: the
cause of previews that stopped playing with several models, and of exports
stuck at one frame and then at 0 after a cancel. Each worker now gets one
pass at a time, a pass that has not come back in 60 s is given up with an
error, and starting or cancelling an export abandons whatever was in flight.

A model's look can change at a keyframe (`Keyframe::looks`): between two keys
that set one, sizes glide and anything that can only be one thing or the
other -- colour on or off, shading, point shape, SH degree, primitive -- is
rendered both ways and mixed on screen by how far along the time is. The
model's own style is the look at the first key.

Effects that rewrite splats -- *Grow in* scales them up from 2% while they
fade in, *Sweep* hides those past the level with a soft band -- need the
splats host-side. They are read from the file on first use, rewritten per
frame under OpenMP and uploaded with `engine_scene_update` inside the render's
`before_render` hook, under the engine lock, and put back in `after_render`,
so the viewport never sees a half-applied frame. Splats deleted in the editor
stay deleted: the rewrite starts from the editor's survivors.

A mesh under a fisheye or equirectangular lens is projected per vertex, so a
long triangle keeps straight edges where the lens would bend them; the splats
and the points are exact. Splats render with the primitive *Render as* names,
*Automatic* unless one is chosen
(`resolve_primitive`): 3DGUT where 3DGS smears -- the whole sphere, or a lens
whose distortion folds over itself somewhere in the frame, which
`lens_needs_ut` finds by the engine's `is_valid_distortion` on a grid twice
the frame's size -- and elsewhere what the model was trained as, Mip or 3DGS,
since 3DGUT cuts a large splat near the camera along its square tiles. The
viewport, which never distorts, keeps its own three and the model's own
default.

The keys are drawn with the web viewer's frustum (`data/FrustumTemplate.h`,
viewer/js/dataset.js `frustumTemplate`, which the viewport's own dataset
cameras now use too): the border at 16 segments an edge through
`camhost::generate_ray`, gridlines at twice that over a wide lens, a globe of
8 meridians and 3 parallels for the whole sphere, and lines from the apex to
the four corners in 4 pieces -- behind the camera too, for a fisheye past
180 degrees. They are all one size in the world: the viewport's rule for a
set of cameras (`frustum_display_size`) applied to the keys, never below what
it gives the dataset's own cameras, held while an operator runs, times the
panel's slider. A click hits any drawn line or axis.
The camera's own picture sits in a resizable corner, fills the view, or sits
beside it: then the pane's controls run the whole width and the view and the
picture share the row under them, a splitter between.

## Transitions

A **shot** is a start time, a model (or nothing), how it arrives and how it
leaves. A project starts with one, the model on screen from the start, so
the list shows what is rendered before any shot is added; a file with none
reads as that one shot. How it arrives:

| | |
|---|---|
| cut, crossfade, dip through a colour | what every editor has |
| wipe (any direction, softness), iris | a moving mask in picture space |
| zoom blur | the old picture rushes past the camera, the new one arrives from far, both smeared radially |
| sweep (up or down, glow and its colour) | a level moves along *up*, one model on each side of it, the band at the cut lit |
| grow in | the new model's splats swell out of points while the old one fades |
| falling dust (fall, rise, blow away; turbulence) | the old model crumbles from one side, the new one settles out of the air |
| spiral in (turns, spread) | the old model spirals up and out, the new one swirls down into place |
| scatter (distance, randomness) | one bursts outward from the middle, the other gathers in |
| rain down (height, stagger) | the new model's elements drop into place with a small hop |
| dissolve (sparkle) | element by element, each at its own moment, with a pop |
| ripple (height, width) | a wave rolls out from the middle, swapping the models as it passes |

Each transition keeps two settings and a colour in its shot, reset to the
kind's own defaults when the kind changes (`transition_defaults`); files from
before the dips and wipes were one each read their colour and direction
across.

How a shot **leaves** is, by default, as the next one arrives: the next
shot's transition takes both at once, as above -- the last one stays to the
end. It can leave its own way
instead -- any transition but a dip, over its own time, starting an *offset*
after the next shot starts -- and then each model is drawn through its own
half on its own: before the next arrives (a negative offset leaves the
background between), overlapping it (a positive one keeps both on screen), or
exactly with it. `shot_mix` says, for any moment, which shot is arriving and
which leaving and how far each is; the frame and the timeline both read it.
The last shot's own way out ends *offset* after the video does, and may be a
dip: into its colour, held to the end -- the fade out. The first shot's dip
comes from its colour, since nothing is before it -- the fade in. So the
project-wide fades are the shots' now; a file from before them has its fades
turned into those two where the shots have none (`settle_shots`), and a fade
that cannot be is kept and shown until it is set to none.

A transition with nothing on its other side -- the first shot's, one into or
out of a gap, and every half of a way out of its own -- has the whole change
to itself. The 3D ones start where their half would and end where it would
(`fx_solo_time`), where before they waited through the half meant for the
other model, black for up to a third; the picture ones ease out rather than
in, so they are under way from the first frame.

The 3D ones (sweep on) move the models' own elements: one set of formulas in
`TransitionFx.h`, run on the host per splat (means, opacities and scales
rewritten in `before_render`, as the reveal effects are) and in the point and
mesh vertex shaders. They act on a scene measured robustly -- the per-axis
median of the elements, unweighted, and the 90th-percentile distance from it
past four median distances left out (`FrameRenderer::scene_core`) -- so a sky
or a floater far away does not stretch them. What they order by goes through
the elements' own quantiles, every 1/32 of them, unweighted
(`scene_quantiles`): the sweep's level, dust's and the spiral's order by
height, blowing dust's across, the ripple's front outward. Each moment then
passes as many elements, where a uniform sweep would race through the nearby
things and spend most of its time climbing into the sky.

Those with a way to go -- sweep, dust, spiral, rain, ripple -- go it *as the
camera sees it* or along the world's up (`Shot::camera`, per shot and per
way out; `RenderSession::fx_frame`). As the camera sees it, the frame is the
camera's at that moment: dust falls and rain drops down the screen and a
sweep climbs it, while a spiral turns and a ripple spreads about the view's
own axis, at the scene's depth -- round the middle of the picture, however it
is framed and wherever the camera stands. What the camera does not see is
not ordered (`FxView`: the quantiles are of the elements in the frame, when
there are enough of them), and the moves are as wide as the picture at the
scene's depth rather than the scene, so a long lens, or a camera inside the
scene, sees the change from its start. Dust, rain and the spiral default
to the camera; the sweep and the ripple to the world, where a level and a
wave over the ground look like what they are. Every one begins with the old
model exactly as it was and ends with the new one exactly as it is, outliers
included (`render_project_test`). Points take a random number of their own from their
index; a mesh's vertices take a smooth noise of where they are, since shared
vertices must move together, and a mesh fades by losing world-space grains.
Both layers are then drawn one over the other.

The timeline draws what `shot_mix` says, column by column: the shot arriving
rising from the bottom of the lane, the one leaving shrinking from the top,
the background between them. A shot's start, the end of its arrival, and the
start and end of its own way out all drag.

*Show each model in turn* splits the video into one shot per model -- points,
then splats, then meshes -- with the transition that suits each; shots can
then be reordered (models and transitions trade places, times stay). Fade in
and fade out are separate from the shots, as in an editor, and a photo has
none.

## Models

The project lists its models by path and remembers which open pane each one
is (`_src_uid`); shots and looks name the project's list, the renderers the
panes that are open (`RenderSession::rt`). So the model shown in the viewport
can be any of them -- *Show* switches the viewport to it and *Edit* opens it in
the editor -- and the camera move crosses from one model's placement to the
other's, all of them taken to share the file frame. Closing the one shown
shows the next. Adding and closing models are steps in the history like any
other: going back to before one was added closes it, and back past a close
opens it again. In the editor, *Move the other open models with it* shows the
placement on every other pane too, so a render of them together stays lined
up; they go back when the editor is left without saving over the file.

## History

Every state the project has been in is kept (200 of them), named by what
changed between it and the one before -- found by comparing the two, so no
action has to name itself -- and listed under *History*, where a click goes
to any of them. A drag is one step, taken when it lets go.

## Output

A photo, a video, or a folder of numbered frames. Photos and frames are PNG,
PNG with transparency, or JPEG, written by `stb_image_write` on a few threads
(`FrameSink.cpp`). A video is an MP4 (H.264, H.265 or AV1), AV1 in WebM
(ffmpeg's alone; the GPU encoder writes MP4), or an animated GIF.
GIF is written here (`GifWriter.cpp`): a median-cut palette per frame, an 8x8
ordered dither that does not crawl between frames as error diffusion would,
and frames dropped where a delay would fall under 2/100 s, which players
stretch. An MP4 is raw RGB piped into an encoder process, chosen per size:

- **`spirula encode`** for H.264 and H.265 when the frame fits the device
  (`--probe` reports each codec's largest frame; NVIDIA's H.264 stops at
  4096). It is `src/video/`'s encoder, so it is **patent-gated**
  (`SS_ENABLE_PATENTED`, off by default), and a process of its own because it
  needs a Vulkan device beside the engine's. If it fails, the export starts
  again with ffmpeg.
- **ffmpeg** otherwise, and first for AV1: libx264 / libx265 / SVT-AV1 where
  it has them, a GPU encoder of its own next. The built-in AV1 predicts every
  P frame from its key frame (src/video/README.md, "Encode"), which is
  correct and about 2.5 times the size.
- Neither: frames and GIF still work, and the panel says why the rest does not.

A save or a render asks where with the folder and the name filled in --
`renders/` beside the model, made if it is not there yet, since a picker
handed a missing folder opens wherever it last was; and the project's own
name, or the model's past the names every run shares (`splat`,
`step-*.ckpt`, `sparse/0`), with the extension the output needs (Windows is
told it as the default extension). Quitting with a camera move unsaved asks:
save over its file, save it as, discard, or stay.

An equirectangular video is tagged as 360 (Spherical Video V1 and V2). Its
sizes are listed as 2:1, and the panel warns when a key's lens is
equirectangular and the size is not. *Render* with no file chosen asks for one,
gives it the extension the output needs, and then goes ahead; a file, or a
folder of frames, already there is replaced only after asking.

An export keeps the renderers busy: the next frame is asked for as soon as one
comes back, before that one is read back, handed to the encoder and the window
drawn, so the render of one overlaps the rest of the other. The splat worker's
raw path keeps its buffers between frames, skips the depth read back and turns
floats into bytes on every core. 1080p splats went from 22 to 68 frames a
second (RTX 5070, H.264 on the GPU).

The window keeps drawing meanwhile. An encoder slower than the renders used
to hold the GUI thread in the pipe's write, and the loop spent 35 ms of every
window frame waiting on the engine: the window drew at 11 frames a second
(95th percentile 225 ms, worst 371 ms) through a 1080p SVT-AV1 export. Now a
finished frame waits in the renderer while the encoder's queue is full
(`FrameSink::full`), the loop takes 12 ms a frame, and the encoder runs a
notch below the window (nice 10; below normal on Windows): 46 frames a
second (95th percentile 31 ms, worst 47 ms), the export as fast as before.
A finished render's message is green, as a finished dataset's is.

## The file

`<dataset>/renders/*.json` by default -- the dataset the run read, or the
folder beside the model when that already has a `renders/` -- with an
`autosave.json` written when the mode is left with a move nobody saved
(quietly on a switch to the editor, where the move is still in memory). The
format is `"format": "spirula-render"`, `"version": 1`; unknown keys are
ignored and missing ones take their defaults, so it can grow without a
version bump. Sources are listed by path: opening a project opens the ones
it names that are not open, and those open besides join it.

## Testing

`render_project_test` covers the pure half: the path through its keys, holds,
eased ends, constant speed, C2 joins against Catmull-Rom's C1, closed loops
(a circle stays a circle within 1%, a looped full turn keeps turning through
the seam), rigid transforms of the path, aim and roll, the lens arithmetic,
JSON round trips including older files, moved-project copies, dataset lens
clustering, the lens glide between keys, when each key is passed at
constant speed, per-key looks and their JSON, the refit after a deletion (an
orbit missing a key comes back to within a third of its gap), transition
settings and the older names they replaced, a shot's way out and camera
setting in the file, the quantile lookups, every 3D transition starting and
ending exactly at rest, and with nothing on the other side starting at once
and still ending at rest, who is on screen when (`shot_mix`: crossfades, a
first shot arriving from nothing, leaving early, overlapping, cutting out,
the last shot's way out at the end), older fades becoming the shots', which
lenses want 3DGUT,
the flight fit (still ends trimmed, a stop kept as flown and dropped between,
the path followed within the tolerance, more detail keeping more keys), and a
GIF read back through a decoder of its own. The rest was
driven through the GUI automation surface ([gui-automation.md](gui-automation.md))
on a trained scene with a mesh: orbit loops, insertion, deletion, R, the
lens-aware gizmos, the three preview layouts, keys following a turned model,
3DGS against 3DGUT, shots and model removal, and H.264 at 4946x3286 through
ffmpeg, SVT-AV1, GIF and RGBA frames checked with ffprobe. `spirula encode`
was checked against the source by PSNR: H.264 40, H.265 39, AV1 39 dB.
