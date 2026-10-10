#pragma once

// JsonField.h -- one scalar field to JSON text and back.
//
// The encoding every settings file in this program shares: `null` means
// "unset" (an empty std::string, a disengaged std::optional), floats are
// Python-json compatible so an infinite default round-trips, and reading a
// missing or null key leaves the field alone.

#include "data/Json.h"
#include "data/JsonWrite.h"

#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace json_field {

// ---- field -> JSON text ---------------------------------------------------

inline std::string emit(bool v) { return v ? "true" : "false"; }
inline std::string emit(int v)  { return std::to_string(v); }
inline std::string emit(uint64_t v) { return std::to_string(v); }
inline std::string emit(double v) { return json_number_exact(v); }
inline std::string emit(float v) {
    if (std::isinf(v)) return v > 0 ? "Infinity" : "-Infinity";
    char buf[32];
    std::snprintf(buf, sizeof buf, "%g", v);
    return buf;
}
inline std::string emit(const std::string& v) {
    return v.empty() ? "null" : json_quote(v);
}
template <typename E, std::enable_if_t<std::is_enum_v<E>, int> = 0>
std::string emit(E v) { return std::to_string((int)v); }
template <typename T> std::string emit(const std::optional<T>& v) {
    return v ? emit(*v) : "null";
}
template <typename T, size_t N> std::string emit(const std::array<T, N>& v) {
    std::string s = "[";
    for (size_t i = 0; i < N; i++) s += (i ? ", " : "") + emit(v[i]);
    return s + "]";
}
// A name -> text map as an object; empty is `null`, like an empty string.
inline std::string emit(const std::map<std::string, std::string>& v) {
    if (v.empty()) return "null";
    std::string s = "{";
    for (const auto& [k, x] : v) s += (s.size() > 1 ? ", " : "") + json_quote(k) + ": " + json_quote(x);
    return s + "}";
}

// ---- JSON value -> field --------------------------------------------------

inline void assign(int& out, const JsonValue& v)   { if (!v.is_null()) out = (int)v.as_int(); }
inline void assign(float& out, const JsonValue& v) { if (!v.is_null()) out = (float)v.as_double(); }
inline void assign(bool& out, const JsonValue& v)  { if (!v.is_null()) out = v.as_bool(); }
inline void assign(double& out, const JsonValue& v) { if (!v.is_null()) out = v.as_double(); }
inline void assign(uint64_t& out, const JsonValue& v) {
    if (v.is_null()) return;
    const double n = v.as_double(-1);
    if (!std::isfinite(n) || n < 0 || n > 9007199254740991.0 || std::floor(n) != n)
        throw std::runtime_error("config JSON: expected a nonnegative exact integer");
    out = (uint64_t)n;
}

inline void assign(std::string& out, const JsonValue& v) {
    out = v.is_null() ? std::string() : v.as_string();
}

template <typename E, std::enable_if_t<std::is_enum_v<E>, int> = 0>
void assign(E& out, const JsonValue& v) {
    if (!v.is_null()) out = (E)(int)v.as_int();
}

inline void assign(std::optional<int>& out, const JsonValue& v) {
    if (v.is_null()) out = std::nullopt; else out = (int)v.as_int();
}
inline void assign(std::optional<float>& out, const JsonValue& v) {
    if (v.is_null()) out = std::nullopt; else out = (float)v.as_double();
}
inline void assign(std::optional<bool>& out, const JsonValue& v) {
    if (v.is_null()) out = std::nullopt; else out = v.as_bool();
}

inline void assign(std::map<std::string, std::string>& out, const JsonValue& v) {
    out.clear();
    if (!v.is_object()) return;
    for (const auto& [k, x] : v.obj)
        if (x.type == JsonValue::Type::String) out[k] = x.as_string();
}

template <typename T, size_t N>
void assign(std::array<T, N>& out, const JsonValue& v) {
    if (v.is_null()) return;
    if (!v.is_array() || v.arr.size() != N)
        throw std::runtime_error("config JSON: expected an array of " +
                                 std::to_string(N) + " numbers");
    for (size_t i = 0; i < N; i++)
        out[i] = (T)(std::is_integral<T>::value ? (double)v.arr[i].as_int()
                                                : v.arr[i].as_double());
}

}  // namespace json_field
