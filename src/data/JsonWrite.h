#pragma once

// JsonWrite.h -- the writing half of Json.h.
//
// Pretty-printed output with the commas, the indent and the string escape
// handled, so the several settings files this program writes do not each
// carry their own copy of them.

#include "data/Json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

// A JSON string literal, quotes included.
inline std::string json_quote(const std::string& v) {
    std::string s = "\"";
    for (char ch : v) {
        switch (ch) {
            case '"':  s += "\\\""; break;
            case '\\': s += "\\\\"; break;
            case '\n': s += "\\n"; break;
            case '\r': s += "\\r"; break;
            case '\t': s += "\\t"; break;
            default:
                // The C0 controls have no literal spelling in JSON.
                if ((unsigned char)ch < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", (unsigned)ch);
                    s += buf;
                } else {
                    s += ch;
                }
        }
    }
    return s + "\"";
}

// A number in the Python-json spelling this project's readers accept: Json.h
// parses Infinity and NaN, which no plain JSON writer emits.
inline std::string json_number(double v) {
    if (std::isnan(v)) return "NaN";
    if (std::isinf(v)) return v > 0 ? "Infinity" : "-Infinity";
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.9g", v);
    return buf;
}

// The shortest spelling that reads back as the same double: a document that
// was parsed and is written again must not come back rounded.
inline std::string json_number_exact(double v) {
    if (std::isnan(v) || std::isinf(v)) return json_number(v);
    char buf[40];
    for (int digits = 15; digits <= 17; digits++) {
        std::snprintf(buf, sizeof buf, "%.*g", digits, v);
        if (std::strtod(buf, nullptr) == v) break;
    }
    return buf;
}

class JsonWriter {
public:
    explicit JsonWriter(bool pretty = true) : _pretty(pretty) {}
    JsonWriter& object() { return open('{', '}'); }
    JsonWriter& array()  { return open('[', ']'); }
    JsonWriter& end() {
        const Frame f = _stack.back();
        _stack.pop_back();
        if (_pretty && f.count) { _out += '\n'; indent(); }
        _out += f.closer;
        return *this;
    }

    JsonWriter& key(const char* k) {
        separate();
        _out += json_quote(k);
        _out += _pretty ? ": " : ":";
        _pending_key = true;
        return *this;
    }

    JsonWriter& value(const std::string& v) { return raw(json_quote(v)); }
    JsonWriter& value(const char* v)        { return raw(json_quote(v)); }
    JsonWriter& value(bool v)               { return raw(v ? "true" : "false"); }
    JsonWriter& value(int v)       { return raw(std::to_string(v)); }
    JsonWriter& value(long long v) { return raw(std::to_string(v)); }
    JsonWriter& value(double v)    { return raw(json_number(v)); }
    JsonWriter& value(float v)     { return raw(json_number(v)); }

    // An already-encoded fragment, for a caller with its own emitter.
    JsonWriter& raw(const std::string& encoded) {
        if (!_pending_key) separate();
        _pending_key = false;
        _out += encoded;
        return *this;
    }

    template <typename T>
    JsonWriter& field(const char* k, const T& v) { return key(k).value(v); }
    JsonWriter& field_raw(const char* k, const std::string& encoded) {
        return key(k).raw(encoded);
    }

    // Pretty-printed files end with a newline; compact records have no delimiter.
    std::string str() const { return _pretty ? _out + "\n" : _out; }

private:
    struct Frame { char closer; int count = 0; };

    JsonWriter& open(char brace, char closer) {
        if (!_pending_key) separate();
        _pending_key = false;
        _out += brace;
        _stack.push_back({closer, 0});
        return *this;
    }
    void indent() { _out.append(4 * _stack.size(), ' '); }
    void separate() {
        if (_stack.empty()) return;   // the document's own root value
        if (_stack.back().count++) _out += ',';
        if (_pretty) {
            _out += '\n';
            indent();
        }
    }

    std::string _out;
    std::vector<Frame> _stack;
    bool _pending_key = false;
    bool _pretty;
};


// A parsed document back out, for the one case that reads a file and writes
// the same shape again: an edited Metashape export saved as Nerfstudio.
inline void json_write(JsonWriter& w, const JsonValue& v) {
    switch (v.type) {
        case JsonValue::Type::Object:
            w.object();
            for (const auto& [k, sub] : v.obj) {
                w.key(k.c_str());
                json_write(w, sub);
            }
            w.end();
            break;
        case JsonValue::Type::Array:
            w.array();
            for (const auto& sub : v.arr) json_write(w, sub);
            w.end();
            break;
        case JsonValue::Type::String: w.value(v.str); break;
        case JsonValue::Type::Number: w.raw(json_number_exact(v.num)); break;
        case JsonValue::Type::Bool:   w.value(v.b); break;
        default:                      w.raw("null"); break;
    }
}
