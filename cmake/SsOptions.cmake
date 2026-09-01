# Build options, backend selection, the source-tree paths every other module
# builds on, and the helpers they share. Included first by the top-level
# CMakeLists.txt.

# ---------------------------------------------------------------------------
# ss_write_if_different(<path> <content>)
#
# file(WRITE) rewrites unconditionally. The dev build scripts re-run
# `cmake -B build` on every invocation, so a generated file written with
# file(WRITE) gets a fresh mtime every time -- and ninja then rebuilds
# everything downstream of it on a tree with no edits at all. Write only when
# the bytes actually differ, so an unchanged generated file keeps its mtime.
#
# Callers pass ONE content string: file(WRITE) concatenates its extra arguments,
# and comparing against what is on disk needs the whole thing up front anyway.
# ---------------------------------------------------------------------------
function(ss_write_if_different path content)
    if(EXISTS "${path}")
        file(READ "${path}" _existing)
        string(COMPARE EQUAL "${_existing}" "${content}" _same)
        if(_same)
            return()
        endif()
    endif()
    file(WRITE "${path}" "${content}")
endfunction()

# ---------------------------------------------------------------------------
# Deprecated SSPLAT_* option names
#
# The prefix became SS_ when spirulae-splat became Spirula Studio. A cached
# SSPLAT_* value from an older build tree, or a scripted -DSSPLAT_*, still
# works and says so. Delete this block -- and the SSPLAT_ fallback in
# src/core/Env.h -- one release after the rename.
#
# SSPLAT_NO_TORCH / SSPLAT_WITH_TORCH deliberately get no alias: there is no
# Torch build left to select, and a warning about an unknown option is
# friendlier than silently ignoring one.
# ---------------------------------------------------------------------------
set(SS_LEGACY_OPTIONS
    BUILD_CLI BUILD_GUI BUILD_SFM BUILD_SAM BUILD_BACKEND_TESTS
    BACKEND SEPARATE_TOOLS DEBUG_SYMBOLS ENABLE_PATENTED
    SLANGC SFM_REALS SFM_LOSSES)
foreach(opt ${SS_LEGACY_OPTIONS})
    if(DEFINED SSPLAT_${opt} AND NOT DEFINED SS_${opt})
        message(DEPRECATION
            "SSPLAT_${opt} is deprecated; use SS_${opt}. "
            "The alias will be removed in the release after next.")
        set(SS_${opt} "${SSPLAT_${opt}}" CACHE STRING "" FORCE)
    endif()
endforeach()

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
# SS_ROOT is the repository root (the directory holding CMakeLists.txt).
# Everything else is expressed relative to it so the modules never care where
# they were included from.
# SS_SRC is also the include root: every local #include in the native
# tree is written relative to it (e.g. `#include "core/Common.cuh"`).
set(SS_ROOT    ${CMAKE_CURRENT_SOURCE_DIR})
set(SS_SRC     ${SS_ROOT}/src)
set(SS_SHADERS ${SS_SRC}/shaders)                      # shared Slang device math
set(SS_VK_SHADERS ${SS_SRC}/backend/vulkan/shaders)   # Vulkan-only entry points

# What SS_FILE trims off __FILE__ (src/core/SourcePath.h), so an error message
# from a shipped binary names src/... and not the build machine's directories.
add_compile_definitions(SS_SOURCE_ROOT="${SS_ROOT}")

# The version the apps report with --version. Declared here and nowhere else;
# it used to be read out of pyproject.toml, back when there was a package.
set(SS_VERSION "2026.8.28")

# The commit goes in it too, so a crash report names an exact tree without
# anyone having to bump a string by hand. Read at configure time, which is
# every dev build: build_develop.bash/.bat re-run `cmake -B build`.
find_package(Git QUIET)
set(SS_COMMIT "")
if(GIT_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short=7 HEAD
                    WORKING_DIRECTORY ${SS_ROOT}
                    OUTPUT_VARIABLE SS_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET)
    if(SS_COMMIT)
        execute_process(COMMAND ${GIT_EXECUTABLE} diff --quiet HEAD
                        WORKING_DIRECTORY ${SS_ROOT}
                        RESULT_VARIABLE SS_GIT_CLEAN ERROR_QUIET)
        if(NOT SS_GIT_CLEAN STREQUAL "0")
            string(APPEND SS_COMMIT "-dirty")
        endif()
        string(APPEND SS_VERSION " (${SS_COMMIT})")
    endif()
endif()

