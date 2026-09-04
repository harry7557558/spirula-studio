#include "app/gui/ReconStamp.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

namespace gui {

namespace {

// The flag `args[i]` belongs to, which is the flag itself or the nearest one
// before it. `args` is a command line, so a bare token is a flag's value.
std::string flag_at(const std::vector<std::string>& args, size_t i) {
    for (size_t j = i + 1; j-- > 0;)
        if (args[j].rfind("--", 0) == 0) return args[j];
    return i < args.size() ? args[i] : std::string();
}

}  // namespace

ReconStamp read_recon_stamp(const std::string& workspace) {
    ReconStamp st;
    if (workspace.empty()) return st;
    FILE* f = std::fopen((fs::path(workspace) / kReconStampFile).string().c_str(), "r");
    if (!f) return st;
    char line[4096];
    bool first = true;
    while (std::fgets(line, sizeof line, f)) {
        std::string s = line;
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        if (first) {
            st.engine = s;
            first = false;
            continue;
        }
        st.args.push_back(std::move(s));
    }
    std::fclose(f);
    st.present = !first;
    return st;
}

void write_recon_stamp(const std::string& workspace, const ReconStamp& st) {
    if (workspace.empty()) return;
    FILE* f = std::fopen((fs::path(workspace) / kReconStampFile).string().c_str(), "w");
    if (!f) return;
    std::fprintf(f, "%s\n", st.engine.c_str());
    for (const std::string& a : st.args) {
        // A newline inside a value would read back as two flags. None of the
        // callers can produce one, and a stamp that fails to match only costs
        // a rebuild, so drop the line rather than escape it.
        if (a.find('\n') != std::string::npos) continue;
        std::fprintf(f, "%s\n", a.c_str());
    }
    std::fclose(f);
}

std::string recon_stamp_change(const ReconStamp& prior, const ReconStamp& now) {
    if (!prior.present) return "";
    if (prior.engine != now.engine) return now.engine;
    const size_t n = std::min(prior.args.size(), now.args.size());
    for (size_t i = 0; i < n; i++)
        if (prior.args[i] != now.args[i]) return flag_at(now.args, i);
    if (prior.args.size() == now.args.size()) return "";
    const std::vector<std::string>& longer =
        prior.args.size() > now.args.size() ? prior.args : now.args;
    return flag_at(longer, n);
}

}  // namespace gui
