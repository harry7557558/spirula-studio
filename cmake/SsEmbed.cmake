# Embedding data files into the executables as byte arrays, so the apps are
# self-contained (no runtime lookup of viewer.html or reference/scripts/mask.py).

# ss_hex_to_literal(<hex string> <out var>)
#
# Bytes as a C++ string literal, not a `{0x..,}` array: the array form costs
# g++ 440 MB of memory per 3 MB embedded, this one 43 MB (measured, g++ 13).
function(ss_hex_to_literal hex out)
    # 40 bytes a line keeps every literal far under MSVC's 64 KB cap. `\x` is
    # greedy, but every byte is followed by a backslash, so it cannot run on.
    string(REPEAT "[0-9a-f][0-9a-f]" 40 _grp)
    string(REGEX REPLACE "(${_grp})" "\\1|" _s "${hex}")
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "\\\\x\\1" _s "${_s}")
    string(REPLACE "|" "\"\n\"" _s "${_s}")
    set(${out} "\"${_s}\"" PARENT_SCOPE)
endfunction()

# ss_embed_file(<input> <output_header> <symbol>)
#
# Writes a header defining `k<symbol>[]` / `k<symbol>Size` holding the bytes of
# <input>. Regenerated at configure time whenever <input> changes.
function(ss_embed_file input output_header symbol)
    file(READ ${input} _hex HEX)
    ss_hex_to_literal("${_hex}" _bytes)
    file(RELATIVE_PATH _rel ${SS_ROOT} ${input})
    # -1: a string literal brings a terminator the byte count must not include.
    string(CONCAT _text
        "#pragma once\n"
        "// AUTO-GENERATED from ${_rel} -- do not edit.\n"
        "#include <cstddef>\n"
        "inline const unsigned char k${symbol}[] =\n${_bytes};\n"
        "inline const size_t k${symbol}Size = sizeof(k${symbol}) - 1;\n")
    ss_write_if_different(${output_header} "${_text}")
    set_property(DIRECTORY ${SS_ROOT} APPEND
        PROPERTY CMAKE_CONFIGURE_DEPENDS ${input})
endfunction()

# ss_cjk_faces()
#
# assets/fonts/cjk_faces.txt -> cjk_faces.h (the downloadable full faces) and
# cjk_subsets.h (the four embedded subsets). See assets/fonts/README.md.
function(ss_cjk_faces)
    set(spec ${SS_ROOT}/assets/fonts/cjk_faces.txt)
    set(header ${CMAKE_BINARY_DIR}/app_generated/cjk_faces.h)
    set(sub_header ${CMAKE_BINARY_DIR}/app_generated/cjk_subsets.h)
    # ENCODING UTF-8, or file(STRINGS) drops the non-ASCII bytes in the
    # comments and hands back the fragments around them as extra "lines".
    file(STRINGS ${spec} lines ENCODING UTF-8)

    set(body "")
    set(sub_arrays "")
    set(sub_table "")
    set(wanted "")
    if(SS_FONT_CJK STREQUAL "all")
        set(wanted sc tc jp kr)
    elseif(NOT SS_FONT_CJK MATCHES "^(fetch|none)$")
        set(wanted ${SS_FONT_CJK})
    endif()

    foreach(line ${lines})
        string(STRIP "${line}" line)
        if(line STREQUAL "" OR line MATCHES "^#")
            continue()
        endif()
        string(REPLACE " | " ";" f "${line}")
        list(LENGTH f n)
        if(NOT n EQUAL 6)
            message(FATAL_ERROR "${spec}: expected 6 ' | '-separated fields, got ${n}:\n  ${line}")
        endif()
        list(GET f 0 id)
        list(GET f 1 file)
        list(GET f 2 url)
        list(GET f 3 sha)
        list(GET f 4 bytes)
        list(GET f 5 label)
        string(APPEND body
            "    {\"${id}\", \"${file}\", \"${url}\",\n"
            "     \"${sha}\", ${bytes}ull, \"${label}\"},\n")

        # The committed subset that goes into the executable. Generated from
        # the catalogs by tools/make_ui_font.py; tools/check_font_coverage.py
        # fails the build when a translation outgrows it.
        string(TOUPPER ${id} ID)
        set(sub ${SS_ROOT}/assets/fonts/SpirulaCJK-${ID}.otf)
        if(NOT EXISTS ${sub})
            message(FATAL_ERROR
                "${sub} is missing.\n"
                "  It is a committed build artifact; regenerate the fonts with\n"
                "  python3 tools/make_ui_font.py")
        endif()
        file(READ ${sub} _sub_hex HEX)
        ss_hex_to_literal("${_sub_hex}" _sub_bytes)
        string(APPEND sub_arrays
            "inline const unsigned char kCjkSubset${ID}[] =\n${_sub_bytes};\n")
        string(APPEND sub_table
            "    {\"${id}\", kCjkSubset${ID}, sizeof(kCjkSubset${ID}) - 1},\n")
        set_property(DIRECTORY ${SS_ROOT} APPEND
            PROPERTY CMAKE_CONFIGURE_DEPENDS ${sub})

        if(id IN_LIST wanted)
            set(dst ${CMAKE_BINARY_DIR}/fonts/${file})
            if(NOT EXISTS ${dst})
                message(STATUS "Downloading CJK font ${file} (${bytes} bytes)")
                file(DOWNLOAD ${url} ${dst}
                     EXPECTED_HASH SHA256=${sha}
                     SHOW_PROGRESS
                     STATUS dl_status)
                list(GET dl_status 0 dl_code)
                if(NOT dl_code EQUAL 0)
                    list(GET dl_status 1 dl_msg)
                    file(REMOVE ${dst})
                    message(FATAL_ERROR
                        "SS_FONT_CJK=${SS_FONT_CJK} needs ${file}, and the "
                        "download failed: ${dl_msg}\n"
                        "  Fetch it by hand into ${CMAKE_BINARY_DIR}/fonts/, "
                        "or build with SS_FONT_CJK=fetch.")
                endif()
            endif()
        endif()
    endforeach()

    if(body STREQUAL "")
        message(FATAL_ERROR "${spec} declared no faces")
    endif()

    file(RELATIVE_PATH rel ${SS_ROOT} ${spec})
    string(CONCAT _faces_text
        "#pragma once\n"
        "// AUTO-GENERATED from ${rel} -- do not edit.\n"
        "#include \"app/gui/Fonts.h\"\n"
        "\n"
        "namespace gui {\n"
        "inline constexpr CjkFace kCjkFaces[] = {\n"
        "${body}"
        "};\n"
        "}  // namespace gui\n")
    ss_write_if_different(${header} "${_faces_text}")

    string(CONCAT _subsets_text
        "#pragma once\n"
        "// AUTO-GENERATED from assets/fonts/SpirulaCJK-*.otf -- do not edit.\n"
        "// Those files are themselves generated: tools/make_ui_font.py.\n"
        "#include \"app/gui/Fonts.h\"\n"
        "\n"
        "namespace gui {\n"
        "${sub_arrays}"
        "inline const CjkSubset kCjkSubsets[] = {\n"
        "${sub_table}"
        "};\n"
        "}  // namespace gui\n")
    ss_write_if_different(${sub_header} "${_subsets_text}")

    set_property(DIRECTORY ${SS_ROOT} APPEND
        PROPERTY CMAKE_CONFIGURE_DEPENDS ${spec})
endfunction()
