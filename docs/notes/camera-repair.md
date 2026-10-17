# Repairing cameras in the editor

A reconstruction often comes out mostly right: a few cameras sit in the wrong
place, and a few images never registered. Running SfM again may not come out
any better. The editor's **Repair** tab fixes just those cameras against the
rest of the model, and leaves everything else where it is. It works the same
for pinhole, fisheye and 360° (equirectangular) cameras.

The tab drives `spirula sfm repair` (`src/sfm/Repair.h`, `Mapper::repair`);
see "Repairing a finished model" in [`src/sfm/README.md`](../../src/sfm/README.md)
for the command line and for how the solver works.

## Opening it

Open the dataset's sparse reconstruction in the editor (**Edit
Reconstruction** on the dataset screen, or **Edit the model** from a recent
reconstruction's right-click menu on the Home screen), then pick the **Repair** tab beside **Select** and
**Transform**. The tab switches the editor to the **Cameras** layer.

Repair needs what the reconstruction left behind: a COLMAP model in binary
form (`sparse/N/*.bin`), plus the `features/` folder and `matches.bin` beside
it. These are kept by default. If any is missing, the tab says so and its
buttons stay off.

## Seeing what is wrong

Select a camera, by clicking it or with any selection tool on the Cameras
layer, and **Camera photo** at the bottom of the tab shows its picture:

- **Previous** and **Next** step through the cameras in order.
- **Look through** puts the view where the camera stands, with its lens. If
  the points seen from there do not match the photo, the camera is in the
  wrong place.
- **Photo over the view** lays the photo across the viewport, at the opacity
  set beside it. Together with Look through, this makes a wrong camera
  obvious.

Frusta are coloured by what has happened to them:

| colour | meaning |
|---|---|
| orange | as the reconstruction left it |
| cyan | selected |
| yellow | moved or placed by hand, not yet repaired |
| green | placed by the repair (in its preview) |
| red | the repair could not place it |
| grey | where a moved camera was before |

## Fixing a camera in the wrong place

- **Re-place selected**: takes the selected cameras out and places them again
  from the rest of the model. Try this first. It is enough when the camera has
  matches to the model and only landed badly.
- **Move by hand**, then **Snap moved**: for a camera too far off to be found
  from where it is. Select it, drag it roughly where it belongs with the
  Transform keys (`G` move, `R` rotate; `X`/`Y`/`Z` lock an axis; `Enter` or
  left-click confirms, `Esc` or right-click cancels), and Snap refines it from
  there. Rough is fine: within a few metres and 20° is close enough. Moves are
  in the undo history. **Forget moves** drops them all, and a plain Save never
  writes them.
- **Check every camera**: looks for cameras the rest of the model
  contradicts, without you naming them, and moves them where the model says
  they belong.

## Adding images that never registered

**Missing images** lists every image that has features but no camera in the
model. Cameras you deleted in the editor are not in it. Click one to see its
photo. Images given a place are drawn in yellow, in the list and in the view.

1. **Guess places** gives every unplaced image a starting pose:
   - If another model in the dataset's `sparse/` folder has the image (a
     reconstruction that split often leaves the missing stretch as a second,
     smaller model), it takes that pose, lined up on the cameras the two
     models share.
   - Otherwise it goes between its neighbours in file order. For a video,
     that is usually close.
2. To place one by hand instead, or to correct a guess, select it in the list.
   The view takes that photo's lens, and moves to the image's current place if
   it has one. Turn on **Photo over the view** and fly until the points line up
   with the photo, then press **Place at view**. **Clear** removes a placement.
   Both are undoable.
3. **Register missing** tries every missing image. Placed ones start from
   where they were put, and unplaced ones are found from their matches alone.
   The placed images are retried as their neighbours come in, so a whole
   missing stretch goes in one run.

## Running, keeping, discarding

Every action above is one repair run, done in the background; the log panel
shows its progress. **Match images first**, on by default, matches the
cameras being fixed against the model before placing them. It is slower, but
an image the reconstruction never matched well cannot be placed without it.
The new matches are added to `matches.bin` (the original is kept once as
`matches.bin.orig`), so a later run reuses them.

The run starts from what is on screen, including any unsaved deletions and
any move of the whole model. When it finishes, the tab lists each camera's
outcome (moved, added, kept in place, failed, removed), and the view shows
the result in the colours above:

- **Keep** writes the repaired model over `sparse/N`, keeping the originals
  once as `.orig`, and reopens it. Images placed but not registered stay
  placed for another try.
- **Discard** throws the result away. Your edits and undo history are as they
  were.

The repair rebuilds the sparse points around the cameras it changed, so the
point cloud needs no separate step. It also keeps the model's coordinate frame:
cameras it did not touch move by a tiny fraction of the scene. Anything
trained from the old model, a splat or a mesh, should be trained again to
benefit.
