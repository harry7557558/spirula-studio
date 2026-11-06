#pragma once

// CheckpointIO -- zero-dependency writers for the resumable-checkpoint format.
//
// A checkpoint's resume payload is a single POSIX ustar `state.tar` bundling:
//   * state.json         -- small runtime/validation manifest (written by the
//                           engine; NOT config -- config lives in config.json)
//   * <slot_name>.npy    -- one flat, typed NumPy array per saved pool buffer,
//                           named by its DevicePool slot ("world.means.npy",
//                           "eng.sh_quant.q.npy", ...).
//
// The device->host copy is chunked through a small reusable host buffer, so
// serializing a multi-GB buffer uses bounded host RAM and ZERO extra device
// memory (nothing is allocated on the GPU during a save).
//
// Phase 2 (load/resume) will add the matching tar + .npy readers here.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <istream>
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
    char z[512] = {0};
    out.write(z, 512);
    out.write(z, 512);
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

    constexpr size_t CHUNK = 32u << 20;   // 32 MiB host staging
    if (host.size() < std::min(nbytes, CHUNK)) host.resize(std::min(nbytes, CHUNK));
    const char* src = (const char*)dptr;
    for (size_t off = 0; off < nbytes; ) {
        size_t n = std::min(CHUNK, nbytes - off);
        backend::memcpy_sync(host.data(), src + off, n, backend::MemcpyKind::DeviceToHost);
        out.write(host.data(), (std::streamsize)n);
        off += n;
    }
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
    in.seekg(0, std::ios::beg);
    char h[512];
    while (in.read(h, 512)) {
        if (h[0] == '\0') break;                       // zero block -> end
        std::string name(h, ::strnlen(h, 100));
        uint64_t size = 0;
        if ((unsigned char)h[124] & 0x80) {            // base-256
            for (int i = 125; i < 136; ++i) size = (size << 8) | (unsigned char)h[i];
        } else {                                       // octal
            for (int i = 124; i < 136 && h[i] >= '0' && h[i] <= '7'; ++i)
                size = size * 8 + (uint64_t)(h[i] - '0');
        }
        uint64_t data_off = (uint64_t)in.tellg();
        members.push_back({ name, data_off, size });
        uint64_t skip = (size + 511u) & ~((uint64_t)511u);   // pad to 512
        in.seekg((std::streamoff)skip, std::ios::cur);
    }
    return members;
}

// Read a whole small member (e.g. state.json) into a string.
inline std::string tar_read_member(std::istream& in, const TarMember& m) {
    std::string s(m.size, '\0');
    in.clear();
    in.seekg((std::streamoff)m.data_offset, std::ios::beg);
    in.read(&s[0], (std::streamsize)m.size);
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
    constexpr size_t CHUNK = 32u << 20;
    if (host.size() < std::min(nbytes, CHUNK)) host.resize(std::min(nbytes, CHUNK));
    char* dst = (char*)dptr;
    for (size_t off = 0; off < nbytes; ) {
        size_t n = std::min(CHUNK, nbytes - off);
        in.read(host.data(), (std::streamsize)n);
        backend::memcpy_sync(dst + off, host.data(), n, backend::MemcpyKind::HostToDevice);
        off += n;
    }
}

} // namespace ckpt