# ---------------------------------------------------------------------------
# Options
#
# Everything the build has goes into ONE executable, `spirula`, which dispatches
# on its first argument (`spirula sfm auto ...`); see src/app/Tools.h. These two
# options decide what is in it: the command-line tools, the window, or both.
# ---------------------------------------------------------------------------
option(SS_BUILD_CLI "Build the command-line tools into spirula" OFF)
option(SS_BUILD_GUI "Build the graphical application into spirula (fetches GLFW + Dear ImGui)" OFF)
option(SS_BUILD_BACKEND_TESTS "Build backend parity test tools" OFF)

# Also build spirula-sfm and spirula-sam as standalone executables. Same code
# and the same dispatcher (src/app/Main.cpp reads argv[0]), but built alone
# neither links the training engine, which is the point: spirula-sfm is 24 MB
# against the combined binary's 61 MB. The other tools are not offered
# separately -- they would be identical to `spirula`, and a separate GUI could
# not run reconstruction, which is this binary re-running itself.
option(SS_SEPARATE_TOOLS "Also build spirula-sfm / spirula-sam standalone" OFF)

# Debug symbols / line info are OFF by default: they bloat the binaries
# massively (nvcc host -g, CUDA cubin lineinfo/source-in-ptx, and slangc -g2
# in the embedded SPIR-V). Turn on for profiling/debugging builds.
option(SS_DEBUG_SYMBOLS "Emit debug symbols / line info (host -g, CUDA cubin lineinfo, SPIR-V -g2)" OFF)

# Embed PTX alongside the cubin in the CUDA fatbin. PTX is only useful for
# JIT onto an architecture the binary was NOT built for, and the CUDA build
# detects the local GPU's compute capability from nvidia-smi, so a dev build
# carries it for nothing -- it is a third of every object file, and the
# per-(camera model, distortion tier) kernel instantiations make that a third
# of a large binary. Turn it on for a redistributable build.
option(SS_CUDA_EMBED_PTX "Embed PTX in the CUDA fatbin for forward-compatible JIT" OFF)

# ---------------------------------------------------------------------------
# Compute backend selection
#
# cuda (default): the full build -- CUDA kernels,
# and the app targets.
# vulkan: the portable engine layer (Engine*.cpp + host support) against the
# Vulkan compute runtime (src/backend/vulkan/, see its README.md), built
# WITHOUT the CUDA toolkit. Produces the backend tests and `spirula` -- with
# the command-line tools always, and the GUI with SS_BUILD_GUI=ON.
# ---------------------------------------------------------------------------
set(SS_BACKEND "cuda" CACHE STRING "Compute backend: cuda | vulkan")
set_property(CACHE SS_BACKEND PROPERTY STRINGS cuda vulkan)

if(NOT SS_BACKEND STREQUAL "cuda" AND NOT SS_BACKEND STREQUAL "vulkan")
    message(FATAL_ERROR "SS_BACKEND must be 'cuda' or 'vulkan', got '${SS_BACKEND}'")
endif()

# ---------------------------------------------------------------------------
# How macOS gets Vulkan (cmake/SsVulkan.cmake)
#
# static (default): MoltenVK linked into the binary, which is then copyable to
# any Mac. loader: the installed Vulkan loader, the only way to reach
# validation layers.
# ---------------------------------------------------------------------------
set(SS_MACOS_VULKAN "static" CACHE STRING
    "macOS Vulkan linkage: static (MoltenVK in the binary) | loader")
set_property(CACHE SS_MACOS_VULKAN PROPERTY STRINGS static loader)

if(APPLE AND NOT SS_MACOS_VULKAN STREQUAL "static"
         AND NOT SS_MACOS_VULKAN STREQUAL "loader")
    message(FATAL_ERROR
        "SS_MACOS_VULKAN must be 'static' or 'loader', got '${SS_MACOS_VULKAN}'")
endif()

# ---------------------------------------------------------------------------
# Structure from Motion (src/sfm/)
#
# The native replacement for the COLMAP subprocess. It is Vulkan-only and needs
# the Vulkan loader + headers, so it is on by default only for the Vulkan build;
# a CUDA build can still opt in with -DSS_BUILD_SFM=ON if the Vulkan SDK is
# present. See cmake/SsSfm.cmake and src/sfm/README.md.
# ---------------------------------------------------------------------------
if(SS_BACKEND STREQUAL "vulkan")
    option(SS_BUILD_SFM "Build the SfM module (`spirula sfm`)" ON)
else()
    option(SS_BUILD_SFM "Build the SfM module (`spirula sfm`)" OFF)
