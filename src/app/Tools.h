#pragma once

// The tools this program is made of, and the entry point of each.
//
// Everything ships as one executable (`spirula`), which dispatches on its first
// argument -- `spirula sfm auto ...`, `spirula train ...`, `spirula` on its own
// for the window. One file is easier to install, to put on a PATH and to copy
// onto another machine than five, and it is what lets the GUI run a
// reconstruction as a child process without depending on a sibling binary
// being present: it re-runs itself.
//
// Each entry point below is what used to be a `main`, unchanged apart from the
// name, and each still works entirely on its own -- nothing here initializes
// anything for anything else. A build with SS_BUILD_GUI off produces a
// pure command-line binary that needs no display and no GL.
//
// Which of these exist is decided at compile time by the SS_TOOL_* macros
// the build sets; see cmake/SsApps.cmake.

#include <string>

namespace app {

// How this tool was invoked, for its own usage and error messages: "spirula
// sfm" as dispatched, or whatever a renamed copy or a symlink was called. Set
// once from argv[0] at the top of each entry point.
inline std::string& program_name() {
    static std::string name = "spirula";
    return name;
}
inline void set_program_name(const char* argv0, const char* fallback) {
    program_name() = (argv0 && argv0[0]) ? argv0 : fallback;
}

// A block of help text with the program name it was written against swapped
// for the real one. The usage examples are worth keeping as readable literals
// rather than threading a %s through thirty of them.
inline std::string help_text(const char* text, const char* written_as) {
    std::string s = text;
    const std::string& prog = program_name();
    const std::string was = written_as;
    if (prog == was || was.empty()) return s;
    for (size_t p = s.find(was); p != std::string::npos;
         p = s.find(was, p + prog.size()))
        s.replace(p, was.size(), prog);
    return s;
}

// The subcommand name each tool answers to. Also matched against argv[0], so a
// symlink named spirula-sfm behaves as `spirula sfm` (which is how the separate
// executables of earlier releases keep working).
constexpr const char* kToolTrain = "train";
constexpr const char* kToolMesh  = "mesh";
constexpr const char* kToolSfm   = "sfm";
constexpr const char* kToolSam   = "sam";
constexpr const char* kToolGeometry = "geometry";
constexpr const char* kToolGui   = "gui";
constexpr const char* kToolEncode = "encode";
constexpr const char* kToolE57 = "e57";

}  // namespace app

#ifdef SS_TOOL_TRAIN
int spirula_train_main(int argc, char** argv);
#endif
#ifdef SS_TOOL_MESH
int spirula_mesh_main(int argc, char** argv);
#endif
#ifdef SS_TOOL_SFM
int spirula_sfm_main(int argc, char** argv);
#endif
#ifdef SS_TOOL_SAM
int spirula_sam_main(int argc, char** argv);
#endif
#ifdef SS_TOOL_GEOMETRY
int spirula_geometry_main(int argc, char** argv);
#endif
#ifdef SS_TOOL_GUI
int spirula_gui_main(int argc, char** argv);
#endif
#ifdef SS_TOOL_E57
int spirula_e57_main(int argc, char** argv);
#endif
#ifdef SS_TOOL_ENCODE
int spirula_encode_main(int argc, char** argv);
#endif
