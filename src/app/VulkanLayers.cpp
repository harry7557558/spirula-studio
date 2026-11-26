// VulkanLayers.cpp -- see VulkanLayers.h.

#include "app/VulkanLayers.h"

#include "core/CrashNotes.h"
#include "core/Env.h"

#include <cstdlib>
#include <cstring>
#include <exception>

#ifdef _WIN32
#include "data/Json.h"

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#endif

namespace app {

namespace {

std::vector<std::string> g_turned_off;

std::string env_or_empty(const char* name) {
    const char* v = std::getenv(name);
    return v ? v : "";
}

// The variables that change which layers and drivers the loader picks.
// Paths are only said to be set: they name users' folders.
std::string loader_environment() {
    std::string s;
    for (const char* v : {"VK_LOADER_LAYERS_DISABLE", "VK_LOADER_LAYERS_ENABLE",
                          "VK_LOADER_LAYERS_ALLOW", "VK_INSTANCE_LAYERS", "SS_VK_DEVICE"}) {
        const std::string value = env_or_empty(v);
        if (!value.empty()) s += std::string(s.empty() ? "" : ", ") + v + "=" + value;
    }
    for (const char* v : {"VK_ICD_FILENAMES", "VK_DRIVER_FILES", "VK_ADD_DRIVER_FILES",
                          "VK_LAYER_PATH", "VK_ADD_LAYER_PATH", "VK_IMPLICIT_LAYER_PATH",
                          "VK_ADD_IMPLICIT_LAYER_PATH"}) {
        if (!env_or_empty(v).empty()) s += std::string(s.empty() ? "" : ", ") + v + " set";
    }
    return s;
}

#ifdef _WIN32

// By the name in the layer's manifest, or the file name of its DLL should a
// release rename the layer. Both ignore case, as the loader does.
struct BadLayer {
    const char* name;
    const char* library;
    const char* product;
};
constexpr BadLayer kBadLayers[] = {
    // Frees an instance without passing vkDestroyInstance down, so the
    // loader's next device call walks freed memory.
    {"VK_LAYER_PlayClaw", "playclawvk64.dll", "PlayClaw"},
    // Faults inside the NVIDIA driver while a video decodes.
    {"VK_MIRILLIS_LAYER", "MirillisActionVulkanLayer.dll", "Mirillis Action!"},
};

bool iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++) {
        char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x = (char)(x + 32);
        if (y >= 'A' && y <= 'Z') y = (char)(y + 32);
        if (x != y) return false;
    }
    return true;
}

const BadLayer* known_bad(const std::string& name, const std::string& library) {
    for (const BadLayer& b : kBadLayers)
        if (iequals(name, b.name) || iequals(library, b.library)) return &b;
    return nullptr;
}

// Whether a VK_LOADER_LAYERS_DISABLE value already covers `name`. Only whole
// names and the catch-alls; the loader's prefix and suffix globs are not read.
bool disabled_by(const std::string& filter, const std::string& name) {
    size_t at = 0;
    while (at <= filter.size()) {
        size_t end = filter.find(',', at);
        if (end == std::string::npos) end = filter.size();
        const std::string token = filter.substr(at, end - at);
        if (iequals(token, name) || token == "*" || token == "~all~" ||
            token == "~implicit~")
            return true;
        at = end + 1;
    }
    return false;
}

struct Layer {
    std::string name;
    std::string library;       // file name only
    std::string disable_var;   // the manifest's own off switch
    std::string disable_value;
};

void read_manifest(const std::wstring& path, std::vector<Layer>& out) {
    try {
        std::ifstream f(std::filesystem::path(path), std::ios::binary);
        if (!f) return;
        const std::string text((std::istreambuf_iterator<char>(f)),
                               std::istreambuf_iterator<char>());
        const JsonValue root = json_parse(text);
        const auto add = [&](const JsonValue& l) {
            const JsonValue* name = l.find("name");
            if (!name || name->as_string().empty()) return;
            Layer layer;
            layer.name = name->as_string();
            if (const JsonValue* lib = l.find("library_path")) {
                const std::string& p = lib->as_string();
                const size_t slash = p.find_last_of("/\\");
                layer.library = slash == std::string::npos ? p : p.substr(slash + 1);
            }
            if (const JsonValue* d = l.find("disable_environment");
                d && d->is_object() && !d->obj.empty()) {
                layer.disable_var = d->obj[0].first;
                layer.disable_value = d->obj[0].second.as_string();
            }
            out.push_back(std::move(layer));
        };
        if (const JsonValue* l = root.find("layer")) add(*l);
        if (const JsonValue* ls = root.find("layers"); ls && ls->is_array())
            for (const JsonValue& l : ls->arr) add(l);
    } catch (const std::exception&) {
        // A manifest the loader cannot read either; nothing to report.
    }
}

