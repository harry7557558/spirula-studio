#pragma once

// Native resume archives: state.json and flat typed NPY device buffers in ustar.
// Two reusable chunks overlap disk I/O with device transfers.

#include "core/BufferedFile.h"
#include <future>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <istream>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "core/Tensor.h"   // NpyScalar, npy_scalar_descr


namespace ckpt {

// --- NPY v1.0 header for a flat array of `numel` elements of descr `descr`. ---
inline std::string npy_header(const char* descr, size_t numel) {
    std::string dict = "{'descr': '" + std::string(descr) +
        "', 'fortran_order': False, 'shape': (" + std::to_string(numel) + ",), }";
    size_t base = 10 + dict.size() + 1;               // +1 for trailing '\n'
    size_t pad  = (64 - (base % 64)) % 64;            // pad header to 64B multiple
    dict.append(pad, ' ');
    dict.push_back('\n');
    std::string h;
    h.reserve(10 + dict.size());
    h.append("\x93NUMPY", 6);
    h.push_back('\x01'); h.push_back('\x00');          // version 1.0
    uint16_t hlen = (uint16_t)dict.size();
    h.push_back((char)(hlen & 0xff));
    h.push_back((char)((hlen >> 8) & 0xff));
    h.append(dict);
    return h;
}

// --- POSIX ustar member header (512 bytes) for a file of `size` bytes. ---
inline void tar_header(std::ostream& out, const std::string& name, size_t size) {
    char h[512] = {0};
    std::snprintf(h + 0,   100, "%s", name.c_str());   // name
    std::snprintf(h + 100, 8,   "%07o", 0644);         // mode
    std::snprintf(h + 108, 8,   "%07o", 0);            // uid
    std::snprintf(h + 116, 8,   "%07o", 0);            // gid
    if (size <= 077777777777ULL) {                     // size: octal if it fits,
        std::snprintf(h + 124, 12, "%011llo", (unsigned long long)size);
    } else {                                            // else GNU base-256
        h[124] = (char)0x80;
        unsigned long long v = size;
        for (int i = 135; i >= 125; --i) { h[i] = (char)(v & 0xff); v >>= 8; }
    }
    std::snprintf(h + 136, 12,  "%011llo", 0ull);      // mtime
    std::memset (h + 148, ' ', 8);                     // checksum field = spaces
    h[156] = '0';                                      // typeflag: regular file
    std::memcpy (h + 257, "ustar", 5);                 // magic ("ustar\0")
    h[263] = '0'; h[264] = '0';                        // version "00"
    unsigned chk = 0;
    for (int i = 0; i < 512; i++) chk += (unsigned char)h[i];
    std::snprintf(h + 148, 7, "%06o", chk & 0777777u); // 6 octal digits + NUL@154
    h[155] = ' ';                                      // trailing space
    out.write(h, 512);
}

inline void tar_pad(std::ostream& out, size_t size) {
    size_t pad = (512 - (size % 512)) % 512;
    if (pad) { char z[512] = {0}; out.write(z, pad); }
}

// --- Append an in-memory member (e.g. state.json). ---
inline void tar_write_bytes(std::ostream& out, const std::string& name,
                            const char* data, size_t size) {
    tar_header(out, name, size);
    out.write(data, size);
    tar_pad(out, size);
}

// --- Two zero blocks terminate the archive. ---
inline void tar_finish(std::ostream& out) {
    const char zero[1024]{};
    if (!out.write(zero, sizeof(zero)))
        throw std::runtime_error("failed to write checkpoint TAR terminator");
}

// --- Stream a device buffer as a flat typed .npy member. Chunked D->H copy
//     through `host` (reused across calls); no device allocation. ---
inline void tar_write_npy_device(std::ostream& out, const std::string& name,
                                 const void* dptr, size_t nbytes, uint8_t dtype_tag,
                                 std::vector<char>& host) {
    auto descr_size = npy_scalar_descr((NpyScalar)dtype_tag);
    const char* descr = descr_size.first;
    size_t esize      = descr_size.second;
    size_t numel      = nbytes / esize;

    std::string hdr = npy_header(descr, numel);
    size_t member   = hdr.size() + nbytes;
    tar_header(out, name, member);
    out.write(hdr.data(), (std::streamsize)hdr.size());

    const size_t chunk = std::min(nbytes, spirula::kFileBufferBytes);
    if (host.size() < 2*chunk) host.resize(2*chunk);
    const char* src = (const char*)dptr;
    std::future<void> pending;
    size_t slot = 0;
    for (size_t off = 0; off < nbytes; off += chunk) {
        const size_t n = std::min(chunk, nbytes-off);
        char* data = host.data()+slot*chunk;
        backend::memcpy_sync(data, src+off, n, backend::MemcpyKind::DeviceToHost);
        if (pending.valid()) pending.get();
        pending = std::async(std::launch::async, [&, data, n] {
            out.write(data, (std::streamsize)n);
            if (!out) throw std::runtime_error("failed to write checkpoint buffer " + name);
        });
        slot = 1-slot;
    }
    if (pending.valid()) pending.get();
    tar_pad(out, member);
}


// ============================================================================
// Readers (checkpoint load / resume).
// ============================================================================

// A member located in a tar stream: its name and the [offset, size) of its
// raw content within the archive.
struct TarMember {
    std::string name;
    uint64_t    data_offset;
    uint64_t    size;
};

// Index every regular-file member of a ustar stream (no extraction). Handles
// octal and GNU base-256 size fields. Leaves the stream position undefined.
inline std::vector<TarMember> tar_index(std::istream& in) {
    std::vector<TarMember> members;
    in.clear();
    if (!in.seekg(0, std::ios::beg))
        throw std::runtime_error("failed to seek to checkpoint TAR start");
    char h[512];
    for (;;) {
        if (!in.read(h, sizeof(h)))
            throw std::runtime_error("truncated checkpoint TAR header or missing terminator");
        if (h[0] == '\0') {
            auto zero_block = [&] {
                return std::all_of(h, h + sizeof(h), [](char c) { return c == '\0'; });
            };
            if (!zero_block() || !in.read(h, sizeof(h)) || !zero_block())
                throw std::runtime_error("invalid checkpoint TAR terminator");
            return members;
        }
        std::string name(h, ::strnlen(h, 100));
        uint64_t size = 0;
        if ((unsigned char)h[124] & 0x80) {            // base-256
            if ((unsigned char)h[124] != 0x80 || h[125] != 0 || h[126] != 0 || h[127] != 0)
                throw std::runtime_error("invalid checkpoint TAR member size");
            for (int i = 128; i < 136; ++i) size = (size << 8) | (unsigned char)h[i];
        } else {                                       // octal
            for (int i = 124; i < 136 && h[i] >= '0' && h[i] <= '7'; ++i)
                size = size * 8 + (uint64_t)(h[i] - '0');
        }
        const std::streamoff data_off = in.tellg();
        const uint64_t pad = (512 - size % 512) % 512;
        if (data_off < 0 || size > uint64_t(std::numeric_limits<std::streamoff>::max() - data_off) ||
            pad > uint64_t(std::numeric_limits<std::streamoff>::max() - data_off) - size ||
            !in.seekg((std::streamoff)(size + pad), std::ios::cur))
            throw std::runtime_error("failed to seek past checkpoint TAR member " + name);
        members.push_back({std::move(name), (uint64_t)data_off, size});
    }
}

// Read a whole small member (e.g. state.json) into a string.
inline std::string tar_read_member(std::istream& in, const TarMember& m) {
    in.clear();
    if (m.data_offset > uint64_t(std::numeric_limits<std::streamoff>::max()) ||
        !in.seekg((std::streamoff)m.data_offset, std::ios::beg))
        throw std::runtime_error("failed to seek to checkpoint TAR member " + m.name);
    if (m.size > uint64_t(std::numeric_limits<std::streamsize>::max()))
        throw std::runtime_error("invalid checkpoint TAR member size for " + m.name);
    std::string s(m.size, '\0');
    if (!in.read(s.data(), (std::streamsize)m.size))
        throw std::runtime_error("truncated checkpoint TAR member " + m.name);
    return s;
}

// Location of the raw array data inside a .npy blob that starts at `npy_start`
// and spans `member_size` bytes. Also returns the numpy descr string.
struct NpyInfo {
    uint64_t    data_offset;   // absolute offset in the stream
    uint64_t    data_bytes;    // member_size - header
    std::string descr;         // e.g. "<f4"
};
inline NpyInfo npy_locate(std::istream& in, uint64_t npy_start, uint64_t member_size) {
    in.clear();
    in.seekg((std::streamoff)npy_start, std::ios::beg);
    if (member_size < 10) throw std::runtime_error("truncated NPY header");
    char pre[10]{};
    in.read(pre, 10);                                  // magic(6) + ver(2) + ...
    if (!in || std::memcmp(pre, "\x93NUMPY", 6) != 0 || pre[6] < 1 || pre[6] > 3)
        throw std::runtime_error("invalid NPY header");
    uint8_t major = (uint8_t)pre[6];
    uint32_t hlen;
    uint32_t prefix;
    if (major >= 2) {                                  // v2/v3: 4-byte hlen
        // pre[8..9] are the first 2 bytes of a 4-byte length; read 2 more.
        char more[2]{}; in.read(more, 2);
        hlen = (uint8_t)pre[8] | ((uint8_t)pre[9] << 8)
             | ((uint8_t)more[0] << 16) | ((uint8_t)more[1] << 24);
        prefix = 12;
    } else {                                           // v1: 2-byte hlen
        hlen = (uint8_t)pre[8] | ((uint8_t)pre[9] << 8);
        prefix = 10;
    }
    if (!in || uint64_t(prefix) + hlen > member_size)
        throw std::runtime_error("NPY header exceeds archive member");
    std::string dict(hlen, '\0');
    in.read(dict.data(), hlen);
    if (!in) throw std::runtime_error("truncated NPY header");
    std::string descr;
    size_t d = dict.find("'descr':");
    if (d != std::string::npos) {
        size_t q1 = dict.find('\'', d + 8);
        size_t q2 = (q1 == std::string::npos) ? q1 : dict.find('\'', q1 + 1);
        if (q2 != std::string::npos) descr = dict.substr(q1 + 1, q2 - q1 - 1);
    }
    NpyInfo info;
    info.data_offset = npy_start + prefix + hlen;
    info.data_bytes  = member_size - (prefix + hlen);
    info.descr       = descr;
    return info;
}

// Stream `nbytes` from `in` (positioned via seek to `offset`) into a device
// buffer, chunked through `host` -- bounded host RAM, no device allocation.
inline void read_into_device(std::istream& in, uint64_t offset,
                             void* dptr, size_t nbytes, std::vector<char>& host) {
    in.clear();
    in.seekg((std::streamoff)offset, std::ios::beg);
    const size_t chunk = std::min(nbytes, spirula::kFileBufferBytes);
    if (host.size() < 2*chunk) host.resize(2*chunk);
    char* dst = (char*)dptr;
    auto read = [&](char* data, size_t n) {
        in.read(data, (std::streamsize)n);
        if (!in) throw std::runtime_error("truncated checkpoint buffer");
    };
    if (nbytes) read(host.data(), chunk);
    std::future<void> pending;
    size_t slot = 0;
    for (size_t off = 0; off < nbytes; off += chunk) {
        const size_t n = std::min(chunk, nbytes-off);
        if (pending.valid()) pending.get();
        if (off+n < nbytes)
            pending = std::async(std::launch::async, read, host.data()+(1-slot)*chunk,
                                 std::min(chunk, nbytes-off-n));
        backend::memcpy_sync(dst+off, host.data()+slot*chunk, n, backend::MemcpyKind::HostToDevice);
        slot = 1-slot;
    }
}

} // namespace ckpt
