// DesktopEntry.cpp -- see DesktopEntry.h.

#include "app/gui/DesktopEntry.h"

#if !defined(_WIN32) && !defined(__APPLE__)
#include "app/AppPaths.h"
#include "app/Tools.h"
#include "app_generated/app_icon.h"
#include "i18n/catalog/Brand.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#endif

namespace gui {

#if !defined(_WIN32) && !defined(__APPLE__)

namespace fs = std::filesystem;

namespace {

// An entry without it was written by hand or by a package, and is not ours.
constexpr const char* kGeneratedKey = "X-Spirula-Generated=true";

fs::path data_home() {
    const char* xdg = std::getenv("XDG_DATA_HOME");
    if (xdg && *xdg) return fs::path(xdg);
    const char* home = std::getenv("HOME");
    return home && *home ? fs::path(home) / ".local" / "share" : fs::path();
}

std::string read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), {});
}

// Through a rename, so the shell watching the directory never reads half a file.
void write_if_changed(const fs::path& path, const std::string& bytes) {
    if (read_file(path) == bytes) return;
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    fs::path tmp = path;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out.write(bytes.data(), (std::streamsize)bytes.size());
        if (!out) {
            out.close();
            fs::remove(tmp, ec);
            return;
        }
    }
    fs::rename(tmp, path, ec);
    if (ec) fs::remove(tmp, ec);
}

std::string entry_string(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            case '\r': out += "\\r"; break;
            default: out += c;
        }
    }
    return out;
}

// One quoted Exec argument, before entry_string(): the spec reserves the
// characters " ` $ \ inside quotes, and a literal % is %% anywhere.
std::string exec_arg(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '`' || c == '$' || c == '\\') out += '\\';
        else if (c == '%') out += '%';
        out += c;
    }
    return out + '"';
}

}  // namespace

void register_desktop_entry() {
    const std::string exe = app::exe_path();
    const fs::path base = data_home();
    if (exe.empty() || base.empty()) return;

    const std::string id = kDesktopAppId;
    const fs::path entry = base / "applications" / (id + ".desktop");
    const std::string current = read_file(entry);
    if (!current.empty() && current.find(kGeneratedKey) == std::string::npos)
        return;

    const fs::path icon = base / "icons" / "hicolor" / "128x128" / "apps" / (id + ".png");
    write_if_changed(icon, std::string((const char*)kAppIcon, kAppIconSize));

    using spirula::i18n::Lang;
    namespace brand = spirula::i18n::msg::brand;
    std::string text = "[Desktop Entry]\nType=Application\n";
    text += "Name=" + entry_string(brand::product.in(Lang::en)) + "\n";
    text += "Comment=" + entry_string(brand::tagline.in(Lang::en)) + "\n";
    // By path: a theme name waits on the shell rescanning its icon theme, and
    // the window maps moments after this runs.
    text += "Icon=" + entry_string(icon.string()) + "\n";
    // Hides the entry once the binary it points at is deleted.
    text += "TryExec=" + entry_string(exe) + "\n";
    text += "Exec=" + entry_string(exec_arg(exe) + " " + app::kToolGui + " %F") + "\n";
    text += "Categories=Graphics;3DGraphics;\n";
    text += std::string(kGeneratedKey) + "\n";
    write_if_changed(entry, text);
}

#else

void register_desktop_entry() {}

#endif

}  // namespace gui
