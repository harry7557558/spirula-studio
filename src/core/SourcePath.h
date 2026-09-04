#pragma once

// SS_FILE -- __FILE__ trimmed to a repo-relative path, "src/core/Common.cuh".
//
// Both build systems hand the compiler absolute source paths, so a bare
// __FILE__ in an error message names a directory on the machine that built the
// binary, which is not one whoever hit the error can open. The trim and the
// separator fold happen at compile time, so the absolute path never reaches
// the binary either.
//
// Header-only for the same reason src/core/Env.h is: the standalone tool
// binaries link neither the engine nor each other.

#include <cstddef>

// The repository root, no trailing separator (cmake/SsOptions.cmake). Empty
// leaves __FILE__ alone, so a build that does not set it still compiles.
#ifndef SS_SOURCE_ROOT
#define SS_SOURCE_ROOT ""
#endif

namespace spirula {
namespace detail {

// Windows spells one path several ways; the comparison folds both sides.
constexpr char path_fold(char c) {
    return c == '\\' ? '/' :
           (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

constexpr std::size_t root_len(const char *root) {
    std::size_t n = 0;
    while (root[n])
        ++n;
    while (n && (root[n - 1] == '/' || root[n - 1] == '\\'))
        --n;
    return n;
}

// Length of the `root` prefix of `p`, the separators after it included; 0 if
// `p` does not start with it. The root has to end on a component boundary, or
// a sibling of the repository directory would trim to a path inside it.
constexpr std::size_t root_prefix(const char *p, const char *root) {
    const std::size_t n = root_len(root);
    if (!n)
        return 0;
    for (std::size_t i = 0; i < n; ++i)
        if (path_fold(p[i]) != path_fold(root[i]))
            return 0;
    if (p[n] != '/' && p[n] != '\\')
        return 0;
    std::size_t i = n;
    while (p[i] == '/' || p[i] == '\\')
        ++i;
    return i;
}

// `root` is a parameter so src/core/tests/source_path.cpp can drive it with
// paths this platform never produces.
template <std::size_t N>
struct SourcePath {
    char v[N];
    constexpr SourcePath(const char (&p)[N], const char *root = SS_SOURCE_ROOT)
        : v{} {
        std::size_t j = 0;
        for (std::size_t i = root_prefix(p, root); i < N; ++i, ++j)
            v[j] = p[i] == '\\' ? '/' : p[i];
    }
};

}  // namespace detail
}  // namespace spirula

// Host code only. The static is what lets the trimmed string outlive the
// expression it is used in.
#define SS_FILE                                                                \
    ([]() -> const char * {                                                    \
        static constexpr ::spirula::detail::SourcePath<sizeof(__FILE__)>       \
            _ss_source_path(__FILE__);                                         \
        return _ss_source_path.v;                                              \
    }())
