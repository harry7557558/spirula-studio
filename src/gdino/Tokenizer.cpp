#include "gdino/Tokenizer.h"

#include "nn/core/Error.h"

#include <fstream>

namespace gdino {
namespace {

// One UTF-8 code point from `s` at `i`, advancing `i`; invalid bytes read as
// U+FFFD so a stray byte cannot stall the loop.
uint32_t next_cp(const std::string& s, size_t& i) {
    const unsigned char c = (unsigned char)s[i];
    int n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
    if (n == 0 || i + n > s.size()) { ++i; return 0xFFFD; }
    uint32_t cp = n == 1 ? c : n == 2 ? (c & 0x1F) : n == 3 ? (c & 0x0F) : (c & 0x07);
    for (int k = 1; k < n; ++k) cp = (cp << 6) | ((unsigned char)s[i + k] & 0x3F);
    i += n;
    return cp;
}

void put_cp(std::string& out, uint32_t cp) {
    if (cp < 0x80) {
        out += (char)cp;
    } else if (cp < 0x800) {
        out += (char)(0xC0 | (cp >> 6));
        out += (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += (char)(0xE0 | (cp >> 12));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    } else {
        out += (char)(0xF0 | (cp >> 18));
        out += (char)(0x80 | ((cp >> 12) & 0x3F));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    }
}

bool is_space(uint32_t cp) {
    return cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' || cp == 0xA0 || cp == 0x3000 ||
           (cp >= 0x2000 && cp <= 0x200A);
}

bool is_control(uint32_t cp) {
    return (cp < 0x20 && cp != '\t' && cp != '\n' && cp != '\r') || (cp >= 0x7F && cp < 0xA0);
}

// BERT's _is_punctuation: every non-alphanumeric ASCII symbol, plus the
// Unicode punctuation blocks a prompt could plausibly contain.
bool is_punct(uint32_t cp) {
    if ((cp >= 33 && cp <= 47) || (cp >= 58 && cp <= 64) || (cp >= 91 && cp <= 96) ||
        (cp >= 123 && cp <= 126))
        return true;
    return (cp >= 0x2010 && cp <= 0x2027) || (cp >= 0x3001 && cp <= 0x3003) ||
           (cp >= 0x300C && cp <= 0x3011) || (cp >= 0xFF01 && cp <= 0xFF0F) ||
           cp == 0xA1 || cp == 0xBF || cp == 0xAB || cp == 0xBB;
}

bool is_cjk(uint32_t cp) {
    return (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0x3400 && cp <= 0x4DBF) ||
           (cp >= 0x20000 && cp <= 0x2A6DF) || (cp >= 0xF900 && cp <= 0xFAFF) ||
           (cp >= 0x2F800 && cp <= 0x2FA1F);
}

// Lower-case, and strip the accent of a Latin-1 / Latin Extended-A letter --
// NFD followed by dropping Mn, which is what BERT does, tabulated for the range
// an English prompt can reach. '*' has no ASCII base and is only lower-cased.
uint32_t fold(uint32_t cp) {
    if (cp >= 'A' && cp <= 'Z') return cp + 32;
    if (cp < 0xC0 || cp >= 0x180) return cp;
    static const char kBase[] =
        "aaaaaa*ceeeeiiii*nooooo**uuuuy**aaaaaa*ceeeeiiii*nooooo**uuuuy*y"
        "aaaaaaccccccccdd**eeeeeeeeeegggggggghh**iiiiiiiii***jjkk*llllll****nnnnnn***oooooo"
        "**rrrrrrsssssssstttt**uuuuuuuuuuuuwwyyyzzzzzz*";
    static_assert(sizeof(kBase) == 0x180 - 0xC0 + 1, "one entry per code point");
    const char c = kBase[cp - 0xC0];
    if (c != '*') return (uint32_t)c;
    if (cp <= 0xDE && cp != 0xD7) return cp + 0x20;
    if (cp >= 0x100 && (cp & 1) == 0) return cp + 1;
    return cp;
}

}  // namespace

void Tokenizer::load(const std::string& vocab_path) {
    std::ifstream f(vocab_path, std::ios::binary);
    NN_CHECK(f, "cannot open the tokenizer vocabulary '%s'", vocab_path.c_str());
    vocab_.clear();
    by_id_.clear();
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        vocab_.emplace(line, (int32_t)by_id_.size());
        by_id_.push_back(line);
    }
    NN_CHECK(by_id_.size() > (size_t)kQuestion && by_id_[kCls] == "[CLS]" &&
                 by_id_[kSep] == "[SEP]" && by_id_[kPeriod] == ".",
             "'%s' is not bert-base-uncased's vocabulary", vocab_path.c_str());
    unk_ = vocab_.count("[UNK]") ? vocab_.at("[UNK]") : 100;
}

const std::string& Tokenizer::token(int32_t id) const {
    static const std::string kNone;
    return id >= 0 && (size_t)id < by_id_.size() ? by_id_[(size_t)id] : kNone;
}

void Tokenizer::wordpiece(const std::string& word, std::vector<int32_t>& out) const {
    if (word.size() > 100) {
        out.push_back(unk_);
        return;
    }
    std::vector<int32_t> pieces;
    size_t start = 0;
    while (start < word.size()) {
        size_t end = word.size();
        int32_t found = -1;
        while (start < end) {
            std::string sub = word.substr(start, end - start);
            if (start > 0) sub = "##" + sub;
            auto it = vocab_.find(sub);
            if (it != vocab_.end()) {
                found = it->second;
                break;
            }
            // Back off one whole code point, never into the middle of one.
            do { --end; } while (end > start && ((unsigned char)word[end] & 0xC0) == 0x80);
        }
        if (found < 0) {
            out.push_back(unk_);
            return;
        }
        pieces.push_back(found);
        start = end;
    }
    out.insert(out.end(), pieces.begin(), pieces.end());
}

std::vector<int32_t> Tokenizer::encode(const std::string& text) const {
    NN_CHECK(loaded(), "gdino: tokenizer used before load");
    // Basic tokenization: clean, split CJK and punctuation into their own
    // words, lower-case and strip accents.
    std::vector<std::string> words(1);
    auto split = [&] { if (!words.back().empty()) words.emplace_back(); };
    for (size_t i = 0; i < text.size();) {
        const uint32_t cp = next_cp(text, i);
        if (cp == 0 || cp == 0xFFFD || is_control(cp)) continue;
        if (is_space(cp)) { split(); continue; }
        if (is_cjk(cp) || is_punct(cp)) {
            split();
            put_cp(words.back(), fold(cp));
            split();
            continue;
        }
        put_cp(words.back(), fold(cp));
    }
    std::vector<int32_t> ids{kCls};
    for (const std::string& w : words)
        if (!w.empty()) wordpiece(w, ids);
    ids.push_back(kSep);
    return ids;
}

}  // namespace gdino
