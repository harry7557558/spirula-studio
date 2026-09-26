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

# What every app target has: the static frame stencil, and the two files
# src/app/Main.cpp needs to arm the crash handler for every subcommand --
# including the ones the GUI spawns.
list(APPEND SS_TOOL_SOURCES
     ${SS_SRC}/app/FrameMask.cpp
     ${SS_SRC}/app/FrameMaskSvg.cpp
     ${SS_SRC}/app/FrameLook.cpp
     ${SS_SRC}/app/FrameMotion.cpp
     ${SS_SRC}/app/Pano360.cpp
     ${SS_SRC}/app/AppPaths.cpp
     ${SS_SRC}/app/CrashLog.cpp)
if(WIN32)
    list(APPEND SS_TOOL_LIBS dbghelp)   # CrashLog.cpp's stack walk
endif()

# Localization settings every app target gets. SS_DEFAULT_LANG is an
# identifier, not a string: src/i18n/Locale.h spells it `Lang::SS_DEFAULT_LANG`,
# so a value that is not a language fails to compile instead of quietly
# falling back to English.
set(SS_I18N_DEFS SS_DEFAULT_LANG=${SS_DEFAULT_LANG})
if(SS_FONT_CJK STREQUAL "none")
    list(APPEND SS_I18N_DEFS SS_FONT_CJK_NONE=1)
endif()

# ---- trainer: no Python at runtime ----
list(APPEND SS_TOOL_SOURCES ${SS_SRC}/app/cli/main.cpp)
list(APPEND SS_TOOL_DEFS SS_TOOL_TRAIN=1)

# ---- mesh extraction ----
# Both backends: the host side is portable (mesh/OccupancyEvaluator.cpp) and
# each has kernels (mesh/Meshing.cu, backend/vulkan/kernels/Meshing.cpp).
list(APPEND SS_TOOL_SOURCES ${SS_SRC}/app/cli/mesh_main.cpp)
list(APPEND SS_TOOL_DEFS SS_TOOL_MESH=1)

if(SS_BUILD_SFM)
    # ---- structure from motion ----
    # The SfM module carries its own Vulkan context and SPIR-V and shares
    # nothing with the training engine.
    list(APPEND SS_TOOL_SOURCES
         ${SS_SRC}/app/cli/sfm_main.cpp
         ${SS_SRC}/app/cli/sfm_ba.cpp)
    list(APPEND SS_TOOL_DEFS SS_TOOL_SFM=1)
    list(APPEND SS_TOOL_LIBS ss_sfm)
endif()