// Where the loader finds the implicit layers programs install: a value of 0
// means load. Layers a GPU driver registers under its own key are left out.
std::vector<Layer> installed_implicit_layers() {
    std::vector<Layer> out;
    for (HKEY root : {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER}) {
        HKEY key = nullptr;
        if (RegOpenKeyExW(root, L"SOFTWARE\\Khronos\\Vulkan\\ImplicitLayers", 0, KEY_READ,
                          &key) != ERROR_SUCCESS)
            continue;
        for (DWORD i = 0;; i++) {
            wchar_t path[4096];
            DWORD path_len = 4096, type = 0, value = 1, size = sizeof value;
            const LONG r = RegEnumValueW(key, i, path, &path_len, nullptr, &type,
                                         reinterpret_cast<BYTE*>(&value), &size);
            if (r == ERROR_NO_MORE_ITEMS) break;
            if (r == ERROR_SUCCESS && type == REG_DWORD && value == 0)
                read_manifest(std::wstring(path, path_len), out);
        }
        RegCloseKey(key);
    }
    return out;
}

// vulkan-1.dll's file version, from the copy this process uses.
bool loader_version(unsigned v[4]) {
    HMODULE m = GetModuleHandleW(L"vulkan-1.dll");
    const bool borrowed = !m;
    if (borrowed)
        m = LoadLibraryExW(L"vulkan-1.dll", nullptr,
                           LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!m) return false;
    bool found = false;
    // VS_VERSION_INFO (1) of type RT_VERSION (16).
    if (HRSRC res = FindResourceW(m, MAKEINTRESOURCEW(1), MAKEINTRESOURCEW(16))) {
        const BYTE* p = static_cast<const BYTE*>(LockResource(LoadResource(m, res)));
        const DWORD n = SizeofResource(m, res);
        // VS_FIXEDFILEINFO sits at a DWORD boundary after its signature.
        for (DWORD o = 0; p && !found && o + 16 <= n; o += 4) {
            DWORD w[4];
            std::memcpy(w, p + o, sizeof w);
            if (w[0] != 0xFEEF04BD) continue;
            v[0] = w[2] >> 16, v[1] = w[2] & 0xFFFF, v[2] = w[3] >> 16, v[3] = w[3] & 0xFFFF;
            found = true;
        }
    }
    if (borrowed) FreeLibrary(m);
    return found;
}

#endif

}  // namespace

std::vector<std::string> prepare_vulkan_layers() {
    g_turned_off.clear();
    std::string loader;
#ifdef _WIN32
    try {
        unsigned ver[4] = {};
        const bool have_ver = loader_version(ver);
        if (have_ver)
            loader = "loader " + std::to_string(ver[0]) + "." + std::to_string(ver[1]) + "." +
                     std::to_string(ver[2]);
        // The loader reads VK_LOADER_LAYERS_DISABLE from 1.3.234; older ones
        // only honour each layer's own disable_environment switch.
        const bool filter_ok =
            !have_ver || ver[0] > 1 || (ver[0] == 1 && (ver[1] > 3 || (ver[1] == 3 && ver[2] >= 234)));
        const bool keep = spirula::env_on("VK_KEEP_LAYERS");

        std::string filter = env_or_empty("VK_LOADER_LAYERS_DISABLE");
        const std::string before = filter;
        std::string listed;
        for (const Layer& l : installed_implicit_layers()) {
            bool off = disabled_by(filter, l.name) ||
                       (!l.disable_var.empty() && !env_or_empty(l.disable_var.c_str()).empty());
            const BadLayer* bad = known_bad(l.name, l.library);
            if (bad && !keep && !off) {
                if (filter_ok) {
                    filter += std::string(filter.empty() ? "" : ",") + l.name;
                } else if (!l.disable_var.empty()) {
                    _putenv_s(l.disable_var.c_str(),
                              l.disable_value.empty() ? "1" : l.disable_value.c_str());
                }
                off = filter_ok || !l.disable_var.empty();
                if (off) g_turned_off.push_back(bad->product);
            }
            listed += std::string(listed.empty() ? "" : ", ") + l.name;
            if (!l.library.empty()) listed += " [" + l.library + "]";
            if (off) listed += " (off)";
        }
        // _putenv_s reaches the process block the loader and children read.
        if (filter != before) _putenv_s("VK_LOADER_LAYERS_DISABLE", filter.c_str());
        spirula::set_crash_note_line("vk layers", listed.empty() ? "none" : listed);
    } catch (const std::exception&) {
        // Reading the registry is a courtesy; Vulkan still works without it.
    }
#endif
    const std::string env = loader_environment();
    if (!env.empty()) loader += std::string(loader.empty() ? "" : ", ") + env;
    if (!loader.empty()) spirula::set_crash_note_line("vulkan", loader);
    return g_turned_off;
}

const std::vector<std::string>& vulkan_layers_turned_off() { return g_turned_off; }

}  // namespace app
