#!/usr/bin/env python3
# /// script
# requires-python = ">=3.10"
# dependencies = ["numpy", "opencv-python-headless"]
# ///
"""Fetch the fixture images and write the pixels the reference ran on.

    uv run tools/flexglue_ref/fetch_images.py [--cache DIR]

The images are not committed (see README.md, "Images"). Each is downloaded from
its pinned URL in fixtures/pairs.json, checked against its sha256, decoded the
way flexglue_ref.py decodes it, and checked again: `rgb_sha256` is the sha256 of
the decoded [H, W, 3] uint8 RGB the fixtures were generated from. A decoder that
rounds differently fails here rather than shifting every stage of the parity
test. The result is fixtures/images/<stem>/image.png, which flexglue_test reads.
"""
import argparse
import hashlib
import json
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from flexglue_ref import fetch, read_rgb  # noqa: E402


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--fixtures", default=str(HERE / "fixtures"))
    ap.add_argument("--cache", default=str(pathlib.Path.home() / ".cache" / "flexglue" / "images"),
                    help="where the downloaded files are kept")
    a = ap.parse_args()
    import cv2

    root = pathlib.Path(a.fixtures)
    spec = json.loads((root / "pairs.json").read_text())
    bad = 0
    for stem, im in spec["images"].items():
        rgb = read_rgb(fetch(im["url"], im["sha256"], pathlib.Path(a.cache)))
        got = hashlib.sha256(rgb.tobytes()).hexdigest()
        if got != im["rgb_sha256"]:
            print(f"{stem}: decoded pixels sha256 {got}, expected {im['rgb_sha256']} "
                  f"(OpenCV {cv2.__version__} decodes this JPEG differently)")
            bad += 1
            continue
        d = root / "images" / stem
        d.mkdir(parents=True, exist_ok=True)
        cv2.imwrite(str(d / "image.png"), rgb[..., ::-1].copy(), [cv2.IMWRITE_PNG_COMPRESSION, 9])
        print(f"{stem}: {rgb.shape[1]}x{rgb.shape[0]}, pixels verified")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
