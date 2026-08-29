# The native application: one `spirula` executable holding every tool this
# build has (see src/app/Tools.h), dispatching on its first argument.
#
# One file instead of five is easier to install and to put on a PATH, and it is
# what lets the GUI run a reconstruction as a child process without a sibling
# binary having to be found next to it -- it re-runs itself. Nothing is shared
# between the tools at run time: each entry point is what used to be its own
# `main`, and a build with the GUI off produces a command-line binary that
# needs neither a display nor GL.
#
# SS_SEPARATE_TOOLS additionally builds the old one-executable-per-tool
# layout. Worth having for spirula-sfm in particular: on its own it does not
# link the engine, so it stays a small, portable binary.
#
# Backend-agnostic: whichever backend module ran first left the engine to link
# in SS_APP_LIBS. Two exceptions: the mesh tool is CUDA-only (the Vulkan
# backend stubs the meshing kernels), and the SfM tool is Vulkan-only.
#
# Dataset parsing, miniz and the embedded web viewer live in the engine
# library (cmake/sources.txt), so every app target shares one build of them --
# there are no source groups to list here.

# ---------------------------------------------------------------------------
# ss_configure_app(<target>)
#
# The settings every app target needs: include paths, the engine libraries,
# host flags.
# ---------------------------------------------------------------------------
function(ss_configure_app target)
    target_include_directories(${target} PRIVATE
        ${SS_SRC}
        ${CMAKE_BINARY_DIR}      # app_generated/{viewer_html,mask_py}.h
        ${CUDAToolkit_INCLUDE_DIRS}
    )
    target_link_libraries(${target} PRIVATE ${SS_APP_LIBS})
    target_compile_options(${target} PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:${SPLAT_CXX_FLAGS}>
    )
    set_property(TARGET ${target} PROPERTY CXX_STANDARD 17)

    # Sibling libraries first, in each linker's own spelling -- $ORIGIN is a
    # Linux-ism the Mach-O loader does not read.
    if(APPLE)
        set_property(TARGET ${target} PROPERTY BUILD_RPATH "@loader_path")
    else()
        set_property(TARGET ${target} PROPERTY BUILD_RPATH "$ORIGIN")
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Which tools this build has
#
# Each block appends the tool's sources, the macro that declares its entry
# point (src/app/Tools.h), and whatever it links. The lists are then used twice:
# once for the combined `spirula`, and once per tool if SS_SEPARATE_TOOLS is
# on.
# ---------------------------------------------------------------------------
set(SS_TOOL_SOURCES "")
set(SS_TOOL_DEFS "")
set(SS_TOOL_LIBS "")

# What every app target has whichever subsystems this build carries: the static
# frame stencil, and the two files src/app/Main.cpp needs to arm the crash
# handler for every subcommand -- including the ones the GUI spawns.
if(SS_BUILD_CLI OR SS_BUILD_GUI)
    list(APPEND SS_TOOL_SOURCES
         ${SS_SRC}/app/FrameMask.cpp
         ${SS_SRC}/app/AppPaths.cpp
         ${SS_SRC}/app/CrashLog.cpp)
    if(WIN32)
        list(APPEND SS_TOOL_LIBS dbghelp)   # CrashLog.cpp's stack walk
    endif()
endif()

# Localization settings every app target gets. SS_DEFAULT_LANG is an
# identifier, not a string: src/i18n/Locale.h spells it `Lang::SS_DEFAULT_LANG`,
# so a value that is not a language fails to compile instead of quietly
# falling back to English.
set(SS_I18N_DEFS SS_DEFAULT_LANG=${SS_DEFAULT_LANG})
if(SS_FONT_CJK STREQUAL "none")
    list(APPEND SS_I18N_DEFS SS_FONT_CJK_NONE=1)
endif()

if(SS_BUILD_CLI)
    # ---- trainer: no Python at runtime ----
    list(APPEND SS_TOOL_SOURCES ${SS_SRC}/app/cli/main.cpp)
    list(APPEND SS_TOOL_DEFS SS_TOOL_TRAIN=1)

    # ---- mesh extraction ----
    # Both backends: the pipeline's host side is portable
    # (mesh/OccupancyEvaluator.cpp) and its kernels exist on each
    # (mesh/Meshing.cu, backend/vulkan/kernels/Meshing.cpp).
    list(APPEND SS_TOOL_SOURCES ${SS_SRC}/app/cli/mesh_main.cpp)
    list(APPEND SS_TOOL_DEFS SS_TOOL_MESH=1)
