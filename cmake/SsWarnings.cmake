# Compiler warnings for the project's own sources, at each compiler's
# -Wall -Wextra level. Include after every target is defined.
#
# Set per source, not per target: vendored code (src/external/, FetchContent
# deps) shares targets with ours, and MSVC warns (D9025) when a later /w
# overrides a target-wide /W4.
# ---------------------------------------------------------------------------

option(SS_WARNINGS "Compile project sources with extra warnings" ON)
if(NOT SS_WARNINGS)
    return()
endif()

# -Wmissing-field-initializers is off: it fires on every Vulkan
# `VkFoo info{VK_STRUCTURE_TYPE_FOO}`, whose zero fill is the point, and
# MSVC has no counterpart.
if(MSVC)
    # /W4 also has what GCC leaves to -Wconversion (4244 4245 4267 4305),
    # -Wshadow (4456-4459) and -Wpedantic (4201), and checks GCC lacks.
    set(ss_warn_cxx /W4
        /wd4244 /wd4245 /wd4267 /wd4305
        /wd4456 /wd4457 /wd4458 /wd4459
        /wd4127 /wd4201 /wd4310 /wd4324 /wd4702)
else()
    set(ss_warn_cxx -Wall -Wextra -Wno-missing-field-initializers)
endif()
set(ss_warn_host ${ss_warn_cxx})
if(MSVC)
    list(APPEND ss_warn_host /wd4211)  # nvcc's generated stub for kernels in unnamed namespaces
endif()
string(REPLACE ";" "," ss_warn_host "${ss_warn_host}")
set(ss_warn_options
    $<$<COMPILE_LANGUAGE:C,CXX,OBJCXX>:${ss_warn_cxx}>
    $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=${ss_warn_host}>)

get_property(ss_warn_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
set(ss_warn_sources "")
foreach(t IN LISTS ss_warn_targets)
    get_target_property(type ${t} TYPE)
    if(type MATCHES "^(STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|OBJECT_LIBRARY|EXECUTABLE)$")
        get_target_property(srcs ${t} SOURCES)
        foreach(s IN LISTS srcs)
            if(s MATCHES "\\$<" OR NOT s MATCHES "\\.(c|cc|cpp|cxx|cu|mm)$")
                continue()
            endif()
            get_filename_component(s "${s}" ABSOLUTE BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
            string(FIND "${s}" "${SS_SRC}/external/" ext)
            if(ext EQUAL 0 OR s MATCHES "/_deps/")
                continue()
            endif()
            list(APPEND ss_warn_sources "${s}")
        endforeach()
    endif()
endforeach()
list(REMOVE_DUPLICATES ss_warn_sources)
set_property(SOURCE ${ss_warn_sources} APPEND PROPERTY COMPILE_OPTIONS ${ss_warn_options})
