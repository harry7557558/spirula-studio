# SS_BACKEND=vulkan: the portable engine layer built against the Vulkan
# compute runtime, without the CUDA toolkit. Kernel coverage is tracked in
# src/backend/vulkan/README.md.
#
# Defines, for the shared app targets in SsApps.cmake:
#   SS_APP_LIBS           (portable engine + Vulkan runtime + threads)

message(STATUS "SS_BACKEND=vulkan: portable engine layer + "
    "backend/vulkan runtime; kernel coverage is tracked in "
    "src/backend/vulkan/README.md.")

# The command-line tools are this build's primary artifact; the GUI is opt-in.
set(SS_BUILD_CLI ON)

include(SsVulkan)
include(SsMacBundle)
ss_vulkan_lib()
find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# Portable engine layer
# ---------------------------------------------------------------------------
# Compile-checked here; links against the Vulkan runtime once kernel launchers
# exist for the symbols it needs.
ss_collect_portable_sources(SS_PORTABLE_SOURCES)

add_library(csrc_portable OBJECT ${SS_PORTABLE_SOURCES})
# Without this the whole portable engine builds at -O0 (CMAKE_BUILD_TYPE is
# deliberately empty), which the CUDA path avoids by applying the same list.
target_compile_options(csrc_portable PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:${SPLAT_CXX_FLAGS}>
    $<$<COMPILE_LANGUAGE:C>:${SPLAT_C_FLAGS}>)
target_compile_definitions(csrc_portable PUBLIC SS_BACKEND_VULKAN)
target_include_directories(csrc_portable PUBLIC ${SS_SRC} ${CMAKE_BINARY_DIR})
target_link_libraries(csrc_portable PUBLIC ss_i18n)

find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    ss_mac_prefer_static(OpenMP::OpenMP_CXX
        "`brew install libomp` ships the archive; \
-DCMAKE_DISABLE_FIND_PACKAGE_OpenMP=ON drops OpenMP instead.")
    target_link_libraries(csrc_portable PUBLIC OpenMP::OpenMP_CXX)
else()
    message(STATUS "No OpenMP: meshing, UV unwrap and metrics run serial")
endif()

# ---------------------------------------------------------------------------
# Slang -> SPIR-V (build tree), then embed
# ---------------------------------------------------------------------------
# Native (no Python): backend/vulkan/shaders/spirv_tool.cpp is compiled by the
# module below,
# which enumerates one slangc custom command per blob (so -j bounds RAM) and
# embeds the result. SPIR-V debug info (slangc -g2) is opt-in via
# SS_DEBUG_SYMBOLS; it otherwise dominates the embedded blob size.
include(SsSlang)
ss_find_slangc(SS_SLANGC_EXE)

set(SS_SPIRV_DIR ${CMAKE_CURRENT_BINARY_DIR}/spirv)
set(SS_SPIRV_EMBED ${CMAKE_CURRENT_BINARY_DIR}/vk_shaders_embedded.cpp)
include(${SS_VK_SHADERS}/SpirvShaders.cmake)
ss_setup_spirv(${SS_SHADERS} ${SS_VK_SHADERS} ${SS_SLANGC_EXE}
    ${SS_SPIRV_DIR} ${SS_SPIRV_EMBED} ${SS_DEBUG_SYMBOLS})

# ---------------------------------------------------------------------------
# Vulkan backend runtime (device context, memory, streams, events) + kernels
# ---------------------------------------------------------------------------
file(GLOB SS_VULKAN_SOURCES CONFIGURE_DEPENDS
    ${SS_SRC}/backend/vulkan/*.cpp
    ${SS_SRC}/backend/vulkan/kernels/*.cpp)
list(APPEND SS_VULKAN_SOURCES ${SS_SPIRV_EMBED})

add_library(ss_backend_vulkan STATIC ${SS_VULKAN_SOURCES})
target_compile_options(ss_backend_vulkan PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:${SPLAT_CXX_FLAGS}>)
target_compile_definitions(ss_backend_vulkan PUBLIC SS_BACKEND_VULKAN)
target_include_directories(ss_backend_vulkan PUBLIC ${SS_SRC})
target_link_libraries(ss_backend_vulkan PUBLIC ss_vulkan ss_i18n)

# ---------------------------------------------------------------------------
# Tests (run manually; see backend/vulkan/README.md)
# ---------------------------------------------------------------------------
# Vulkan-only unit tests.
file(GLOB SS_VULKAN_TESTS CONFIGURE_DEPENDS ${SS_SRC}/backend/vulkan/tests/*.cpp)
foreach(test_src ${SS_VULKAN_TESTS})
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src})
    target_link_libraries(${test_name} PRIVATE ss_backend_vulkan)
    target_compile_options(${test_name} PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:${SPLAT_CXX_FLAGS}>)
endforeach()

# Cross-backend parity tools (the same sources build in the CUDA branch, where
# they dump the reference outputs these compare against). The portable engine
# is what the host-only tests among them (split_faces_test) call into.
file(GLOB SS_PARITY_TESTS CONFIGURE_DEPENDS ${SS_SRC}/backend/tests/*.cpp)
foreach(test_src ${SS_PARITY_TESTS})
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src})
    target_link_libraries(${test_name} PRIVATE csrc_portable ss_backend_vulkan
        Threads::Threads)
    target_compile_options(${test_name} PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:${SPLAT_CXX_FLAGS}>)
endforeach()

# Engine-level parity tools: drive the real engine (forward_3dgs + background
# + blit) through the portable layer. The backend lib provides the ported
# kernels plus throwing stubs (kernels/TrainingStubs*.cpp) for everything still
# CUDA-only.
file(GLOB SS_ENGINE_TESTS CONFIGURE_DEPENDS ${SS_SRC}/backend/tests/engine/*.cpp)
foreach(test_src ${SS_ENGINE_TESTS})
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src})
    target_link_libraries(${test_name} PRIVATE csrc_portable
        ss_backend_vulkan Threads::Threads)
    target_compile_options(${test_name} PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:${SPLAT_CXX_FLAGS}>)
endforeach()

# The app targets build against the
# portable engine + Vulkan backend instead.
set(SS_APP_LIBS csrc_portable ss_backend_vulkan Threads::Threads)
