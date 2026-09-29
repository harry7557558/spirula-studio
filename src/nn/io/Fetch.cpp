#include "nn/io/Fetch.h"

#include "core/Env.h"
#include "core/ModelMirror.h"
#include "core/Sha256.h"
#include "nn/core/Error.h"
#include "nn/core/Log.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace fs = std::filesystem;

namespace nn {
namespace {

std::string env_str(const char* name) {
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
}

fs::path cache_root() {
#ifdef _WIN32
    fs::path dir = env_str("LOCALAPPDATA");
#else
    fs::path dir = env_str("XDG_CACHE_HOME");
    if (dir.empty()) {
        const std::string home = env_str("HOME");
        if (!home.empty()) dir = fs::path(home) / ".cache";
    }
#endif
    if (dir.empty()) dir = ".";
    // An existing spirulae-splat/ from before the rename is adopted where it
    // is -- the checkpoints in it are a big download to repeat.
    std::error_code ec;
    if (!fs::exists(dir / "spirula-studio", ec) &&
        fs::is_directory(dir / "spirulae-splat", ec))
        return dir / "spirulae-splat";
    return dir / "spirula-studio";
}

// Abort a connection that never opens, or a transfer under 1 KB/s for a minute.
const char* kCurlTimeouts = "--connect-timeout 30 --speed-limit 1024 --speed-time 60";

bool have_curl() {
#ifdef _WIN32
    return std::system("curl --version >NUL 2>&1") == 0;
#else
    return std::system("curl --version >/dev/null 2>&1") == 0;
#endif
}

}  // namespace

std::string model_cache_dir() {
    return (cache_root() / "models").string();
}

std::string cached_path(const FetchFile& f) {
    return (cache_root() / "models" / f.file).string();
}

std::string mirror_url(const FetchFile& f) {
    return f.mirror ? std::string(f.mirror) : spirula::model_mirror_url(f.file);
}

std::string sha256_file(const std::string& path) {
    return spirula::sha256_file(path);
}

std::string ensure_file(const FetchFile& f, const char* tag) {
    const fs::path dst = cached_path(f);
    std::error_code ec;

    if (fs::exists(dst, ec)) {
        const std::string got = sha256_file(dst.string());
        if (got == f.sha256) return dst.string();
        // A cached file that does not hash is a failed or interrupted download
        // from a previous run, not a reason to stop: say so and refetch.
        NN_LOG_WARN("[%s] cached %s does not match its checksum; re-downloading\n", tag,
                    f.file);
        fs::remove(dst, ec);
    }

    NN_CHECK(!spirula::env_on("NO_AUTO_FETCH"),
             "%s is not in the model cache, and this process may not download "
             "it.\n  Get it from the application's own download button, or "
             "fetch\n    %s\n  or\n    %s\n  to\n    %s\n  by hand.",
             f.file, f.url, mirror_url(f).c_str(), dst.string().c_str());

    fs::create_directories(dst.parent_path(), ec);
    NN_CHECK(!ec, "cannot create %s: %s", dst.parent_path().string().c_str(),
             ec.message().c_str());

    NN_CHECK(have_curl(),
             "curl was not found, and it is how checkpoints are fetched.\n"
             "  Install curl, or download\n    %s\n  to\n    %s\n  by hand.",
             f.url, dst.string().c_str());

    fs::path part = dst;
    part += ".part";

    const std::string urls[] = {f.url, mirror_url(f)};
    std::string why;
    for (const std::string& url : urls) {
        if (!why.empty())
            NN_LOG_WARN("[%s] %s; trying %s\n", tag, why.c_str(), url.c_str());
        NN_LOG_INFO("[%s] fetching %s (%.1f MB) from %s\n", tag, f.file,
                    (double)f.bytes / 1e6, url.c_str());
        // -C - resumes a partial .part file; -f makes an HTTP error an exit code
        // rather than a saved error page. The timeouts turn a blocked host into
        // a failure the mirror can answer instead of a hang.
        const std::string cmd = "curl -L -f --progress-bar -C - " + std::string(kCurlTimeouts) +
                                " -o \"" + part.string() + "\" \"" + url + "\"";
        int rc = std::system(cmd.c_str());
#ifndef _WIN32
        if (WIFSIGNALED(rc) && WTERMSIG(rc) == SIGINT) {
            fs::remove(part, ec);
            nn::fail("downloading %s was interrupted", f.file);
        }
        if (WIFEXITED(rc)) rc = WEXITSTATUS(rc);
#endif
        if (rc != 0) {
            fs::remove(part, ec);
            why = "downloading " + std::string(f.file) + " from " + url +
                  " failed (curl exit " + std::to_string(rc) + ")";
            continue;
        }
        const std::string got = sha256_file(part.string());
        if (got != f.sha256) {
            fs::remove(part, ec);
            why = std::string(f.file) + " from " + url + " has SHA-256 " + got +
                  ", expected " + f.sha256;
            continue;
        }
        why.clear();
        break;
    }
    if (!why.empty())
        nn::fail("%s.\n  Fetch it by hand from\n    %s\n  or\n    %s\n  and save it as\n    %s",
                 why.c_str(), f.url, urls[1].c_str(), dst.string().c_str());

    fs::rename(part, dst, ec);
    NN_CHECK(!ec, "cannot move the download into place: %s", ec.message().c_str());
    NN_LOG_INFO("[%s] saved %s\n", tag, dst.string().c_str());
    return dst.string();
}

}  // namespace nn
