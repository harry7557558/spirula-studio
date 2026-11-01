#include "nn/io/SafetensorsWriter.h"

#include "nn/core/Error.h"

namespace nn {
namespace {

constexpr uint64_t kAlign = 256;

uint64_t dtype_bytes(const std::string& d) {
    if (d == "F32" || d == "I32") return 4;
    if (d == "F16" || d == "BF16") return 2;
    if (d == "U8") return 1;
    fail("safetensors writer: no dtype '%s'", d.c_str());
}

bool seek(std::FILE* f, uint64_t off) {
#ifdef _WIN32
    return _fseeki64(f, (long long)off, SEEK_SET) == 0;
#else
    return fseeko(f, (off_t)off, SEEK_SET) == 0;
#endif
}

}  // namespace

SafetensorsWriter::~SafetensorsWriter() { close(); }

void SafetensorsWriter::add(const std::string& name, const std::string& dtype,
                            std::vector<int64_t> shape) {
    NN_CHECK(!planned_, "safetensors writer: '%s' added after the layout was fixed",
             name.c_str());
    NN_CHECK(!index_.count(name), "safetensors writer: '%s' added twice", name.c_str());
    index_[name] = entries_.size();
    entries_.push_back({name, dtype, std::move(shape)});
}

void SafetensorsWriter::metadata(const std::string& key, const std::string& value) {
    NN_CHECK(!planned_, "safetensors writer: metadata after the layout was fixed");
    meta_.emplace_back(key, value);
}

void SafetensorsWriter::plan() {
    if (planned_) return;
    planned_ = true;
    std::vector<Planned> laid;
    uint64_t at = 0;
    int pads = 0;
    for (Planned& e : entries_) {
        if (at % kAlign) {
            const uint64_t gap = kAlign - at % kAlign;
            laid.push_back({"__pad" + std::to_string(pads++), "U8", {(int64_t)gap}, at, at + gap});
            at += gap;
        }
        uint64_t n = dtype_bytes(e.dtype);
        for (int64_t d : e.shape) n *= (uint64_t)d;
        e.begin = at;
        e.end = at + n;
        at = e.end;
        laid.push_back(e);
    }
    entries_ = std::move(laid);
    index_.clear();
    for (size_t i = 0; i < entries_.size(); ++i) index_[entries_[i].name] = i;

    std::string h = "{";
    if (!meta_.empty()) {
        h += "\"__metadata__\":{";
        for (size_t i = 0; i < meta_.size(); ++i)
            h += (i ? ",\"" : "\"") + meta_[i].first + "\":\"" + meta_[i].second + "\"";
        h += "}";
    }
    for (size_t i = 0; i < entries_.size(); ++i) {
        const Planned& e = entries_[i];
        h += (i || !meta_.empty()) ? ",\"" : "\"";
        h += e.name + "\":{\"dtype\":\"" + e.dtype + "\",\"shape\":[";
        for (size_t d = 0; d < e.shape.size(); ++d)
            h += (d ? "," : "") + std::to_string(e.shape[d]);
        h += "],\"data_offsets\":[" + std::to_string(e.begin) + "," + std::to_string(e.end) + "]}";
    }
    h += "}";
    // The data section starts aligned too, so an offset aligned in the data is
    // aligned in the file.
    while ((8 + h.size()) % kAlign) h += ' ';
    header_ = std::move(h);
    data_offset_ = 8 + header_.size();
}

const std::string& SafetensorsWriter::header() {
    plan();
    return header_;
}

uint64_t SafetensorsWriter::fileBytes() {
    plan();
    return data_offset_ + (entries_.empty() ? 0 : entries_.back().end);
}

uint64_t SafetensorsWriter::offset(const std::string& name) {
    plan();
    auto it = index_.find(name);
    NN_CHECK(it != index_.end(), "safetensors writer: no tensor '%s'", name.c_str());
    return data_offset_ + entries_[it->second].begin;
}

uint64_t SafetensorsWriter::bytes(const std::string& name) {
    plan();
    auto it = index_.find(name);
    NN_CHECK(it != index_.end(), "safetensors writer: no tensor '%s'", name.c_str());
    return entries_[it->second].end - entries_[it->second].begin;
}

void SafetensorsWriter::open(const std::string& path, bool fresh) {
    plan();
    close();
    path_ = path;
    f_ = std::fopen(path.c_str(), fresh ? "w+b" : "r+b");
    NN_CHECK(f_, "cannot open '%s' for writing", path.c_str());
    if (!fresh) return;
    uint8_t len[8];
    const uint64_t n = header_.size();
    for (int i = 0; i < 8; ++i) len[i] = (uint8_t)(n >> (8 * i));
    NN_CHECK(std::fwrite(len, 1, 8, f_) == 8 &&
                 std::fwrite(header_.data(), 1, n, f_) == n,
             "cannot write '%s'", path.c_str());
    // The last byte claims the whole size up front, so a full disk fails here
    // and not three hours in. Filler entries are left as the holes they are.
    const uint8_t zero = 0;
    NN_CHECK(seek(f_, fileBytes() - 1) && std::fwrite(&zero, 1, 1, f_) == 1,
             "cannot size '%s' to %llu bytes -- is the disk full?", path.c_str(),
             (unsigned long long)fileBytes());
}

void SafetensorsWriter::write(const std::string& name, const void* data, uint64_t n,
                              uint64_t at) {
    NN_CHECK(f_, "safetensors writer: write before open");
    NN_CHECK(at + n <= bytes(name), "safetensors writer: %llu bytes at %llu overrun '%s'",
             (unsigned long long)n, (unsigned long long)at, name.c_str());
    NN_CHECK(seek(f_, offset(name) + at) && std::fwrite(data, 1, n, f_) == n,
             "cannot write '%s' to '%s' -- is the disk full?", name.c_str(), path_.c_str());
}

void SafetensorsWriter::read(const std::string& name, void* data, uint64_t n, uint64_t at) {
    NN_CHECK(f_, "safetensors writer: read before open");
    NN_CHECK(at + n <= bytes(name), "safetensors writer: %llu bytes at %llu overrun '%s'",
             (unsigned long long)n, (unsigned long long)at, name.c_str());
    NN_CHECK(std::fflush(f_) == 0 && seek(f_, offset(name) + at) &&
                 std::fread(data, 1, n, f_) == n,
             "cannot read '%s' back from '%s'", name.c_str(), path_.c_str());
}

void SafetensorsWriter::flush() {
    if (f_) NN_CHECK(std::fflush(f_) == 0, "cannot flush '%s'", path_.c_str());
}

void SafetensorsWriter::close() {
    if (!f_) return;
    std::fclose(f_);
    f_ = nullptr;
}

}  // namespace nn
