# JPEG XL dataset images, through libjxl. OFF by default: nothing else in the
# build needs it, and a dataset of JPEGs reads exactly as before.
#
#   -DSS_WITH_JXL=ON -DSS_JXL_ROOT=<prefix holding include/jxl and lib/jxl.lib>
#
# Defines the interface target ss_jxl and SS_HAVE_JXL on everything that links it.

option(SS_WITH_JXL "Read JPEG XL dataset images through libjxl" OFF)
set(SS_JXL_ROOT "" CACHE PATH "Prefix of a libjxl install (include/, lib/, bin/)")

if(SS_WITH_JXL)
    if(NOT EXISTS "${SS_JXL_ROOT}/include/jxl/decode.h")
        message(FATAL_ERROR "SS_WITH_JXL: no include/jxl/decode.h under SS_JXL_ROOT='${SS_JXL_ROOT}'")
    endif()
    add_library(ss_jxl INTERFACE)
    target_include_directories(ss_jxl INTERFACE "${SS_JXL_ROOT}/include")
    if(WIN32)
        target_link_libraries(ss_jxl INTERFACE
            "${SS_JXL_ROOT}/lib/jxl.lib" "${SS_JXL_ROOT}/lib/jxl_threads.lib"
            "${SS_JXL_ROOT}/lib/jxl_cms.lib")
        # The import libraries name DLLs that have to sit beside the executables.
        file(GLOB _ss_jxl_dlls "${SS_JXL_ROOT}/bin/*.dll")
        file(COPY ${_ss_jxl_dlls} DESTINATION "${CMAKE_BINARY_DIR}")
    else()
        target_link_directories(ss_jxl INTERFACE "${SS_JXL_ROOT}/lib")
        target_link_libraries(ss_jxl INTERFACE jxl jxl_threads jxl_cms)
    endif()
    target_compile_definitions(ss_jxl INTERFACE SS_HAVE_JXL=1)
endif()

option(SS_WITH_AVIF "Read AVIF dataset images through libavif (from SS_JXL_ROOT)" OFF)
if(SS_WITH_AVIF)
    if(NOT EXISTS "${SS_JXL_ROOT}/include/avif/avif.h")
        message(FATAL_ERROR "SS_WITH_AVIF: no include/avif/avif.h under SS_JXL_ROOT")
    endif()
    if(NOT TARGET ss_jxl)
        add_library(ss_jxl INTERFACE)
        target_include_directories(ss_jxl INTERFACE "${SS_JXL_ROOT}/include")
        file(GLOB _ss_jxl_dlls "${SS_JXL_ROOT}/bin/*.dll")
        file(COPY ${_ss_jxl_dlls} DESTINATION "${CMAKE_BINARY_DIR}")
    endif()
    if(WIN32)
        target_link_libraries(ss_jxl INTERFACE "${SS_JXL_ROOT}/lib/avif.lib")
    else()
        target_link_libraries(ss_jxl INTERFACE avif)
    endif()
    target_compile_definitions(ss_jxl INTERFACE SS_HAVE_AVIF=1)
endif()

# Every library that compiles core/ImageFile.cpp or core/JxlImage.cpp.
function(ss_link_jxl target)
    if(TARGET ss_jxl)
        target_link_libraries(${target} PUBLIC ss_jxl)
    endif()
endfunction()
