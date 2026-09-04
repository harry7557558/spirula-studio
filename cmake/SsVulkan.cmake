# One Vulkan link target, `ss_vulkan`, for the three modules that each carry
# their own Vulkan context: the compute backend, SfM and the inference layer.
#
# On macOS the default links MoltenVK statically, because the alternative does
# not survive being copied: a loader-linked binary records an absolute
# /opt/homebrew path for libvulkan, and the driver manifest that tells the
# loader where MoltenVK lives is Homebrew's too, so the binary runs only on the
# machine that built it. Statically linked, it depends on nothing outside
# /System and /usr/lib and needs no manifest, no VK_DRIVER_FILES and no
# installed loader. The cost is that a static build has no loader to load
# validation layers -- configure with -DSS_MACOS_VULKAN=loader for that.
#
# Everywhere else this is the platform loader as found by FindVulkan.

set(SS_MOLTENVK_VERSION "1.4.2")
set(SS_MOLTENVK_DIR "" CACHE PATH
    "Unpacked MoltenVK release (holds MoltenVK/include and MoltenVK/static); \
empty = fetch the pinned release")

# _ss_fetch_moltenvk(<lib_var> <include_var>)
#
# Locates the pinned MoltenVK package, downloading it into the build tree on a
# miss. The upstream release is the only source of a *universal* static library
# (Homebrew's is arm64-only), and it carries the Vulkan headers with it, so the
# macOS build needs no Vulkan SDK and no Homebrew at all.
function(_ss_fetch_moltenvk lib_var include_var)
    set(_root "${SS_MOLTENVK_DIR}")
    if(NOT _root)
        set(_name "MoltenVK-${SS_MOLTENVK_VERSION}-macos")
        set(_root ${CMAKE_BINARY_DIR}/${_name})
        if(NOT EXISTS ${_root}/MoltenVK/static)
            set(_url "https://github.com/KhronosGroup/MoltenVK/releases/download/v${SS_MOLTENVK_VERSION}/MoltenVK-macos.tar")
            set(_tar ${CMAKE_BINARY_DIR}/${_name}.tar)
            message(STATUS "Downloading ${_url} (~60 MB, once)")
            file(DOWNLOAD ${_url} ${_tar} SHOW_PROGRESS STATUS _dl)
            list(GET _dl 0 _dl_code)
            if(NOT _dl_code EQUAL 0)
                file(REMOVE ${_tar})
                message(FATAL_ERROR "Failed to download MoltenVK "
                    "${SS_MOLTENVK_VERSION}: ${_dl}. Unpack the release by "
                    "hand and pass -DSS_MOLTENVK_DIR=/path/to/MoltenVK, or "
                    "build against the loader with -DSS_MACOS_VULKAN=loader.")
            endif()
            # The release nests everything under a MoltenVK/ directory; keep
            # the two parts the build reads and drop the ~25 MB of dynamic
            # framework and iOS slices beside them.
            file(ARCHIVE_EXTRACT INPUT ${_tar} DESTINATION ${_root}
                PATTERNS "MoltenVK/MoltenVK/include/*"
                         "MoltenVK/MoltenVK/static/*")
            file(REMOVE ${_tar})
            file(RENAME ${_root}/MoltenVK/MoltenVK ${_root}/.mvk_tmp)
            file(REMOVE_RECURSE ${_root}/MoltenVK)
            file(RENAME ${_root}/.mvk_tmp ${_root}/MoltenVK)
        endif()
    endif()

    set(_lib ${_root}/MoltenVK/static/MoltenVK.xcframework/macos-arm64_x86_64/libMoltenVK.a)
    set(_inc ${_root}/MoltenVK/include)
    if(NOT EXISTS ${_lib} OR NOT EXISTS ${_inc}/vulkan/vulkan.h)
        message(FATAL_ERROR "MoltenVK package at ${_root} is missing "
            "MoltenVK/static/.../libMoltenVK.a or MoltenVK/include/vulkan. "
            "Point -DSS_MOLTENVK_DIR at an unpacked MoltenVK-macos release.")
    endif()
    set(${lib_var} ${_lib} PARENT_SCOPE)
    set(${include_var} ${_inc} PARENT_SCOPE)
endfunction()

