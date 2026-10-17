# `spirula sam`

Segmentation, tracking, fixed-area masks and frame extraction: the scriptable
side of what the GUI's masking step does. Vulkan builds with `SS_BUILD_SAM=ON`
only. Diagnostics go to stderr and results to stdout, so
`spirula sam segment ... 2>/dev/null > detections.tsv` keeps just the table.
Background is in `src/sam/README.md`.

```text
spirula sam devices
spirula sam segment --model <file> --image <file> [options]
spirula sam track   --model <file> --frames <dir> [options]
spirula sam mask    <images> [options]
spirula sam video   --info <file>
spirula sam extract <video> [options]
spirula sam --help
```

`--model` takes a SAM 3 or SAM 2 `.ggml` checkpoint, or `birefnet` /
`birefnet-lite` (or a BiRefNet `.safetensors` file). BiRefNet masks the main
subject of each image by itself: it reads no prompt, and by default the
subject is what is kept.

Exit status is 0 on success, 2 for a usage error, and 1 or 2 for a failure in
the run.

## `devices`

Lists the Vulkan devices, with index, name, type, memory, UUID and whether
each meets the baseline. Takes no options.

## `segment`

One image. Writes a tab-separated table of detections to stdout.

| flag | default | meaning |
|---|---|---|
| `--model <file>` | required | Checkpoint, as above. |
| `--image <file>` | required | The image to segment. |
| `--text <phrase>` | | Concept prompt: every matching instance. |
| `--detector <id\|file>` | | Grounding DINO for the text prompt: `gdino-tiny`, `gdino-base` or a `.safetensors` file. It finds boxes and the SAM model segments them, which lets a SAM 2 checkpoint take words. |
| `--detector-threshold <f>` | 0.3 | How sure Grounding DINO must be of a box, 0..1. |
| `--box x0,y0,x1,y1` | | Exemplar box. Repeatable. |
| `--neg-box x0,y0,x1,y1` | | Negative exemplar box. Repeatable. |
| `--point x,y` | | Positive click (visual prompt). Repeatable. |
| `--neg-point x,y` | | Negative click. Repeatable. |
| `--prompt-box x0,y0,x1,y1` | | Box prompt for the visual path. |
| `--multimask` | off | Return the three ambiguity masks. |
| `--threshold <f>` | 0.5 | Detection score threshold. |
| `--nms <f>` | 0.1 | NMS IoU threshold. |
| `--out <dir>` | | Write the mask PNGs and an overlay here. |

## `track`

A folder of frames, in name order. Instances are detected and carried from
frame to frame by the memory bank.

| flag | default | meaning |
|---|---|---|
| `--model <file>` | required | Checkpoint, as above. |
| `--frames <dir>` | required | The frames to track through. |
| `--text <phrase>` | | Detect and track matching instances; separate several concepts with `;`. |
| `--neg-text <phrase>` | | Concepts to keep even where `--text` matches. |
| `--point x,y` | | A click on an object to track. Repeatable. |
| `--neg-point x,y` | | A click on something that is not it. |
| `--object` | | End this object and start the next: two things need two objects. |
| `--at-frame <n>` | 0 | Put the clicks that follow on frame `n` (as this command numbers them), and use them to correct the object there. |
| `--detect-every <n>` | 1 | Run the detector every `n` frames; the memory bank carries tracks in between. |
| `--memory-frames <n>` | 0 (model default) | Cap on spatial memory frames per instance. |
| `--max-frames <n>` | 0 (all) | Stop after `n` frames. |
| `--detector <id\|file>` | | As for `segment`. |
| `--detector-threshold <f>` | 0.3 | As for `segment`. |
| `--threshold <f>` | 0.5 | Detection score threshold. |
| `--nms <f>` | 0.1 | NMS IoU threshold. |
| `--keep-prompted` | | White marks the prompted objects. |
| `--remove-prompted` | | Black marks the prompted objects: the default, except with BiRefNet. |
| `--dilate-ratio <f>` | 0.05 | Move each detection's outline by this share of its own size before it joins the mask; negative trims, 0 turns it off. |
| `--overlay` | off | Write a colour overlay instead of a binary mask. |
| `--out <dir>` | | Write one mask PNG per frame here. |

For example, `--point 640,360 --at-frame 90 --point 700,300 --object --point 120,500`
is one object clicked once and corrected at frame 90, and a second object.

## `mask`

```text
spirula sam mask <images> [options]
```

Masks the part of every frame that is never scene (a fisheye border, a
watermark, the rig in shot) with a fixed shape. No model is needed. Without
`--shape` it looks for a dark border round the image.

| flag | default | meaning |
|---|---|---|
| `--out <dir>` | `masks` beside the image folder | Where the masks go. |
| `--shape <spec>` | find the border | Use these shapes instead: `ellipse cx,cy,rx,ry` or `rect x0,y0,x1,y1` in fractions of the frame, `;` between shapes, a leading `-` to cut one out again (`"ellipse 0.5,0.5,0.49,0.49; -rect 0,0.93,1,1"`). A path ending in `.svg` reads the shapes the GUI saves, with the rest of that camera set. |
| `--shrink <f>` | 0.01 | Pull the found boundary inwards by this fraction of its radius. |
| `--samples <n>` | 24 | Frames read per camera when looking for the border. |
| `--dark <0..255>` | 16 | Brightness at or below which a pixel counts as black. |
| `--mask-image <file>` | | Also drop wherever this image is black. |
| `--preview <file>` | | Write one frame with the mask drawn over it. |
| `--print` | off | Print the shapes and write nothing. |
| `--replace` | off | Overwrite masks already in the output folder; otherwise they are intersected. |

