// `--accept-license` through the real `spirula` binary: Main.cpp consumes it for
// every command, wherever it stands in argv, before the command sees argv.
// Scratch config directory, stdin from /dev/null (so there is no terminal to ask).

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

bool has(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

std::string slurp(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

struct Run {
    int code = -1;
    std::string out;   // stdout and stderr together
};

fs::path g_root;

fs::path gui_conf() { return g_root / "config" / "spirula-studio" / "gui.conf"; }

void fresh() {
    std::error_code ec;
    fs::remove_all(g_root, ec);
    fs::create_directories(g_root / "config", ec);
    fs::create_directories(g_root / "cache", ec);
}

Run run(const std::string& args, const std::string& env_prefix = "") {
    const fs::path log = g_root / "out.txt";
    const std::string env = env_prefix.empty()
        ? "XDG_CONFIG_HOME='" + (g_root / "config").string() + "' XDG_CACHE_HOME='" +
              (g_root / "cache").string() + "'"
        : env_prefix;
    // From an empty directory of its own, so "nothing was written to the working
    // directory" is a fact about this run and not about wherever ctest started.
    fs::create_directories(g_root / "cwd");
    const std::string cmd = "cd '" + (g_root / "cwd").string() + "' && " + env + " '" +
                            SS_SPIRULA_EXE + "' " + args + " < /dev/null > '" +
                            log.string() + "' 2>&1";
    Run r;
    const int rc = std::system(cmd.c_str());
#ifndef _WIN32
    r.code = WIFEXITED(rc) ? WEXITSTATUS(rc) : -1;
#else
    r.code = rc;
#endif
    r.out = slurp(log);
    return r;
}

}  // namespace

int main() {
#ifdef _WIN32
    std::printf("SKIP (POSIX shell syntax)\n");
    return 0;
#else
    g_root = fs::temp_directory_path() / "spirula_accept_license_cli_test";

    fresh();
    Run r = run("--version");
    check(r.code == 0 && !fs::exists(gui_conf()), "baseline: --version alone records nothing");
    const std::string version = r.out;

    fresh();
    r = run("--lang en --accept-license sam3=yes,birefnet=yes --version");
    const std::string conf = slurp(gui_conf());
    check(r.code == 0 && has(r.out, version), "leading flag: the command still runs (--version)");
    check(has(conf, "accepted_license=sam3\n") && has(conf, "accepted_license=birefnet\n"),
          "leading flag: both families recorded in gui.conf");
    check(has(r.out, "SAM License") && has(r.out, "MIT License"),
          "leading flag: the full terms were printed first");
    check(std::all_of(r.out.begin(), r.out.end(), [](unsigned char c) { return c < 128; }),
          "English license output uses ASCII punctuation");

    fresh();
    r = run("--accept-license roma=yes --version");
    check(r.code == 0 && has(slurp(gui_conf()), "accepted_license=roma\n") &&
              has(r.out, "DINOv3 License") && has(r.out, "Johan Edstedt"),
          "a registered family (RoMa): known to the CLI, both texts printed, recorded");

    fresh();
    r = run("--accept-license flexglue=yes --version");
    check(r.code == 0 && has(slurp(gui_conf()), "accepted_license=flexglue\n") &&
              has(r.out, "DINOv3 License") && has(r.out, "Computer Vision and Geometry Lab"),
          "a registered family (FlexGlue): known to the CLI, both texts printed, recorded");

    fresh();
    r = run("--version --accept-license=birefnet=yes");
    check(r.code == 0 && has(slurp(gui_conf()), "accepted_license=birefnet\n"),
          "trailing --accept-license=family=yes spelling is consumed too");

    fresh();
    r = run("sam --accept-license sam3=yes --help");
    check(r.code == 0 && has(slurp(gui_conf()), "accepted_license=sam3\n") &&
              !has(r.out, "accept-license"),
          "after a subcommand: consumed, and the subcommand never sees it");

    fresh();
    r = run("--accept-license sam3 --version");
    check(r.code == 2 && has(r.out, "sam3=yes") && !has(r.out, version) &&
              !fs::exists(gui_conf()),
          "no terminal and no =yes: exit 2, names the =yes form, runs nothing, records nothing");

    fresh();
    r = run("--accept-license bogus=yes --version");
    check(r.code == 2 && has(r.out, "unknown licence") && !fs::exists(gui_conf()),
          "an unknown family: exit 2");

    fresh();
    r = run("--accept-license sam3=yes --version",
            "env -u XDG_CONFIG_HOME -u HOME XDG_CACHE_HOME='" + (g_root / "cache").string() + "'");
    check(r.code == 2 && has(r.out, "XDG_CONFIG_HOME") && fs::is_empty(g_root / "cwd"),
          "no XDG_CONFIG_HOME and no HOME: refused, nothing written to the working directory");

    // The SAM tool is gated too: a SAM checkpoint named on the command line needs
    // its licence, whatever the file's contents. With no terminal it refuses BEFORE
    // opening the file, so a nonexistent path is enough to tell the gate from a load.
    for (const char* verb : {"segment --image x.png --text cat", "track --frames frames --text cat",
                             "extract clip.mp4"}) {
        fresh();
        r = run(std::string("sam ") + verb + " --model sam3-f16.ggml");
        check(r.code == 2 && has(r.out, "--accept-license sam3=yes") && has(r.out, "No terminal available") &&
                  !fs::exists(gui_conf()),
              std::string("sam ") + verb + ": refused without consent, naming --accept-license");
        r = run("--accept-license sam3=yes sam " + std::string(verb) + " --model sam3-f16.ggml");
        check(!has(r.out, "has not been accepted") && has(slurp(gui_conf()), "accepted_license=sam3\n"),
              std::string("sam ") + verb + ": with consent the gate is passed (any later failure is the file's)");
    }
    fresh();
    r = run("sam segment --image x.png --text cat --model sam2.1_hiera_tiny_f16.ggml");
    check(r.code == 2 && has(r.out, "--accept-license sam2=yes"),
          "a SAM 2.1 checkpoint is gated under its own family, sam2");
    fresh();
    r = run("sam segment --image x.png --text cat --model my-own-model.safetensors");
    check(!has(r.out, "has not been accepted"), "control: a file with no SAM family name is not gated");

    std::error_code ec;
    fs::remove_all(g_root, ec);
    std::printf("%s (%d failure%s)\n", g_failures ? "FAILED" : "PASSED", g_failures,
                g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
#endif
}