# src/video/ compiles against VK_KHR_video_decode_av1, which appeared in header
# 277; Ubuntu 24.04 ships 275. The extension declares no commands, only structs
# and enums, so newer headers build fine against an older loader.
set(SS_VULKAN_HEADERS_MIN 277)
set(SS_VULKAN_HEADERS_VERSION "1.4.357")

function(_ss_vulkan_header_version inc out_var)
    set(${out_var} 0 PARENT_SCOPE)
    if(NOT EXISTS ${inc}/vulkan/vulkan_core.h)
        return()
    endif()
    file(STRINGS ${inc}/vulkan/vulkan_core.h _line
        REGEX "^#define VK_HEADER_VERSION ")
    if(_line MATCHES "VK_HEADER_VERSION[ \t]+([0-9]+)")
        set(${out_var} ${CMAKE_MATCH_1} PARENT_SCOPE)
    endif()
endfunction()

# _ss_fetch_vulkan_headers(<include_var>)
#
# Unpacks the pinned Vulkan-Headers release into the build tree. Header-only
# and architecture-independent, so this covers every platform whose loader is
# current but whose headers are not.
function(_ss_fetch_vulkan_headers out_var)
    set(_name "Vulkan-Headers-${SS_VULKAN_HEADERS_VERSION}")
    set(_inc ${CMAKE_BINARY_DIR}/${_name}/include)
    if(NOT EXISTS ${_inc}/vulkan/vulkan_core.h)
        set(_url "https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/v${SS_VULKAN_HEADERS_VERSION}.tar.gz")
        set(_tar ${CMAKE_BINARY_DIR}/${_name}.tar.gz)
        message(STATUS "Downloading ${_url}")
        file(DOWNLOAD ${_url} ${_tar} STATUS _dl)
        list(GET _dl 0 _dl_code)
        if(NOT _dl_code EQUAL 0)
            file(REMOVE ${_tar})
            message(FATAL_ERROR "Failed to download Vulkan-Headers "
                "${SS_VULKAN_HEADERS_VERSION}: ${_dl}. Install a Vulkan SDK "
                "with headers >= ${SS_VULKAN_HEADERS_MIN}.")
        endif()
        file(ARCHIVE_EXTRACT INPUT ${_tar} DESTINATION ${CMAKE_BINARY_DIR})
        file(REMOVE ${_tar})
    endif()
    set(${out_var} ${_inc} PARENT_SCOPE)
endfunction()

# ss_vulkan_lib()
#
# Defines the `ss_vulkan` interface target. Idempotent: the three modules that
# need Vulkan each call it, and whichever runs first wins.
function(ss_vulkan_lib)
    if(TARGET ss_vulkan)
        return()
    endif()
    add_library(ss_vulkan INTERFACE)

    if(APPLE AND SS_MACOS_VULKAN STREQUAL "static")
        _ss_fetch_moltenvk(_mvk_lib _mvk_inc)
        message(STATUS "Vulkan: MoltenVK ${SS_MOLTENVK_VERSION} linked "
            "statically (${_mvk_lib})")
        target_link_libraries(ss_vulkan INTERFACE ${_mvk_lib}
            "-framework Metal"       # what libMoltenVK.a leaves undefined
            "-framework Foundation"
            "-framework QuartzCore"
            "-framework IOSurface"
            "-framework IOKit"
            "-framework CoreGraphics"
            "-framework AppKit")
        target_include_directories(ss_vulkan SYSTEM INTERFACE ${_mvk_inc})
        # VulkanContext skips VK_KHR_portability_enumeration when the
        # extension is absent, which it is with no loader in the picture.
        return()
    endif()

    find_package(Vulkan REQUIRED)
    target_link_libraries(ss_vulkan INTERFACE Vulkan::Vulkan)

    _ss_vulkan_header_version("${Vulkan_INCLUDE_DIR}" _vk_hdr)
    if(_vk_hdr LESS SS_VULKAN_HEADERS_MIN)
        message(STATUS "Vulkan headers at ${Vulkan_INCLUDE_DIR} are version "
            "${_vk_hdr} (need ${SS_VULKAN_HEADERS_MIN}); fetching "
            "Vulkan-Headers ${SS_VULKAN_HEADERS_VERSION}")
        _ss_fetch_vulkan_headers(_vk_inc)
        target_include_directories(ss_vulkan SYSTEM BEFORE INTERFACE ${_vk_inc})
    endif()
endfunction()
