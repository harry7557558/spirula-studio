# SS_BUILD_SAM: the GPU inference layer (src/nn/) and what currently sits
# on it -- SAM 2 / SAM 3 segmentation (src/sam/) and, when patented modules are
# enabled, hardware video decoding (src/video/).
#
# Defines:
#   ss_nn      the reusable inference layer (Vulkan runtime + tensor + ops)
#   ss_sam     SAM 2 / SAM 3 on top of it
#   ss_video   demux + VK_KHR_video_decode_*  (SS_ENABLE_PATENTED only)
#   nn_ops_test / sam_pipeline_test   one executable per src/*/tests/*.cpp
#
# The `spirula-sam` CLI lives with the other app targets, in SsApps.cmake.
#
# Like cmake/SsSfm.cmake, this needs Vulkan and slangc but NOT the compute
# backend: src/nn/vk/ is its own Vulkan context, so the module builds
# identically under either SS_BACKEND.
#
# Shaders: one slangc edge per .slang module (all of a file's entry points land
# in one blob, which is what the pipeline cache's "<stem>.<entry>" key expects),
# then one generated TU per library that registers its blobs with
# nn/vk/EmbeddedSpirv.h's process registry. Three libraries, one pipeline cache.

include(SsVulkan)
ss_vulkan_lib()
find_package(Threads REQUIRED)

include(SsSlang)
ss_find_slangc(SS_NN_SLANGC)
ss_build_spirv_tool(SS_NN_SPIRV_TOOL)

set(SS_NN_SHADER_ROOT ${SS_SRC}/nn/shaders)

# Serialize slangc.
#
# Several slangc processes at once crash it intermittently (0xC0000005 /
# 0xC0000409 on Windows, on a varying subset of the modules; a plain rerun
# succeeds). Whatever the shared state is, this shader set compiles in a few
# seconds, so a one-wide pool costs nothing and turns a flaky build into a
# reliable one. The SfM and engine shader sets have not hit it; revisit if a
# future slangc proves safe.
set_property(GLOBAL APPEND PROPERTY JOB_POOLS ss_nn_slangc=1)

