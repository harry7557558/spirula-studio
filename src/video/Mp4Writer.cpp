// Mp4Writer.cpp -- see Mp4Writer.h. Box layouts are ISO/IEC 14496-12 and
// 14496-15; the spherical boxes are Google's Spherical Video V1 and V2 RFCs.

#include "video/Mp4Writer.h"

#include <cmath>
#include <cstring>

namespace video {

namespace {

// A box built in memory: header, then whatever is appended, size patched on
// finish().
class Box {
public:
    explicit Box(const char* type, int version = -1, uint32_t flags = 0) {
        u32(0);
        data.insert(data.end(), type, type + 4);
        if (version >= 0) u32(((uint32_t)version << 24) | (flags & 0xFFFFFF));
    }
    void u8(uint8_t v) { data.push_back(v); }
    void u16(uint16_t v) { u8((uint8_t)(v >> 8)); u8((uint8_t)v); }
    void u32(uint32_t v) { u16((uint16_t)(v >> 16)); u16((uint16_t)v); }
    void u64(uint64_t v) { u32((uint32_t)(v >> 32)); u32((uint32_t)v); }
    void bytes(const void* p, size_t n) {
        const uint8_t* b = (const uint8_t*)p;
        data.insert(data.end(), b, b + n);
    }
    void zeros(size_t n) { data.insert(data.end(), n, 0); }
    void child(const Box& b) {
        const std::vector<uint8_t> d = b.finish();
        data.insert(data.end(), d.begin(), d.end());
    }
    std::vector<uint8_t> finish() const {
        std::vector<uint8_t> d = data;
        const uint32_t n = (uint32_t)d.size();
        d[0] = (uint8_t)(n >> 24); d[1] = (uint8_t)(n >> 16);
        d[2] = (uint8_t)(n >> 8);  d[3] = (uint8_t)n;
        return d;
    }
    std::vector<uint8_t> data;
};

// Past 2 GB `long` is too narrow on Windows.
int seek64(std::FILE* f, uint64_t at) {
#ifdef _WIN32
    return _fseeki64(f, (long long)at, SEEK_SET);
#else
    return fseeko(f, (off_t)at, SEEK_SET);
#endif
}

void matrix(Box& b) {
    const uint32_t m[9] = {0x00010000, 0, 0, 0, 0x00010000, 0, 0, 0, 0x40000000};
    for (uint32_t v : m) b.u32(v);
}

// Exp-Golomb reader over an RBSP, for the few SPS fields the sample entry
// repeats.
struct Bits {
    std::vector<uint8_t> rbsp;
    size_t pos = 0;
    explicit Bits(const std::vector<uint8_t>& nal, size_t skip) {
        for (size_t i = skip; i < nal.size(); i++) {
            if (i >= skip + 2 && nal[i] == 3 && nal[i - 1] == 0 && nal[i - 2] == 0) continue;
            rbsp.push_back(nal[i]);
        }
    }
    uint32_t bit() {
        if (pos >= rbsp.size() * 8) return 0;
        const uint32_t v = (rbsp[pos >> 3] >> (7 - (pos & 7))) & 1u;
        pos++;
        return v;
    }
    uint32_t bits(int n) {
        uint32_t v = 0;
        for (int i = 0; i < n; i++) v = (v << 1) | bit();
        return v;
    }
    uint32_t ue() {
        int z = 0;
        while (bit() == 0 && z < 32) z++;
        return z ? ((1u << z) - 1u + bits(z)) : 0u;
    }
};

}  // namespace

std::vector<std::pair<int, std::pair<const uint8_t*, size_t>>> split_obus(const uint8_t* p,
                                                                          size_t n) {
    std::vector<std::pair<int, std::pair<const uint8_t*, size_t>>> out;
    size_t i = 0;
    while (i < n) {
        const uint8_t h = p[i];
        const int type = (h >> 3) & 0xF;
        size_t at = i + 1 + ((h & 0x04) ? 1 : 0);
        size_t size = n - at;
        if (h & 0x02) {
            size = 0;
            for (int k = 0; k < 8 && at < n; k++) {
                const uint8_t b = p[at++];
                size |= (size_t)(b & 0x7F) << (7 * k);
                if (!(b & 0x80)) break;
            }
        }
        if (at + size > n) break;
        out.push_back({type, {p + i, at + size - i}});
        i = at + size;
    }
    return out;
}

std::vector<std::pair<const uint8_t*, size_t>> split_annexb(const uint8_t* p, size_t n) {
    std::vector<std::pair<const uint8_t*, size_t>> out;
    size_t i = 0, start = SIZE_MAX;
    while (i + 2 < n) {
        if (p[i] == 0 && p[i + 1] == 0 && p[i + 2] == 1) {
            if (start != SIZE_MAX) {
                size_t end = i;
                while (end > start && p[end - 1] == 0) end--;
                out.push_back({p + start, end - start});
            }
            i += 3;
            start = i;
        } else {
            i++;
        }
    }
    if (start != SIZE_MAX && start < n) out.push_back({p + start, n - start});
    return out;
}

Mp4Writer::~Mp4Writer() {
    if (_f) std::fclose(_f);
}

bool Mp4Writer::open(const std::string& path, Codec codec, int width, int height,
                     double fps, bool spherical, std::string& error) {
    _codec = codec;
    _h265 = codec == Codec::H265;
    _av1 = codec == Codec::Av1;
    _spherical = spherical;
    _width = width;
    _height = height;
    // 90 kHz when a frame is a whole number of ticks, as it is at every
    // common rate; otherwise a millisecond-and-a-bit scale.
    const double d = 90000.0 / fps;
    if (std::fabs(d - std::round(d)) < 1e-6) {
        _timescale = 90000;
        _delta = (uint32_t)std::lround(d);
    } else {
        _timescale = (uint32_t)std::lround(fps * 1000.0);
        _delta = 1000;
    }
    if (!_io_buffer) _io_buffer.reset(new char[spirula::kFileBufferBytes]);
    _f = std::fopen(path.c_str(), "wb");
    if (!_f) {
        error = "cannot create " + path;
        return false;
    }
    std::setvbuf(_f, _io_buffer.get(), _IOFBF, spirula::kFileBufferBytes);
    Box ftyp("ftyp");
    ftyp.bytes("isom", 4);
    ftyp.u32(0x200);
    for (const char* b : {"isom", "iso2", _av1 ? "av01" : _h265 ? "hvc1" : "avc1", "mp41"})
        ftyp.bytes(b, 4);
    const std::vector<uint8_t> f = ftyp.finish();
    std::fwrite(f.data(), 1, f.size(), _f);
    // A 64-bit mdat header: four bytes of size 1, the type, then the size.
    _mdat_start = f.size();
    const uint8_t hdr[16] = {0, 0, 0, 1, 'm', 'd', 'a', 't', 0, 0, 0, 0, 0, 0, 0, 0};
    std::fwrite(hdr, 1, sizeof hdr, _f);
    _written = _mdat_start + sizeof hdr;
    return true;
}

void Mp4Writer::set_parameter_sets(const std::vector<uint8_t>& annexb) {
    if (_av1) {
        for (const auto& [type, obu] : split_obus(annexb.data(), annexb.size()))
            if (type == 1) _seq_obu.assign(obu.first, obu.first + obu.second);
        return;
    }
    for (const auto& [p, n] : split_annexb(annexb.data(), annexb.size())) {
        if (!n) continue;
        std::vector<uint8_t> nal(p, p + n);
        if (_h265) {
            const int t = (p[0] >> 1) & 0x3F;
            if (t == 32) _vps.push_back(nal);
            else if (t == 33) _sps.push_back(nal);
            else if (t == 34) _pps.push_back(nal);
        } else {
            const int t = p[0] & 0x1F;
            if (t == 7) _sps.push_back(nal);
            else if (t == 8) _pps.push_back(nal);
        }
    }
}

bool Mp4Writer::write_sample(const uint8_t* annexb, size_t bytes, bool sync) {
    if (!_f || _failed) return false;
    const uint64_t at = _written;
    uint32_t size = 0;
    if (_av1) {
        // A temporal unit without its delimiter, the sequence header at
        // every key frame.
        const auto obus = split_obus(annexb, bytes);
        bool has_seq = false;
        for (const auto& o : obus) has_seq = has_seq || o.first == 1;
        std::vector<uint8_t> sample;
        if (sync && !has_seq) sample = _seq_obu;
        for (const auto& [type, obu] : obus)
            if (type != 2) sample.insert(sample.end(), obu.first, obu.first + obu.second);
        if (!sample.empty() && std::fwrite(sample.data(), 1, sample.size(), _f) != sample.size()) {
            _failed = true;
            return false;
        }
        size = (uint32_t)sample.size();
        _written += size;
        _offsets.push_back(at);
        _sizes.push_back(size);
        if (sync) _syncs.push_back((uint32_t)_sizes.size());
        return true;
    }
    for (const auto& [p, n] : split_annexb(annexb, bytes)) {
        if (!n) continue;
        // Parameter sets live in the sample entry.
        const int t = _h265 ? (p[0] >> 1) & 0x3F : p[0] & 0x1F;
        if (_h265 ? (t >= 32 && t <= 34) : (t == 7 || t == 8)) continue;
        const uint8_t len[4] = {(uint8_t)(n >> 24), (uint8_t)(n >> 16), (uint8_t)(n >> 8),
                                (uint8_t)n};
        if (std::fwrite(len, 1, 4, _f) != 4 || std::fwrite(p, 1, n, _f) != n) {
            _failed = true;
            return false;
        }
        size += 4 + (uint32_t)n;
    }
    _written += size;
    _offsets.push_back(at);
    _sizes.push_back(size);
    if (sync) _syncs.push_back((uint32_t)_sizes.size());
    return true;
}

bool Mp4Writer::close(std::string& error) {
    if (!_f) return false;
    if (_failed) {
        error = "write failed";
        return false;
    }
    if (_av1 ? _seq_obu.empty() : (_sps.empty() || _pps.empty() || (_h265 && _vps.empty()))) {
        error = "no parameter sets";
        return false;
    }
    const uint32_t n = (uint32_t)_sizes.size();
    const uint64_t duration = (uint64_t)n * _delta;
    const uint64_t movie_duration = duration * 1000 / _timescale;

    // ---- sample entry ----
    Box entry(_av1 ? "av01" : _h265 ? "hvc1" : "avc1");
    entry.zeros(6);
    entry.u16(1);                     // data reference index
    entry.zeros(16);
    entry.u16((uint16_t)_width);
    entry.u16((uint16_t)_height);
    entry.u32(0x00480000);            // 72 dpi
    entry.u32(0x00480000);
    entry.u32(0);
    entry.u16(1);                     // frame count
    entry.zeros(32);                  // compressor name
    entry.u16(0x0018);
    entry.u16(0xFFFF);
    if (_av1) {
        // The first operating point's profile, level and tier, read from the
        // sequence header past its OBU header and size.
        std::vector<uint8_t> body;
        {
            const uint8_t h = _seq_obu[0];
            size_t at = 1 + ((h & 0x04) ? 1 : 0);
            if (h & 0x02)
                while (at < _seq_obu.size() && (_seq_obu[at++] & 0x80)) {}
            body.assign(_seq_obu.begin() + (ptrdiff_t)std::min(at, _seq_obu.size()), _seq_obu.end());
        }
        Bits b(body, 0);
        b.pos = 0;
        b.rbsp = body;
        const uint32_t profile = b.bits(3);
        b.bit();                                   // still_picture
        const uint32_t reduced = b.bit();
        uint32_t level = 8, tier = 0;
        if (reduced) {
            level = b.bits(5);
        } else if (!b.bit()) {                     // no timing info to step over
            b.bit();                               // initial_display_delay_present
            b.bits(5);                             // operating_points_cnt_minus_1
            b.bits(12);                            // operating_point_idc
            level = b.bits(5);
            if (level > 7) tier = b.bit();
        }
        Box av1c("av1C");
        av1c.u8(0x81);                             // marker, version 1
        av1c.u8((uint8_t)((profile << 5) | (level & 0x1F)));
        // 8-bit 4:2:0, chroma position unknown.
        av1c.u8((uint8_t)((tier << 7) | (1 << 3) | (1 << 2)));
        av1c.u8(0);
        av1c.bytes(_seq_obu.data(), _seq_obu.size());
        entry.child(av1c);
    } else if (!_h265) {
        const std::vector<uint8_t>& sps = _sps[0];
        Box avcc("avcC");
        avcc.u8(1);
        avcc.u8(sps.size() > 1 ? sps[1] : 100);
        avcc.u8(sps.size() > 2 ? sps[2] : 0);
        avcc.u8(sps.size() > 3 ? sps[3] : 40);
        avcc.u8(0xFF);                // 4-byte lengths
        avcc.u8((uint8_t)(0xE0 | _sps.size()));
        for (const auto& s : _sps) { avcc.u16((uint16_t)s.size()); avcc.bytes(s.data(), s.size()); }
        avcc.u8((uint8_t)_pps.size());
        for (const auto& s : _pps) { avcc.u16((uint16_t)s.size()); avcc.bytes(s.data(), s.size()); }
        const int profile = sps.size() > 1 ? sps[1] : 100;
        if (profile == 100 || profile == 110 || profile == 122 || profile == 144) {
            avcc.u8(0xFC | 1);        // 4:2:0
            avcc.u8(0xF8);            // 8-bit luma
            avcc.u8(0xF8);            // 8-bit chroma
            avcc.u8(0);
        }
        entry.child(avcc);
    } else {
        // profile_tier_level straight out of the SPS: after the two-byte NAL
        // header, 4 bits of VPS id, 3 of max sub-layers and one of nesting.
        const std::vector<uint8_t>& sps = _sps[0];
        Bits b(sps, 2);
        b.bits(4);
        const uint32_t max_sub_layers = b.bits(3);
        b.bit();
        const uint32_t profile_space = b.bits(2), tier = b.bit(), profile_idc = b.bits(5);
        const uint32_t compat = b.bits(32);
        uint8_t constraint[6];
        for (uint8_t& c : constraint) c = (uint8_t)b.bits(8);
        const uint32_t level_idc = b.bits(8);
        (void)max_sub_layers;
        Box hvcc("hvcC");
        hvcc.u8(1);
        hvcc.u8((uint8_t)((profile_space << 6) | (tier << 5) | profile_idc));
        hvcc.u32(compat);
        hvcc.bytes(constraint, 6);
        hvcc.u8((uint8_t)level_idc);
        hvcc.u16(0xF000);             // min_spatial_segmentation_idc 0
        hvcc.u8(0xFC);                // parallelismType 0
        hvcc.u8(0xFC | 1);            // chroma 4:2:0
        hvcc.u8(0xF8);                // luma bit depth 8
        hvcc.u8(0xF8);                // chroma bit depth 8
        hvcc.u16(0);                  // avgFrameRate
        hvcc.u8(0x0F);                // constantFrameRate 0, 1 temporal layer, nested, 4-byte lengths
        const std::vector<std::vector<uint8_t>>* arrays[3] = {&_vps, &_sps, &_pps};
        const uint8_t types[3] = {32, 33, 34};
        hvcc.u8(3);
        for (int a = 0; a < 3; a++) {
            hvcc.u8((uint8_t)(0x80 | types[a]));
            hvcc.u16((uint16_t)arrays[a]->size());
            for (const auto& s : *arrays[a]) {
                hvcc.u16((uint16_t)s.size());
                hvcc.bytes(s.data(), s.size());
            }
        }
        entry.child(hvcc);
    }
    // BT.709, studio range: what the encoder's VUI says, repeated for players
    // that only read the container.
    Box colr("colr");
    colr.bytes("nclx", 4);
    colr.u16(1);
    colr.u16(1);
    colr.u16(1);
    colr.u8(0);
    entry.child(colr);
    if (_spherical) {
        Box st3d("st3d", 0, 0);
        st3d.u8(0);                   // monoscopic
        entry.child(st3d);
        Box sv3d("sv3d");
        Box svhd("svhd", 0, 0);
        svhd.bytes("Spirula Studio", 15);
        sv3d.child(svhd);
        Box proj("proj");
        Box prhd("prhd", 0, 0);
        prhd.u32(0); prhd.u32(0); prhd.u32(0);
        proj.child(prhd);
        Box equi("equi", 0, 0);
        equi.u32(0); equi.u32(0); equi.u32(0); equi.u32(0);
        proj.child(equi);
        sv3d.child(proj);
        entry.child(sv3d);
    }

    Box stsd("stsd", 0, 0);
    stsd.u32(1);
    stsd.child(entry);
    Box stts("stts", 0, 0);
    stts.u32(1);
    stts.u32(n);
    stts.u32(_delta);
    Box stss("stss", 0, 0);
    stss.u32((uint32_t)_syncs.size());
    for (uint32_t s : _syncs) stss.u32(s);
    Box stsc("stsc", 0, 0);
    stsc.u32(1);
    stsc.u32(1);
    stsc.u32(1);
    stsc.u32(1);
    Box stsz("stsz", 0, 0);
    stsz.u32(0);
    stsz.u32(n);
    for (uint32_t s : _sizes) stsz.u32(s);
    Box co64("co64", 0, 0);
    co64.u32(n);
    for (uint64_t o : _offsets) co64.u64(o);
    Box stbl("stbl");
    stbl.child(stsd);
    stbl.child(stts);
    stbl.child(stss);
    stbl.child(stsc);
    stbl.child(stsz);
    stbl.child(co64);

    Box vmhd("vmhd", 0, 1);
    vmhd.zeros(8);
    Box dref("dref", 0, 0);
    dref.u32(1);
    Box url("url ", 0, 1);
    dref.child(url);
    Box dinf("dinf");
    dinf.child(dref);
    Box minf("minf");
    minf.child(vmhd);
    minf.child(dinf);
    minf.child(stbl);

    Box mdhd("mdhd", 1, 0);
    mdhd.u64(0);
    mdhd.u64(0);
    mdhd.u32(_timescale);
    mdhd.u64(duration);
    mdhd.u16(0x55C4);                 // language "und"
    mdhd.u16(0);
    Box hdlr("hdlr", 0, 0);
    hdlr.u32(0);
    hdlr.bytes("vide", 4);
    hdlr.zeros(12);
    hdlr.bytes("VideoHandler", 13);
    Box mdia("mdia");
    mdia.child(mdhd);
    mdia.child(hdlr);
    mdia.child(minf);

    Box tkhd("tkhd", 1, 3);
    tkhd.u64(0);
    tkhd.u64(0);
    tkhd.u32(1);                      // track id
    tkhd.u32(0);
    tkhd.u64(movie_duration);
    tkhd.zeros(8);
    tkhd.u16(0);                      // layer
    tkhd.u16(0);                      // alternate group
    tkhd.u16(0);                      // volume
    tkhd.u16(0);
    matrix(tkhd);
    tkhd.u32((uint32_t)_width << 16);
    tkhd.u32((uint32_t)_height << 16);

    Box trak("trak");
    trak.child(tkhd);
    if (_spherical) {
        static const uint8_t kUuid[16] = {0xff, 0xcc, 0x82, 0x63, 0xf8, 0x55, 0x4a, 0x93,
                                          0x88, 0x14, 0x58, 0x7a, 0x02, 0x52, 0x1f, 0xdd};
        const char* xml =
            "<?xml version=\"1.0\"?><rdf:SphericalVideo "
            "xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\" "
            "xmlns:GSpherical=\"http://ns.google.com/videos/1.0/spherical/\">"
            "<GSpherical:Spherical>true</GSpherical:Spherical>"
            "<GSpherical:Stitched>true</GSpherical:Stitched>"
            "<GSpherical:StitchingSoftware>Spirula Studio</GSpherical:StitchingSoftware>"
            "<GSpherical:ProjectionType>equirectangular</GSpherical:ProjectionType>"
            "</rdf:SphericalVideo>";
        Box uuid("uuid");
        uuid.bytes(kUuid, 16);
        uuid.bytes(xml, std::strlen(xml));
        trak.child(uuid);
    }
    trak.child(mdia);

    Box mvhd("mvhd", 1, 0);
    mvhd.u64(0);
    mvhd.u64(0);
    mvhd.u32(1000);
    mvhd.u64(movie_duration);
    mvhd.u32(0x00010000);             // rate
    mvhd.u16(0x0100);                 // volume
    mvhd.zeros(10);
    matrix(mvhd);
    mvhd.zeros(24);
    mvhd.u32(2);                      // next track id
    Box moov("moov");
    moov.child(mvhd);
    moov.child(trak);

    // The mdat's size, now that it is known.
    const uint64_t mdat_size = _written - _mdat_start;
    uint8_t sz[8];
    for (int i = 0; i < 8; i++) sz[i] = (uint8_t)(mdat_size >> (56 - 8 * i));
    const std::vector<uint8_t> m = moov.finish();
    bool ok = std::fwrite(m.data(), 1, m.size(), _f) == m.size();
    ok = ok && seek64(_f, _mdat_start + 8) == 0 &&
         std::fwrite(sz, 1, 8, _f) == 8;
    ok = std::fclose(_f) == 0 && ok;
    _f = nullptr;
    if (!ok) error = "write failed";
    return ok;
}

}  // namespace video
