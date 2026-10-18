# Native Slang -> SPIR-V build, replacing the former Python scripts.
# No Python required.
#
# spirv_tool.cpp (next to this file) is compiled once (via try_compile, using the project
# toolchain, so it is portable across Windows/Linux/macOS) and used to:
#   1. discover -- scan the compute shaders and enumerate every SPIR-V blob;
#   2. embed    -- gather the compiled .spv blobs into one C++ TU.
#
# Each blob becomes its own slangc custom command (one build-graph edge), so the
# build's -j governs how many slangc run concurrently -- bounding peak RAM the
# same way source compiles are -- and Ninja prints "[n/m] SPIR-V <name>" as each
# finishes.

# Where SlangcRetry.cmake lives. Captured at file scope: inside the function
# below, CMAKE_CURRENT_LIST_DIR is the *caller's* directory, not this one.
set(SS_SPIRV_LIST_DIR ${CMAKE_CURRENT_LIST_DIR})
#
# ss_setup_spirv(<shared_dir> <vk_dir> <slangc> <spirv_dir> <embed_cpp> <debug>)
# defines the custom commands producing <embed_cpp>.
#   <shared_dir> = src/shaders            device math shared with the CUDA build
#   <vk_dir>     = src/backend/vulkan/shaders   Vulkan-only compute entry points
function(ss_setup_spirv shared_dir vk_dir slangc spirv_dir embed_cpp debug)
    # The host tool is shared with the SfM module (cmake/SsSfm.cmake), so
    # it is built by SsSlang.cmake rather than here.
    ss_build_spirv_tool(tool_exe)

    # -I<src> lets a Vulkan shader reach the shared device math by its
    # src-relative path (`#include "shaders/densify.slang"`), matching the C++
    # convention and disambiguating the names that exist in both directories.
    set(incdirs -I${SS_SRC} -I${shared_dir} -I${vk_dir})

    # Common slangc arguments. Debug info (-g2) is opt-in; the release path
    # strips line directives and slang's capability diagnostics (the embed
    # step's capability audit still runs).
    set(spirv_args -target spirv -stage compute -O2)
    # atomic_float.slang's exchange-based add, for AMD's Windows driver only:
    # MoltenVK turns its float exchange into atomic_float, which MSL < 3.0 lacks.
    if(WIN32)
        list(APPEND spirv_args -DSS_XCHG_FLOAT_ADD)
    endif()
    if(debug)
        list(APPEND spirv_args -g2)
    else()
        list(APPEND spirv_args -ignore-capabilities -line-directive-mode none)
    endif()

    # Discover every blob (name / source / entry / defines / include-closure).
    file(GLOB slang_sources CONFIGURE_DEPENDS ${vk_dir}/*.slang)
    execute_process(
        COMMAND ${tool_exe} discover ${incdirs} ${slang_sources}
        OUTPUT_VARIABLE manifest
        RESULT_VARIABLE disc_rv
        ERROR_VARIABLE disc_err)
    if(NOT disc_rv EQUAL 0)
        message(FATAL_ERROR "spirv_tool discover failed (${disc_rv}):\n${disc_err}")
    endif()
    if(disc_err)
        message(STATUS "spirv_tool discover: ${disc_err}")
    endif()

    # slangc is memory-hungry on the big shaders and can crash. Cap the whole set by
    # memory instead. Serializing outright would cost the better part of an
    # hour for 680 blobs; SlangcRetry.cmake mops up whatever still slips through.
    cmake_host_system_information(RESULT _ss_ram QUERY AVAILABLE_PHYSICAL_MEMORY)
    cmake_host_system_information(RESULT _ss_cores QUERY NUMBER_OF_LOGICAL_CORES)
    math(EXPR _ss_spirv_jobs "${_ss_ram} / 3500")
    if(_ss_spirv_jobs GREATER _ss_cores)
        set(_ss_spirv_jobs ${_ss_cores})
    endif()
    if(_ss_spirv_jobs LESS 1)
        set(_ss_spirv_jobs 1)
    endif()
    set_property(GLOBAL APPEND PROPERTY JOB_POOLS ss_spirv=${_ss_spirv_jobs})
    message(STATUS "SPIR-V: at most ${_ss_spirv_jobs} concurrent slangc "
                   "(${_ss_ram} MB available)")

    string(REPLACE "\n" ";" manifest_lines "${manifest}")
    set(blob_outputs "")
    foreach(line ${manifest_lines})
        if(line STREQUAL "")
            continue()
        endif()
        # fields: name <tab> source <tab> entry <tab> defines <tab> deps
        string(REPLACE "\t" ";" fields "${line}")
        list(GET fields 0 name)
        list(GET fields 1 source)
        list(GET fields 2 entry)
        list(GET fields 3 defines)
        list(GET fields 4 deps)

        set(define_args "")
        if(NOT defines STREQUAL "-")
            string(REPLACE " " ";" define_args "${defines}")
        endif()
        string(REPLACE " " ";" dep_list "${deps}")

        set(out ${spirv_dir}/${name}.spv)
        # Through SlangcRetry.cmake rather than directly: see the crash it
        # exists for. '|' joins the arguments because -D cannot carry a list.
        set(_blob_cmd "${slangc}|${source}|-entry|${entry}|-o|${out}")
        foreach(_arg ${incdirs} ${spirv_args} ${define_args})
            string(APPEND _blob_cmd "|${_arg}")
        endforeach()
        add_custom_command(
            OUTPUT ${out}
            COMMAND ${CMAKE_COMMAND} -DSS_CMD=${_blob_cmd} -DSS_BLOB=${name}
                    -P ${SS_SPIRV_LIST_DIR}/SlangcRetry.cmake
            DEPENDS ${dep_list}
            COMMENT "SPIR-V ${name}"
            JOB_POOL ss_spirv
            VERBATIM)
        list(APPEND blob_outputs ${out})
    endforeach()

    list(LENGTH blob_outputs blob_count)
    message(STATUS "SPIR-V: ${blob_count} blobs to compile (per-blob, -j aware)")

    # List file for the embed step (avoids a very long command line).
    set(listfile ${spirv_dir}/blobs.txt)
    string(REPLACE ";" "\n" listfile_body "${blob_outputs}")
    ss_write_if_different(${listfile} "${listfile_body}\n")

    add_custom_command(
        OUTPUT ${embed_cpp}
        COMMAND ${tool_exe} embed ${embed_cpp} --list ${listfile}
        DEPENDS ${tool_exe} ${blob_outputs}
        COMMENT "Embedding SPIR-V (${blob_count} blobs)"
        VERBATIM)
endfunction()
