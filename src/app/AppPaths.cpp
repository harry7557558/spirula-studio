// AppPaths.cpp -- see AppPaths.h.

#include "app/AppPaths.h"

#include <cstdlib>
#include <filesystem>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace app {

namespace {

fs::path env_path(const char* var) {
    const char* v = std::getenv(var);
    return v && *v ? fs::path(v) : fs::path();
}

fs::path ensure(fs::path dir) {
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}

// <base>/spirula-studio, or an existing spirulae-splat/ from before the rename,
// adopted where it is so recents and cached models survive the upgrade. Drop it
// with the other compatibility shims (cmake/SsOptions.cmake, src/core/Env.h).
fs::path app_dir(const fs::path& base) {
    std::error_code ec;
    const fs::path current = base / "spirula-studio";
    if (!fs::exists(current, ec) && fs::is_directory(base / "spirulae-splat", ec))
        return base / "spirulae-splat";
    return ensure(current);
}

}  // namespace

std::string config_dir() {
#ifdef _WIN32
    fs::path dir = env_path("APPDATA");
#else
    fs::path dir = env_path("XDG_CONFIG_HOME");
    if (dir.empty()) {
        fs::path home = env_path("HOME");
        if (!home.empty()) dir = home / ".config";
    }
#endif
    if (dir.empty()) dir = ".";
    return app_dir(dir).string();
}

std::string cache_dir() {
#ifdef _WIN32
    fs::path dir = env_path("LOCALAPPDATA");
#else
    fs::path dir = env_path("XDG_CACHE_HOME");
    if (dir.empty()) {
        fs::path home = env_path("HOME");
        if (!home.empty()) dir = home / ".cache";
    }
#endif
    if (dir.empty()) dir = ".";
    return app_dir(dir).string();
}

std::string exe_path() {
    static const std::string cached = [] {
        std::error_code ec;
#ifdef _WIN32
        wchar_t buf[32768];
        DWORD n = GetModuleFileNameW(nullptr, buf, (DWORD)std::size(buf));
        if (n == 0 || n >= std::size(buf)) return std::string();
        return fs::path(std::wstring(buf, n)).string();
#elif defined(__APPLE__)
        uint32_t n = 0;
        _NSGetExecutablePath(nullptr, &n);
        std::string buf(n, '\0');
        if (_NSGetExecutablePath(buf.data(), &n) != 0) return std::string();
        fs::path p = fs::weakly_canonical(fs::path(buf.c_str()), ec);
        return ec ? std::string() : p.string();
#else
        fs::path p = fs::read_symlink("/proc/self/exe", ec);
        return ec ? std::string() : p.string();
#endif
    }();
    return cached;
}

std::string exe_dir() {
    const std::string p = exe_path();
    return p.empty() ? p : fs::path(p).parent_path().string();
}

void add_desktop_search_paths() {
#ifdef __APPLE__
    const char* dirs[] = { "/opt/homebrew/bin", "/usr/local/bin", "/opt/local/bin" };
    const char* cur = std::getenv("PATH");
    std::string path = cur ? cur : "";
    for (const char* dir : dirs) {
        std::error_code ec;
        if (!fs::is_directory(dir, ec)) continue;
        bool have = false;
        for (size_t i = 0; i <= path.size() && !have; ) {
            size_t end = path.find(':', i);
            if (end == std::string::npos) end = path.size();
            have = path.compare(i, end - i, dir) == 0;
            i = end + 1;
        }
        if (have) continue;
        if (!path.empty()) path += ':';
        path += dir;
    }
    setenv("PATH", path.c_str(), 1);
#endif
}

}  // namespace app