endif()

if((SS_BUILD_CLI OR SS_BUILD_GUI) AND SS_BUILD_SFM)
    # ---- structure from motion ----
    # The SfM module carries its own Vulkan context and SPIR-V and shares
    # nothing with the training engine.
    list(APPEND SS_TOOL_SOURCES
         ${SS_SRC}/app/cli/sfm_main.cpp
         ${SS_SRC}/app/cli/sfm_ba.cpp)
    list(APPEND SS_TOOL_DEFS SS_TOOL_SFM=1)
    list(APPEND SS_TOOL_LIBS ss_sfm)
endif()

if((SS_BUILD_CLI OR SS_BUILD_GUI) AND SS_BUILD_SAM)
    # ---- segmentation / frame extraction ----
    list(APPEND SS_TOOL_SOURCES ${SS_SRC}/app/cli/sam_main.cpp)
    list(APPEND SS_TOOL_DEFS SS_TOOL_SAM=1)
    list(APPEND SS_TOOL_LIBS ss_sam)
    if(SS_ENABLE_PATENTED)
        list(APPEND SS_TOOL_SOURCES
             ${SS_SRC}/app/cli/sam_extract.cpp
             ${SS_SRC}/app/FrameExtract.cpp)
        list(APPEND SS_TOOL_LIBS ss_video)
    endif()

    # ---- monocular depth and normals ----
    # Rides on SS_BUILD_SAM because that is what builds the inference layer;
    # the dataset side is the engine's parsers, which every app target has.
    list(APPEND SS_TOOL_SOURCES
         ${SS_SRC}/app/cli/geometry_main.cpp
         ${SS_SRC}/app/GeometryModel.cpp
         ${SS_SRC}/app/GeometryWarp.cpp
         ${SS_SRC}/app/DepthPng.cpp)
    list(APPEND SS_TOOL_DEFS SS_TOOL_GEOMETRY=1)
    list(APPEND SS_TOOL_LIBS ss_metric3d ss_moge)
endif()

