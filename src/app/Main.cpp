// The one entry point -- see app/Tools.h for why there is only one.
//
//   spirula                    the window
//   spirula <file-or-folder>   the window, opening what was named
//   spirula sfm auto ...       structure from motion
//   spirula train ...          the trainer
//   spirula sam segment ...    segmentation
//   spirula geometry ...       depth and normals for a dataset
//   spirula mesh ...           mesh extraction
//
// A first argument that is not a subcommand goes to the GUI untouched, so
// "Open with" from a file manager and a shell alias both land on the right
// screen. On a build with no GUI it is an error naming the subcommands, which
// is the only thing such a build can do.

#include "app/AppPaths.h"
#include "app/CrashLog.h"
#include "app/Tools.h"
#include "i18n/Locale.h"
#include "i18n/catalog/Cli.h"

#include <cctype>
#include <cstdio>
#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif
#include <cstring>
#include <string>
#include <vector>

namespace {

namespace cmsg = spirula::i18n::msg::cli;

// The subcommand NAME is an identifier and prints as it is written; the
// summary is a message, so `spirula --help` follows --lang like everything
// else. It is a pointer rather than a copy because a Msg is immortal .rodata
// and the language may change under it (it cannot here, but the habit is what
// keeps a cached `const char*` from ever appearing).
struct Tool {
    const char* name;
    const spirula::i18n::Msg* summary;
    int (*run)(int, char**);
};

const std::vector<Tool>& tools() {
    static const std::vector<Tool> kTools = {
#ifdef SS_TOOL_GUI
        {app::kToolGui, &cmsg::tool_gui, spirula_gui_main},
#endif
#ifdef SS_TOOL_SFM
        {app::kToolSfm, &cmsg::tool_sfm, spirula_sfm_main},
#endif
#ifdef SS_TOOL_TRAIN
        {app::kToolTrain, &cmsg::tool_train, spirula_train_main},
#endif
#ifdef SS_TOOL_SAM
        {app::kToolSam, &cmsg::tool_sam, spirula_sam_main},
#endif
#ifdef SS_TOOL_GEOMETRY
        {app::kToolGeometry, &cmsg::tool_geometry, spirula_geometry_main},
#endif
#ifdef SS_TOOL_MESH
        {app::kToolMesh, &cmsg::tool_mesh, spirula_mesh_main},
#endif
    };
    return kTools;
}

const Tool* find_tool(const char* name) {
    if (!name) return nullptr;
    for (const Tool& t : tools())
        if (std::strcmp(t.name, name) == 0) return &t;
    return nullptr;
}

// argv[0] as a tool name: a copy or symlink called spirula-sfm runs the SfM
// tool, which is how the separately-named executables of earlier releases keep
// working. Only the basename matters, and only the part after the prefix.
// "ssplat-" is the pre-rename spelling and still resolves.
const Tool* tool_from_argv0(const char* argv0) {
    if (!argv0) return nullptr;
    std::string s = argv0;
    const size_t slash = s.find_last_of("/\\");
    if (slash != std::string::npos) s = s.substr(slash + 1);
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    if (s.size() > 4 && s.compare(s.size() - 4, 4, ".exe") == 0)
        s.resize(s.size() - 4);
    for (const char* prefix : {"spirula-", "ssplat-"})
        if (s.rfind(prefix, 0) == 0)
            return find_tool(s.c_str() + std::strlen(prefix));
    return nullptr;
}

void print_usage() {
    std::printf("Spirula Studio " SS_VERSION "\n\n");
#ifdef SS_TOOL_GUI
    std::printf("  spirula                        %s\n",
                cmsg::usage_open_app.get());
    std::printf("  spirula <file-or-folder>       %s\n",
                cmsg::usage_open_target.get());
#endif
    std::printf("  spirula <command> [options]\n\n");
    std::printf("%s\n", cmsg::usage_commands.get());
    for (const Tool& t : tools())
        std::printf("  %-8s %s\n", t.name, t.summary->get());
    std::printf("\n%s\n", cmsg::usage_per_command_help.get());
    std::printf("\n%s\n\n%s", cmsg::usage_lang.get(),
                spirula::i18n::language_list().c_str());
}

}  // namespace