## `video`

```text
spirula sam video --info <file>
```

Probes a video file and reports codec, geometry and whether it can be decoded
in-process. Needs a build with `SS_ENABLE_PATENTED=ON`.

## `extract`

```text
spirula sam extract <video> [options]
```

Writes the sharpest frames of a video, optionally masked in the same pass.
Decodes in-process where the build and device can, otherwise with ffmpeg.

Frame selection:

| flag | default | meaning |
|---|---|---|
| `-o`, `--out <dir>` | `<video-without-ext>/images` | Output directory. |
| `-s`, `--skip <n>` | 1 | Write one frame every `n` source frames. |
| `-k`, `--keep <n>` | -1 | Choose the sharpest of the last `n` frames; -1 is `round(skip/2)`, 0 turns selection off. |
| `-n`, `--max-frames <n>` | 0 (all) | Stop after writing `n` frames. |
| `--start <time>` | the start | Read from this time on: seconds, or `mm:ss` / `hh:mm:ss`, with a fraction. |
| `--end <time>` | the end | Stop reading at this time, in the same form. Frames keep their source numbers. |
| `-q`, `--quality <0..100>` | 95 | JPEG quality; a value outside 0..100 writes PNG. |
| `-r`, `--rotate <deg>` | 0 | 0, 90, 180 or 270, clockwise. |
| `--no-autorotate` | | Ignore the rotation the file asks for. `--autorotate` turns it back on. |
| `--scale <f>` | 1 | Resize factor, at most 1. |
| `--lut <file\|name>` | | Colour LUT for every frame: a `.cube` file, a built-in name, or `auto` for the one the clip's metadata names. `spirula sam extract --help` lists the built-in names. |
| `--track <i>` | every track | Video track to read; with all of them, each goes to `<out>/cam0`, `<out>/cam1`, ... |
| `--sync` | off | Decode a multi-track file in lockstep so every track keeps the same instants; the frames of one name are then a rig. |
| `--adaptive` | off | Space kept frames by how much the view changes rather than by time; `--skip` then sets the average. |
| `--adaptive-range <f>` | 4 | How far the adaptive rate may stray from the average, either way. |
| `--threads <n>` | cores - 1 | Image-encoder threads. |
| `--decoder <d>` | `auto` | `auto`, `builtin` or `ffmpeg`. |
| `--ffmpeg <exe>` | `ffmpeg` on `PATH` | The ffmpeg executable. |

360 cameras (GoPro MAX `.360`):

| flag | default | meaning |
|---|---|---|
| `--360 <mode>` | `faces` | `faces` (ten seam-free perspective views, five per lens), `equirect` (one 2:1 panorama) or `off` (the raw tracks). |
| `--360-size <n>` | from the source | Pixels a side per face, or the equirectangular width. |
| `--360-orient <y,p,r>` | `0,0,0` | Yaw, pitch, roll in degrees applied to the sphere; `0,0,180` fixes an inverted mount. |

Masking (needs `--model`):

| flag | default | meaning |
|---|---|---|
| `--model <file>` | | SAM 3 checkpoint, or BiRefNet as above. |
| `--detector <id>` | | As for `segment`. |
| `--detector-threshold <f>` | 0.3 | As for `segment`. |
| `--text <phrases>` | | `;`-separated noun phrases, e.g. `"person; car"`. |
| `--neg-text <phrases>` | | Phrases to subtract from the mask. |
| `--mask-mode <m>` | `video` | `video` tracks instances across frames; `image` treats each frame on its own. |
| `--mask-keep <w>` | `background` | `background`: the prompt names distractors and the rest is kept. `subject`: the prompt names what to keep. |
| `--mask-out <dir>` | `masks` beside the images | Where the masks go. |
| `--detect-every <n>` | 1 | As for `track`. |
| `--memory-frames <n>` | 0 | As for `track`. |
| `--max-size <n>` | 1600 | Longest side handed to the model; 0 turns it off. |
| `--threshold <f>` | 0.5 | Detection score threshold. |
| `--nms <f>` | 0.1 | NMS IoU threshold. |
| `--dilate-ratio <f>` | 0.05 | As for `track`. |
| `--overlay` | off | Also write a colour overlay beside each mask. |

`extract` also takes `--device`, `--profile` and `--validate`.

## Common options

`segment`, `track`, `mask` and `video` all accept these, and each uses those that apply to it.

| flag | default | meaning |
|---|---|---|
| `--device <index\|name\|auto\|uuid:hex>` | `auto` | Vulkan device. |
| `--vram` | off | Print a device-memory report after `segment` or `track`. |
| `--profile` | off | Per-kernel timings. |
| `--validate` | off | Enable the Vulkan validation layers. |
| `--img-size <n>` | 0 (model default) | Model input size. |
| `--max-size <n>` | 1600 | Downscale inputs to fit; 0 turns it off. |
| `--image-gamut <name>` | the file's own | Colour primaries the frames are in; they convert to sRGB first. |
| `--image-linear` / `--no-image-linear` | the file's own | Frames are linear light, or display-encoded. |
| `--image-exposure auto\|<stops>` | | Brighten what the model sees, in linear light. |

## Environment

`SS_NN_LOG=0..3`, `SS_VK_DEVICE`, `SS_PROFILE=1`, `SS_VK_VALIDATION=1`,
`SS_NN_DEBUG_SYNC=1`; see [README.md](README.md#environment-variables).
