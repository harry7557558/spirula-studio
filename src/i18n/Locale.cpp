// Locale.cpp -- see Locale.h.

#include "i18n/Locale.h"

#include "core/Env.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#elif defined(__APPLE__)
#  include <CoreFoundation/CoreFoundation.h>
#endif

namespace spirula {
namespace i18n {

namespace detail {
std::atomic<Lang> g_current{kDefaultLang};
std::atomic<bool> g_terminal_output{false};
}

void set_current(Lang l) {
    detail::g_current.store(l, std::memory_order_relaxed);
}

void set_terminal_output(bool enabled) {
    detail::g_terminal_output.store(enabled, std::memory_order_relaxed);
}

const char* code(Lang l) {
    switch (l) {
#define X(id, c, native, english) case Lang::id: return c;
        SS_LANGUAGES(X)
#undef X
    }
    return "en";
}

const char* native_name(Lang l) {
    switch (l) {
#define X(id, c, native, english) case Lang::id: return native;
        SS_LANGUAGES(X)
#undef X
    }
    return "English";
}

const char* english_name(Lang l) {
    switch (l) {
#define X(id, c, native, english) case Lang::id: return english;
        SS_LANGUAGES(X)
#undef X
    }
    return "English";
}

bool needs_cjk_font(Lang l) {
    switch (l) {
#define X(id) case Lang::id: return true;
        SS_LANGUAGES_CJK(X)
#undef X
        default: return false;
    }
}

namespace {

char lower(char c) {
    return (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c;
}

// "zh_Hans_CN.UTF-8@modifier" -> {"zh", "hans", "cn"}, lowercased, at most
// three subtags (nothing we match on needs a fourth).
struct Subtags {
    std::string t[3];
    int n = 0;
};

Subtags split_locale(const char* s) {
    Subtags out;
    std::string cur;
    for (const char* p = s; *p; p++) {
        // The encoding and the modifier say nothing about the language.
        if (*p == '.' || *p == '@') break;
        if (*p == '-' || *p == '_') {
            if (!cur.empty() && out.n < 3) out.t[out.n++] = cur;
            cur.clear();
            continue;
        }
        cur += lower(*p);
    }
    if (!cur.empty() && out.n < 3) out.t[out.n++] = cur;
    return out;
}

// zh needs a script, and an OS will express it as a script subtag, a region,
// or -- on older Windows -- as the "CHS"/"CHT" pseudo-scripts.
bool resolve_chinese(const Subtags& st, Lang* out) {
    for (int i = 1; i < st.n; i++) {
        const std::string& t = st.t[i];
        if (t == "hans" || t == "chs") { *out = Lang::zh_hans; return true; }
        if (t == "hant" || t == "cht") { *out = Lang::zh_hant; return true; }
        // Region. Macau and Hong Kong are traditional; Singapore and Malaysia
        // follow the mainland.
        if (t == "cn" || t == "sg" || t == "my") { *out = Lang::zh_hans; return true; }
        if (t == "tw" || t == "hk" || t == "mo") { *out = Lang::zh_hant; return true; }
    }
    // A bare "zh" with no script and no region. A Traditional build should
    // stay Traditional; everything else takes the more common script.
    *out = (kDefaultLang == Lang::zh_hant) ? Lang::zh_hant : Lang::zh_hans;
    return true;
}

}  // namespace

bool parse_lang(const char* s, Lang* out) {
    if (!s || !*s) return false;

    const Subtags st = split_locale(s);
    if (st.n == 0) return false;

    // "C" and "POSIX" mean "the user expressed no preference", which is not
    // the same as "the user wants English" -- fall through to the next step of
    // the chain rather than pinning en here.
    if (st.t[0] == "c" || st.t[0] == "posix") return false;

    if (st.t[0] == "zh") return resolve_chinese(st, out);

    // Our own canonical codes, and the identifier spelling ("zh_hans") that
    // SS_DEFAULT_LANG and the settings file use.
    std::string joined = st.t[0];
    for (int i = 1; i < st.n; i++) joined += "-" + st.t[i];

#define X(id, c, native, english)                                             \
    {                                                                         \
        std::string canon(c);                                                 \
        for (auto& ch : canon) ch = lower(ch);                                \
        if (joined == canon || st.t[0] == canon) { *out = Lang::id; return true; } \
    }
    SS_LANGUAGES(X)
#undef X

    return false;
}

Lang detect_os_lang() {
    Lang l = kDefaultLang;

#if defined(_WIN32)
    // The user's UI language, e.g. L"zh-Hans-CN". Present since Vista.
    wchar_t buf[LOCALE_NAME_MAX_LENGTH] = {0};
    if (GetUserDefaultLocaleName(buf, LOCALE_NAME_MAX_LENGTH) > 0) {
        // Locale names are ASCII; a narrowing copy is exact.
        char narrow[LOCALE_NAME_MAX_LENGTH] = {0};
        size_t n = 0;
        for (; n + 1 < sizeof narrow && buf[n]; n++)
            narrow[n] = (buf[n] < 128) ? char(buf[n]) : '?';
        narrow[n] = '\0';
        if (parse_lang(narrow, &l)) return l;
    }
#elif defined(__APPLE__)
    // POSIX env vars are usually absent for an app launched from Finder, so
    // ask CoreFoundation. (CFLocale is C; no Objective-C is involved.)
    if (CFLocaleRef loc = CFLocaleCopyCurrent()) {
        char buf[128] = {0};
        const bool ok = CFStringGetCString(CFLocaleGetIdentifier(loc), buf,
                                           sizeof buf, kCFStringEncodingUTF8);
        CFRelease(loc);
        if (ok && parse_lang(buf, &l)) return l;
    }
#endif

    // POSIX, and the fallback everywhere: LC_ALL beats LC_MESSAGES beats LANG.
    for (const char* var : {"LC_ALL", "LC_MESSAGES", "LANG"})
        if (const char* v = std::getenv(var))
            if (parse_lang(v, &l)) return l;

    return kDefaultLang;
}

Lang init(const char* cli, const char* saved) {
    Lang l = kDefaultLang;

    auto try_set = [&](const char* s, const char* origin) {
        if (!s || !*s) return false;
        if (parse_lang(s, &l)) return true;
        std::fprintf(stderr, "warning: unknown language '%s' (%s); using %s\n"
                             "%s",
                     s, origin, code(kDefaultLang), language_list().c_str());
        return false;
    };

    if (try_set(cli, "--lang")) { set_current(l); return l; }
    if (try_set(env("LANG"), "SS_LANG")) { set_current(l); return l; }
    if (try_set(saved, "settings")) { set_current(l); return l; }

    l = detect_os_lang();
    set_current(l);
    return l;
}

namespace {
const char* g_lang_arg = nullptr;
}

const char* lang_arg() { return g_lang_arg; }

const char* take_lang_arg(int* argc, char** argv) {
    const char* value = nullptr;
    int out = 1;
    for (int i = 1; i < *argc; i++) {
        const char* a = argv[i];
        if (std::strcmp(a, "--lang") == 0 && i + 1 < *argc) {
            value = argv[++i];
            continue;
        }
        if (std::strncmp(a, "--lang=", 7) == 0) {
            value = a + 7;
            continue;
        }
        argv[out++] = argv[i];
    }
    argv[out] = nullptr;
    *argc = out;
    g_lang_arg = value;
    return value;
}

std::string language_list() {
    std::string s;
#define X(id, c, native, english)                                             \
    {                                                                         \
        std::string row = std::string("    ") + c;                            \
        row.resize(14, ' ');                                                  \
        s += row; s += english; s += "\n";                                    \
    }
    SS_LANGUAGES(X)
#undef X
    return s;
}

// ---------------------------------------------------------------------------
// Positional substitution -- see the rule in Message.h.
// ---------------------------------------------------------------------------

Arg::Arg(int v)                : s(std::to_string(v)) {}
Arg::Arg(long v)               : s(std::to_string(v)) {}
Arg::Arg(long long v)          : s(std::to_string(v)) {}
Arg::Arg(unsigned v)           : s(std::to_string(v)) {}
Arg::Arg(unsigned long v)      : s(std::to_string(v)) {}
Arg::Arg(unsigned long long v) : s(std::to_string(v)) {}

Arg::Arg(double v) {
    // std::to_string(double) is always %f, so 0.5 becomes "0.500000" and 1e-7
    // becomes "0.000000". %g is what a UI wants.
    char buf[32];
    std::snprintf(buf, sizeof buf, "%g", v);
    s = buf;
}

std::string format(const char* pattern, std::initializer_list<Arg> args) {
    std::string out;
    if (!pattern) return out;
    for (const char* p = pattern; *p;) {
        if (p[0] == '{' && p[1] == '{') { out += '{'; p += 2; continue; }
        if (p[0] == '}' && p[1] == '}') { out += '}'; p += 2; continue; }
        if (*p != '{') { out += *p++; continue; }

        const char* q = p + 1;
        size_t idx = 0;
        bool digits = false;
        while (*q >= '0' && *q <= '9') { idx = idx * 10 + size_t(*q - '0'); q++; digits = true; }
        if (!digits || *q != '}' || idx >= args.size()) {
            // Not a placeholder, or one with no argument. Copy it through so
            // the mistake is visible as `{2}` in the UI instead of vanishing.
            out += *p++;
            continue;
        }
        out += (args.begin() + idx)->s;
        p = q + 1;
    }
    return out;
}

namespace {

// East Asian Wide / Fullwidth, in the ranges a translation can plausibly land
// in. Not a full UAX #11 table.
bool wide_cp(unsigned int cp) {
    return (cp >= 0x1100 && cp <= 0x115F) ||    // hangul jamo
           (cp >= 0x2E80 && cp <= 0x303E) ||    // CJK radicals, punctuation
           (cp >= 0x3041 && cp <= 0x33FF) ||    // kana, hangul compat, CJK compat
           (cp >= 0x3400 && cp <= 0x4DBF) ||    // CJK ext A
           (cp >= 0x4E00 && cp <= 0x9FFF) ||    // CJK unified
           (cp >= 0xA000 && cp <= 0xA4CF) ||    // yi
           (cp >= 0xAC00 && cp <= 0xD7A3) ||    // hangul syllables
           (cp >= 0xF900 && cp <= 0xFAFF) ||    // CJK compat ideographs
           (cp >= 0xFE30 && cp <= 0xFE6F) ||    // CJK compat forms
           (cp >= 0xFF00 && cp <= 0xFF60) ||    // fullwidth forms
           (cp >= 0xFFE0 && cp <= 0xFFE6) ||
           (cp >= 0x20000 && cp <= 0x3FFFD);    // CJK ext B+
}

}  // namespace

int display_width(const char* s) {
    // UTF-8 decode, counting East Asian Wide / Fullwidth as two columns.
    int w = 0;
    if (!s) return 0;
    for (const unsigned char* p = (const unsigned char*)s; *p;) {
        unsigned int cp = *p;
        int len = 1;
        if (cp >= 0xF0)      { cp &= 0x07; len = 4; }
        else if (cp >= 0xE0) { cp &= 0x0F; len = 3; }
        else if (cp >= 0xC0) { cp &= 0x1F; len = 2; }
        for (int i = 1; i < len; i++) {
            if ((p[i] & 0xC0) != 0x80) { len = i; break; }
            cp = (cp << 6) | (p[i] & 0x3F);
        }
        p += len;
        w += wide_cp(cp) ? 2 : 1;
    }
    return w;
}

namespace {

// One UTF-8 character: its codepoint, its byte length, and how many terminal
// columns it takes.
struct Ch { unsigned int cp; int bytes; int width; };

Ch decode_one(const unsigned char* p) {
    Ch c{*p, 1, 1};
    if (c.cp >= 0xF0)      { c.cp &= 0x07; c.bytes = 4; }
    else if (c.cp >= 0xE0) { c.cp &= 0x0F; c.bytes = 3; }
    else if (c.cp >= 0xC0) { c.cp &= 0x1F; c.bytes = 2; }
    for (int i = 1; i < c.bytes; i++) {
        if ((p[i] & 0xC0) != 0x80) { c.bytes = i; break; }
        c.cp = (c.cp << 6) | (p[i] & 0x3F);
    }
    c.width = wide_cp(c.cp) ? 2 : 1;
    return c;
}

// A character a line may be broken BEFORE. True for wide scripts, which are
// written without spaces; false for the punctuation that may not open a line.
bool can_break_before(unsigned int cp) {
    if (!wide_cp(cp)) return false;
    switch (cp) {
        case 0x3001: case 0x3002:                        // 、。
        case 0xFF0C: case 0xFF0E: case 0xFF1A: case 0xFF1B:  // ，．：；
        case 0xFF01: case 0xFF1F:                        // ！？
        case 0x3009: case 0x300B: case 0x300D: case 0x300F:  // 〉》」』
        case 0x3011: case 0x3015: case 0xFF09: case 0xFF3D:  // 】〕）］
        case 0x30FC:                                     // ー
        case 0x3041: case 0x3043: case 0x3045: case 0x3047: case 0x3049:
        case 0x3063: case 0x3083: case 0x3085: case 0x3087:
        case 0x30A1: case 0x30A3: case 0x30A5: case 0x30A7: case 0x30A9:
        case 0x30C3: case 0x30E3: case 0x30E5: case 0x30E7:
            return false;
        default:
            return true;
    }
}

}  // namespace

const char* ascii_copy(const char* text) {
    if (!text) return text;
    const auto* p = reinterpret_cast<const unsigned char*>(text);
    while (*p && *p < 128) ++p;
    if (!*p) return text;

    static std::mutex mutex;
    // Msg pointers can outlive insertions into this cache.
    static std::unordered_map<std::string, std::string> copies;
    std::lock_guard<std::mutex> lock(mutex);
    auto [it, inserted] = copies.try_emplace(text);
    if (inserted) {
        std::string& out = it->second;
        out.assign(text, reinterpret_cast<const char*>(p));
        while (*p) {
            const Ch c = decode_one(p);
            if (c.cp < 128 && c.bytes == 1) {
                out.push_back(static_cast<char>(c.cp));
            } else {
                switch (c.cp) {
#define SS_ASCII_PUNCTUATION(cp, replacement) case cp: out += replacement; break;
#include "i18n/AsciiPunctuation.inc"
#undef SS_ASCII_PUNCTUATION
                    default: out.push_back('?'); break;
                }
            }
            p += c.bytes;
        }
    }
    return it->second.c_str();
}

std::vector<std::string> wrap(const std::string& text, int columns) {
    std::vector<std::string> out;
    if (columns < 2) columns = 2;
    const unsigned char* p = (const unsigned char*)text.c_str();

    std::string line;          // what is committed to the current line
    int line_w = 0;
    std::string word;          // the run since the last break opportunity
    int word_w = 0;

    // The space a break was taken at belongs to neither line.
    auto rtrim = [](std::string s) {
        while (!s.empty() && s.back() == ' ') s.pop_back();
        return s;
    };

    auto commit = [&]() {      // word -> line, breaking the line first if need be
        if (word.empty()) return;
        if (line_w + word_w > columns && !line.empty()) {
            out.push_back(rtrim(line));
            line.clear();
            line_w = 0;
            // A space that fell at a break is consumed by the break.
            while (!word.empty() && word[0] == ' ') { word.erase(0, 1); word_w--; }
        }
        line += word;
        line_w += word_w;
        word.clear();
        word_w = 0;
    };

    while (*p) {
        if (*p == '\n') {
            commit();
            out.push_back(rtrim(line));
            line.clear();
            line_w = 0;
            p++;
            continue;
        }
        const Ch c = decode_one(p);
        // A space ends the previous word; a wide character both ends the
        // previous one and may itself start a line.
        if (c.cp == ' ' || can_break_before(c.cp)) commit();
        word.append((const char*)p, (size_t)c.bytes);
        word_w += c.width;
        p += c.bytes;
        if (c.cp == ' ') commit();
    }
    commit();
    line = rtrim(line);
    if (!line.empty() || out.empty()) out.push_back(line);
    return out;
}

std::string pad_to(const std::string& s, int columns) {
    std::string out = s;
    out.append((size_t)std::max(0, columns - display_width(s)), ' ');
    return out;
}

// The inverse -- see the note in Message.h. Two passes: take the pattern apart
// into literal runs and placeholder indices, then walk the text matching the
// runs in order and calling whatever lies between them the argument.
bool scan(const char* pattern, const std::string& text,
          std::vector<std::string>& out) {
    out.clear();
    if (!pattern) return false;

    std::vector<std::string> lits;   // lits[i] precedes slot[i]; one extra tail
    std::vector<size_t> slot;        // placeholder index of each gap
    std::string cur;
    for (const char* p = pattern; *p;) {
        if (p[0] == '{' && p[1] == '{') { cur += '{'; p += 2; continue; }
        if (p[0] == '}' && p[1] == '}') { cur += '}'; p += 2; continue; }
        if (*p != '{') { cur += *p++; continue; }
        const char* q = p + 1;
        size_t idx = 0;
        bool digits = false;
        while (*q >= '0' && *q <= '9') { idx = idx * 10 + size_t(*q - '0'); q++; digits = true; }
        if (!digits || *q != '}') { cur += *p++; continue; }
        // A placeholder with nothing before it but another placeholder has no
        // separator to find, so the split would be a guess. Say no instead.
        if (!lits.empty() && cur.empty()) return false;
        lits.push_back(cur);
        slot.push_back(idx);
        cur.clear();
        p = q + 1;
    }
    lits.push_back(cur);              // the tail after the last placeholder

    if (text.compare(0, lits[0].size(), lits[0]) != 0) return false;
    size_t pos = lits[0].size();
    std::vector<std::string> got(slot.size());
    for (size_t i = 0; i < slot.size(); i++) {
        const std::string& next = lits[i + 1];
        size_t at;
        if (next.empty()) {
            // Last placeholder, pattern ends with it: it takes the remainder.
            if (i + 1 != slot.size()) return false;
            at = text.size();
        } else {
            at = text.find(next, pos);
            if (at == std::string::npos) return false;
        }
        got[i] = text.substr(pos, at - pos);
        pos = at + next.size();
    }
    if (pos != text.size()) return false;

    // Placeholders may appear in any order in a translation, so index by slot.
    size_t n = 0;
    for (size_t s : slot) n = std::max(n, s + 1);
    out.assign(n, std::string());
    for (size_t i = 0; i < slot.size(); i++) out[slot[i]] = std::move(got[i]);
    return true;
}

}  // namespace i18n
}  // namespace spirula