int main(int argc, char** argv) {
    // Stop FULL buffering of stdout whenever it is NOT a terminal. Every tool
    // here is routinely run as a CHILD PROCESS with its output piped to a live
    // log -- the GUI does it for reconstruction, masking and meshing -- and the
    // C runtime's default for a pipe is full buffering, which holds 4 KB of
    // progress back until the buffer fills or the process exits. A run that
    // prints one line a minute then appears to print nothing at all for the
    // first hour, which is exactly the failure it looks like.
    //
    // The two runtimes need different modes for the same result, and the
    // Windows one is unforgiving about both halves:
    //   * its CRT has no line buffering at all -- `_IOLBF` is documented to be
    //     treated as `_IOFBF` -- so asking for it would leave the stall in
    //     place;
    //   * and it rejects a zero size for the buffered modes, through an
    //     invalid-parameter handler that is `__fastfail`. The process then dies
    //     at this line with status 0xC0000409 and NOTHING on either stream --
    //     which reads, from the parent, as a tool that failed instantly for no
    //     reason. Only `_IONBF` is allowed a zero size there.
    if (!isatty(fileno(stdout))) {
#ifdef _WIN32
        std::setvbuf(stdout, nullptr, _IONBF, 0);
#else
        std::setvbuf(stdout, nullptr, _IOLBF, 0);
#endif
    }

    // --lang is handled here and removed from argv, so no tool's own parser
    // has to know about it. The chain that decides the language is in
    // src/i18n/Locale.h; the GUI re-runs it once it has read its settings
    // file, which is a step below --lang and the environment.
    const char* lang = spirula::i18n::take_lang_arg(&argc, argv);
    spirula::i18n::init(lang, nullptr);

    // Every tool, not only the window: the GUI runs reconstruction, masking
    // and meshing as child processes, and a child that dies of a fault leaves
    // its parent an exit status and nothing else.
    app::install_crash_log(app::config_dir());

    // An explicit subcommand wins over the argv[0] hint, so a binary that was
    // renamed or symlinked still answers to every tool it holds. No subcommand
    // name collides with an argument any of them takes, so `spirula-sfm auto`
    // falls through to the name check below and reaches the SfM tool.
    if (argc > 1) {
        if (const Tool* t = find_tool(argv[1])) {
            // The tool sees "spirula sfm" as its program name, so its own usage
            // text prints a command line that can be pasted back.
            std::string prog = std::string(argv[0] ? argv[0] : "spirula") + " " + t->name;
            app::set_crash_note(prog);
            std::vector<char*> sub;
            sub.push_back(prog.data());
            for (int i = 2; i < argc; i++) sub.push_back(argv[i]);
            sub.push_back(nullptr);
            return t->run((int)sub.size() - 1, sub.data());
        }
    }

    // Named as a tool: hand it everything, --help and --version included, so a
    // spirula-sfm symlink behaves exactly as the separate executable did.
    if (const Tool* t = tool_from_argv0(argc > 0 ? argv[0] : nullptr)) {
        app::set_crash_note(t->name);
        return t->run(argc, argv);
    }

    if (argc > 1) {
        const std::string a = argv[1];
        if (a == "help" || a == "--help" || a == "-h") {
            print_usage();
            return 0;
        }
        if (a == "--version" || a == "-v") {
            std::printf("%s\n", SS_VERSION);
            return 0;
        }
    }

#ifdef SS_TOOL_GUI
    return spirula_gui_main(argc, argv);
#else
    if (argc > 1)
        std::fprintf(stderr, "%s\n\n",
                     spirula::i18n::format(cmsg::err_unknown_command,
                                           {argv[1]}).c_str());
    else
        std::fprintf(stderr, "%s\n\n", cmsg::err_no_gui.get());
    print_usage();
    return 2;
#endif
}
