# FlexGlue (`src/flexglue/`)

FlexGlue ([cvg/FlexGlue](https://github.com/cvg/FlexGlue), MIT code) on the
inference layer: a DINOv3 ConvNeXt-B descriptor that turns each image into a
`[64, 64, 256]` map, and a sparse matcher that matches two images' keypoints
through those maps and scores whether the pair is covisible at all.

The configuration is COLMAP's: the image squashed to 512x512, fp32, mutual
nearest neighbours between keypoints and predicted points, early-stop checks
after layers 2, 4, 6 and 8, `min_covisibility 0.5`, `early_stop_decay 0.5`.
`tools/flexglue_ref/ARCHITECTURE.md` has every shape and constant.

## Weights and licences

`flexglue.pth` from the authors' release, read by `nn::TorchCheckpoint` by
name. It bundles Meta's DINOv3 ConvNeXt-B, so it needs the DINOv3 licence as
well as FlexGlue's MIT one. Both are one family, `flexglue`, registered in
`app/ModelLicenses.h` the way RoMa v2's are. The licence is asked for before any
checkpoint is opened, including one named with `--flexglue-model`. Never
bundled.

## Layout

```
FlexGlue.h            Model: describe() an image, match() two
model/Model.cpp       the forward pass, next to descriptor.py and flexglue.py
model/Fetch.{h,cpp}   the checkpoint URL + SHA-256, the licence gate
shaders/flexglue.slang  the 1.5-entmax window readout, the offset head's
                        binned expectation, the 4-D mutual nearest neighbour
tests/flexglue_test.cpp parity against a tools/flexglue_ref/ pair
tests/flexglue_license_test.cpp  the gate, on any path
```

## In SfM

`spirula sfm --matcher flexglue` with any `--features`, SIFT included: FlexGlue
reads only the keypoint locations. It reads the pixels too, so `auto` hands it
its image directory and `match` needs `--flexglue-images`. Every flag has
COLMAP's default; `--flexglue-min-covisibility` is the pair filter (0 keeps
every pair) and `--flexglue-early-stop-decay` how early a clear non-pair stops.
The dense features of the last `--flexglue-cache` images stay resident (4 MB
each). A pair the focal or gyro sample matched is not matched again; those
results are held until reused, within 256 MB.

Layer 1's self block (input projection, self attention, FFN) reads one image
only, so `match()` keeps its output, 8 MB, for the 64 most recently used live
images (`SS_FLEXGLUE_SELF_CACHE=N`, 0 off) and starts a hit at the cross block.
It is bit-identical to the uncached path (`flexglue_test`'s self-cache checks;
`--warm-cache` dumps the hit path for `compare.py`).

## Parity

`flexglue_test` runs every pair in `tools/flexglue_ref/fixtures/` twice, as the
reference did (`nostop`, then `colmap`), against the fixed bars of
`tools/flexglue_ref/README.md`. `SS_FLEXGLUE_DUMP=<dir> flexglue_test --pair
NAME` writes every stage for `compare.py check`, which also applies the
torch-vs-torch floors. The fixture images are fetched, not committed: run
`uv run tools/flexglue_ref/fetch_images.py` first. One sampling path cannot be
pinned: `f_match` is read at a mean of cell centres, which never reaches the
outer half-cell, so its border padding is unobservable.

## Speed

This is the fp32 path, with no cooperative-matrix kernels. Per pair the cost is
dominated by attention: nine layers of 4096 x 4096 tokens, 8 heads, self and
cross, for both images. An early stop after layer 4 skips about half of it.
