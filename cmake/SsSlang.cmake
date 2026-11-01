# Locating slangc (the Slang -> SPIR-V compiler) for the Vulkan backend.
#
# SPIR-V blobs are never committed; they are compiled at build time (one slangc
# edge per blob, see src/backend/vulkan/shaders/SpirvShaders.cmake) and embedded into the
# binary. No Python is required for the Vulkan build.

set(SS_SLANG_VERSION "2026.12.0.1")
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    # 2026.12.0.1 ships no macOS release assets; use the nearest release
    # that does (upstream publishes macos-aarch64/x86_64 for 2026.12.1).
    set(SS_SLANG_VERSION "2026.12.1")
endif()
set(SS_SLANGC "" CACHE FILEPATH
    "slangc to use (empty = find in PATH, fetch on miss/mismatch)")

function(_ss_slangc_version exe out_var)
    set(${out_var} "" PARENT_SCOPE)
    if(NOT exe OR NOT EXISTS ${exe})
        return()
    endif()
    execute_process(COMMAND ${exe} -v
        OUTPUT_VARIABLE _v ERROR_VARIABLE _v
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _r)
    if(_r EQUAL 0)
        string(STRIP "${_v}" _v)
        set(${out_var} "${_v}" PARENT_SCOPE)
    endif()
endfunction()

# ss_find_slangc(<out_var>)
#
# Returns a slangc executable matching SS_SLANG_VERSION: the one named by
# -DSS_SLANGC, else one in PATH, else the pinned upstream release
# downloaded into the build tree.
function(ss_find_slangc out_var)
    set(_slangc "${SS_SLANGC}")
    if(NOT _slangc)
        find_program(_slangc_in_path slangc)
        if(_slangc_in_path)
            set(_slangc ${_slangc_in_path})
        endif()
    endif()
    _ss_slangc_version("${_slangc}" _slangc_ver)

    if(NOT _slangc_ver STREQUAL SS_SLANG_VERSION)
        if(_slangc)
            message(STATUS "slangc at ${_slangc} is version '${_slangc_ver}' "
                "(need ${SS_SLANG_VERSION}); fetching the pinned release")
        else()
            message(STATUS "slangc not found; fetching the pinned release "
                "${SS_SLANG_VERSION}")
        endif()
        if(WIN32)
            set(_slang_plat "windows-x86_64")
            set(_slang_ext "zip")
            set(_slang_exe "bin/slangc.exe")
        else()
            if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
                set(_slang_arch "aarch64")
            else()
                set(_slang_arch "x86_64")
            endif()
            if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
                set(_slang_plat "macos-${_slang_arch}")
            else()
                set(_slang_plat "linux-${_slang_arch}")
            endif()
            set(_slang_ext "tar.gz")
            set(_slang_exe "bin/slangc")
        endif()
        set(_slang_name "slang-${SS_SLANG_VERSION}-${_slang_plat}")
        set(_slang_dir ${CMAKE_CURRENT_BINARY_DIR}/${_slang_name})
        set(_slangc ${_slang_dir}/${_slang_exe})
        if(NOT EXISTS ${_slangc})
            set(_slang_url "https://github.com/shader-slang/slang/releases/download/v${SS_SLANG_VERSION}/${_slang_name}.${_slang_ext}")
            set(_slang_archive ${CMAKE_CURRENT_BINARY_DIR}/${_slang_name}.${_slang_ext})
            message(STATUS "Downloading ${_slang_url}")
            file(DOWNLOAD ${_slang_url} ${_slang_archive}
                SHOW_PROGRESS STATUS _dl_status)
            list(GET _dl_status 0 _dl_code)
            if(NOT _dl_code EQUAL 0)
                message(FATAL_ERROR "Failed to download slang release: "
                    "${_dl_status}. Install slang-${SS_SLANG_VERSION} "
                    "and pass -DSS_SLANGC=/path/to/slangc.")
            endif()
            file(ARCHIVE_EXTRACT INPUT ${_slang_archive}
                DESTINATION ${_slang_dir})
            file(REMOVE ${_slang_archive})
        endif()
        _ss_slangc_version("${_slangc}" _slangc_ver)
        if(NOT _slangc_ver STREQUAL SS_SLANG_VERSION)
            message(FATAL_ERROR "Fetched slangc reports version "
                "'${_slangc_ver}', expected ${SS_SLANG_VERSION}")
        endif()
    endif()

    message(STATUS "Using slangc ${_slangc} (${_slangc_ver})")
    set(${out_var} ${_slangc} PARENT_SCOPE)