endif()

# ---------------------------------------------------------------------------
# GPU inference (src/nn/) and segmentation (src/sam/)
#
# The native replacement for the reference/scripts/mask.py subprocess: SAM 2
# / SAM 3 on the same Vulkan + Slang stack as the SfM module, over a
# reusable inference layer. Same rule as SfM -- Vulkan-only, on by default
# only for the Vulkan build, opt-in for CUDA if the Vulkan SDK is present.
# See cmake/SsNn.cmake and src/nn/README.md.
# ---------------------------------------------------------------------------
if(SS_BACKEND STREQUAL "vulkan")
    option(SS_BUILD_SAM "Build the inference layer + SAM segmentation" ON)
else()
    option(SS_BUILD_SAM "Build the inference layer + SAM segmentation" OFF)
endif()

# ---------------------------------------------------------------------------
# Localization (src/i18n/, docs/i18n.md)
#
# The language set is declared once, in src/i18n/Languages.h, and read back out
# of it here rather than repeated -- adding a locale must be a one-file change.
# ---------------------------------------------------------------------------
file(READ ${SS_SRC}/i18n/Languages.h _ss_langs_h)
string(REGEX MATCHALL "X\\(([a-z_]+),[ \t]+\"" _ss_lang_hits "${_ss_langs_h}")
set(SS_LANGUAGES "")
foreach(hit ${_ss_lang_hits})
    string(REGEX REPLACE "X\\(([a-z_]+),.*" "\\1" lang "${hit}")
    list(APPEND SS_LANGUAGES ${lang})
endforeach()
if(NOT SS_LANGUAGES)
    message(FATAL_ERROR "Could not read SS_LANGUAGES out of src/i18n/Languages.h")
endif()
set_property(DIRECTORY ${SS_ROOT} APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS ${SS_SRC}/i18n/Languages.h)

# The language used when none is detected: headless runs, containers with no
# LANG, stripped Windows environments. NOT the same as "English" -- a build
# shipped as -DSS_DEFAULT_LANG=ja has to come up Japanese in those cases.
set(SS_DEFAULT_LANG "en" CACHE STRING "Locale used when none is detected")
set_property(CACHE SS_DEFAULT_LANG PROPERTY STRINGS ${SS_LANGUAGES})
if(NOT SS_DEFAULT_LANG IN_LIST SS_LANGUAGES)
    string(REPLACE ";" " " _ss_langs_pretty "${SS_LANGUAGES}")
    message(FATAL_ERROR
        "SS_DEFAULT_LANG='${SS_DEFAULT_LANG}' is not a language this build has.\n"
        "  Choose one of: ${_ss_langs_pretty}")
endif()

# This does NOT decide whether the UI renders in Japanese, Korean or Chinese.
# It always does: the four subset faces in assets/fonts/ are embedded
# unconditionally (422 KB, this program's own vocabulary in each region's own
# glyph forms), so there is no build of this GUI whose language picker cannot
# be read. See src/app/gui/Fonts.h.
#
# What this decides is the FULL faces, which cover the rest of Unicode -- the
# CJK that turns up in dataset paths, file names and typed mask prompts, none
# of which a subset of our own strings can anticipate. Each is 4-8 MB and they
# are NOT interchangeable: Han unification means the shared codepoints have
# different default glyph forms per region.
#
#   fetch  (default)  downloaded on first use of a CJK language, into the cache
#                     directory. The executable stays self-contained.
#   none              no download offered. The UI is unaffected; CJK file names
#                     may show boxes. For a build that must not touch the
#                     network.
#   sc|tc|jp|kr|all   ship the full face(s) beside the executable, for an
#                     offline regional build. Note this is a BUNDLE, not an
#                     embed: a 16 MB byte array is not something to put a
#                     compiler through, and `all` would be four of them.
set(SS_FONT_CJK "fetch" CACHE STRING "Full CJK font: fetch | none | sc | tc | jp | kr | all")
set_property(CACHE SS_FONT_CJK PROPERTY STRINGS fetch none sc tc jp kr all)
if(NOT SS_FONT_CJK MATCHES "^(fetch|none|sc|tc|jp|kr|all)$")
    message(FATAL_ERROR "SS_FONT_CJK must be fetch|none|sc|tc|jp|kr|all, got '${SS_FONT_CJK}'")
endif()

