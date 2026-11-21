#pragma once
// Workspace-relative path helpers for portable manifests, signatures, and records.

#include <cctype>
#include <filesystem>
#include <string>

namespace spirula {

namespace detail {

inline std::filesystem::path normalize_for_rel(const std::string& path,
                                               std::error_code& ec) {
    namespace fs = std::filesystem;
    fs::path p = fs::absolute(fs::u8path(path), ec).lexically_normal();
    if (ec) return {};
    if (p.has_relative_path() && p.filename().empty()) p = p.parent_path();
    return p;
}

inline void match_drive_case(std::filesystem::path& path,
                             const std::filesystem::path& base) {
    if (!path.has_root_name() || !base.has_root_name()) return;
    const std::string rp = path.root_name().u8string();
    const std::string rb = base.root_name().u8string();
    if (rp != rb && rp.size() == 2 && rb.size() == 2 && rp[1] == ':' && rb[1] == ':' &&
        std::tolower((unsigned char)rp[0]) == std::tolower((unsigned char)rb[0]) &&
        path.has_root_directory()) {
        std::string s = path.generic_u8string();
        if (s.size() >= 2 && s[1] == ':') {
            s[0] = rb[0];
            path = std::filesystem::u8path(s);
        }
    }
}

}  // namespace detail

// Relativize `path` against `base` using forward slashes; keeps an absolute
// path only when no relative path exists (e.g., another drive).
inline std::string relative_to_dir(const std::string& path, const std::string& base) {
    namespace fs = std::filesystem;
    if (path.empty() || base.empty()) return path;
    std::error_code ec;
    fs::path fp = detail::normalize_for_rel(path, ec);
    if (ec) return path;
    fs::path fb = detail::normalize_for_rel(base, ec);
    if (ec) return fp.u8string();
    detail::match_drive_case(fp, fb);
    const fs::path rel = fp.lexically_relative(fb);
    if (rel.empty() || rel.is_absolute()) return fp.u8string();
    return rel.generic_u8string();
}

// Resolve `path` against `base`. When `fallback_sibling_filename` is set and an
// external/absolute path is missing, check `<base>/../<filename>`.
inline std::string resolve_from_dir(const std::string& path, const std::string& base,
                                    bool fallback_sibling_filename = false) {
    namespace fs = std::filesystem;
    if (path.empty() || base.empty()) return path;
    std::error_code ec;
    const fs::path p = fs::u8path(path);
    const fs::path ws = detail::normalize_for_rel(base, ec);
    if (ec || ws.empty()) return path;

    const bool anchored = p.is_absolute() || p.has_root_name() || p.has_root_directory();
    fs::path resolved = anchored ? p : (ws / p).lexically_normal();
    if (!anchored && resolved.has_relative_path() && resolved.filename().empty())
        resolved = resolved.parent_path();
    if (fs::exists(resolved, ec)) return anchored ? path : resolved.u8string();

    if (fallback_sibling_filename) {
        fs::path norm = p.lexically_normal();
        if (norm.has_relative_path() && norm.filename().empty()) norm = norm.parent_path();
        const bool outside_ws = anchored || (!norm.empty() && *norm.begin() == "..");
        const fs::path name = norm.filename();
        if (outside_ws && !name.empty() && name != "." && name != ".." &&
            ws.has_parent_path()) {
            const fs::path sibling = ws.parent_path() / name;
            if (fs::exists(sibling, ec)) return sibling.u8string();
        }
    }
    return anchored ? path : resolved.u8string();
}

}  // namespace spirula