# ---------------------------------------------------------------------------
# spirula-gui -- optional, off by default. Dear ImGui + GLFW + OpenGL 3
# (imgui's embedded GL loader; no GLEW/GLAD). Dependencies are fetched at
# configure time via FetchContent and built statically, so the only system
# requirements are OpenGL dev headers (Linux: libgl1-mesa-dev + xorg-dev for
# GLFW; Windows/macOS: nothing extra).
# ---------------------------------------------------------------------------
if(SS_BUILD_GUI)
    include(FetchContent)

    set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
        GIT_SHALLOW    TRUE)
    FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.92.8
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(glfw imgui)
    # glfw brings its own CMakeLists and CMAKE_BUILD_TYPE is empty, so its C
    # sources would build at -O0 like everything else that is not told.
    target_compile_options(glfw PRIVATE
        $<$<COMPILE_LANGUAGE:C>:${SPLAT_C_FLAGS}>)

    find_package(OpenGL REQUIRED)

    # imgui ships no CMakeLists; build the core + glfw/gl3 backends +
    # std::string InputText helper as a small static lib.
    add_library(imgui_glfw STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
        ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
    )
    target_compile_options(imgui_glfw PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:${SPLAT_CXX_FLAGS}>)
    target_include_directories(imgui_glfw PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${imgui_SOURCE_DIR}/misc/cpp
    )
    target_link_libraries(imgui_glfw PUBLIC glfw)
    set_property(TARGET imgui_glfw PROPERTY CXX_STANDARD 17)

    # Embed reference/scripts/mask.py (AI masking helper, run via external
    # Python) so the exe is self-contained. Same mechanism as the
    # viewer.html embed.
    ss_embed_file(
        ${SS_ROOT}/reference/scripts/mask.py
        ${CMAKE_BINARY_DIR}/app_generated/mask_py.h
        MaskPy)

    # ---- fonts (src/app/gui/Fonts.h, docs/i18n.md) ----
    #
    # The Latin/Cyrillic face IS embedded: at 59 KB it costs nothing, and
    # without it the built-in ImGui font renders German, French, Turkish and
    # Russian as boxes -- which was true of this GUI before localization was
    # ever on the table. ss_cjk_faces() below embeds the four CJK subsets on
    # the same reasoning, at 422 KB for all of them. Rebuild all five with
    # tools/make_ui_font.py.
    ss_embed_file(
        ${SS_ROOT}/assets/fonts/SpirulaUI-Regular.ttf
        ${CMAKE_BINARY_DIR}/app_generated/ui_font.h
        UiFont)
    ss_cjk_faces()

    # The window icon X11 wants handed to it (GuiMain.cpp). Windows gets its
    # from app.rc below and macOS from the bundle, so this is the small one.
    ss_embed_file(
        ${SS_ROOT}/assets/icon_128.png
        ${CMAKE_BINARY_DIR}/app_generated/app_icon.h
        AppIcon)

    # The home screen's masthead.
    ss_embed_file(
        ${SS_ROOT}/assets/banner.png
        ${CMAKE_BINARY_DIR}/app_generated/app_banner.h
        AppBanner)

    file(GLOB SS_GUI_SOURCES CONFIGURE_DEPENDS ${SS_SRC}/app/gui/*.cpp)
    list(APPEND SS_TOOL_SOURCES ${SS_GUI_SOURCES})
    list(APPEND SS_TOOL_DEFS SS_TOOL_GUI=1)
    list(APPEND SS_TOOL_LIBS imgui_glfw OpenGL::GL)

    # The desktop's own file picker (src/app/gui/NativeDialog.h). Windows and
    # macOS need the platform toolkit for it; Linux spawns zenity/kdialog and
    # links nothing.
    if(WIN32)
        list(APPEND SS_TOOL_LIBS ole32 uuid shell32)
    elseif(APPLE)
        # Deliberately no enable_language(OBJCXX): CMake would then hand every
        # .m in the build to clang as Objective-C++, and GLFW's whole Cocoa
        # layer is .m. Left disabled, a .mm compiles as CXX, and clang reads
        # the extension the same way -- which is all this one file needs.
        set_source_files_properties(${SS_SRC}/app/gui/NativeDialogMac.mm
            PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
        list(APPEND SS_TOOL_SOURCES ${SS_SRC}/app/gui/NativeDialogMac.mm)
        list(APPEND SS_TOOL_LIBS "-framework AppKit")
    endif()

    # In-process segmentation (interactive preview + dataset masking) and, when
    # patented modules are enabled, in-process video decoding. Both are
    # optional: DatasetPrep falls back to python + reference/scripts/mask.py
    # and to ffmpeg, and the GUI hides what this build cannot do rather than
    # failing at run time.
    if(SS_BUILD_SAM)
        list(APPEND SS_TOOL_DEFS SS_BUILD_SAM=1)
        if(SS_ENABLE_PATENTED)
            list(APPEND SS_TOOL_SOURCES ${SS_SRC}/app/FrameExtract.cpp)
        endif()
    endif()
endif()

# ---------------------------------------------------------------------------
# spirula -- the executable
# ---------------------------------------------------------------------------
if(SS_BUILD_CLI OR SS_BUILD_GUI)
    # FrameExtract is claimed by both the segmentation tool and the GUI.
    list(REMOVE_DUPLICATES SS_TOOL_SOURCES)
    list(REMOVE_DUPLICATES SS_TOOL_LIBS)

    # app.rc is the icon Explorer and the taskbar draw; utf8.manifest makes the
    # PROCESS code page UTF-8, so the -A Win32 calls, the CRT, argv and getenv
    # agree with "/utf-8". A source, not /MANIFESTINPUT: CMake runs mt.exe too.
    if(WIN32)
        enable_language(RC)
        list(APPEND SS_TOOL_SOURCES ${SS_SRC}/app/app.rc
                                    ${SS_SRC}/app/utf8.manifest)
    endif()

    add_executable(spirula ${SS_SRC}/app/Main.cpp ${SS_TOOL_SOURCES})
    ss_configure_app(spirula)
    target_link_libraries(spirula PRIVATE ${SS_TOOL_LIBS})
    target_compile_definitions(spirula PRIVATE
        ${SS_TOOL_DEFS} SS_VERSION="${SS_VERSION}" ${SS_I18N_DEFS})
    if(WIN32)
        target_link_libraries(spirula PRIVATE ws2_32)
    endif()

    # A regional build ships its face beside the executable; Fonts.cpp looks
    # in <exe dir>/fonts before the cache directory.
    if(SS_BUILD_GUI AND NOT SS_FONT_CJK MATCHES "^(fetch|none)$")
        add_custom_command(TARGET spirula POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${CMAKE_BINARY_DIR}/fonts
                    $<TARGET_FILE_DIR:spirula>/fonts
            COMMENT "Bundling CJK font(s) for SS_FONT_CJK=${SS_FONT_CJK}")
    endif()

    # ---- the standalone CLI tools, on request ----
    # Same dispatcher, one tool compiled into each: src/app/Main.cpp reads its
    # own argv[0], so `spirula-sfm auto ...` reaches the SfM tool with its
    # arguments untouched, --help and all.
    #
    # Only these two, and deliberately: built alone they skip
    # ss_configure_app(), so neither drags in the training engine, which
    # is the entire reason to want them separate -- spirula-sfm is 24 MB
    # against the combined binary's 61 MB. A separate
    # spirula-train or spirula-mesh would be byte-for-byte the work `spirula` does
    # anyway, and a separate spirula-gui would be worse than the combined one:
    # it could not run reconstruction, since that is this binary re-running
    # itself. Symlink `spirula` if you want those names.
    if(SS_SEPARATE_TOOLS)
        function(ss_tool_exe name sources defs libs)
            if(WIN32)
                list(APPEND sources ${SS_SRC}/app/utf8.manifest)
            endif()
            # ss_i18n is linked explicitly: these targets deliberately do not
            # link the engine library, and Main.cpp's `--lang` handling needs
            # it. It is a leaf (cmake/SsI18n.cmake), so this costs nothing.
            add_executable(${name} ${SS_SRC}/app/Main.cpp ${sources})
            target_include_directories(${name} PRIVATE ${SS_SRC} ${CMAKE_BINARY_DIR})
            target_link_libraries(${name} PRIVATE ${libs} ss_i18n)
            target_compile_definitions(${name} PRIVATE
                ${defs} SS_VERSION="${SS_VERSION}" ${SS_I18N_DEFS})
            target_compile_options(${name} PRIVATE
                $<$<COMPILE_LANGUAGE:CXX>:${SPLAT_CXX_FLAGS}>)
            set_property(TARGET ${name} PROPERTY CXX_STANDARD 17)
        endfunction()

        if(SS_BUILD_SFM)
            ss_tool_exe(spirula-sfm
                "${SS_SRC}/app/cli/sfm_main.cpp;${SS_SRC}/app/cli/sfm_ba.cpp"
                "SS_TOOL_SFM=1" "ss_sfm")
        endif()
        if(SS_BUILD_SAM)
            set(_sam_src ${SS_SRC}/app/cli/sam_main.cpp ${SS_SRC}/app/FrameMask.cpp)
            set(_sam_lib ss_sam)
            if(SS_ENABLE_PATENTED)
                list(APPEND _sam_src ${SS_SRC}/app/cli/sam_extract.cpp
                                     ${SS_SRC}/app/FrameExtract.cpp)
                list(APPEND _sam_lib ss_video)
            endif()
            ss_tool_exe(spirula-sam "${_sam_src}" "SS_TOOL_SAM=1" "${_sam_lib}")
        endif()
    endif()
endif()

# ---------------------------------------------------------------------------
# Host-side tests for src/core/ and src/mesh/. Backend-agnostic -- both builds
# get them, and they link the engine library, which is where those .cpp live.
# ---------------------------------------------------------------------------
file(GLOB SS_CORE_TESTS CONFIGURE_DEPENDS
     ${SS_SRC}/core/tests/*.cpp ${SS_SRC}/mesh/tests/*.cpp)
foreach(test_src ${SS_CORE_TESTS})
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src})
    ss_configure_app(${test_name})
endforeach()
