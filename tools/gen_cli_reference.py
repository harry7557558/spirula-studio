#!/usr/bin/env python3
"""Regenerate docs/cli/train-flags.md and docs/cli/sfm-flags.md.

Builds tools/cli_reference/dump_flags.cpp against src/ (a C++20 compiler and
the Vulkan headers, which src/sfm/ includes) and writes what it prints.

Usage:  python3 tools/gen_cli_reference.py [--check]

--check writes nothing and exits 1 when a generated file is out of date.
"""

import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUTPUTS = {"train": "docs/cli/train-flags.md", "sfm": "docs/cli/sfm-flags.md"}


def compiler():
    for cxx in (os.environ.get("CXX"), "c++", "g++", "clang++"):
        if cxx and shutil.which(cxx):
            return cxx
    sys.exit("gen_cli_reference: no C++ compiler found (set CXX)")


def build(exe):
    cmd = [compiler(), "-std=c++20", "-O0", "-w", "-I", str(ROOT / "src")]
    sdk = os.environ.get("VULKAN_SDK")
    if sdk:
        cmd += ["-I", str(Path(sdk) / "include")]
    cmd += [str(ROOT / "tools/cli_reference/dump_flags.cpp"), "-o", str(exe)]
    if subprocess.run(cmd).returncode != 0:
        sys.exit("gen_cli_reference: dump_flags.cpp did not build")


def main():
    check = "--check" in sys.argv[1:]
    stale = []
    with tempfile.TemporaryDirectory() as tmp:
        exe = Path(tmp) / "dump_flags"
        build(exe)
        for which, rel in OUTPUTS.items():
            text = subprocess.run([str(exe), which], check=True, capture_output=True,
                                  encoding="utf-8").stdout
            path = ROOT / rel
            old = path.read_text(encoding="utf-8") if path.exists() else None
            if old == text:
                continue
            stale.append(rel)
            if not check:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(text, encoding="utf-8", newline="\n")
    for rel in stale:
        print(("out of date: " if check else "wrote ") + rel)
    return 1 if check and stale else 0


if __name__ == "__main__":
    sys.exit(main())
