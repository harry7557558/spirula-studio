# The macOS bundle carries one binary (docs/build.md#packaging), so nothing it
# links may sit outside /usr/lib and /System/Library: a recorded /opt/homebrew
# path runs only on the machine that built it. MoltenVK is static for that
# reason (cmake/SsVulkan.cmake), tools/package_macos.sh rejects a bundle that
# breaks the rule, and a dependency found as a dylib goes through here.

# ss_mac_prefer_static(<imported target> [<hint>]) -- links the .a beside each
# dylib in the target's link interface. Warns with <hint> and leaves the target
# alone when an archive is missing; a no-op off macOS.
function(ss_mac_prefer_static target)
    if(NOT APPLE)
        return()
    endif()

    get_target_property(_libs ${target} INTERFACE_LINK_LIBRARIES)
    if(NOT _libs)
        return()
    endif()

    set(_static "")
    foreach(_lib IN LISTS _libs)
        # Anything that is not an absolute dylib path -- a genex, a link flag,
        # another target -- is not ours to rewrite.
        if(NOT _lib MATCHES "\\.dylib$")
            list(APPEND _static "${_lib}")
            continue()
        endif()
        string(REGEX REPLACE "\\.dylib$" ".a" _archive "${_lib}")
        if(NOT EXISTS "${_archive}")
            message(WARNING "No ${_archive}, so ${target} keeps ${_lib}, which "
                "packaging rejects (tools/package_macos.sh). ${ARGV1}")
            return()
        endif()
        list(APPEND _static "${_archive}")
    endforeach()

    set_property(TARGET ${target} PROPERTY INTERFACE_LINK_LIBRARIES "${_static}")
    list(JOIN _static " " _joined)
    message(STATUS "${target}: ${_joined} linked statically")
endfunction()
