# The dataset screen's input preview

Before anything runs, the right-hand column of the dataset screen shows the
input selected in the list on the left (`app/gui/SourcePreview.h`). Clicking a
row selects it -- any part of the row its own widgets do not cover -- and a
row just added is selected for you. Once a run has something to show, a
switch above the column chooses between the input and the run; starting a run
switches to the run.

What it shows depends on what the input is:

| input | tabs |
|---|---|
| video | Video (frames, the timeline, playback and the clip), Sensors, Details |
| photo folder | Photos (grid and single view), Sensors, Details |
| a dataset's photos | the same, plus 3D: the reconstruction already there |
| laser scan | 3D (points, and the scanner's photographs as cameras), Photos (an E57's images), Details |

The first tab is the one a new user needs; Details and the Sensors tab's
Diagnostics (the reader's own report, in English like every deep diagnostic)
are for the user who wants to know why. Sensors appears only once a GPS, an
IMU or an exposure was found: a phone video with none has no tab to puzzle over.

## Video

`VideoBrowser` shows the frame at the playhead and a strip of sixteen frames
along the video, filled coarse to fine. Frames are read the way the run reads
them (`PreviewFrames.h`): through the same LUT, auto-rotation and, for a
360 file, the same unwrap -- the "Stitched 360°" view is an equirectangular
panorama turned by the dataset's own 360 orientation, so a camera held upside
down shows upside down here and is fixed in the same place it would be fixed
for the run. "As recorded" shows the tracks as the camera wrote them. A
multi-track file shows its tracks side by side, and so does a frame that packs
both fisheyes into one picture (an Insta360 `.lrv`, a QooCam 3 Ultra's stacked
`.mp4`): its lenses are cut apart as the run cuts them into `cam0/`, `cam1/`.

**Decoding.** On the GPU the preview keeps the video open between frames
(`app::FrameCursor`): every track decoded in step, so the next frame is one
decode per track and a seek is one keyframe and the GOP after it. On a
3840x3840 10-bit HEVC pair (an Osmo 360 `.OSV`) that is ~3 ms a frame per
track, a seek within 0.3 s for both tracks, and 24 fps playback with none
dropped; a GoPro MAX `.360` stitched plays at its 30 fps. While the playhead
is dragged frames are decoded at the pane's size as fast as they come, and at
2048 px once it rests. Space (or Play) plays at the video's own speed, read a
few frames ahead; the Sensors tab's playhead follows, and Space works there
too. The decoders hold ~1.5 GB of device memory for that pair, given back
after 10 s idle and whenever the device is taken back.

Through ffmpeg, a seek is a process and a software decode from the keyframe:
1.4 s per still per track on the same pair (ffmpeg decodes it at ~15 fps on 8
cores). So ffmpeg is asked once per request for every track (one seek, ~1.5 s
for both), the strip is read from keyframes alone (`-skip_frame nokey`, 0.4 s
for both tracks), the exact frame is asked for only once the playhead rests,
with the strip's nearest pictures standing in, and there is no playback --
the Play button says why. A multi-track file keeps a slot per track, sized
from the file, so a late track leaves its place empty rather than moving the
other.

**Which decoder.** The in-process one shares the inference device's queues, so
the preview uses it only when nothing else is on that device (no run, no mask
or geometry preview, no mask editor; `GuiApp::update_source_preview_gpu`).
The first video shown makes the device choice if nothing has yet, as opening
a mask preview does (`GuiApp::draw_source_preview`): ffmpeg is too slow for a
4K capture to be looked through. Without the in-process decoder, or while the
device is busy, ffmpeg reads the frames. `stop_inference_users()` takes the
device back -- closing the kept decoder -- before anything else starts.

### The clip (issue #158)

The strip's green and red handles, the Start here / End here buttons (keys I
and O), or the two time boxes set `PrepInput::clip_start` / `clip_end`, in
seconds of the video. Only that part is extracted, by every path:

- **The built-in decoder** seeks to the keyframe before the clip, brings
  every track level, and stops at its end (`SelectClock::span`). Frames keep
  their **source** index as their name, so a clip's frames meet the IMU and GPS
  exactly where the whole video's would.
- **`app/FrameDecodeFfmpeg.cpp`** seeks with `-ss` half a frame early and
  counts on from the clip's first frame, so its names are the built-in
  decoder's: on a dual-fisheye Insta360 clip the same name is the same picture
  (0.4 grey levels apart; the neighbouring frame is 8).
- **The GUI's ffmpeg fallback** resamples with `fps=`, numbering candidates
  from 0 at the clip's start; the capture records that start as `time_offset`
  and the manifest carries it. Without it the gyro and the poses disagree on
  that clip; with it they agree to 2.3 deg.
- An adaptive plan is budgeted on the clip's length (`MotionPlanInput::span`).

