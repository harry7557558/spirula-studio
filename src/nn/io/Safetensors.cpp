#include "nn/io/Safetensors.h"

#include "nn/core/Error.h"
#include "nn/core/Half.h"

#include <cstdio>
#include <cstring>
#include <memory>

namespace nn {
namespace {

struct FileCloser {
    void operator()(std::FILE* f) const { if (f) std::fclose(f); }
};
using File = std::unique_ptr<std::FILE, FileCloser>;

bool seek(std::FILE* f, uint64_t off) {
#ifdef _WIN32
    return _fseeki64(f, (long long)off, SEEK_SET) == 0;
#else
    return fseeko(f, (off_t)off, SEEK_SET) == 0;
#endif
}

uint64_t file_size(std::FILE* f) {
#ifdef _WIN32
    _fseeki64(f, 0, SEEK_END);
    return (uint64_t)_ftelli64(f);
#else
    fseeko(f, 0, SEEK_END);
    return (uint64_t)ftello(f);
#endif
}

// The header is JSON, but only three shapes of it: an object of objects whose
// values are strings, integer arrays, or (under __metadata__) strings. This
// walks that and nothing more, and throws on anything else.
struct Json {
    const char* p;
    const char* end;
    const std::string& path;

    [[noreturn]] void bad(const char* why) const {
        fail("%s: malformed safetensors header (%s)", path.c_str(), why);
    }
    void ws() {
        while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p;
    }
    void expect(char c) {
        ws();
        if (p >= end || *p != c) bad("unexpected character");
        ++p;
    }
    bool peek(char c) {
        ws();
        return p < end && *p == c;
    }
    std::string str() {
        expect('"');
        std::string s;
        while (p < end && *p != '"') {
            if (*p == '\\') {
                if (++p >= end) bad("truncated escape");
            }
            s.push_back(*p++);
        }
        if (p >= end) bad("unterminated string");
        ++p;
        return s;
    }
    int64_t integer() {
        ws();
        char* stop = nullptr;
        const long long v = std::strtoll(p, &stop, 10);
        if (stop == p) bad("expected an integer");
        p = stop;
        return (int64_t)v;
    }
    std::vector<int64_t> ints() {
        std::vector<int64_t> v;
        expect('[');
        if (peek(']')) { ++p; return v; }
        while (true) {
            v.push_back(integer());
            if (peek(',')) { ++p; continue; }
            expect(']');
            return v;
        }
    }
    void skip_value() {
        ws();
        if (peek('"')) { str(); return; }
        if (peek('[')) { ints(); return; }
        if (peek('{')) {
            ++p;
            if (peek('}')) { ++p; return; }
            while (true) {
                str();
                expect(':');
                skip_value();
                if (peek(',')) { ++p; continue; }
                expect('}');
                return;
            }
        }
        integer();
    }
};

}  // namespace

SafetensorsFile::SafetensorsFile(const std::string& path) : path_(path) {
    File f(std::fopen(path.c_str(), "rb"));
    NN_CHECK(f, "cannot open '%s'", path.c_str());
    file_size_ = file_size(f.get());
    uint8_t len_le[8];
    NN_CHECK(seek(f.get(), 0) && std::fread(len_le, 1, 8, f.get()) == 8,
             "%s: too short to be a safetensors file", path.c_str());
    uint64_t n = 0;
    for (int i = 7; i >= 0; --i) n = (n << 8) | len_le[i];
    NN_CHECK(n > 1 && n < (100ull << 20) && 8 + n <= file_size_,
             "%s: header length %llu does not fit the file", path.c_str(),
             (unsigned long long)n);
    std::string header((size_t)n, '\0');
    NN_CHECK(std::fread(&header[0], 1, (size_t)n, f.get()) == n,
             "%s: truncated header", path.c_str());
    data_offset_ = 8 + n;

    Json j{header.data(), header.data() + header.size(), path_};
    j.expect('{');
    while (!j.peek('}')) {
        const std::string name = j.str();
        j.expect(':');
        if (name == "__metadata__") {
            j.skip_value();
        } else {
            Entry e;
            bool offsets = false;
            j.expect('{');
            while (!j.peek('}')) {
                const std::string key = j.str();
                j.expect(':');
                if (key == "dtype") {
                    e.dtype = j.str();
                } else if (key == "shape") {
                    e.shape = j.ints();
                } else if (key == "data_offsets") {
                    const std::vector<int64_t> o = j.ints();
                    if (o.size() != 2 || o[0] < 0 || o[1] < o[0]) j.bad("bad data_offsets");
                    e.begin = (uint64_t)o[0];
                    e.end = (uint64_t)o[1];
                    offsets = true;
                } else {
                    j.skip_value();
                }
                if (j.peek(',')) ++j.p;
            }
            ++j.p;
            NN_CHECK(offsets && !e.dtype.empty(), "%s: tensor '%s' lacks dtype or offsets",
                     path.c_str(), name.c_str());
            NN_CHECK(data_offset_ + e.end <= file_size_,
                     "%s: tensor '%s' runs past the end of the file (truncated download?)",
                     path.c_str(), name.c_str());
            entries_[name] = std::move(e);
        }
        if (j.peek(',')) ++j.p;
    }
}

const SafetensorsFile::Entry& SafetensorsFile::entry(const std::string& name) const {
    auto it = entries_.find(name);
    NN_CHECK(it != entries_.end(), "%s: no tensor '%s'", path_.c_str(), name.c_str());
    return it->second;
}

std::vector<std::string> SafetensorsFile::names() const {
    std::vector<std::string> v;
    v.reserve(entries_.size());
    for (const auto& kv : entries_) v.push_back(kv.first);
    return v;
}

OnnxTensor SafetensorsFile::read(const std::string& name) const {
    const Entry& e = entry(name);
    OnnxTensor t;
    t.name = name;
    t.shape = e.shape;
    const int64_t n = t.numel();
    uint32_t elem = 0;
    if (e.dtype == "F32") elem = 4;
    else if (e.dtype == "F16" || e.dtype == "BF16") elem = 2;
    else fail("%s: tensor '%s' is %s; only float tensors are read", path_.c_str(),
              name.c_str(), e.dtype.c_str());
    NN_CHECK(e.end - e.begin == (uint64_t)n * elem,
             "%s: tensor '%s' holds %llu bytes for %lld %s elements", path_.c_str(),
             name.c_str(), (unsigned long long)(e.end - e.begin), (long long)n,
             e.dtype.c_str());

    File f(std::fopen(path_.c_str(), "rb"));
    NN_CHECK(f, "cannot open '%s'", path_.c_str());
    std::vector<uint8_t> raw((size_t)(e.end - e.begin));
    NN_CHECK(seek(f.get(), data_offset_ + e.begin) &&
                 std::fread(raw.data(), 1, raw.size(), f.get()) == raw.size(),
             "%s: short read of '%s'", path_.c_str(), name.c_str());

    t.data.resize((size_t)n);
    if (elem == 4) {
        std::memcpy(t.data.data(), raw.data(), raw.size());
    } else if (e.dtype == "F16") {
        t.was_f16 = true;
        for (int64_t i = 0; i < n; ++i) {
            uint16_t h;
            std::memcpy(&h, raw.data() + 2 * i, 2);
            t.data[(size_t)i] = half_to_float(h);
        }
    } else {
        for (int64_t i = 0; i < n; ++i) {
            uint16_t h;
            std::memcpy(&h, raw.data() + 2 * i, 2);
            const uint32_t bits = (uint32_t)h << 16;
            std::memcpy(&t.data[(size_t)i], &bits, 4);
        }
    }
    return t;
}

}  // namespace nn