endfunction()

# ss_build_spirv_tool(<out_var>)
#
# Compiles src/backend/vulkan/shaders/spirv_tool.cpp once with the project
# toolchain (try_compile + COPY_FILE, so it is portable across Windows / Linux /
# macOS and needs neither Python nor spirv-tools) and returns the executable.
#
# One tool for the whole repository: the Vulkan backend uses its `discover` and
# `embed` modes, the SfM module its `nocontract` and `embed --sfm` modes. It
# lives under backend/vulkan/shaders/ because that is its main consumer; it is a
# pure host program and pulls in no Vulkan headers, so a CUDA build with
# SS_BUILD_SFM=ON can compile it too.
function(ss_build_spirv_tool out_var)
    set(tool_src ${SS_SRC}/backend/vulkan/shaders/spirv_tool.cpp)
    set(tool_exe ${CMAKE_BINARY_DIR}/spirv_tool${CMAKE_EXECUTABLE_SUFFIX})

    # Rebuilt only when its source is newer than the copied executable.
    if(NOT EXISTS ${tool_exe} OR ${tool_src} IS_NEWER_THAN ${tool_exe})
        try_compile(_spirv_tool_ok ${CMAKE_BINARY_DIR}/spirv_tool_build
            SOURCES ${tool_src}
            COPY_FILE ${tool_exe}
            CXX_STANDARD 17 CXX_STANDARD_REQUIRED TRUE
            OUTPUT_VARIABLE _spirv_tool_log)
        if(NOT _spirv_tool_ok)
            message(FATAL_ERROR "Failed to build spirv_tool:\n${_spirv_tool_log}")
        endif()
        message(STATUS "Built SPIR-V build tool: ${tool_exe}")
    endif()

    set(${out_var} ${tool_exe} PARENT_SCOPE)
endfunction()

# ss_embed_spirv(<out_cpp> <listfile> <comment> BLOBS <spv>... FLAGS <arg>...)
#
# The one route from .spv blobs into a binary (spirv_tool embed). Records
# <listfile> for shader_limits_test, so no embedded shader set escapes it.
function(ss_embed_spirv out_cpp listfile comment)
    cmake_parse_arguments(PARSE_ARGV 3 _e "" "" "BLOBS;FLAGS")
    ss_build_spirv_tool(_tool)
    string(REPLACE ";" "\n" _body "${_e_BLOBS}")
    ss_write_if_different(${listfile} "${_body}\n")
    add_custom_command(OUTPUT ${out_cpp}
        COMMAND ${_tool} embed ${_e_FLAGS} ${out_cpp} --list ${listfile}
        DEPENDS ${_tool} ${_e_BLOBS} ${listfile}
        COMMENT "${comment}"
        VERBATIM)
    set_property(GLOBAL APPEND PROPERTY SS_SPIRV_LISTFILES ${listfile})
    set_property(GLOBAL APPEND PROPERTY SS_SPIRV_BLOBS ${_e_BLOBS})
endfunction()

# ss_shader_limits_test()
#
# Call after every ss_embed_spirv(): the test reads the listfiles recorded so far.
function(ss_shader_limits_test)
    get_property(_lists GLOBAL PROPERTY SS_SPIRV_LISTFILES)
    get_property(_blobs GLOBAL PROPERTY SS_SPIRV_BLOBS)
    if(NOT _lists)
        return()
    endif()
    set(_index ${CMAKE_BINARY_DIR}/spirv_listfiles.txt)
    string(REPLACE ";" "\n" _body "${_lists}")
    ss_write_if_different(${_index} "${_body}\n")
    add_custom_target(ss_spirv_blobs DEPENDS ${_blobs})
    add_executable(shader_limits_test
        ${SS_SRC}/backend/vulkan/shaders/tests/shader_limits_test.cpp)
    target_compile_definitions(shader_limits_test PRIVATE
        SS_SPIRV_LISTFILES="${_index}")
    target_compile_options(shader_limits_test PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:${SPLAT_CXX_FLAGS}>)
    add_dependencies(shader_limits_test ss_spirv_blobs)
endfunction()