# ---------------------------------------------------------------------------
# ss_nn_shaders(<tag> <shader_dir> <out_embed_cpp>)
#
# Compiles every .slang in <shader_dir> (files starting with '_' are
# include-only helpers) and emits the generated registration TU.
# ---------------------------------------------------------------------------
function(ss_nn_shaders tag shader_dir out_embed_cpp)
    set(_spv_dir ${CMAKE_CURRENT_BINARY_DIR}/nn_spirv/${tag})
    file(MAKE_DIRECTORY ${_spv_dir})

    file(GLOB _shaders CONFIGURE_DEPENDS ${shader_dir}/*.slang)
    if(NOT _shaders)
        message(FATAL_ERROR "no .slang files under ${shader_dir}")
    endif()
    # Helpers live in .slang files too and any module may include any of them,
    # so every blob depends on the whole shared set: a helper edit rebuilds all
    # of them. The set is small enough that parsing includes would not pay.
    file(GLOB _common CONFIGURE_DEPENDS ${SS_NN_SHADER_ROOT}/*.slang)

    set(_args -target spirv -O3 -fvk-use-entrypoint-name
              -matrix-layout-row-major -I${SS_NN_SHADER_ROOT})
    if(SS_DEBUG_SYMBOLS)
        list(APPEND _args -g2)
    endif()

    set(_blobs "")
    foreach(_src ${_shaders})
        get_filename_component(_stem ${_src} NAME_WE)
        if(_stem MATCHES "^_")
            continue()
        endif()
        set(_out ${_spv_dir}/${_stem}.spv)
        add_custom_command(OUTPUT ${_out}
            COMMAND ${SS_NN_SLANGC} ${_src} -o ${_out} ${_args}
            DEPENDS ${_shaders} ${_common}
            COMMENT "SPIR-V ${_stem}"
            JOB_POOL ss_nn_slangc
            VERBATIM)
        list(APPEND _blobs ${_out})
    endforeach()

    list(LENGTH _blobs _n)
    message(STATUS "Inference SPIR-V (${tag}): ${_n} modules")

    set(_listfile ${_spv_dir}/blobs.txt)
    string(REPLACE ";" "\n" _listbody "${_blobs}")
    ss_write_if_different(${_listfile} "${_listbody}\n")

    set(_embed ${CMAKE_CURRENT_BINARY_DIR}/nn_shaders_${tag}.cpp)
    add_custom_command(OUTPUT ${_embed}
        COMMAND ${SS_NN_SPIRV_TOOL} embed --nn ${tag} ${_embed}
                --list ${_listfile}
        DEPENDS ${SS_NN_SPIRV_TOOL} ${_blobs} ${_listfile}
        COMMENT "Embedding SPIR-V (${tag}: ${_n} modules)"
        VERBATIM)

    set(${out_embed_cpp} ${_embed} PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# ss_nn -- the reusable inference layer
# ---------------------------------------------------------------------------
ss_nn_shaders(nn ${SS_SRC}/nn/shaders SS_NN_EMBED)

file(GLOB_RECURSE SS_NN_SOURCES CONFIGURE_DEPENDS ${SS_SRC}/nn/*.cpp)
list(FILTER SS_NN_SOURCES EXCLUDE REGEX "/tests/")

add_library(ss_nn STATIC
    ${SS_NN_SOURCES}
    ${SS_NN_EMBED}
    # Instantiated once for the repository. As archive members these are only
    # pulled in when nothing else provides them, so a target that also links
    # the engine (spirula-gui) does not see a duplicate definition.
    ${SS_SRC}/external/stb_image_impl.cpp
    ${SS_SRC}/external/stb_image_write_impl.cpp
    ${SS_SRC}/core/ExrImage.cpp
    ${SS_SRC}/external/miniz.c
)
target_include_directories(ss_nn PUBLIC ${SS_SRC})
# PUBLIC and on ss_nn, not on ss_video: the flag means "this build
# contains the video decoder", and what reads it is nn/vk/Context.cpp, which
# has to request a video-decode queue at device creation -- before any decoder
# exists to ask for one.
if(SS_ENABLE_PATENTED)
    target_compile_definitions(ss_nn PUBLIC SS_HAVE_VIDEO=1)
endif()
target_link_libraries(ss_nn PUBLIC ss_vulkan Threads::Threads)
target_compile_options(ss_nn PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:${SPLAT_CXX_FLAGS}>
    $<$<COMPILE_LANGUAGE:C>:${SPLAT_C_FLAGS}>)
set_property(TARGET ss_nn PROPERTY CXX_STANDARD 17)

# ---------------------------------------------------------------------------
# ss_sam -- SAM 2 / SAM 3
# ---------------------------------------------------------------------------
ss_nn_shaders(sam ${SS_SRC}/sam/shaders SS_SAM_EMBED)

file(GLOB_RECURSE SS_SAM_SOURCES CONFIGURE_DEPENDS ${SS_SRC}/sam/*.cpp)
list(FILTER SS_SAM_SOURCES EXCLUDE REGEX "/tests/")

add_library(ss_sam STATIC ${SS_SAM_SOURCES} ${SS_SAM_EMBED})
target_link_libraries(ss_sam PUBLIC ss_nn)
target_compile_options(ss_sam PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:${SPLAT_CXX_FLAGS}>)
set_property(TARGET ss_sam PROPERTY CXX_STANDARD 17)

# ---------------------------------------------------------------------------
# ss_aliked -- the ALIKED / LightGlue learned SfM frontend
#
# Its checkpoints are ONNX files fetched from COLMAP's releases and parsed in
# process, so this needs no protobuf, no onnxruntime and no converter --
# src/nn/io/Onnx.cpp is a varint walk.
#
# The ops it needs that nn/ did not have -- deformable convolution, point-wise
# grid sample, average pooling, row L2 normalization -- are general and went
# into nn/shaders. What is here is only what could not be general: the
# detector's suppression rule and soft-argmax, and its coordinate conversions.
# ---------------------------------------------------------------------------
ss_nn_shaders(aliked ${SS_SRC}/aliked/shaders SS_ALIKED_EMBED)

file(GLOB_RECURSE SS_ALIKED_SOURCES CONFIGURE_DEPENDS ${SS_SRC}/aliked/*.cpp)
list(FILTER SS_ALIKED_SOURCES EXCLUDE REGEX "/tests/")

add_library(ss_aliked STATIC ${SS_ALIKED_SOURCES} ${SS_ALIKED_EMBED})
target_link_libraries(ss_aliked PUBLIC ss_nn)
target_compile_options(ss_aliked PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:${SPLAT_CXX_FLAGS}>)
set_property(TARGET ss_aliked PROPERTY CXX_STANDARD 17)

# ---------------------------------------------------------------------------
# ss_loma -- the LoMa frontend (DaD + DeDoDe + the LoMa matcher). Same shape as
# ss_aliked; the one general op it needed, bicubic resize, went into nn/.
# ---------------------------------------------------------------------------
ss_nn_shaders(loma ${SS_SRC}/loma/shaders SS_LOMA_EMBED)

file(GLOB_RECURSE SS_LOMA_SOURCES CONFIGURE_DEPENDS ${SS_SRC}/loma/*.cpp)
list(FILTER SS_LOMA_SOURCES EXCLUDE REGEX "/tests/")

add_library(ss_loma STATIC ${SS_LOMA_SOURCES} ${SS_LOMA_EMBED})
target_link_libraries(ss_loma PUBLIC ss_nn)
target_compile_options(ss_loma PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:${SPLAT_CXX_FLAGS}>)
set_property(TARGET ss_loma PROPERTY CXX_STANDARD 17)

# ---------------------------------------------------------------------------
# ss_metric3d -- Metric3D v2 monocular depth and normals
#
# Same shape as ss_aliked: the checkpoint is an ONNX file fetched from the
# onnx-community exports and parsed in process. The general ops it needed --
# tanh / elu / silu, padded average pooling, fractional-scale nearest resize,
# row softmax -- went into nn/shaders. What is here is the RAFT decoder's
# convex upsampler, its GRU blend, and the two places its six-channel state is
# taken apart.
# ---------------------------------------------------------------------------
ss_nn_shaders(metric3d ${SS_SRC}/metric3d/shaders SS_METRIC3D_EMBED)

file(GLOB_RECURSE SS_METRIC3D_SOURCES CONFIGURE_DEPENDS ${SS_SRC}/metric3d/*.cpp)
list(FILTER SS_METRIC3D_SOURCES EXCLUDE REGEX "/tests/")

add_library(ss_metric3d STATIC ${SS_METRIC3D_SOURCES} ${SS_METRIC3D_EMBED})
target_link_libraries(ss_metric3d PUBLIC ss_nn)
target_compile_options(ss_metric3d PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:${SPLAT_CXX_FLAGS}>)
set_property(TARGET ss_metric3d PROPERTY CXX_STANDARD 17)

# ---------------------------------------------------------------------------
# ss_moge -- MoGe-2 monocular point maps, normals and a validity mask
#
# The only one of these libraries with NO shaders of its own, hence no
# ss_nn_shaders() edge: the ops it needed were general and went into nn/.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE SS_MOGE_SOURCES CONFIGURE_DEPENDS ${SS_SRC}/moge/*.cpp)
list(FILTER SS_MOGE_SOURCES EXCLUDE REGEX "/tests/")

add_library(ss_moge STATIC ${SS_MOGE_SOURCES})
target_link_libraries(ss_moge PUBLIC ss_nn)
target_compile_options(ss_moge PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:${SPLAT_CXX_FLAGS}>)
set_property(TARGET ss_moge PROPERTY CXX_STANDARD 17)

# ---------------------------------------------------------------------------
# ss_video -- container demux + VK_KHR_video_decode_*
#
# Gated on SS_ENABLE_PATENTED: H.264 / H.265 / AV1 bitstream parsing is the
# patent-encumbered code in this repository, and a default build leaves it out.
# Everything that uses it falls back to an external ffmpeg, so OFF costs a
# subprocess, not a feature.
# ---------------------------------------------------------------------------
if(SS_ENABLE_PATENTED)
    ss_nn_shaders(video ${SS_SRC}/video/shaders SS_VIDEO_EMBED)

    file(GLOB_RECURSE SS_VIDEO_SOURCES CONFIGURE_DEPENDS
         ${SS_SRC}/video/*.cpp)
    add_library(ss_video STATIC ${SS_VIDEO_SOURCES} ${SS_VIDEO_EMBED})
    target_link_libraries(ss_video PUBLIC ss_nn)
    target_compile_options(ss_video PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:${SPLAT_CXX_FLAGS}>)
    set_property(TARGET ss_video PROPERTY CXX_STANDARD 17)
endif()

# ---------------------------------------------------------------------------
# Tests -- one executable per file, as in src/backend/tests/
# ---------------------------------------------------------------------------
file(GLOB SS_NN_TESTS CONFIGURE_DEPENDS
     ${SS_SRC}/nn/tests/*.cpp ${SS_SRC}/sam/tests/*.cpp
     ${SS_SRC}/aliked/tests/*.cpp ${SS_SRC}/loma/tests/*.cpp
     ${SS_SRC}/metric3d/tests/*.cpp
     ${SS_SRC}/moge/tests/*.cpp)
foreach(test_src ${SS_NN_TESTS})
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src})
    # Every test links every library above it: the four are small, and one
    # rule here beats a per-directory list that drifts.
    target_link_libraries(${test_name} PRIVATE ss_sam ss_aliked ss_loma ss_metric3d
                                                ss_moge)
    set_property(TARGET ${test_name} PROPERTY CXX_STANDARD 17)
    target_compile_options(${test_name} PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:${SPLAT_CXX_FLAGS}>)
endforeach()
