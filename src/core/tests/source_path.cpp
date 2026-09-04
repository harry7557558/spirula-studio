// source_path -- the SS_FILE trim (core/SourcePath.h) against the path
// spellings each toolchain produces, none of which one host can generate.

#include "core/SourcePath.h"

#include <cstdio>
#include <cstring>

using spirula::detail::SourcePath;

namespace {

int g_failures = 0;

template <std::size_t N>
void expect(const char (&path)[N], const char *root, const char *want) {
    const SourcePath<N> got(path, root);
    if (std::strcmp(got.v, want) == 0) {
        std::printf("ok   %s -> %s\n", path, got.v);
    } else {
        std::printf("BAD  %s -> %s (want %s)\n", path, got.v, want);
        g_failures++;
    }
}

constexpr bool same(const char *a, const char *b) {
    for (; *a && *b; ++a, ++b)
        if (*a != *b)
            return false;
    return *a == *b;
}

constexpr char kWinPath[] = "C:\\GitHub\\spirula\\src\\core\\Common.cuh";
constexpr SourcePath<sizeof(kWinPath)> kTrimmed(kWinPath, "c:/github/spirula");
static_assert(same(kTrimmed.v, "src/core/Common.cuh"), "trim is not constant");

}  // namespace

int main() {
    // MSVC + Ninja: backslashes throughout, and a drive letter whose case is
    // not the one CMake recorded.
    expect("C:\\GitHub\\spirula\\src\\core\\Common.cuh", "C:/GitHub/spirula",
           "src/core/Common.cuh");
    expect("c:\\github\\spirula\\src\\core\\Common.cuh", "C:/GitHub/spirula",
           "src/core/Common.cuh");
    // GCC / Clang / nvcc.
    expect("/opt/build/spirula/src/nn/vk/Context.cpp", "/opt/build/spirula",
           "src/nn/vk/Context.cpp");
    // A root spelled with a trailing separator must not eat the one after it.
    expect("/opt/build/spirula/src/a.cpp", "/opt/build/spirula/", "src/a.cpp");
    // Outside the tree, or no root configured: keep the whole path rather
    // than trim it to one that does not exist.
    expect("/opt/vulkansdk/include/x.h", "/opt/build/spirula",
           "/opt/vulkansdk/include/x.h");
    expect("/opt/build/spirula/src/a.cpp", "", "/opt/build/spirula/src/a.cpp");
    expect("/opt/build/spirula-splat/src/a.cpp", "/opt/build/spirula",
           "/opt/build/spirula-splat/src/a.cpp");

    if (std::strcmp(SS_FILE, "src/core/tests/source_path.cpp") != 0) {
        std::printf("BAD  SS_FILE here is %s\n", SS_FILE);
        g_failures++;
    } else {
        std::printf("ok   SS_FILE here -> %s\n", SS_FILE);
    }

    std::printf("\n%d failures\n", g_failures);
    return g_failures ? 1 : 0;
}
