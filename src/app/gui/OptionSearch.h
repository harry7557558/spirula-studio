#pragma once

#include "i18n/Message.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace gui::option_search {

inline std::string lower_ascii(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return s;
}

inline void add_text(std::string& out, const spirula::i18n::Msg& msg) {
    out += ' ';
    out += msg.get();
    const char* en = msg.in(spirula::i18n::Lang::en);
    if (en != msg.get()) {
        out += ' ';
        out += en;
    }
}

inline std::vector<std::string> query_tokens(const char* query) {
    std::vector<std::string> tokens;
    std::string token;
    for (const char* c = query;; ++c) {
        if (*c && !std::strchr(" \t_-+/,.", *c)) {
            token += *c;
            continue;
        }
        if (!token.empty()) {
            tokens.push_back(lower_ascii(token));
            token.clear();
        }
        if (!*c) break;
    }
    return tokens;
}

inline bool matches(const std::string& lowercase_text, const std::vector<std::string>& tokens) {
    return std::all_of(tokens.begin(), tokens.end(), [&](const std::string& token) {
        return lowercase_text.find(token) != std::string::npos;
    });
}

}  // namespace gui::option_search
