#!/usr/bin/env python3
"""Generate the TIFF conformance corpus src/core/tests/tiff_decode.cpp reads.

Dev-time only, and the output is not committed. Files are written by two
independent encoders -- tifffile (imagecodecs) and Pillow (libtiff) -- and the
reference for each is what tifffile decodes, mapped to what the reader hands
back: grey widened to RGB, WhiteIsZero undone, half and double as float32.

    pip install tifffile imagecodecs pillow
    python tools/gen_tiff_cases.py /tmp/tiff_cases
    ./build_vulkan/tiff_decode /tmp/tiff_cases

Cases whose name starts with `err_` must be REFUSED by the reader; every other
case must decode to its .ref companion within the listed tolerance (0 = exact).
"""
import os
import sys

import numpy as np
import tifffile
from PIL import Image


def base(h, w, seed=0):
    """Smooth structure (so the predictors and LZW do real work) plus noise."""
    rng = np.random.default_rng(seed)
    y, x = np.mgrid[0:h, 0:w]
    s = np.sin(x / 7.0) * np.cos(y / 5.0) * 0.5 + 0.5
    return np.stack([np.clip(s * (0.5 + 0.2 * k) + rng.random((h, w)) * 0.1, 0, 1)
                     for k in range(3)], axis=-1)


def as_type(v, dtype):
    if dtype == np.uint8:
        return np.round(v * 255).astype(np.uint8)
    if dtype == np.uint16:
        return np.round(v * 65535).astype(np.uint16)
    out = v * 4.0           # floats keep values above 1
    out[2:5, 3:9] = 1e5 if dtype != np.float16 else 6e4
    return out.astype(dtype)


def reference(data, channels, white_is_zero=False, extra_is_alpha=True):
    """`data` as tifffile decoded it, laid out the way the reader returns it."""
    a = np.asarray(data)
    if a.dtype in (np.float16, np.float64):
        a = a.astype(np.float32)
    if a.ndim == 2:
        a = a[..., None]
    if not extra_is_alpha:
        a = a[..., :1] if a.shape[-1] == 2 else a[..., :3]
    colour = 1 if a.shape[-1] in (1, 2) else 3
    has_alpha = a.shape[-1] in (2, 4)
    if white_is_zero:
        top = 1.0 if a.dtype == np.float32 else np.iinfo(a.dtype).max
        a = a.copy()
        a[..., :colour] = top - a[..., :colour]
    rgb = np.repeat(a[..., :1], 3, axis=-1) if colour == 1 else a[..., :3]
    if channels == 3:
        return np.ascontiguousarray(rgb)
    top = 1.0 if a.dtype == np.float32 else np.iinfo(a.dtype).max
    alpha = a[..., -1:] if has_alpha else np.full(rgb.shape[:2] + (1,), top, a.dtype)
    return np.ascontiguousarray(np.concatenate([rgb, alpha], axis=-1))


