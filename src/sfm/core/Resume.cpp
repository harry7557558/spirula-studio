// Resume.cpp -- see Resume.h.

#include "sfm/core/Resume.h"

#include <cerrno>
#include <chrono>
#include <cstring>

namespace fs = std::filesystem;

namespace sfm {
namespace resume {

namespace {

// What a kill may cost: whichever of the two comes first. The size bound keeps
// a fast capture from flushing per pair; the clock keeps a slow one -- a
// learned matcher is seconds per pair -- from holding an hour in memory.
constexpr size_t kFlushBytes = 4u << 20;
constexpr double kFlushSeconds = 5.0;
constexpr size_t kRecordHeaderBytes = 20;
constexpr size_t kMatchBytes = 8;

std::error_code streamError() {
    return errno ? std::error_code(errno, std::generic_category())
                 : std::make_error_code(std::errc::io_error);
}

double steadyNow() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

void put_u32(std::string& b, uint32_t v) { b.append((const char*)&v, 4); }
void put_i32(std::string& b, int32_t v) { b.append((const char*)&v, 4); }

bool read_u32(std::istream& f, uint32_t& v) {
    f.read((char*)&v, 4);
    return f.gcount() == 4;
}

// The signature block every file here opens with: u32 length, then the text.
void put_signature(std::string& b, const std::string& sig) {
    put_u32(b, (uint32_t)sig.size());
    b += sig;
}

bool check_signature(std::istream& f, const std::string& sig) {
    uint32_t len = 0;
    if (!read_u32(f, len) || len > (1u << 20)) return false;
    std::string seen(len, '\0');
    f.read(&seen[0], (std::streamsize)len);
    return f.gcount() == (std::streamsize)len && seen == sig;
}

bool magic_is(std::istream& f, const char* want) {
    char m[4];
    f.read(m, 4);
    return f.gcount() == 4 && std::memcmp(m, want, 4) == 0;
}

}  // namespace

fs::path dir(const std::string& workspace) { return fs::path(workspace) / kDir; }

std::string recorded(const fs::path& file) {
    std::ifstream f(file, std::ios::binary);
    if (!f) return "";
    return std::string(std::istreambuf_iterator<char>(f),
                       std::istreambuf_iterator<char>());
}

void store(const fs::path& file, const std::string& signature) {
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    std::ofstream f(file, std::ios::binary | std::ios::trunc);
    if (f) f << signature;
}

void forget(const fs::path& file) {
    std::error_code ec;
    fs::remove(file, ec);
}

void clear(const std::string& workspace) {
    if (workspace.empty()) return;
    std::error_code ec;
    fs::remove_all(dir(workspace), ec);
}

// ---------------------------------------------------------------------------
// The pair list
// ---------------------------------------------------------------------------

bool readPairs(const fs::path& file, const std::string& signature,
               std::vector<std::pair<uint32_t, uint32_t>>& pairs) {
    std::ifstream f(file, std::ios::binary);
    if (!f || !magic_is(f, "VKRP") || !check_signature(f, signature)) return false;
    uint32_t n = 0;
    if (!read_u32(f, n)) return false;
    std::vector<std::pair<uint32_t, uint32_t>> out(n);
    f.read((char*)out.data(), (std::streamsize)n * 8);
    if (f.gcount() != (std::streamsize)n * 8) return false;
    pairs.swap(out);
    return true;
}

void writePairs(const fs::path& file, const std::string& signature,
                const std::vector<std::pair<uint32_t, uint32_t>>& pairs) {
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    std::string head;
    head += "VKRP";
    put_signature(head, signature);
    put_u32(head, (uint32_t)pairs.size());
    std::ofstream f(file, std::ios::binary | std::ios::trunc);
    if (!f) return;
    f.write(head.data(), (std::streamsize)head.size());
    f.write((const char*)pairs.data(), (std::streamsize)pairs.size() * 8);
}

// ---------------------------------------------------------------------------
// The verification journal
// ---------------------------------------------------------------------------

std::error_code MatchJournal::open(const fs::path& file, const std::string& signature,
                                   std::optional<uintmax_t> append_at) {
    close();
    std::error_code ec;
    if (append_at) {
        const auto size = fs::file_size(file, ec);
        if (ec) return ec;
        if (size < *append_at) return std::make_error_code(std::errc::invalid_argument);
        if (size > *append_at) {
            fs::resize_file(file, *append_at, ec);
            if (ec) return ec;
        }
    } else if (file.has_parent_path()) {
        fs::create_directories(file.parent_path(), ec);
        if (ec) return ec;
    }
    errno = 0;
    _f.open(file, std::ios::binary | (append_at ? std::ios::app : std::ios::trunc));
    if (!_f) return streamError();
    if (!append_at) {
        std::string head;
        head += "VKRJ";
        put_signature(head, signature);
        errno = 0;
        _f.write(head.data(), (std::streamsize)head.size());
        if (!_f) return streamError();
    }
    _flushed_at = steadyNow();
    _armed = true;
    return {};
}

void MatchJournal::record(uint32_t a, uint32_t b, int32_t config, uint32_t putative,
                          const uint32_t* idx1, const uint32_t* idx2,
                          size_t stride, uint32_t count) {
    if (!_armed) return;
    // Packed before the lock: this runs on every verification worker.
    std::string rec;
    rec.reserve(kRecordHeaderBytes + (size_t)count * kMatchBytes);
    put_u32(rec, a);
    put_u32(rec, b);
    put_i32(rec, config);
    put_u32(rec, putative);
    put_u32(rec, count);
    for (uint32_t i = 0; i < count; i++) {
        rec.append((const char*)idx1 + i * stride, 4);
        rec.append((const char*)idx2 + i * stride, 4);
    }
    std::lock_guard<std::mutex> lk(_mu);
    _buf += rec;
    write_locked(false);
}

void MatchJournal::write_locked(bool force) {
    const double t = steadyNow();
    if (_buf.empty() ||
        (!force && _buf.size() < kFlushBytes && t - _flushed_at < kFlushSeconds))
        return;
    _f.write(_buf.data(), (std::streamsize)_buf.size());
    _f.flush();
    _buf.clear();
    _flushed_at = t;
}

void MatchJournal::flush() {
    std::lock_guard<std::mutex> lk(_mu);
    if (_armed) write_locked(true);
}

void MatchJournal::close() {
    std::lock_guard<std::mutex> lk(_mu);
    if (_armed) write_locked(true);
    _f.close();
    _f.clear();
    _buf.clear();
    _armed = false;
}

std::optional<uintmax_t> readJournal(const fs::path& file, const std::string& signature,
                                     const std::vector<ImageEntry>& images,
                                     std::unordered_map<uint64_t, TwoViewMatches>& kept,
                                     std::vector<uint64_t>& done, uint64_t& putative,
                                     std::error_code& error) {
    const auto size = fs::file_size(file, error);
    if (error) {
        if (error == std::errc::no_such_file_or_directory) error.clear();
        return std::nullopt;
    }
    errno = 0;
    spirula::BufferedInputFile f(file, std::ios::binary);
    if (!f) {
        error = streamError();
        return std::nullopt;
    }
    const auto at_eof = [&] {
        if (f.bad() || !f.eof()) return false;
        f.clear();
        const std::streamoff end = f.tellg();
        return end >= 0 && (uintmax_t)end == size;
    };
    if (!magic_is(f, "VKRJ") || !check_signature(f, signature)) {
        if (!f && !at_eof()) error = std::make_error_code(std::errc::io_error);
        return std::nullopt;
    }
    const std::streamoff header_end = f.tellg();
    if (header_end < 0) {
        error = std::make_error_code(std::errc::io_error);
        return std::nullopt;
    }
    uintmax_t valid_end = (uintmax_t)header_end;
    std::unordered_map<uint64_t, TwoViewMatches> k;
    std::vector<uint64_t> d;
    uint64_t put = 0;
    const uint32_t nimg = (uint32_t)images.size();
    for (;;) {
        uint32_t a = 0, b = 0, count = 0, offered = 0;
        int32_t config = 0;
        if (!read_u32(f, a) || !read_u32(f, b)) break;
        f.read((char*)&config, 4);
        if (f.gcount() != 4 || !read_u32(f, offered) || !read_u32(f, count)) break;
        if (a >= nimg || b >= nimg || count > images[a].num_features) {
            error = std::make_error_code(std::errc::illegal_byte_sequence);
            return std::nullopt;
        }
        TwoViewMatches tvm;
        tvm.image1 = a;
        tvm.image2 = b;
        tvm.config = config;
        std::vector<FeatureMatch>& m = tvm.matches.mut();
        m.resize(count);
        bool torn = false;
        for (uint32_t i = 0; i < count; i++) {
            f.read((char*)&m[i].idx1, 4);
            f.read((char*)&m[i].idx2, 4);
            if (!f) { torn = true; break; }
        }
        if (torn) break;
        valid_end += kRecordHeaderBytes + (uintmax_t)count * kMatchBytes;
        const uint64_t key = pairKey(a, b);
        d.push_back(key);
        put += offered;
        if (count) k.emplace(key, std::move(tvm));
    }
    if (!at_eof()) {
        error = std::make_error_code(std::errc::io_error);
        return std::nullopt;
    }
    kept.swap(k);
    done.swap(d);
    putative = put;
    return valid_end;
}

}  // namespace resume
}  // namespace sfm
