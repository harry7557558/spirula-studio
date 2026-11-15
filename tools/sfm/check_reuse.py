#!/usr/bin/env python3
"""Check byte-identical fresh/reused SfM models on a supplied image set.

Usage: check_reuse.py EXE IMAGES NEW_WORKSPACE [-- extra sfm auto options]
A telemetry manifest exercises sensor calibration across the cache boundary.
"""
import argparse
import hashlib
from pathlib import Path
import subprocess


def digest(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('images', type=Path)
    parser.add_argument('workspace', type=Path)
    parser.add_argument('options', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    ws = args.workspace.resolve()
    ws.mkdir(parents=True, exist_ok=False)
    options = args.options
    if options[:1] == ['--']:
        options = options[1:]
    cmd = [str(args.exe.resolve()), 'sfm', 'auto', str(args.images.resolve()), '-o', str(ws), *options]
    snapshots = {}
    for arm, policy in [('fresh', 'redo'), ('reuse', 'keep')]:
        with (ws / (arm + '.log')).open('wb') as log:
            subprocess.run(cmd + ['--reuse-features', policy, '--reuse-matches', policy],
                           stdout=log, stderr=subprocess.STDOUT, check=True)
        sparse = ws / 'sparse'
        if not (sparse / '0' / 'images.bin').is_file():
            raise AssertionError(f'{arm}: no reconstructed model; see {ws / (arm + ".log")}')
        snapshots[arm] = {str(p.relative_to(sparse)): digest(p)
                          for p in sparse.rglob('*') if p.is_file()}
        snapshots[arm]['matches.bin'] = digest(ws / 'matches.bin')
        sparse.rename(ws / (arm + '-sparse'))
    different = sorted(k for k in snapshots['fresh'].keys() | snapshots['reuse'].keys()
                       if snapshots['fresh'].get(k) != snapshots['reuse'].get(k))
    if different:
        raise AssertionError('fresh/reuse differ: ' + ', '.join(different))
    print(f'PASS: {len(snapshots["fresh"])} fresh/reuse artifacts byte-identical')


if __name__ == '__main__':
    main()