SAMPLE = {np.dtype(np.uint8): "u8", np.dtype(np.uint16): "u16", np.dtype(np.float32): "f32"}


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "/tmp/tiff_cases"
    os.makedirs(out, exist_ok=True)
    manifest = []

    def case(name, write, channels=3, white_is_zero=False, extra_is_alpha=True, tol=0,
             planar=False):
        path = os.path.join(out, name + ".tif")
        write(path)
        if name.startswith("err_"):
            manifest.append(f"{name} 0 0 {channels} u8 0")
            return
        data = tifffile.imread(path)
        if planar:
            data = np.moveaxis(data, 0, -1)
        ref = reference(data, channels, white_is_zero, extra_is_alpha)
        ref.tofile(os.path.join(out, name + ".ref"))
        h, w = ref.shape[:2]
        manifest.append(f"{name} {w} {h} {channels} {SAMPLE[ref.dtype]} {tol}")

    def tf(data, **kw):
        return lambda p: tifffile.imwrite(p, data, **kw)

    H, W = 61, 83            # odd sizes, so strips and tiles have ragged edges
    img = base(H, W)
    for dt in (np.uint8, np.uint16, np.float16, np.float32, np.float64):
        d = as_type(img, dt)
        tag = np.dtype(dt).name
        for comp in (None, "lzw", "deflate", "packbits"):
            if comp == "packbits" and dt not in (np.uint8,):
                continue
            ctag = comp or "none"
            case(f"rgb_{tag}_{ctag}", tf(d, photometric="rgb", compression=comp,
                                         rowsperstrip=7))
            if comp in ("lzw", "deflate"):
                case(f"rgb_{tag}_{ctag}_pred", tf(d, photometric="rgb", compression=comp,
                                                  predictor=True, rowsperstrip=9))
                case(f"rgb_{tag}_{ctag}_tiled", tf(d, photometric="rgb", compression=comp,
                                                   predictor=True, tile=(32, 48)))
                case(f"rgb_{tag}_{ctag}_planar",
                     tf(np.moveaxis(d, -1, 0), photometric="rgb", compression=comp,
                        predictor=True, planarconfig="separate", rowsperstrip=11),
                     planar=True)
                case(f"rgb_{tag}_{ctag}_be", tf(d, photometric="rgb", compression=comp,
                                                predictor=True, byteorder=">"))
        case(f"rgb_{tag}_bigtiff", tf(d, photometric="rgb", bigtiff=True,
                                      compression="deflate", tile=(16, 16)))
        case(f"grey_{tag}", tf(d[..., 1], photometric="minisblack", compression="lzw"))

    rgba = np.concatenate([as_type(img, np.uint16),
                           as_type(img[..., :1], np.uint16)], axis=-1)
    case("rgba_u16_unassoc", tf(rgba, photometric="rgb", extrasamples=["unassalpha"]), 4)
    case("rgba_u16_as_rgb", tf(rgba, photometric="rgb", extrasamples=["unassalpha"]), 3)
    case("rgbx_u8_unspecified",
         tf(np.concatenate([as_type(img, np.uint8), as_type(img[..., :1], np.uint8)], -1),
            photometric="rgb", extrasamples=["unspecified"]), 4, extra_is_alpha=False)
    ga = np.stack([as_type(img[..., 0], np.uint8), as_type(img[..., 1], np.uint8)], -1)
    case("greya_u8", tf(ga, photometric="minisblack", extrasamples=["unassalpha"]), 4)
    case("miniswhite_u8", tf(as_type(img[..., 0], np.uint8), photometric="miniswhite"),
         white_is_zero=True)
    case("one_strip_u16", tf(as_type(img, np.uint16), photometric="rgb",
                             compression="lzw", rowsperstrip=H))
    big = base(700, 1100, seed=3)
    case("large_u16_lzw", tf(as_type(big, np.uint16), photometric="rgb",
                             compression="lzw", predictor=True))
    case("large_f32_tiled", tf(as_type(big, np.float32), photometric="rgb",
                               compression="deflate", predictor=True, tile=(256, 256)))

    # libtiff's own encoders, through Pillow.
    u8 = Image.fromarray(as_type(big, np.uint8))
    for comp in ("tiff_lzw", "tiff_adobe_deflate", "packbits", "raw"):
        case(f"pil_rgb_u8_{comp}", lambda p, c=comp: u8.save(p, compression=c))
    case("pil_grey_u16_lzw", lambda p: Image.fromarray(as_type(big[..., 0], np.uint16))
         .save(p, compression="tiff_lzw"))

    case("err_jpeg", tf(as_type(img, np.uint8), photometric="rgb", compression="jpeg"))
    case("err_zstd", tf(as_type(img, np.uint8), photometric="rgb", compression="zstd"))
    case("err_int16", tf((as_type(img, np.uint16) // 2).astype(np.int16),
                         photometric="rgb"))
    case("err_uint32", tf(as_type(img, np.uint16).astype(np.uint32), photometric="rgb"))
    case("err_palette", tf(as_type(img[..., 0], np.uint8), photometric="palette",
                           colormap=np.tile(np.arange(256, dtype=np.uint16) * 257, (3, 1))))
    case("err_cmyk", tf(np.concatenate([as_type(img, np.uint8)] * 2, -1)[..., :4],
                        photometric="separated"))
    case("err_truncated", lambda p: (tifffile.imwrite(p, as_type(big, np.uint8),
                                                      photometric="rgb"),
                                     os.truncate(p, os.path.getsize(p) // 2)))

    with open(os.path.join(out, "cases.txt"), "w") as f:
        f.write("\n".join(manifest) + "\n")
    print(f"{len(manifest)} cases in {out}")


if __name__ == "__main__":
    main()
