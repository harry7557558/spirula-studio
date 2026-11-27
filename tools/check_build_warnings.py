#!/usr/bin/env python3
"""Run a build command, pass its output through, and fail after it on any
compiler warning. build_develop --warnings-at-end uses it in CI, where -Werror
would stop the build at the first warning and leave the rest out of the log.

    python3 tools/check_build_warnings.py -- cmake --build build_vulkan -j8
"""
import os
import re
import subprocess
import sys

# GCC, Clang and ld "x: warning:", MSVC "warning C4100:" / "warning D9025 :",
# nvcc "warning #177-D:", nvcc and ptxas "warning :", slangc "warning 30081:".
WARNING = re.compile(r'(?<![\w-])warning(?: [A-Z]*\d+| #\d+(?:-D)?)? ?:')


def is_warning(line):
    return WARNING.search(line) is not None


def run(cmd, out):
    """Stream cmd's output to out (binary); return (exit code, warning lines)."""
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    warnings = []
    for raw in iter(proc.stdout.readline, b''):
        out.write(raw)
        out.flush()
        line = raw.decode('utf-8', 'replace').rstrip('\r\n')
        if is_warning(line):
            warnings.append(line)
    return proc.wait(), warnings


def main(argv):
    if argv[:1] == ['--']:
        argv = argv[1:]
    if not argv:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    code, warnings = run(argv, sys.stdout.buffer)
    if warnings:
        roots = {os.getcwd() + os.sep, os.getcwd().replace(os.sep, '/') + '/'}  # nvcc on Windows prints C:/...
        for root in roots:
            warnings = [w.replace(root, '') for w in warnings]
        distinct = list(dict.fromkeys(warnings))
        print(f'\n{len(distinct)} distinct warning lines ({len(warnings)} in all); '
              'the log above shows each in context:')
        for w in distinct:
            print('  ' + w)
    sys.stdout.flush()
    if code != 0:
        return code
    if warnings:
        print('Build finished with warnings; failing it.', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
