# SS_BACKEND=cuda (default): the CUDA kernels and the engine library.
#
# Defines, for the shared app targets in SsApps.cmake:
#   SS_APP_LIBS     the engine library the apps link
#   SPLAT_CXX_FLAGS     host flags the app targets reuse

enable_language(CUDA)

find_package(CUDAToolkit REQUIRED)

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
ss_collect_sources(SPLAT_SOURCES)

# The CUDA half of the backend seam (backend/common/SortScan.h). src/backend/**
# is not listed in cmake/sources.txt on purpose -- the backend targets own it --
# so the CUDA branch adds its own here, exactly as SsBackendVulkan.cmake globs
# backend/vulkan/.
file(GLOB SS_CUDA_BACKEND_SOURCES CONFIGURE_DEPENDS ${SS_SRC}/backend/cuda/*.cu)
list(APPEND SPLAT_SOURCES ${SS_CUDA_BACKEND_SOURCES})

# ---------------------------------------------------------------------------
# Detect CUDA architectures
# ---------------------------------------------------------------------------
if(NOT DEFINED TORCH_CUDA_ARCH_LIST)
    if(NOT TORCH_CUDA_ARCH_LIST)
        # Ask the driver which architectures to build for.
        execute_process(
            COMMAND nvidia-smi --query-gpu=compute_cap --format=csv,noheader
            OUTPUT_VARIABLE SS_SMI_CAPS
            RESULT_VARIABLE SS_SMI_RESULT
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(SS_SMI_RESULT EQUAL 0)
            string(REPLACE "\r" "" SS_SMI_CAPS "${SS_SMI_CAPS}")
            string(REPLACE "\n" " " TORCH_CUDA_ARCH_LIST "${SS_SMI_CAPS}")
        endif()
    endif()

    if(NOT TORCH_CUDA_ARCH_LIST)
        message(FATAL_ERROR "Could not detect a CUDA GPU. "
            "Pass -DTORCH_CUDA_ARCH_LIST=\"8.6\" (or similar) to set the target architecture.")
    endif()
endif()

# normalize: "8.6" / "8.6 7.5" / "86;75" all become a deduped list like "86;75"
string(REGEX REPLACE "\\." "" TORCH_CUDA_ARCH_LIST "${TORCH_CUDA_ARCH_LIST}")
string(REPLACE " " ";" TORCH_CUDA_ARCH_LIST "${TORCH_CUDA_ARCH_LIST}")
list(REMOVE_DUPLICATES TORCH_CUDA_ARCH_LIST)

message(STATUS "CUDA architecture(s): ${TORCH_CUDA_ARCH_LIST}")

# "<arch>-real" emits cubin only; the bare form also emits PTX. See
# SS_CUDA_EMBED_PTX in SsOptions.cmake for why the default drops it.
set(CMAKE_CUDA_ARCHITECTURES "")
foreach(arch ${TORCH_CUDA_ARCH_LIST})
    if(SS_CUDA_EMBED_PTX)
        list(APPEND CMAKE_CUDA_ARCHITECTURES "${arch}")
    else()
        list(APPEND CMAKE_CUDA_ARCHITECTURES "${arch}-real")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# Compiler flags
#
# The base host flags are backend-neutral and set in SsOptions.cmake; this
# only appends the CUDA-specific parts.
# ---------------------------------------------------------------------------
if(MSVC)
    add_compile_options(
        $<$<COMPILE_LANGUAGE:CXX>:/Zc:preprocessor>
        $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=/Zc:preprocessor>
    )
endif()

# Detect OpenMP
find_package(OpenMP)
if(OpenMP_CXX_FOUND AND NOT APPLE)
    message(STATUS "Compiling with OpenMP")
    list(APPEND SPLAT_CXX_FLAGS "-DAT_PARALLEL_OPENMP")
else()
    message(STATUS "Compiling without OpenMP...")
endif()

# macOS ARM64 special case
if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    message(STATUS "Enabling macOS ARM64 flags")
    add_compile_options("-arch" "arm64")
    add_link_options("-arch" "arm64")
endif()

# nvcc flags
set(SPLAT_NVCC_FLAGS
    "-O3" "--use_fast_math"
    "--expt-relaxed-constexpr"
    "-Xcudafe=--diag_suppress=20012"
    "-Xcudafe=--diag_suppress=550"
    "--threads" "0"
    "-Xptxas" "--warn-on-double-precision-use"
)

# cubin/PTX line info (adds embedded source; needed for profiling but bloats
# the fatbin). Off unless SS_DEBUG_SYMBOLS.
if(SS_DEBUG_SYMBOLS)
    list(APPEND SPLAT_NVCC_FLAGS "-lineinfo" "--generate-line-info" "--source-in-ptx")
endif()



# Host side optimizations
if(NOT WIN32)
    # list(APPEND SPLAT_NVCC_FLAGS "-Xcompiler=-O3,-march=native")
    # Host debug symbols (nvcc -Xcompiler=-g). Off unless SS_DEBUG_SYMBOLS.
    if(SS_DEBUG_SYMBOLS)
        list(APPEND SPLAT_NVCC_FLAGS "-Xcompiler=-g")
    endif()
endif()

# ---------------------------------------------------------------------------
# The engine library
# ---------------------------------------------------------------------------
# Static: the engine API has no dllexport annotations (a Linux .so exports
# everything by default), so a static lib sidesteps that on Windows and makes
# `spirula` a self-contained executable.
add_library(csrc STATIC ${SPLAT_SOURCES})

target_include_directories(csrc PRIVATE
    ${SS_SRC}
    ${CMAKE_BINARY_DIR}      # app_generated/viewer_html.h
    ${CUDAToolkit_INCLUDE_DIRS}
)

find_package(Threads REQUIRED)
target_link_libraries(csrc CUDA::cudart Threads::Threads ss_i18n)
if(OpenMP_CXX_FOUND AND NOT APPLE)
    # OpenMP::OpenMP_CXX's flags are language-guarded, so nvcc never sees them.
    target_link_libraries(csrc OpenMP::OpenMP_CXX)
endif()

target_compile_options(csrc PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:${SPLAT_CXX_FLAGS}>
    $<$<COMPILE_LANGUAGE:C>:${SPLAT_C_FLAGS}>
    $<$<COMPILE_LANGUAGE:CUDA>:${SPLAT_NVCC_FLAGS}>
)

set_property(TARGET csrc PROPERTY CXX_STANDARD 17)
set_property(TARGET csrc PROPERTY CUDA_STANDARD 17)

# backend/cuda/KernelProfilerCuda.cu interposes cudaLaunchKernel here. Without
# --wrap its object is never pulled out of the archive and the SS_PROFILE
# per-kernel table is absent; nothing else changes.
include(CheckLinkerFlag)
check_linker_flag(CXX "LINKER:--wrap=cudaLaunchKernel" SS_HAS_LINKER_WRAP)
if(SS_HAS_LINKER_WRAP)
    target_link_options(csrc INTERFACE "LINKER:--wrap=cudaLaunchKernel")
endif()

set(SS_APP_LIBS csrc CUDA::cudart)

# ---------------------------------------------------------------------------
# Cross-backend parity tools -- CUDA side (dump reference outputs; the Vulkan
# build's twin executables compare against them). See backend/vulkan/README.md.
# The Vulkan branch builds its comparing twins unconditionally.
# ---------------------------------------------------------------------------
if(SS_BUILD_BACKEND_TESTS)
    file(GLOB SS_PARITY_TESTS CONFIGURE_DEPENDS
        ${SS_SRC}/backend/tests/*.cpp
        ${SS_SRC}/backend/tests/engine/*.cpp)
    foreach(test_src ${SS_PARITY_TESTS})
        get_filename_component(test_name ${test_src} NAME_WE)
        add_executable(${test_name} ${test_src})
        target_include_directories(${test_name} PRIVATE
            ${SS_SRC}
            ${CUDAToolkit_INCLUDE_DIRS})
        target_link_libraries(${test_name} PRIVATE csrc CUDA::cudart)
        target_compile_options(${test_name} PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:${SPLAT_CXX_FLAGS}>)
    endforeach()
endif()
