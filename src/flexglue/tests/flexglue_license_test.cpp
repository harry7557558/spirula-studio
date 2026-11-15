// Every FlexGlue checkpoint carries Meta's DINOv3 weights, so resolving one asks
// the licence gate first: the fetched file and a path the user names alike.
// Needs no GPU and no checkpoint; the file it resolves is an empty stand-in.

#include "flexglue/model/Fetch.h"
#include "nn/io/Fetch.h"

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#define SS_SETENV(k, v) _putenv_s(k, v)
#else
#define SS_SETENV(k, v) setenv(k, v, 1)
#endif

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void check(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) ++g_failures;
}

// The message resolve_checkpoint threw, or "" when it returned.
std::string thrown(const std::string& arg) {
    try {
        flexglue::resolve_checkpoint(arg);
    } catch (const std::exception& e) {
        return e.what()[0] ? e.what() : "(empty)";
    }
    return "";
}

bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }

}  // namespace

int main() {
    const fs::path dir = fs::temp_directory_path() / "spirula_flexglue_license_test";
    std::error_code ec;
    fs::create_directories(dir, ec);
    const fs::path pth = dir / "my_flexglue.pth";
    std::ofstream(pth) << "not a checkpoint";
    const std::string user = pth.string();

    check(std::string(flexglue::kCheckpoint.license_family) == flexglue::kLicenseFamily,
          "the release checkpoint names the flexglue family");

    // No gate installed: nothing may load, whatever the path.
    std::string e = thrown(user);
    check(has(e, "licence gate"), "no gate: a user path is refused");

    std::string asked;
    nn::set_license_gate([&](const char* f) {
        asked = f;
        throw std::runtime_error("declined");
    });
    e = thrown(user);
    check(has(e, "declined") && asked == flexglue::kLicenseFamily,
          "a declining gate: a user path is refused, and the flexglue family was asked");
    asked.clear();
    e = thrown("does/not/exist.pth");
    check(has(e, "declined") && asked == flexglue::kLicenseFamily,
          "a declining gate is asked before the path is even looked at");
    asked.clear();
    // The id form fetches; SS_NO_AUTO_FETCH keeps a gate that let it through
    // from downloading anything.
    SS_SETENV("SS_NO_AUTO_FETCH", "1");
    e = thrown("flexglue");
    check(has(e, "declined") && asked == flexglue::kLicenseFamily,
          "a declining gate: the fetched checkpoint is refused too");

    nn::set_license_gate([&](const char* f) { asked = f; });
    asked.clear();
    e = thrown(user);
    check(e.empty() && asked == flexglue::kLicenseFamily,
          "an accepting gate: the user path resolves, after the family was asked");
    e = thrown("does/not/exist.pth");
    check(has(e, "neither"), "an accepting gate: a missing path still fails, naming it");

    fs::remove_all(dir, ec);
    std::printf("\n%d failures\n", g_failures);
    return g_failures ? 1 : 0;
}