# ---------------------------------------------------------------------------
# Patent-encumbered modules
#
# OFF by default, and deliberately so: this repository is GPLv3, and the video
# codecs are the one piece of it that carries third-party patent exposure
# (H.264 / H.265 via MPEG LA / Access Advance, AV1 via the claims asserted
# against AOMedia). With it OFF, src/video/ -- the container demuxers, the
# H.264 / H.265 / AV1 bitstream parsers and the VK_KHR_video_decode_* driver --
# is neither compiled nor linked, and everything that wanted it shells out to
# an external ffmpeg instead. That costs a subprocess and a temporary folder of
# JPEGs; no feature disappears from the GUI.
#
# Turn it ON to decode video in-process on the GPU (roughly 15x faster frame
# extraction, and no ffmpeg to install). Distributors should check what their
# jurisdiction and their users require before shipping a binary built with it.
# ---------------------------------------------------------------------------
option(SS_ENABLE_PATENTED
    "Compile patent-encumbered modules (in-process H.264/H.265/AV1 video decode)"
    OFF)
if(SS_ENABLE_PATENTED AND NOT SS_BUILD_SAM)
    message(FATAL_ERROR
        "SS_ENABLE_PATENTED=ON needs SS_BUILD_SAM=ON: the video "
        "decoder is built on the inference layer's Vulkan runtime (src/nn/vk).")
endif()

# ---------------------------------------------------------------------------
# Shared C++ settings
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Host compiler flags
#
# SPLAT_CXX_FLAGS is what every target compiled from this tree gets for C++:
# the engine (csrc), the apps, and ss_sfm. It lives here, not in a backend
# module, because it is backend-neutral -- when it was set in
# SsBackendCuda.cmake the whole Vulkan build (engine *and* the SfM
# pipeline, which is host-heavy: stb decode, RANSAC, the mapper) compiled at
# -O0, since CMAKE_BUILD_TYPE is deliberately left empty.
#
# Backend modules may append to it (OpenMP, for one).
# ---------------------------------------------------------------------------

# SPLAT_C_FLAGS exists because $<COMPILE_LANGUAGE:CXX> skips the one C file in
# the tree, external/miniz.c -- the DEFLATE codec behind depth-PNG I/O and EXR
# decode, which is not something to run interpreted.

if(MSVC)
    # /utf-8: the i18n catalogs are UTF-8 source. Without it MSVC reads them in
    # the machine's ANSI codepage and every non-ASCII string is silently
    # mojibake -- on the developer's machine as well as the user's.
    set(SPLAT_CXX_FLAGS "/O2" "/utf-8")
    set(SPLAT_C_FLAGS "/O2")
else()
    set(SPLAT_CXX_FLAGS "-O3")
    list(APPEND SPLAT_CXX_FLAGS "-Wno-sign-compare")
    set(SPLAT_C_FLAGS "-O3")
endif()

# Host line info. The option reached only the CUDA and SPIR-V compilers, so a
# Vulkan build got none and crash reports stayed module+RVA (CrashLog.h). /Z7
# rather than /Zi: one shared vc140.pdb serializes a parallel Ninja build.
if(SS_DEBUG_SYMBOLS)
    if(MSVC)
        list(APPEND SPLAT_CXX_FLAGS "/Z7")
        list(APPEND SPLAT_C_FLAGS "/Z7")
        add_link_options("/DEBUG" "/OPT:REF" "/OPT:ICF")
    else()
        list(APPEND SPLAT_CXX_FLAGS "-g")
        list(APPEND SPLAT_C_FLAGS "-g")
    endif()
endif()

if(WIN32)
    add_compile_definitions(_USE_MATH_DEFINES NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()

# MSVC 14.40 zeroes std::mutex storage in a constexpr constructor instead of
# calling _Mtx_init_in_situ; an MSVCP140.dll older than that still dispatches
# through a vptr there, and null-derefs on the first lock(). Global on purpose.
if(MSVC)
    add_compile_definitions(_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)
endif()

# The same trim for the __FILE__ this project does not write: assert() in
# src/external/, GLFW, Dear ImGui. No replacement for SS_FILE -- MSVC has no
# equivalent, and nvcc expands __FILE__ instead of forwarding the flag.
if(NOT MSVC)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-fmacro-prefix-map=${SS_ROOT}/=" SS_HAS_PREFIX_MAP)
    if(SS_HAS_PREFIX_MAP)
        # Here, not in a backend module, so FetchContent subdirectories inherit it.
        add_compile_options(
            $<$<COMPILE_LANGUAGE:C,CXX>:-fmacro-prefix-map=${SS_ROOT}/=>)
    endif()
endif()
