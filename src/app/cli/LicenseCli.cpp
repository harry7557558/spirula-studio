#include "app/cli/LicenseCli.h"

#include "core/Env.h"
#include "core/LicenseConsent.h"
#include "i18n/Message.h"
#include "i18n/catalog/Cli.h"
#include "nn/core/Error.h"
#include "nn/io/Fetch.h"

#include <iostream>

#ifdef _WIN32
#include <io.h>
#define SS_ISATTY(fd) _isatty(fd)
#else
#include <unistd.h>
#define SS_ISATTY(fd) isatty(fd)
#endif

namespace cmsg = spirula::i18n::msg::cli;
namespace lic = spirula::license;

namespace app {

bool terminal_allowed(bool is_tty) { return is_tty && !spirula::env_on("NO_AUTO_FETCH"); }

ConsentIO stdio_consent() {
    ConsentIO io;
    io.tty = terminal_allowed(SS_ISATTY(0) && SS_ISATTY(1));
    io.read_line = [] {
        std::string a;
        std::getline(std::cin, a);
        return a;
    };
    return io;
}

namespace {

// The whole agreement on the terminal, then the one question. A reader who
// scrolls past it still answers for the full text: it is all printed.
void print_terms(const lic::Terms& t, const ConsentIO& io) {
    const char* text = spirula::i18n::ascii_copy(t.text);
    std::fprintf(io.out, "\n==== %s ====\n%s\n\n%s\n==== end of %s ====\n\n", t.title, t.url,
                 text, t.title);
}

void record_and_say(const lic::Terms& t, const ConsentIO& io) {
    NN_CHECK(lic::record(t.family), "cannot record the acceptance of %s in %s", t.title,
             lic::settings_path().c_str());
    std::fprintf(io.out, "[license] accepted %s; recorded in %s\n", t.title,
                 lic::settings_path().c_str());
}

// Asks, and records only on a literal "yes". The caller has printed the terms.
void ask_and_record(const lic::Terms& t, const ConsentIO& io) {
    std::fprintf(io.out, "%s", spirula::i18n::format(cmsg::license_ask_confirm, {t.title}).c_str());
    std::fflush(io.out);
    const std::string answer = io.read_line();
    if (answer != "yes")
        nn::fail("%s", spirula::i18n::format(cmsg::license_ask_declined, {t.title, answer}).c_str());
    record_and_say(t, io);
}

}  // namespace

void require_license(const char* family, const ConsentIO& io) {
    if (!family || !*family) return;
    for (const std::string& fam : lic::missing(family)) {
        const lic::Terms* t = lic::terms_for(fam);
        const bool no_home = lic::settings_path().empty();
        NN_CHECK(t, "unknown licence family '%s' (known: %s)", fam.c_str(),
                 lic::known_families().c_str());
        if (!io.tty || !io.read_line || no_home) {
            std::string m = spirula::i18n::format(cmsg::license_no_terminal, {t->title, t->url, fam});
            if (no_home) m += "\n  " + std::string(cmsg::license_no_home.get());
            nn::fail("%s", m.c_str());
        }
        std::fprintf(io.out, "%s\n", spirula::i18n::format(cmsg::license_ask_header, {t->title}).c_str());
        print_terms(*t, io);
        ask_and_record(*t, io);
    }
}

void require_license(const char* family) { require_license(family, stdio_consent()); }

void accept_licenses(const std::string& spec, const ConsentIO& io) {
    NN_CHECK(!lic::settings_path().empty(),
             "--accept-license: neither XDG_CONFIG_HOME nor HOME is set, so there is nowhere "
             "to record an acceptance.");
    for (const std::string& token : lic::split_families(spec)) {
        const size_t eq = token.find('=');
        const std::string fam = token.substr(0, eq);
        const std::string how = eq == std::string::npos ? "" : token.substr(eq + 1);
        const lic::Terms* t = lic::terms_for(fam);
        NN_CHECK(t, "--accept-license: unknown licence '%s' (known: %s)", fam.c_str(),
                 lic::known_families().c_str());
        NN_CHECK(how.empty() || how == "yes", "--accept-license %s: only '=yes' follows a family",
                 fam.c_str());
        if (lic::accepted(fam)) {
            std::fprintf(io.out, "[license] %s is already accepted.\n", t->title);
            continue;
        }
        print_terms(*t, io);
        if (how.empty()) {
            NN_CHECK(io.tty && io.read_line,
                     "--accept-license %s: there is no terminal to ask on. After reading the "
                     "terms above, pass --accept-license %s=yes.", fam.c_str(), fam.c_str());
            ask_and_record(*t, io);
        } else {
            record_and_say(*t, io);
        }
    }
}

int consume_accept_license_args(int& argc, char** argv) {
    int found = 0, w = 1;
    const ConsentIO io = stdio_consent();
    for (int r = 1; r < argc; ++r) {
        const std::string a = argv[r];
        std::string spec;
        if (a == "--accept-license") {
            NN_CHECK(r + 1 < argc, "--accept-license needs a value, e.g. sam3,gdino=yes");
            spec = argv[++r];
        } else if (a.rfind("--accept-license=", 0) == 0) {
            spec = a.substr(std::string("--accept-license=").size());
        } else {
            argv[w++] = argv[r];
            continue;
        }
        accept_licenses(spec, io);
        ++found;
    }
    argc = w;
    return found;
}

void install_license_gate() {
    nn::set_license_gate([](const char* families) { require_license(families); });
}

}  // namespace app
