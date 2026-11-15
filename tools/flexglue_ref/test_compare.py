#!/usr/bin/env python3
# /// script
# requires-python = ">=3.10"
# dependencies = ["numpy"]
# ///
"""compare.py's index scoring on a synthetic pair with duplicate keypoints.

    uv run tools/flexglue_ref/test_compare.py

Image 1 repeats 50 of its keypoints, so a match to either copy is correct and
indices are scored modulo copies (pairs differing over the reference's pairs).
The torch-vs-torch floor must be in that same unit: a floor counted in raw rows
would let real errors through, which is what these tests pin.
"""
import argparse
import contextlib
import io
import json
import pathlib
import sys
import tempfile
import unittest

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import compare  # noqa: E402

N0, N1, DUP = 200, 200, 50   # image 1's rows 100..149 repeat rows 0..49
P = 100                      # reference pairs: query i -> keypoint i, i < 100


def keypoints():
    rng = np.random.default_rng(0)
    k0 = rng.uniform(10, 500, (N0, 2)).astype(np.float32)
    k1 = rng.uniform(10, 500, (N1, 2)).astype(np.float32)
    k1[100:100 + DUP] = k1[:DUP]
    return k0, k1


def reference_matches():
    m = np.full(N0, -1, np.int64)
    m[:P] = np.arange(P)
    return m


def write_tree(root, kpts, matches, floor, units):
    """A one-pair full-tier reference and a dump of `matches` beside it."""
    pdir = root / "ref" / "pairs" / "p"
    pdir.mkdir(parents=True)
    ref = {
        "kpts0": kpts[0], "kpts1": kpts[1],
        "colmap.matches0": reference_matches(),
        "colmap.mscores0": np.where(reference_matches() >= 0, 0.9, 0.0).astype(np.float32),
        "colmap.covisibility_score": np.array([0.9], np.float32),
        "colmap.stop_layer": np.array([9], np.int64),
        "colmap_decay02.stop_layer": np.array([9], np.int64),
        "colmap_decay02.matches0": reference_matches(),
    }
    for name, arr in ref.items():
        np.save(pdir / f"{name}.npy", arr)
    fl = {"worst": {"colmap.matches0": floor}} if floor is not None else {"worst": {}}
    if units:
        fl["discrete"] = units
    (pdir / "manifest.json").write_text(json.dumps(dict(
        finished=True, exit_status=0, images=["a", "b"], files={n: "full" for n in ref},
        image_files={"a": {}, "b": {}}, floor=fl)))
    dump = root / "dump"
    dump.mkdir()
    for name, arr in ref.items():
        out = matches if name == "colmap.matches0" else arr
        np.save(dump / f"{name}.npy", np.asarray(out, np.float32))   # the C++ side dumps f32
    (dump / "manifest.json").write_text(json.dumps(dict(finished=True, exit_status=0,
                                                        files=list(ref))))
    return root / "ref", dump


def score(matches, floor=None, units=None):
    """(the colmap.matches0 row, check's exit status)."""
    with tempfile.TemporaryDirectory() as d:
        root = pathlib.Path(d)
        ref, dump = write_tree(root, keypoints(), matches, floor, units)
        out = root / "rows.json"
        a = argparse.Namespace(ref=str(ref), pair="p", dump=str(dump), json=str(out),
                               expect_gated=None)
        with contextlib.redirect_stdout(io.StringIO()):
            status = compare.check(a)
        rows = json.loads(out.read_text())["rows"]
    row = next(r for r in rows if r["stage"] == "colmap.matches0" and r["ok"] is not None)
    return row, status


def other_copy():
    """The reference's matches with every duplicated target swapped for its twin."""
    m = reference_matches()
    m[:DUP] += 100
    return m


def wrong(k):
    """The reference's matches with k queries sent to a keypoint that is no copy."""
    m = reference_matches()
    m[P - k:P] = np.arange(150, 150 + k)   # rows 150+ are unique, and unmatched
    return m


def old_bar(floor_rows):
    """The bar the earlier compare.py set: a raw-row floor over the pair count."""
    return max(compare.DISCRETE_FRAC, compare.FLOOR_FACTOR * floor_rows / P)