if(SS_BUILD_SAM)
    # ---- segmentation / frame extraction ----
    list(APPEND SS_TOOL_SOURCES ${SS_SRC}/app/cli/sam_main.cpp)
    list(APPEND SS_TOOL_DEFS SS_TOOL_SAM=1)
    list(APPEND SS_TOOL_LIBS ss_sam)
    if(SS_ENABLE_PATENTED)
        list(APPEND SS_TOOL_SOURCES
             ${SS_SRC}/app/cli/sam_extract.cpp
             ${SS_SRC}/app/FrameExtract.cpp)
        list(APPEND SS_TOOL_LIBS ss_video)
        # ---- video encoding: what the GUI's render mode pipes frames into ----
        list(APPEND SS_TOOL_SOURCES ${SS_SRC}/app/cli/encode_main.cpp)
        list(APPEND SS_TOOL_DEFS SS_TOOL_ENCODE=1)
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
    # The item hooks src/app/gui/Automation.cpp implements: one never-taken
    # branch per widget until it arms them. Not an option -- imgui references
    # the hooks once this is on, so a build without Automation.cpp would fail.
    target_compile_definitions(imgui_glfw PUBLIC IMGUI_ENABLE_TEST_ENGINE)
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

    file(GLOB SS_GUI_SOURCES CONFIGURE_DEPENDS
        ${SS_SRC}/app/gui/*.cpp ${SS_SRC}/app/gui/edit/*.cpp
        ${SS_SRC}/app/gui/render/*.cpp ${SS_SRC}/app/gui/mask/*.cpp)
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
# Only these two: built alone they skip ss_configure_app() and so never link
# the training engine -- 24 MB against the combined binary's 61 MB.
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
        set(_sam_src ${SS_SRC}/app/cli/sam_main.cpp ${SS_SRC}/app/FrameMask.cpp
                     ${SS_SRC}/app/FrameMaskSvg.cpp
                     ${SS_SRC}/app/FrameLook.cpp ${SS_SRC}/app/FrameMotion.cpp
                     ${SS_SRC}/app/Pano360.cpp)
        set(_sam_lib ss_sam)
        if(SS_ENABLE_PATENTED)
            list(APPEND _sam_src ${SS_SRC}/app/cli/sam_extract.cpp
                                 ${SS_SRC}/app/FrameExtract.cpp)
            list(APPEND _sam_lib ss_video)
        endif()
        ss_tool_exe(spirula-sam "${_sam_src}" "SS_TOOL_SAM=1" "${_sam_lib}")
    endif()
endif()

# ---------------------------------------------------------------------------
# Host-side tests for src/core/, src/data/ and src/mesh/. Backend-agnostic --
# both builds get them, and they link the engine library, which is where those
# .cpp live.
# ---------------------------------------------------------------------------
file(GLOB SS_CORE_TESTS CONFIGURE_DEPENDS
     ${SS_SRC}/core/tests/*.cpp ${SS_SRC}/data/tests/*.cpp
     ${SS_SRC}/mesh/tests/*.cpp)
foreach(test_src ${SS_CORE_TESTS})
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src})
    ss_configure_app(${test_name})
endforeach()

# The frame plan: no device, no GUI, and a wrong answer is silent.
add_executable(frame_motion_test
    ${SS_SRC}/app/tests/frame_motion_test.cpp
    ${SS_SRC}/app/FrameMotion.cpp
    ${SS_SRC}/app/Pano360.cpp)
ss_configure_app(frame_motion_test)

add_executable(packed_lens_test
    ${SS_SRC}/app/tests/packed_lens_test.cpp
    ${SS_SRC}/app/Pano360.cpp)
ss_configure_app(packed_lens_test)

# resolve_color() and the seed colours; both live in the engine library.
add_executable(color_resolution_test
    ${SS_SRC}/app/tests/color_resolution_test.cpp)
ss_configure_app(color_resolution_test)

# The same flag through TrainerSession's own setup and one real step.
add_executable(dlogm_session_test
    ${SS_SRC}/app/tests/dlogm_session_test.cpp)
ss_configure_app(dlogm_session_test)

# `--image-color-log auto` against a dataset's record of its clips.
add_executable(dataset_color_test
    ${SS_SRC}/app/tests/dataset_color_test.cpp)
ss_configure_app(dataset_color_test)

# The stencil shapes, spelling and fill, with no GUI: FrameMask.cpp is compiled
# into the CLI too, so this must link without imgui.
add_executable(frame_mask_test
    ${SS_SRC}/app/tests/frame_mask_test.cpp
    ${SS_SRC}/app/FrameMask.cpp
    ${SS_SRC}/app/FrameMaskSvg.cpp
    ${SS_SRC}/app/FrameLook.cpp)
ss_configure_app(frame_mask_test)

# The GUI files with no GUI in them: the stamp that decides whether a finished
# reconstruction is kept or built again, and the preset serializers. Named
# rather than globbed -- each such test names its own sources.
if(SS_BUILD_GUI)
    add_executable(recon_stamp_test
        ${SS_SRC}/app/gui/tests/recon_stamp_test.cpp
        ${SS_SRC}/app/gui/ReconStamp.cpp)
    ss_configure_app(recon_stamp_test)

    add_executable(frames_stamp_test
        ${SS_SRC}/app/gui/tests/frames_stamp_test.cpp
        ${SS_SRC}/app/gui/ReconStamp.cpp)
    ss_configure_app(frames_stamp_test)

    add_executable(command_argv_test
        ${SS_SRC}/app/gui/tests/command_argv_test.cpp
        ${SS_SRC}/app/gui/Subprocess.cpp)
    ss_configure_app(command_argv_test)

    add_executable(align_fit_test
        ${SS_SRC}/app/gui/tests/align_fit_test.cpp)
    ss_configure_app(align_fit_test)

    add_executable(attributes_test
        ${SS_SRC}/app/gui/tests/attributes_test.cpp
        ${SS_SRC}/app/gui/edit/Attributes.cpp
        ${SS_SRC}/app/gui/edit/EditDoc.cpp
        ${SS_SRC}/app/gui/edit/ElementGrid.cpp
        ${SS_SRC}/app/gui/edit/Selection.cpp)
    ss_configure_app(attributes_test)

    add_executable(rig_guess_test
        ${SS_SRC}/app/gui/tests/rig_guess_test.cpp
        ${SS_SRC}/app/gui/RigGuess.cpp)
    ss_configure_app(rig_guess_test)

    add_executable(render_project_test
        ${SS_SRC}/app/gui/tests/render_project_test.cpp
        ${SS_SRC}/app/gui/render/FlightFit.cpp
        ${SS_SRC}/app/gui/render/GifWriter.cpp
        ${SS_SRC}/app/gui/render/LensPresets.cpp
        ${SS_SRC}/app/gui/render/RenderProject.cpp
        ${SS_SRC}/app/gui/render/Trajectory.cpp)
    ss_configure_app(render_project_test)

    add_executable(preset_roundtrip_test
        ${SS_SRC}/app/gui/tests/preset_roundtrip_test.cpp
        ${SS_SRC}/app/gui/DatasetPreset.cpp
        ${SS_SRC}/app/gui/MeshJob.cpp
        ${SS_SRC}/app/gui/MeshPreset.cpp
        ${SS_SRC}/app/gui/PresetFile.cpp
        ${SS_SRC}/app/AppPaths.cpp)
    ss_configure_app(preset_roundtrip_test)

    # A run's config.json read back as a preset keeps `--image-color-log auto`.
    add_executable(run_config_preset_test
        ${SS_SRC}/app/gui/tests/run_config_preset_test.cpp
        ${SS_SRC}/app/gui/TrainPreset.cpp
        ${SS_SRC}/app/gui/PresetFile.cpp
        ${SS_SRC}/app/AppPaths.cpp)
    ss_configure_app(run_config_preset_test)

    add_executable(stencil_edit_test
        ${SS_SRC}/app/gui/tests/stencil_edit_test.cpp
        ${SS_SRC}/app/gui/StencilEdit.cpp
        ${SS_SRC}/app/gui/StencilPreset.cpp
        ${SS_SRC}/app/gui/PresetFile.cpp
        ${SS_SRC}/app/AppPaths.cpp
        ${SS_SRC}/app/FrameMask.cpp
        ${SS_SRC}/app/FrameMaskSvg.cpp
        ${SS_SRC}/app/FrameLook.cpp)
    ss_configure_app(stencil_edit_test)

    # The mask editor's layer, document and session, none of which draw.
    add_executable(mask_doc_test
        ${SS_SRC}/app/gui/tests/mask_doc_test.cpp
        ${SS_SRC}/app/gui/mask/MaskLayer.cpp
        ${SS_SRC}/app/gui/mask/MaskDoc.cpp
        ${SS_SRC}/app/gui/mask/MaskAdd.cpp
        ${SS_SRC}/app/gui/mask/MaskSam.cpp
        ${SS_SRC}/app/gui/mask/MaskSession.cpp
        ${SS_SRC}/app/gui/mask/MaskWindow.cpp
        ${SS_SRC}/app/gui/edit/EditDoc.cpp
        ${SS_SRC}/app/gui/edit/SelectShape.cpp
        ${SS_SRC}/app/gui/edit/Selection.cpp
        ${SS_SRC}/app/FrameMask.cpp
        ${SS_SRC}/app/FrameMaskSvg.cpp
        ${SS_SRC}/app/FrameLook.cpp
        ${SS_SRC}/app/gui/mask/Livewire.cpp
        ${SS_SRC}/app/gui/mask/PathTool.cpp
        ${SS_SRC}/app/gui/Picture.cpp
        ${SS_SRC}/app/gui/mask/MaskSlideshow.cpp)
    ss_configure_app(mask_doc_test)

    # DatasetPrep's two seams with the mask editor's layer folder, run for
    # real. Built without SS_BUILD_SAM or the video decoder: no model.
    add_executable(dataset_prep_test
        ${SS_SRC}/app/gui/tests/dataset_prep_test.cpp
        ${SS_SRC}/app/gui/DatasetPrep.cpp
        ${SS_SRC}/app/gui/FrameSelect.cpp
        ${SS_SRC}/app/gui/PrepProgress.cpp
        ${SS_SRC}/app/gui/ReconStamp.cpp
        ${SS_SRC}/app/gui/Subprocess.cpp
        ${SS_SRC}/app/gui/mask/MaskLayer.cpp
        ${SS_SRC}/app/FrameMask.cpp
        ${SS_SRC}/app/FrameMaskSvg.cpp
        ${SS_SRC}/app/FrameLook.cpp
        ${SS_SRC}/app/FrameMotion.cpp
        ${SS_SRC}/app/Pano360.cpp)
    ss_configure_app(dataset_prep_test)

    # 16-bit frames through a real ffmpeg on a generated fixture; SKIP without one.
    add_executable(frame_bits_test
        ${SS_SRC}/app/gui/tests/frame_bits_test.cpp
        ${SS_SRC}/app/gui/DatasetPrep.cpp
        ${SS_SRC}/app/gui/FrameSelect.cpp
        ${SS_SRC}/app/gui/PrepProgress.cpp
        ${SS_SRC}/app/gui/ReconStamp.cpp
        ${SS_SRC}/app/gui/Subprocess.cpp
        ${SS_SRC}/app/gui/mask/MaskLayer.cpp
        ${SS_SRC}/app/FrameMask.cpp
        ${SS_SRC}/app/FrameMaskSvg.cpp
        ${SS_SRC}/app/FrameLook.cpp
        ${SS_SRC}/app/FrameMotion.cpp
        ${SS_SRC}/app/Pano360.cpp)
    ss_configure_app(frame_bits_test)
endif()