The clip is part of what the frames were made with (`frames_fields`, key
`clip`), so changing it re-extracts, and it comes back from a workspace's
record with the rate. Clicks drawn in the mask preview belong to frames of the
old clip and are dropped when it changes. `spirula sam extract --start/--end`
is the same thing on the command line.

## Photos

`PhotoBrowser` lists the folder as the run will walk it
(`DatasetPrep::image_sizes`) and loads thumbnails for the cells on screen,
newest request first, on a few threads; a camera's own EXIF thumbnail is used
when it is big enough and the photo's shape, which turns a 24 MP decode into a
few KB read. At most a few hundred thumbnails stay on the GPU. Clicking one
opens it at the pane's size (arrows, Home/End, Escape); masks that came with
the folder are tinted in. Pictures are shown upright, as the run reads them.

## Sensors: GPS, IMU and exposure

`SensorView` reads a video's track with the reconstruction's own reader and
checks (`sfm/core/Telemetry.h`), and a folder's EXIF GPS, exposure and drone
attitude.

- The map is metric -- metres east and north of the track's mean, a scale bar,
  north up -- with no basemap: tiles would mean sending the location to a tile
  server on every look. "Open in OpenStreetMap" does that on request instead,
  and says so. The track is coloured early to late; the fixes the
  reconstruction drops (DOP over 10, jumps over 50 m/s) are red ×s, pinned to
  the edge when they are off the map; photos far from the rest (over ten
  times the typical distance from their median, and over 200 m) likewise. A
  drone photo carries a tick for its heading.
- 3D shows the same track with its height: a grid at the lowest fix, the
  track's shadow on it and lines down to it, and a ruler in true metres. Drift
  in altitude is a slope or a step. Heights are stretched by default until the
  climb is a fifth of the track's width (1x to 20x, chosen beside it), since a
  few metres over a long walk is otherwise a hair; drone photos also show
  where the gimbal pointed, pitch included.
- Under it, one plot per stream the input carries, on a shared time axis
  (photo index for photos): speed from positions and as reported, altitude
  (and above take-off), DOP, rotation rate, acceleration (9.81 dashed),
  gravity, magnetic field and the camera's attitude, the axes as x, y, z in
  red, green and blue as in `telemetry.html`. "Plots..." picks which; DOP,
  gravity and the magnetometer start hidden. Each plot names its sample rate,
  shades where its stream went quiet for five of its own intervals, and the
  wheel zooms time for all of them (right-drag pans, double-click or "Whole
  length" undoes it). Hovering reads every channel and a vector's magnitude.
  The map, the 3D view and the plots keep the wheel, so it never scrolls the
  pane under them.
- Clicking a plot or the track moves the video there; clicking a photo's dot
  or its place on a plot opens the photo. Photos get altitude, distance from
  the previous photo (a jump stands out of a survey's even steps) and the
  gimbal's direction, yaw left as written.
- Plain verdicts over the map ("GPS: looks usable", "Distinct positions: 45"
  when a phone repeated one stale fix), with the checks' own warnings on hover.

**Exposure.** What a camera logs per frame (GoPro `SHUT`/`ISOE`, the
Insta360 trailer's shutter, DJI's ISO and shutter, a QooCam's `EXP`) or a
photo's EXIF records (with the APEX tags as fallback) is plotted in stops:

- **Exposure (EV)** -- shown by default. Each factor as the light it lets in
  (log2 t, log2 ISO, -2 log2 N) about its own mean, and their sum over the
  factors every record has, which is what `--ppisp-exposure-from-exif`
  compares; 0 is the input's mean, +1 twice as bright. Factors that do not
  move are left out of it; with only one factor logged, the sum is that factor.
- **Shutter speed, ISO, Aperture** -- hidden until picked: each on a log axis,
  read as 1/250 s, ISO 400, f/2.8. Hovering any of the four reads the record's
  own values.

A bracketed set is a sawtooth in the first plot, and a camera walking from
shade into sun a step. "Exposure records: n" (a video) or "Exposure: n of m
photos" sits with the verdicts; a video's frames carry the same values into
their EXIF when they are extracted (`docs/datasets.md`).

**All-zero GPS.** A receiver with no fix writes latitude 0, longitude 0. Such
samples are dropped by `telemetry_read` itself (`Telemetry::gps_zero` counts
them), and an EXIF position of 0, 0 is not a position (`ExifData::gps_zero`),
so neither the preview, nor the row's sensor switches, nor the reconstruction
ever sees one. The preview says how many it ignored.

## 3D

A scan is read through `cloud::Reader` with a deterministic draw of 1.5M
points, so a 680M-point file previews in the time it takes to stream it. An
E57's posed images become cameras; a scan without them shows where its
stations stood. A dataset's model is parsed as the trainer parses it. Both
draw in a `ViewportPanel`, z up for scans.
