#pragma once

// Msg -- a translated string, as a *type* rather than a lookup key.
//
// The whole design exists to make "somebody added a language, or a message,
// and forgot a translation" a compile error. A catalog entry
//
//     SS_MSG(open_dataset, EN("Open a Dataset..."), JA("..."), ...);
//
// expands to a constexpr object with one slot per language plus a
// `static_assert` that every slot is filled. There is no runtime lookup, no
// catalog file to ship, and no fallback path that can rot unnoticed.
//
// Properties worth knowing:
//   - Each tag carries its own slot index, so the ORDER of the tags does not
//     matter. Write them in whatever order reads best in the source.
//   - A duplicated tag necessarily leaves another slot empty, so it is caught
//     by the same assert rather than silently overwriting.
//   - A wrong tag COUNT is caught by its own assert with a clearer message.
//   - Every message carries `id` -- its own C++ name -- which src/app/gui/Ui.h
//     uses as the stable ImGui widget ID, so switching language does not
//     collapse open headers or reset scroll positions.
//
// Cost: all 13 languages are always linked, ~104 bytes of pointers per message
// plus the strings. At the current catalog size that is well under a megabyte
// of .rodata, which is cheaper than any resource-file scheme would be to
// build, ship and get wrong.
//
// See docs/i18n.md and src/i18n/README.md.

#include "i18n/Languages.h"

#include <atomic>
#include <initializer_list>
#include <string>
#include <vector>

namespace spirula {
namespace i18n {

enum class Lang : unsigned {
#define X(id, code, native, english) id,
    SS_LANGUAGES(X)
#undef X
};

inline constexpr unsigned kLangCount = 0
#define X(id, code, native, english) +1
    SS_LANGUAGES(X)
#undef X
    ;

// A translation tagged with the language it is for. The tag macros in
// BeginCatalog.h (EN, JA, ZH_HANS, ...) are the only intended way to build one.
template <Lang L>
struct Tr {
    const char* s;
};

struct Msg {
    const char* v[kLangCount] = {};
    const char* id = "";

    template <Lang... Ls>
    constexpr Msg(const char* id_, Tr<Ls>... t) : v{}, id(id_) {
        static_assert(sizeof...(Ls) == kLangCount,
                      "i18n: wrong number of translations -- one tag per "
                      "language in SS_LANGUAGES, no more and no fewer");
        ((v[unsigned(Ls)] = t.s), ...);   // C++17 fold over the tagged slots
    }

    // Untranslated escape hatch; see en_only() below.
    struct EnOnlyTag {};
    constexpr Msg(EnOnlyTag, const char* id_, const char* s) : v{}, id(id_) {
        for (unsigned i = 0; i < kLangCount; i++) v[i] = s;
    }

    constexpr bool complete() const {
        for (unsigned i = 0; i < kLangCount; i++)
            if (!v[i] || !*v[i]) return false;
        return true;
    }

    // The string for the language the UI is currently in.
    const char* get() const;
    const char* terminal() const;

    const char* in(Lang l) const { return v[unsigned(l)]; }
};

// The staged-rollout escape hatch: every slot gets the English string, so the
// message is usable and `complete()` holds, but the entry is greppable.
//   grep -rc SS_MSG_EN src/i18n/catalog/
// IS the remaining translation work. A catalog flips to full enforcement one
// at a time as it is translated.
constexpr Msg en_only(const char* id, const char* s) {
    return Msg(Msg::EnOnlyTag{}, id, s);
}

// ---------------------------------------------------------------------------
// Current language
// ---------------------------------------------------------------------------
// Resolved once at startup by i18n::init() (Locale.h) and changed by the GUI's
// language picker. Atomic because worker threads format status messages while
// the UI thread may be switching languages; relaxed is enough -- the strings
// are immortal .rodata, so the worst a race can do is render one frame, or one
// log line, in the previous language.
namespace detail {
extern std::atomic<Lang> g_current;
extern std::atomic<bool> g_terminal_output;
}

inline Lang current() {
    return detail::g_current.load(std::memory_order_relaxed);
}
void set_current(Lang l);

void set_terminal_output(bool enabled);
// Converted templates are cached for the lifetime of the process.
const char* ascii_copy(const char* text);

inline const char* Msg::terminal() const {
    const Lang lang = current();
    const char* text = in(lang);
    return lang == Lang::en ? ascii_copy(text) : text;
}

inline const char* Msg::get() const {
    return detail::g_terminal_output.load(std::memory_order_relaxed)
        ? terminal() : in(current());
}

// ---------------------------------------------------------------------------
// Positional substitution
// ---------------------------------------------------------------------------
// NEVER build a sentence by concatenating message fragments. Every one of
// these languages reorders clauses relative to English, and Japanese, Korean
// and Turkish are verb-final -- a sentence assembled from pieces in English
// order cannot be translated at all. Write the whole sentence as one message
// with {0} / {1} placeholders and substitute here:
//
//     i18n::format(msg::gui::frames_extracted, {count, path})
//
// `{{` is a literal `{`. An index with no argument is left in place verbatim,
// so the mistake shows up in the UI as `{2}` rather than as a crash or a
// silently truncated sentence.
//
// The other half of the rule: no plural-sensitive sentences in the catalog.
// Write "Images: 5", not "5 images" -- otherwise Russian needs a three-form
// CLDR plural rule and every message that counts anything triples.
struct Arg {
    std::string s;

