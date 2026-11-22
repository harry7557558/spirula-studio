// Licence consent: an unaccepted family REFUSES in the CLI path (ensure_file),
// before any network or cache access; acceptance is recorded in the key the GUI
// reads; the terms shown are the published text and nothing more.
//
// Runs against a scratch config and cache directory, never the user's.

#include "app/cli/LicenseCli.h"
#include "app/gui/FetchSource.h"
#include "core/LicenseConsent.h"
#include "core/ModelMirror.h"
#include "i18n/Message.h"
#include "i18n/catalog/Cli.h"
#include "nn/core/Error.h"
#include "nn/io/Fetch.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#define SS_SETENV(k, v) _putenv_s(k, v)
#define SS_UNSETENV(k) _putenv_s(k, "")
#else
#include <unistd.h>
#define SS_SETENV(k, v) setenv(k, v, 1)
#define SS_UNSETENV(k) unsetenv(k)
#endif

namespace fs = std::filesystem;
namespace lic = spirula::license;

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

std::string message_of(const std::function<void()>& fn) {
    try {
        fn();
    } catch (const nn::Error& e) {
        return e.what();
    }
    return "<did not throw>";
}

bool has(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

bool refuses_family(const std::string& message, const char* family) {
    const auto* terms = lic::terms_for(family);
    return terms && has(message, spirula::i18n::format(
        spirula::i18n::msg::cli::license_no_terminal, {terms->title, terms->url, family}));
}

std::string slurp(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

fs::path g_root;

// A fresh, empty config + cache for each case.
void reset_dirs() {
    std::error_code ec;
    fs::remove_all(g_root, ec);
    fs::create_directories(g_root / "config", ec);
    fs::create_directories(g_root / "cache", ec);
#ifdef _WIN32
    SS_SETENV("APPDATA", (g_root / "config").string().c_str());
    SS_SETENV("LOCALAPPDATA", (g_root / "cache").string().c_str());
#else
    SS_SETENV("XDG_CONFIG_HOME", (g_root / "config").string().c_str());
    SS_SETENV("XDG_CACHE_HOME", (g_root / "cache").string().c_str());
#endif
}

fs::path gui_conf() { return lic::settings_path(); }

// Each call captures what accept_licenses prints into its own file.
struct Capture {
    std::FILE* f = std::tmpfile();
    std::string text() {
        std::fflush(f);
        std::rewind(f);
        std::string s;
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) s.append(buf, n);
        return s;
    }
    ~Capture() { if (f) std::fclose(f); }
};

// A file that would download from nowhere: a refusal that is NOT about consent
// shows up as a curl failure, a different message.
const char* kNever = "http://127.0.0.1:9/never-downloaded.bin";

nn::FetchFile never_file(const char* family) {
    nn::FetchFile f;
    f.file = "consent-test.bin";
    f.url = kNever;
    f.sha256 = "0000000000000000000000000000000000000000000000000000000000000000";
    f.bytes = 1;
    f.license_family = family;
    f.mirror = "http://127.0.0.1:9/never-mirrored.bin";   // the project mirror is a real host
    return f;
}

void test_refusal() {
    reset_dirs();
    const nn::FetchFile f = never_file("sam3,gdino");
    const std::string m = message_of([&] { nn::ensure_file(f, "test"); });
    check(refuses_family(m, "sam3") && has(m, "--accept-license sam3"),
          "unaccepted family: ensure_file REFUSES, naming the family and the flag");
    check(!fs::exists(g_root / "cache" / "spirula-studio" / "models" / "consent-test.bin.part") &&
              !fs::exists(g_root / "cache" / "spirula-studio" / "models" / "consent-test.bin"),
          "unaccepted family: nothing was fetched or written");
    check(!fs::exists(gui_conf()), "unaccepted family: the refusal records nothing");

    // Only the DOWNLOAD is gated: a verified copy already in the cache loads with
    // no consent recorded, so a user whose files predate consent keeps working.
    fs::create_directories(g_root / "cache" / "spirula-studio" / "models");
    const fs::path dst = nn::cached_path(f);
    { std::ofstream o(dst, std::ios::binary); o << "abc"; }
    nn::FetchFile cached = f;
    cached.sha256 = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    check(nn::ensure_file(cached, "test") == dst.string() && !lic::accepted("sam3"),
          "a cached, hash-correct copy loads with no consent recorded");
    fs::remove(dst);
    const std::string m2 = message_of([&] { nn::ensure_file(cached, "test"); });
    check(refuses_family(m2, "sam3"), "the same file, absent: the DOWNLOAD is refused");

    // Positive control: with consent, the same call gets as far as the download.
    // Without it the refusals above could be any failure at all.
    app::ConsentIO io;
    Capture cap;
    io.out = cap.f;
    app::accept_licenses("sam3=yes", io);
    const std::string m3 = message_of([&] { nn::ensure_file(cached, "test"); });
    check(refuses_family(m3, "gdino") && has(m3, "--accept-license gdino") &&
              !has(m3, "--accept-license sam3"),
          "accepting sam3 alone still refuses a file that also needs gdino, naming gdino");
    app::accept_licenses("gdino=yes", io);
    const std::string m4 = message_of([&] { nn::ensure_file(f, "test"); });
    check(!refuses_family(m4, "sam3") && !refuses_family(m4, "gdino") && m4 != "<did not throw>" &&
              has(m4, "never-downloaded.bin"),
          "accepted but absent: it proceeds to the download, which fails on its own terms");
}

// What the dialogs and a batch lean on, in core: the tick policy and the family list.
void test_dialog_policy() {
    for (const char* fam : {"sam3", "sam2", "gdino", "birefnet"}) {
        check(!lic::accept_enabled(fam, false), std::string(fam) + ": Accept is OFF until ticked");
        check(lic::accept_enabled(fam, true), std::string(fam) + ": Accept is ON once ticked");
    }
    check(!lic::accept_enabled("nonsense", true), "a family with no terms can never be accepted");
    const std::vector<std::string> want = {"sam3", "gdino", "sam2", "birefnet"};
    check(lic::unique_families({"sam3", "", "gdino, sam3", "sam2,birefnet", "birefnet"}) == want,
          "unique_families: comma lists flattened, each family once, first-seen order");
    check(lic::unique_families({}).empty() && lic::unique_families({"", " , "}).empty(),
          "unique_families: nothing named, nothing asked");
}

// A terminal is asked; a script is not. Both paths go through require_license, which
// is what ensure_file calls, so this is the prompt a gated download shows.
void test_require_prompt() {
    reset_dirs();
    Capture cap;
    app::ConsentIO io;
    io.out = cap.f;
    io.tty = true;
    int asked = 0;
    io.read_line = [&] { ++asked; return std::string("no"); };
    std::string m = message_of([&] { app::require_license("sam3", io); });
    const std::string shown = cap.text();
    check(has(m, "was not accepted") && !lic::accepted("sam3") && asked == 1,
          "TTY, 'no': refused after one question, nothing recorded");
    check(has(shown, spirula::i18n::ascii_copy(lic::terms_for("sam3")->text)),
          "TTY: the whole SAM license is printed before the question");
    check(has(shown, "read and accept the terms of SAM License (Meta)"),
          "TTY: the question says the user has read and accepts, naming the licence");
    io.read_line = [&] { ++asked; return std::string("y"); };
    m = message_of([&] { app::require_license("sam3", io); });
    check(has(m, "was not accepted") && !lic::accepted("sam3"), "TTY, 'y': only 'yes' accepts");
    io.read_line = [&] { ++asked; return std::string("yes"); };
    check(message_of([&] { app::require_license("sam3", io); }) == "<did not throw>" &&
              lic::accepted("sam3"),
          "TTY, 'yes': proceeds, and the acceptance is recorded");
    const int before = asked;
    app::require_license("sam3", io);
    check(asked == before, "an accepted family is not asked again");

    // Two families: asked once each, in order, and a refusal of the second keeps the first.
    reset_dirs();
    std::vector<std::string> answers = {"yes", "no"};
    io.read_line = [&] { const std::string a = answers.front(); answers.erase(answers.begin()); return a; };
    m = message_of([&] { app::require_license("sam3,birefnet", io); });
    check(has(m, "MIT") && lic::accepted("sam3") && !lic::accepted("birefnet"),
          "two families: the first accepted and kept, the second refused by name");

    // No terminal.
    reset_dirs();
    app::ConsentIO script;
    Capture cap2;
    script.out = cap2.f;
    script.read_line = [] { return std::string("yes"); };   // present, but tty is false
    m = message_of([&] { app::require_license("gdino", script); });
    check(refuses_family(m, "gdino") && has(m, "--accept-license gdino=yes") && !lic::accepted("gdino"),
          "no terminal: refused, naming --accept-license gdino=yes, even with an input present");
    check(cap2.text().empty(), "no terminal: nothing is printed to a script's stdout");
    check(message_of([&] { app::require_license(nullptr, script); }) == "<did not throw>" &&
              message_of([&] { app::require_license("", script); }) == "<did not throw>",
          "a null or empty family needs nothing");
    check(has(message_of([&] { app::require_license("nonsense", script); }), "unknown licence"),
          "an unknown family is refused, not waved through");
}

void test_no_family_unaffected() {
    reset_dirs();
    nn::FetchFile f = never_file(nullptr);
    f.mirror = nullptr;
    SS_SETENV("SS_NO_AUTO_FETCH", "1");   // the mirror is a real host; do not call it
    const std::string m = message_of([&] { nn::ensure_file(f, "test"); });
    SS_UNSETENV("SS_NO_AUTO_FETCH");
    check(has(m, "may not download") && !refuses_family(m, "sam3"),
          "a file with no license_family is never gated");
    check(has(m, "modelscope.cn"), "control: an ordinary file's refusal names the project mirror");
    check(!nn::mirror_url(f).empty(), "default FetchFile keeps its project mirror");
}

void test_record_and_gui_key() {
    reset_dirs();
    fs::create_directories(gui_conf().parent_path());
    {   // What GuiApp::save_settings writes, plus an accepted family.
        std::ofstream o(gui_conf(), std::ios::binary);
        o << "colmap_exe=/opt/colmap\nlang=de\naccepted_license=sam3\nui_scale=1.250\n";
    }
    check(lic::accepted("sam3") && !lic::accepted("sam2"),
          "reads the GUI's accepted_license= lines");
    check(lic::record("sam2") && lic::accepted("sam2"), "record() accepts a family");
    const std::string conf = slurp(gui_conf());
    check(has(conf, "accepted_license=sam2\n"), "the key the GUI writes: accepted_license=sam2");
    check(has(conf, "colmap_exe=/opt/colmap\nlang=de\naccepted_license=sam3\nui_scale=1.250\n"),
          "every other line survives, in order");
    lic::record("sam2");
    size_t n = 0, at = 0;
    while ((at = conf.find("accepted_license=sam2", at)) != std::string::npos) { ++n; ++at; }
    check(n == 1 && slurp(gui_conf()) == conf, "record() is idempotent");
    check(!lic::record(""), "an empty family is not recorded");
    check(!lic::record("nonsense") && !lic::accepted("nonsense"),
          "a family with no terms is not recorded, so a typo cannot pose as consent");
    check(lic::accepted_all() == std::vector<std::string>{"sam3", "sam2"},
          "accepted_all() lists them in file order");
    // A prefix of an accepted family is not that family.
    check(!lic::accepted("sam") && !lic::accepted("sam2x"),
          "a family is matched whole, not as a prefix");
}

void test_prompts() {
    reset_dirs();
    Capture cap;
    app::ConsentIO io;
    io.out = cap.f;
    io.tty = true;
    io.read_line = [] { return std::string("no"); };
    std::string m = message_of([&] { app::accept_licenses("birefnet", io); });
    check(has(m, "was not accepted") && !lic::accepted("birefnet"),
          "TTY, answer 'no': refused and nothing recorded");
    io.read_line = [] { return std::string("YES"); };
    m = message_of([&] { app::accept_licenses("birefnet", io); });
    check(has(m, "was not accepted") && !lic::accepted("birefnet"),
          "TTY, answer 'YES': only the exact word 'yes' accepts");
    io.read_line = [] { return std::string("yes"); };
    app::accept_licenses("birefnet", io);
    check(lic::accepted("birefnet"), "TTY, answer 'yes': accepted and recorded");

    app::ConsentIO script;
    Capture cap2;
    script.out = cap2.f;
    m = message_of([&] { app::accept_licenses("gdino", script); });
    check(has(m, "gdino=yes") && !lic::accepted("gdino"),
          "no terminal and no '=yes': refused, and the message names the =yes form");
    m = message_of([&] { app::accept_licenses("nonsense=yes", script); });
    check(has(m, "unknown licence") && !lic::accepted("nonsense"), "an unknown family is refused");
    m = message_of([&] { app::accept_licenses("gdino=maybe", script); });
    check(has(m, "=yes") && !lic::accepted("gdino"), "only '=yes' follows a family");
    app::accept_licenses("gdino=yes", script);
    check(lic::accepted("gdino"), "no terminal, '=yes': accepted");
}

void test_terms_shown() {
    reset_dirs();
    Capture cap;
    app::ConsentIO io;
    io.out = cap.f;
    app::accept_licenses("sam3=yes,birefnet=yes", io);
    const std::string shown = cap.text();
    const lic::Terms* d = lic::terms_for("sam3");
    const lic::Terms* r = lic::terms_for("birefnet");
    check(d && r, "both families have terms");
    if (!d || !r) return;
    const std::string sam = spirula::i18n::ascii_copy(d->text);
    const std::string birefnet = spirula::i18n::ascii_copy(r->text);
    check(has(shown, sam), "the full SAM License uses terminal punctuation");
    check(has(shown, birefnet), "the full BiRefNet MIT text uses terminal punctuation");
    // Everything printed besides the two texts is ours; it must add no term.
    std::string ours = shown;
    for (const std::string& t : {sam, birefnet}) {
        const size_t at = ours.find(t);
        if (at != std::string::npos) ours.erase(at, t.size());
    }
    check(!has(lower(ours), "commercial") && !has(lower(ours), "redistribut"),
          "our framing adds no commercial-use or redistribution wording");

    // The embedded text is the file under LICENSES/, not a stale copy.
    const fs::path lic_dir = fs::path(SS_REPO_ROOT) / "LICENSES";
    if (!fs::exists(lic_dir / "SAM3-License.txt")) {
        check(false, "LICENSES/ not found next to the sources");
        return;
    }
    // Every built-in family has its terms, and they are the files under LICENSES/.
    const struct { const char* fam; const char* file; } kAll[] = {
        {"sam3", "SAM3-License.txt"}, {"sam2", "Apache-2.0-SAM2.txt"},
        {"gdino", "Apache-2.0-GroundingDINO.txt"}, {"birefnet", "MIT-BiRefNet.txt"}};
    for (const auto& e : kAll) {
        const lic::Terms* t = lic::terms_for(e.fam);
        check(t && std::string(t->text).size() > 500 && slurp(lic_dir / e.file) == t->text,
              std::string("terms_for(") + e.fam + ") is LICENSES/" + e.file + ", non-trivially");
    }
    check(lic::terms_for("nonsense") == nullptr, "an unknown family has no terms");
}

void test_argv() {
    reset_dirs();
    std::vector<std::string> store = {"spirula", "sam", "--accept-license", "sam3=yes",
                                      "ds", "--accept-license=birefnet=yes", "--preset", "base"};
    std::vector<char*> argv;
    for (auto& s : store) argv.push_back(s.data());
    argv.push_back(nullptr);
    int argc = (int)store.size();
    const int n = app::consume_accept_license_args(argc, argv.data());
    check(n == 2 && argc == 5, "both spellings are found and removed from argv");
    check(std::string(argv[1]) == "sam" && std::string(argv[2]) == "ds" &&
              std::string(argv[3]) == "--preset" && std::string(argv[4]) == "base",
          "every other argument keeps its order");
    check(lic::accepted("sam3") && lic::accepted("birefnet"), "and both were recorded");
    int c2 = 2;
    char a0[] = "x", a1[] = "--accept-license";
    char* v2[] = {a0, a1, nullptr};
    check(message_of([&] { app::consume_accept_license_args(c2, v2); }).find("needs a value") !=
              std::string::npos,
          "--accept-license with no value is an error");
}

// Every route a download takes resolves its fallback through spirula::mirror_for.
void test_mirror_paths() {
    const nn::FetchFile gated = never_file("sam3,gdino");
    check(spirula::mirror_for(gated.file, gated.mirror) == gated.mirror &&
              nn::mirror_url(gated) == gated.mirror,
          "mirror_for and nn::mirror_url agree: a file's own mirror");
    const gui::PendingDownload p = gui::pending_download(gated);
    check(p.mirror == gated.mirror && p.url == gated.url && p.license_family == "sam3,gdino" &&
              p.dest == nn::cached_path(gated),
          "GUI download queue entry: the mirror, both licences carried, the cache path");
    // An own mirror still wins for an ordinary file, and the project's is the default.
    nn::FetchFile own;
    own.file = "x.bin";
    own.url = "https://example.invalid/primary/x.bin";
    own.mirror = "https://example.invalid/x.bin";
    check(gui::pending_download(own).mirror == "https://example.invalid/x.bin",
          "control: a file's own mirror is carried");
    own.mirror = nullptr;
    check(has(gui::pending_download(own).mirror, "modelscope.cn") &&
              gui::pending_download(own).license_family.empty(),
          "control: an ordinary file gets the project mirror and no licence");
}

// ensure_file asks the application's gate; with none installed a gated file is refused.
void test_gate_installation() {
    reset_dirs();
    const nn::FetchFile f = never_file("gdino,sam2");
    nn::set_license_gate(nullptr);
    std::string m = message_of([&] { nn::ensure_file(f, "test"); });
    check(has(m, "has not installed a licence gate") && !fs::exists(nn::cached_path(f)),
          "no gate installed: a gated file is refused, not waved through");
    check(message_of([&] { nn::ensure_file(never_file(nullptr), "test"); }) != "<did not throw>",
          "control: an ungated file reaches the download, which fails on its own terms");

    std::vector<std::string> asked;
    nn::set_license_gate([&](const char* families) { asked.push_back(families); });
    m = message_of([&] { nn::ensure_file(f, "test"); });
    check(asked == std::vector<std::string>{"gdino,sam2"} && !refuses_family(m, "gdino") && !refuses_family(m, "sam2"),
          "the gate is handed the family list as written, and its silence lets the file through");
    nn::set_license_gate([](const char*) { nn::fail("declined by the test gate"); });
    m = message_of([&] { nn::ensure_file(f, "test"); });
    check(has(m, "declined by the test gate"), "a gate that throws refuses the file");
    app::install_license_gate();
}

// A family a feature registers is a family everywhere: terms, record, the list in errors.
void test_registered_family() {
    reset_dirs();
    static const lic::Terms kExtra{"testfam", "Test Licence", "https://example.invalid/l", "TERMS TEXT"};
    check(lic::terms_for("testfam") == nullptr && !lic::record("testfam"),
          "before registering, the family is unknown and cannot be recorded");
    lic::register_terms(&kExtra);
    lic::register_terms(&kExtra);   // idempotent
    check(lic::terms_for("testfam") == &kExtra, "registered: terms_for finds it");
    const std::string known = lic::known_families();
    check(known == "sam3, sam2, gdino, birefnet, testfam",
          "known_families lists the built-ins, then registrations: " + known);
    const lic::Terms clash{"sam3", "Impostor", "x", "x"};
    lic::register_terms(&clash);
    check(std::string(lic::terms_for("sam3")->title) == "SAM License (Meta)",
          "a registration cannot replace a built-in family");
    Capture cap;
    app::ConsentIO io;
    io.out = cap.f;
    app::accept_licenses("testfam=yes", io);
    check(lic::accepted("testfam") && has(cap.text(), "TERMS TEXT"),
          "a registered family is accepted through the same path and its text printed");
}

void test_helpers() {
    SS_UNSETENV("SS_NO_AUTO_FETCH");
    check(app::terminal_allowed(true) && !app::terminal_allowed(false),
          "a terminal may be asked; no terminal may not");
    SS_SETENV("SS_NO_AUTO_FETCH", "1");
    check(!app::terminal_allowed(true),
          "SS_NO_AUTO_FETCH is 'no terminal': a GUI's child never waits on stdin");
    SS_UNSETENV("SS_NO_AUTO_FETCH");
    check(lic::split_families(" sam3 , ,gdino,") == std::vector<std::string>{"sam3", "gdino"},
          "split_families trims and drops empty items");
    const std::string a = lic::scratch_path_for("/x/gui.conf"), b = lic::scratch_path_for("/x/gui.conf");
    check(a != b && a.rfind("/x/gui.conf", 0) == 0,
          "scratch_path_for: two writers never share a temporary name");
}

}  // namespace

int main() {
    g_root = fs::temp_directory_path() / "spirula_fetch_consent_test";
    app::install_license_gate();
    try {
        test_refusal();
        test_no_family_unaffected();
        test_record_and_gui_key();
        test_prompts();
        test_terms_shown();
        test_require_prompt();
        test_dialog_policy();
        test_argv();
        test_gate_installation();
        test_registered_family();
        test_helpers();
        test_mirror_paths();
    } catch (const std::exception& e) {
        std::printf("FAIL exception: %s\n", e.what());
        return 1;
    }
    std::error_code ec;
    fs::remove_all(g_root, ec);
    std::printf("%s (%d failure%s)\n", g_failures ? "FAILED" : "PASSED", g_failures,
                g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
