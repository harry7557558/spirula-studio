cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(spirula_studio LANGUAGES C CXX)  # CUDA enabled by the cuda backend module

# The build (see build_develop.bash / build_develop.bat and docs/build.md).
#
# The logic lives in cmake/; this file is just the running order:
#
#   SsOptions.cmake        options, backend selection, tree paths
#   SsSources.cmake        the engine/kernel source list
#   SsBackendCuda.cmake    CUDA kernels + the engine library         } one
#   SsBackendVulkan.cmake  portable engine + Vulkan compute runtime } of
#   SsSfm.cmake            the SfM module (optional; Vulkan-only)
#   SsNn.cmake             inference layer + SAM + video (optional; Vulkan-only)
#   SsI18n.cmake           ss_i18n, the translation library (a leaf)
#   SsLicense.cmake        ss_license, accepted model licences and their texts (a leaf)
#   SsApps.cmake           the `spirula` executable (every tool in one)
#   SsPackage.cmake        the redistributable form of it (macOS .app / .dmg)
#   SsChecks.cmake         the source lints every build runs (last: it makes
#                            every target above depend on them)
#
# Each backend module leaves SS_APP_LIBS behind for the shared app targets.

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(SsOptions)
include(SsSources)

# A leaf everything else may link: the backends print device names, and their
# test binaries link a backend without the engine.
include(SsI18n)
include(SsLicense)

# The web viewer's browser client is compiled into the engine library
# (app/webviewer/Viewer.cpp), so it must be embedded before either backend
# module defines its targets -- not alongside the app targets.
include(SsEmbed)
ss_embed_file(
    ${SS_SRC}/app/webviewer/viewer.html
    ${CMAKE_BINARY_DIR}/app_generated/viewer_html.h
    ViewerHtml)

if(SS_BACKEND STREQUAL "vulkan")
    include(SsBackendVulkan)
else()
    include(SsBackendCuda)
endif()

# Before SsApps: the app links the ss_sfm target.
if(SS_BUILD_SFM)
    include(SsSfm)
endif()

# Likewise for `spirula sam` and the GUI's built-in masking.
if(SS_BUILD_SAM)
    include(SsNn)
endif()

if(COMMAND ss_shader_limits_test)
    ss_shader_limits_test()
endif()

include(SsApps)

# After SsApps: the packaging targets depend on the `spirula` target.
include(SsPackage)

# Last: every target above gains a dependency on the lints.
include(SsChecks)

message(STATUS "spirula_studio configured (backend: ${SS_BACKEND}).")
