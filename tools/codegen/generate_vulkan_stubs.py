#!/usr/bin/env python3
"""Generate throwing stubs for kernel-launch functions the Vulkan backend has
not implemented yet, so the portable engine layer links against
ss_backend_vulkan today and loses stubs one module at a time as ports
land.

Usage:
    python3 generate_vulkan_stubs.py <build-vulkan-dir> <output.cpp>

The script link-probes csrc_portable's objects against
libss_backend_vulkan.a, collects the undefined C++ symbols, locates each
function's declaration in the per-kernel src/kernels/**/*.cuh headers, and emits one
throwing definition per function. Functions with no header declaration (class
methods, templates) are listed in the generated file's header comment for
manual stubbing in TrainingStubsManual.cpp.

Run it again whenever the engine layer gains new kernel calls or a port
removes the need for a stub (a ported symbol would otherwise be defined
twice).
"""

import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SRC = REPO / "src"

# Symbols intentionally not auto-stubbed (defined manually or genuinely
# external, e.g. OpenMP runtime which the executable links instead).
SKIP_PREFIXES = ("GOMP_", "omp_", "__kmpc")


def probe_undefined(build_dir: Path) -> list[str]:
    objs = sorted(
        (build_dir / "CMakeFiles" / "csrc_portable.dir").rglob("*.o"))
    lib = build_dir / "libss_backend_vulkan.a"
    if not objs or not lib.exists():
        sys.exit("build csrc_portable and ss_backend_vulkan first")
    probe = build_dir / "_stub_probe.cpp"
    probe.write_text("int main(){return 0;}\n")
    r = subprocess.run(
        ["g++", "-o", "/dev/null", str(probe)] + [str(o) for o in objs] +
        [str(lib), "-lvulkan", "-lpthread"],
        capture_output=True, text=True)
    syms = set()
    for line in r.stderr.splitlines():
        m = re.search(r"undefined reference to [`'](.+)'", line)
        if m:
            syms.add(m.group(1))
    return sorted(syms)


def demangle(symbols: list[str]) -> dict[str, str]:
    out = subprocess.run(["c++filt"], input="\n".join(symbols),
                         capture_output=True, text=True).stdout.splitlines()
    return dict(zip(symbols, out))


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)
    return text


def find_declaration(headers: dict[str, str], name: str):
    """Return (header, declaration) for a free function `name`."""
    pat = re.compile(r"\b" + re.escape(name) + r"\s*\(")
    for hdr, text in headers.items():
        for m in pat.finditer(text):
            # walk forward to the matching ')' then expect ';'
            i = m.end() - 1
            depth = 0
            while i < len(text):
                if text[i] == "(":
                    depth += 1
                elif text[i] == ")":
                    depth -= 1
                    if depth == 0:
                        break
                i += 1
            tail = text[i + 1:i + 32].lstrip()
            if not tail.startswith(";"):
                continue  # a call or a definition, not a declaration
            # walk backward to the previous ';', '}' or '{'
            j = m.start()
            while j > 0 and text[j - 1] not in ";}{#":
                j -= 1
            decl = text[j:i + 1]
            if j > 0 and text[j - 1] == "#":
                # stopped inside a preprocessor directive: drop its line
                decl = decl.split("\n", 1)[1] if "\n" in decl else decl
            decl = decl.strip()
            if decl.startswith("template"):
                return None  # templates need manual explicit instantiation
            if "typedef" in decl or "using " in decl:
                continue
            # a call site (`name(args);` inside an inline function) has no
            # return type before the name — not a declaration
            if decl.startswith(name):
                continue
            return hdr, decl
    return None


def strip_default_args(decl: str) -> str:
    # remove `= <default>` up to the next ',' or ')' at paren depth 1
    out, depth, i = [], 0, 0
    while i < len(decl):
        c = decl[i]
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
        if c == "=" and depth >= 1:
            j = i
            d2 = 0
            while j < len(decl):
                if decl[j] in "([{":
                    d2 += 1
                elif decl[j] in ")]}":
                    if d2 == 0:
                        break
                    d2 -= 1
                elif decl[j] == "," and d2 == 0:
                    break
                j += 1
            i = j
            continue
        out.append(c)
        i += 1
    return "".join(out)


def anonymize_params(decl: str) -> str:
    return decl  # parameter names are fine in definitions


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    build_dir = Path(sys.argv[1])
    out_path = Path(sys.argv[2])

    symbols = probe_undefined(build_dir)
    dem = demangle(symbols)

    headers = {}
    for hdr in sorted(SRC.rglob("*.cuh")) + sorted(SRC.rglob("*.h")):
        headers[hdr.name] = strip_comments(hdr.read_text())

    names = {}
    for sym, d in dem.items():
        if d.startswith(SKIP_PREFIXES) or sym.startswith(SKIP_PREFIXES):
            continue
        base = d.split("(")[0].strip()
        names.setdefault(base, d)

    stubs = []       # (header, name, decl)
    manual = []      # demangled names needing manual stubs
    used_headers = set()
    for base in sorted(names):
        if "::" in base or "<" in base:
            manual.append(names[base])
            continue
        hit = find_declaration(headers, base)
        if hit is None:
            manual.append(names[base])
            continue
        hdr, decl = hit
        used_headers.add(hdr)
        stubs.append((hdr, base, strip_default_args(decl)))

    lines = []
    lines.append("// GENERATED by tools/codegen/generate_vulkan_stubs.py — "
                 "DO NOT EDIT.")
    lines.append("//")
    lines.append("// Throwing stubs for kernel-launch functions the Vulkan "
                 "backend does not")
    lines.append("// implement yet (training/meshing phases). They satisfy "
                 "the linker for the")
    lines.append("// portable engine layer; calling one throws. Regenerate "
                 "after porting a")
    lines.append("// module (a ported symbol must lose its stub) or when the "
                 "engine grows new")
    lines.append("// kernel calls.")
    if manual:
        lines.append("//")
        lines.append("// NOT auto-stubbed (templates / class methods) — "
                     "kept in TrainingStubsManual.cpp:")
        for d in manual:
            lines.append(f"//   {d}")
    lines.append("")
    for hdr in sorted(used_headers):
        lines.append(f"#include <{hdr}>")
    lines.append("")
    lines.append("#include <stdexcept>")
    lines.append("#include <string>")
    lines.append("")
    if stubs:  # else the unused helper trips -Wunused-function
        lines.append("namespace {")
        lines.append("[[noreturn]] void _vk_stub(const char* name) {")
        lines.append("    throw std::runtime_error(std::string(\"Vulkan backend: "
                     "\") + name +")
        lines.append("                             \" is not implemented yet "
                     "(training phase)\");")
        lines.append("}")
        lines.append("}  // namespace")
        lines.append("")
    for hdr, base, decl in stubs:
        lines.append(f"// from {hdr}")
        lines.append(f"{decl} {{ _vk_stub(\"{base}\"); }}")
        lines.append("")

    out_path.write_text("\n".join(lines))
    print(f"wrote {len(stubs)} stubs to {out_path}"
          f" ({len(manual)} manual symbols listed)")


if __name__ == "__main__":
    main()