class CanonicalScoring(unittest.TestCase):
    def test_the_other_copy_is_correct(self):
        row, status = score(other_copy())
        self.assertEqual(row["metric"], "pairs differing, mod copies")
        self.assertEqual(row["value"], 0.0)
        self.assertEqual(status, 0)

    def test_the_fixture_needs_copy_identification(self):
        # Row by row the other-copy dump differs on DUP of N0 rows, far over any bar:
        # only the canonical metric can pass it.
        diff = (other_copy() != reference_matches()).sum() / N0
        self.assertGreater(diff, 10 * compare.DISCRETE_FRAC)

    def test_real_errors_are_counted_both_ways(self):
        row, _ = score(wrong(6))
        self.assertAlmostEqual(row["value"], 12 / P)   # 6 lost + 6 gained pairs

    def test_no_floor_fails_real_errors(self):
        row, status = score(wrong(6))
        self.assertEqual(row["bar"], compare.DISCRETE_FRAC)
        self.assertFalse(row["ok"])
        self.assertEqual(status, 1)

    def test_a_raw_row_floor_does_not_loosen_the_canonical_bar(self):
        # 30 rows of argmin ties between copies, torch against itself: zero pairs
        # once copies are identified, so it is no evidence of any pair-level noise.
        floor_rows = 30
        self.assertGreaterEqual(old_bar(floor_rows), 12 / P,
                                "the fixture must be one the old bar would have passed")
        row, status = score(wrong(6), floor=floor_rows, units=None)
        self.assertEqual(row["bar"], compare.DISCRETE_FRAC)
        self.assertIsNone(row["floor"])
        self.assertFalse(row["ok"])
        self.assertEqual(status, 1)

    def test_a_metric_floor_sets_the_bar(self):
        floor = 0.05   # pairs differing over reference pairs, torch against itself
        row, status = score(wrong(4), floor=floor, units="metric")
        self.assertAlmostEqual(row["bar"], compare.FLOOR_FACTOR * floor)
        self.assertAlmostEqual(row["value"], 8 / P)
        self.assertTrue(row["ok"])
        self.assertEqual(status, 0)
        row, status = score(wrong(6), floor=floor, units="metric")
        self.assertFalse(row["ok"])
        self.assertEqual(status, 1)


class FloorRecording(unittest.TestCase):
    def test_the_reference_records_floors_in_the_metric_units(self):
        # flexglue_ref.py's floor for a torch run that picked the other copies:
        # 0 pairs, where a raw count would have recorded DUP rows.
        import flexglue_ref
        k0, k1 = keypoints()
        ref_pair = {"kpts0": k0, "kpts1": k1, "colmap.matches0": reference_matches()}
        draw = dict(ref_pair, **{"colmap.matches0": other_copy()})
        imgs = [{st: np.zeros(4, np.float32) for st in flexglue_ref.DESC_STAGES}] * 2
        out = flexglue_ref.metrics(imgs, draw, imgs, ref_pair)
        self.assertEqual(out["colmap.matches0"], 0.0)
        draw["colmap.matches0"] = wrong(6)
        self.assertAlmostEqual(flexglue_ref.metrics(imgs, draw, imgs, ref_pair)["colmap.matches0"],
                               12 / P)


class PlainScoring(unittest.TestCase):
    def test_a_raw_row_floor_converts_on_the_plain_path(self):
        # With no duplicates the metric is rows differing over rows, so a raw count
        # divides exactly by that same denominator.
        k0, k1 = keypoints()
        canon = [compare.first_copy(k0), None]
        value, metric, n = compare.discrete_metric("colmap.matches0", wrong(3),
                                                   reference_matches(), canon)
        self.assertEqual((metric, n), ("fraction differing", N0))
        self.assertAlmostEqual(value, 3 / N0)
        ref = argparse.Namespace(floor={"colmap.matches0": 4}, floor_units="rows")
        bar, fl = compare.discrete_bar(ref, "colmap.matches0", metric, n)
        self.assertAlmostEqual(fl, 4 / N0)
        self.assertAlmostEqual(bar, max(compare.DISCRETE_FRAC, compare.FLOOR_FACTOR * 4 / N0))


if __name__ == "__main__":
    unittest.main()