    Arg(const char* v) : s(v ? v : "") {}
    Arg(const std::string& v) : s(v) {}
    Arg(std::string&& v) : s(std::move(v)) {}
    Arg(int v);
    Arg(long v);
    Arg(long long v);
    Arg(unsigned v);
    Arg(unsigned long v);
    Arg(unsigned long long v);
    Arg(double v);           // "%g" -- for anything else, format it yourself
    Arg(float v) : Arg(double(v)) {}
};

std::string format(const char* pattern, std::initializer_list<Arg> args);

inline std::string format(const Msg& m, std::initializer_list<Arg> args) {
    return format(m.get(), args);
}

// ---------------------------------------------------------------------------
// The inverse: reading a line back
// ---------------------------------------------------------------------------
// A subprocess prints translated lines and the parent has to understand them --
// the GUI's progress bar reads what `spirula mesh` writes, and both are in the
// language the user picked. Matching on English fragments is exactly the thing
// that breaks the moment the child is translated, so match on the MESSAGE
// instead: scan() takes the same Msg the child printed with, matches its
// literal parts against the line, and hands back what stood in the {0}, {1},
// ... positions.
//
//     std::vector<std::string> got;
//     if (scan(msg::mesh::cameras_rendered, rest, got))  // "12/120 cameras"
//         frac = atof(got[0].c_str()) / atof(got[1].c_str());
//
// The whole line must match, start to end. Literal parts are matched at the
// earliest position that still allows the rest to match, so a placeholder never
// swallows the separator that follows it. Two adjacent placeholders have
// nothing to split on and are rejected rather than guessed at.
bool scan(const char* pattern, const std::string& text,
          std::vector<std::string>& out);

// Terminal columns a UTF-8 string occupies (East Asian wide characters count
// as two), for the places that lay text out in a fixed column: a log tag, a
// table of labels. Not a full UAX #11 table -- the ranges a translation can
// plausibly land in.
int display_width(const char* s);
inline int display_width(const std::string& s) { return display_width(s.c_str()); }

// `s` padded on the right to `columns` terminal columns (never truncated).
std::string pad_to(const std::string& s, int columns);

// `text` broken into lines of at most `columns` terminal columns, for the help
// printers. Breaks at spaces where there are any -- and BETWEEN CHARACTERS
// where there are not, which is the only way Chinese and Japanese wrap at all:
// they are written without spaces, so a space-only wrapper leaves one line
// four hundred columns wide. A line never begins with the closing punctuation
// or the small kana that may not start one.
std::vector<std::string> wrap(const std::string& text, int columns);

inline bool scan(const Msg& m, const std::string& text,
                 std::vector<std::string>& out) {
    const char* pattern = m.get();
    if (scan(pattern, text, out)) return true;
    const char* terminal = m.terminal();
    return terminal != pattern && scan(terminal, text, out);
}

}  // namespace i18n
}  // namespace spirula

// ---------------------------------------------------------------------------
// Catalog entry points
// ---------------------------------------------------------------------------
// Both must appear between #include "i18n/BeginCatalog.h" and
// #include "i18n/EndCatalog.h", which define and then #undef the short tag
// macros so EN/JA/... never leak into ordinary code.

#define SS_MSG(name, ...)                                                     \
    inline constexpr ::spirula::i18n::Msg name{#name, __VA_ARGS__};           \
    static_assert(name.complete(),                                            \
                  "i18n: '" #name "' is missing a translation")

#define SS_MSG_EN(name, s)                                                    \
    inline constexpr ::spirula::i18n::Msg name =                              \
        ::spirula::i18n::en_only(#name, s)
