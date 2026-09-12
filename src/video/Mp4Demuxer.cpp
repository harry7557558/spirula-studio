#include "video/Mp4Demuxer.h"

#include "nn/core/Log.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace video {

const char* codec_name(Codec c) {
    switch (c) {
        case Codec::H264: return "h264";
        case Codec::H265: return "h265";
        case Codec::AV1:  return "av1";
        default:          return "unknown";
    }
}

namespace {

uint32_t rd32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
uint64_t rd64(const uint8_t* p) {
    return ((uint64_t)rd32(p) << 32) | rd32(p + 4);
}
uint16_t rd16(const uint8_t* p) { return (uint16_t)((p[0] << 8) | p[1]); }

struct Box {
    uint32_t type = 0;
    const uint8_t* payload = nullptr;
    size_t payload_size = 0;
    size_t total_size = 0;
};

constexpr uint32_t fourcc(const char s[5]) {
    return ((uint32_t)(uint8_t)s[0] << 24) | ((uint32_t)(uint8_t)s[1] << 16) |
           ((uint32_t)(uint8_t)s[2] << 8) | (uint32_t)(uint8_t)s[3];
}

bool read_box(const uint8_t* data, size_t avail, Box& out) {
    if (avail < 8) return false;
    uint64_t size = rd32(data);
    out.type = rd32(data + 4);
    size_t header = 8;
    if (size == 1) {
        if (avail < 16) return false;
        size = rd64(data + 8);
        header = 16;
    } else if (size == 0) {
        size = avail;  // "to end of file"
    }
    if (size < header || size > avail) return false;
    out.payload = data + header;
    out.payload_size = (size_t)size - header;
    out.total_size = (size_t)size;
    return true;
}

bool find_box(const uint8_t* data, size_t size, uint32_t type, Box& out) {
    size_t off = 0;
    while (off + 8 <= size) {
        Box b;
        if (!read_box(data + off, size - off, b)) return false;
        if (b.type == type) {
            out = b;
            return true;
        }
        off += b.total_size;
    }
    return false;
}

// The tkhd display matrix maps stored coordinates to display ones as
// (x', y') = (a x + c y, b x + d y), so the turn is atan2(b, a) and a negative
// determinant is a mirror -- the two numbers av_display_rotation_get reads.
void parse_display_matrix(const uint8_t* p, TrackInfo& info) {
    // 16.16 fixed point; only the four in-plane entries matter.
    auto fp = [p](int i) { return (double)(int32_t)rd32(p + 4 * i) / 65536.0; };
    const double a = fp(0), b = fp(1), c = fp(3), d = fp(4);
    const double sa = std::hypot(a, b);
    if (sa < 1e-9) return;
    int deg = (int)std::lround(std::atan2(b, a) * 180.0 / 3.14159265358979323846);
    deg = ((deg % 360) + 360) % 360;
    // Anything that is not a quarter turn is a shear or a scale we cannot
    // express, and guessing at one would rotate the capture by the wrong angle.
    if (deg % 90 != 0) return;
    info.rotate = deg;
    info.mirror = a * d - b * c < 0;
}

}  // namespace

// ================
// Parsing
// ================

bool Mp4Demuxer::open(const std::string& path, std::string& error) {
    path_ = path;
    file_.open(path, std::ios::binary);
    if (!file_) {
        error = "cannot open '" + path + "'";
        return false;
    }

    // Walk the top-level boxes looking for `moov`; it may follow `mdat`, which
    // is why we scan rather than assume.
    uint8_t header[16];
    uint64_t off = 0;
    std::vector<uint8_t> moov;
    while (true) {
        file_.clear();
        file_.seekg((std::streamoff)off, std::ios::beg);
        file_.read((char*)header, 8);
        if (file_.gcount() < 8) break;
        uint64_t size = rd32(header);
        const uint32_t type = rd32(header + 4);
        uint64_t hdr = 8;
        if (size == 1) {
            file_.read((char*)header + 8, 8);
            if (file_.gcount() < 8) break;
            size = rd64(header + 8);
            hdr = 16;
        }
        if (size < hdr) break;

        if (type == fourcc("moof")) {
            error = "'" + path +
                    "' is a fragmented MP4 (moof); this demuxer reads only "
                    "non-fragmented files. Remux with `mp4box -inter` or an "
                    "equivalent, or feed frames as images.";
            return false;
        }
        if (type == fourcc("moov")) {
            moov.resize((size_t)(size - hdr));
            file_.read((char*)moov.data(), (std::streamsize)moov.size());
            if ((uint64_t)file_.gcount() != (uint64_t)moov.size()) {
                error = "truncated moov box";
                return false;
            }
            break;
        }
        off += size;
    }
    if (moov.empty()) {
        error = "'" + path + "' has no moov box (not an MP4/MOV file?)";
        return false;
    }
    if (!parseMoov(moov.data(), moov.size(), error)) return false;
    if (tracks_.empty()) {
        error = "'" + path + "' has no supported video track (looked for avc1/hvc1/av01)";
        return false;
    }

    infos_.clear();
    for (size_t i = 0; i < tracks_.size(); ++i) {
        tracks_[i].info.index = (int)i;
        infos_.push_back(tracks_[i].info);
    }
    return selectTrack(0, error);
}

bool Mp4Demuxer::parseMoov(const uint8_t* data, size_t size, std::string& error) {
    size_t off = 0;
    while (off + 8 <= size) {
        Box b;
        if (!read_box(data + off, size - off, b)) break;
        if (b.type == fourcc("trak")) parseTrak(b.payload, b.payload_size);
        off += b.total_size;
    }
    (void)error;
    return true;
}

bool Mp4Demuxer::parseTrak(const uint8_t* data, size_t size) {
    Box mdia, minf, stbl, hdlr;
    if (!find_box(data, size, fourcc("mdia"), mdia)) return false;
    if (find_box(mdia.payload, mdia.payload_size, fourcc("hdlr"), hdlr)) {
        // handler type sits at payload+8 (version/flags, then pre_defined).
        if (hdlr.payload_size >= 12 && rd32(hdlr.payload + 8) != fourcc("vide"))
            return false;
    }

    Track tk;
    {
        // tkhd: version/flags (4) + times and ids (20 at v0, 32 at v1) +
        // reserved/layer/group/volume (16), then the 3x3 matrix.
        Box tkhd;
        if (find_box(data, size, fourcc("tkhd"), tkhd) && tkhd.payload_size >= 4) {
            const size_t off = 4 + (tkhd.payload[0] == 1 ? 32 : 20) + 16;
            if (tkhd.payload_size >= off + 36)
                parse_display_matrix(tkhd.payload + off, tk.info);
        }
    }
    {
        Box mdhd;
        if (find_box(mdia.payload, mdia.payload_size, fourcc("mdhd"), mdhd) &&
            mdhd.payload_size >= 20) {
            const uint8_t version = mdhd.payload[0];
            if (version == 1 && mdhd.payload_size >= 32) {
                tk.timescale = rd32(mdhd.payload + 20);
                tk.duration = rd64(mdhd.payload + 24);
            } else {
                tk.timescale = rd32(mdhd.payload + 12);
                tk.duration = rd32(mdhd.payload + 16);
            }
        }
    }
    if (!find_box(mdia.payload, mdia.payload_size, fourcc("minf"), minf)) return false;
    if (!find_box(minf.payload, minf.payload_size, fourcc("stbl"), stbl)) return false;

    // ---- sample description: codec + geometry + configuration record ----
    Box stsd;
    if (!find_box(stbl.payload, stbl.payload_size, fourcc("stsd"), stsd) ||
        stsd.payload_size <= 8)
        return false;
    {
        const uint8_t* entries = stsd.payload + 8;  // version/flags + entry count
        const size_t avail = stsd.payload_size - 8;
        Box entry;
        if (!read_box(entries, avail, entry)) return false;
        if (entry.type == fourcc("avc1") || entry.type == fourcc("avc3"))
            tk.info.codec = Codec::H264;
        else if (entry.type == fourcc("hvc1") || entry.type == fourcc("hev1"))
            tk.info.codec = Codec::H265;
        else if (entry.type == fourcc("av01"))
            tk.info.codec = Codec::AV1;
        else
            return false;

        if (entry.payload_size < 78) return false;
        // VisualSampleEntry: 6 reserved + 2 data_ref + 16 pre_defined, then
        // width/height at offset 24.
        tk.info.width = rd16(entry.payload + 24);
        tk.info.height = rd16(entry.payload + 26);
        const uint8_t* extra = entry.payload + 78;
        const size_t extra_size = entry.payload_size - 78;
        Box cfg;
        const uint32_t want = (tk.info.codec == Codec::H264)   ? fourcc("avcC")
                              : (tk.info.codec == Codec::H265) ? fourcc("hvcC")
                                                               : fourcc("av1C");
        if (find_box(extra, extra_size, want, cfg)) {
            tk.info.codec_config.assign(cfg.payload, cfg.payload + cfg.payload_size);
            if (tk.info.codec == Codec::H264 && cfg.payload_size >= 5)
                tk.info.nal_length_size = (cfg.payload[4] & 3) + 1;
            else if (tk.info.codec == Codec::H265 && cfg.payload_size >= 22)
                tk.info.nal_length_size = (cfg.payload[21] & 3) + 1;
            else if (tk.info.codec == Codec::AV1)
                tk.info.nal_length_size = 0;
        }
    }

    // ---- sample tables ----
    std::vector<uint32_t> stsz_sizes;
    std::vector<uint64_t> chunk_offsets;
    struct StscEntry { uint32_t first_chunk, samples_per_chunk; };
    std::vector<StscEntry> stsc_entries;
    struct SttsEntry { uint32_t count; int32_t delta; };
    std::vector<SttsEntry> stts_entries, ctts_entries;
    std::vector<uint32_t> sync_samples;
    bool has_stss = false;

    Box b;
    if (find_box(stbl.payload, stbl.payload_size, fourcc("stsz"), b) &&
        b.payload_size >= 12) {
        const uint32_t uniform = rd32(b.payload + 4);
        const uint32_t count = rd32(b.payload + 8);
        if (uniform == 0) {
            stsz_sizes.resize(count);
            for (uint32_t i = 0; i < count && 12 + 4 * (size_t)i + 4 <= b.payload_size; ++i)
                stsz_sizes[i] = rd32(b.payload + 12 + 4 * (size_t)i);
        } else {
            stsz_sizes.assign(count, uniform);
        }
    }
    if (find_box(stbl.payload, stbl.payload_size, fourcc("stco"), b) &&
        b.payload_size >= 8) {
        const uint32_t n = rd32(b.payload + 4);
        for (uint32_t i = 0; i < n && 8 + 4 * (size_t)i + 4 <= b.payload_size; ++i)
            chunk_offsets.push_back(rd32(b.payload + 8 + 4 * (size_t)i));
    } else if (find_box(stbl.payload, stbl.payload_size, fourcc("co64"), b) &&
               b.payload_size >= 8) {
        const uint32_t n = rd32(b.payload + 4);
        for (uint32_t i = 0; i < n && 8 + 8 * (size_t)i + 8 <= b.payload_size; ++i)
            chunk_offsets.push_back(rd64(b.payload + 8 + 8 * (size_t)i));
    }
    if (find_box(stbl.payload, stbl.payload_size, fourcc("stsc"), b) &&
        b.payload_size >= 8) {
        const uint32_t n = rd32(b.payload + 4);
        const uint8_t* e = b.payload + 8;
        for (uint32_t i = 0; i < n && 8 + 12 * (size_t)i + 12 <= b.payload_size; ++i)
            stsc_entries.push_back({rd32(e + 12 * (size_t)i), rd32(e + 12 * (size_t)i + 4)});
    }
    if (find_box(stbl.payload, stbl.payload_size, fourcc("stts"), b) &&
        b.payload_size >= 8) {
        const uint32_t n = rd32(b.payload + 4);
        const uint8_t* e = b.payload + 8;
        for (uint32_t i = 0; i < n && 8 + 8 * (size_t)i + 8 <= b.payload_size; ++i)
            stts_entries.push_back({rd32(e + 8 * (size_t)i), (int32_t)rd32(e + 8 * (size_t)i + 4)});
    }
    // Composition offsets: without these, PTS == DTS and a stream with B-frames
    // comes out in the wrong order.
    if (find_box(stbl.payload, stbl.payload_size, fourcc("ctts"), b) &&
        b.payload_size >= 8) {
        const uint32_t n = rd32(b.payload + 4);
        const uint8_t* e = b.payload + 8;
        for (uint32_t i = 0; i < n && 8 + 8 * (size_t)i + 8 <= b.payload_size; ++i)
            ctts_entries.push_back({rd32(e + 8 * (size_t)i), (int32_t)rd32(e + 8 * (size_t)i + 4)});
    }
    if (find_box(stbl.payload, stbl.payload_size, fourcc("stss"), b) &&
        b.payload_size >= 8) {
        has_stss = true;
        const uint32_t n = rd32(b.payload + 4);
        const uint8_t* e = b.payload + 8;
        for (uint32_t i = 0; i < n && 8 + 4 * (size_t)i + 4 <= b.payload_size; ++i)
            sync_samples.push_back(rd32(e + 4 * (size_t)i));
    }

    if (stsz_sizes.empty() || chunk_offsets.empty() || stsc_entries.empty()) return false;

    const size_t n_samples = stsz_sizes.size();
    tk.samples.resize(n_samples);

    // Walk chunks, distributing samples per the stsc run-length table.
    size_t sample = 0;
    for (size_t c = 0; c < chunk_offsets.size() && sample < n_samples; ++c) {
        uint32_t per_chunk = stsc_entries.back().samples_per_chunk;
        for (size_t e = 0; e < stsc_entries.size(); ++e) {
            if (stsc_entries[e].first_chunk > c + 1) {
                per_chunk = (e > 0) ? stsc_entries[e - 1].samples_per_chunk
                                    : stsc_entries[0].samples_per_chunk;
                break;
            }
            per_chunk = stsc_entries[e].samples_per_chunk;
        }
        uint64_t offset = chunk_offsets[c];
        for (uint32_t i = 0; i < per_chunk && sample < n_samples; ++i, ++sample) {
            tk.samples[sample].offset = offset;
            tk.samples[sample].size = stsz_sizes[sample];
            offset += stsz_sizes[sample];
        }
    }

    int64_t dts = 0;
    size_t idx = 0;
    for (const auto& e : stts_entries)
        for (uint32_t i = 0; i < e.count && idx < n_samples; ++i, ++idx) {
            tk.samples[idx].dts = dts;
            tk.samples[idx].pts = dts;
            dts += e.delta;
        }
    idx = 0;
    for (const auto& e : ctts_entries)
        for (uint32_t i = 0; i < e.count && idx < n_samples; ++i, ++idx)
            tk.samples[idx].pts = tk.samples[idx].dts + e.delta;

    if (tk.duration == 0) tk.duration = (uint64_t)dts;

    if (has_stss) {
        for (uint32_t s : sync_samples)
            if (s >= 1 && s <= n_samples) tk.samples[s - 1].is_sync = true;
    } else {
        for (auto& s : tk.samples) s.is_sync = true;  // all-intra
    }

    tk.info.frame_count = (int64_t)n_samples;
    if (tk.timescale) {
        tk.info.duration_sec = (double)tk.duration / (double)tk.timescale;
        if (tk.duration) tk.info.fps = (double)n_samples / tk.info.duration_sec;
    }
    tracks_.push_back(std::move(tk));
    return true;
}

bool Mp4Demuxer::selectTrack(int index, std::string& error) {
    if (index < 0 || index >= (int)tracks_.size()) {
        error = "video track index out of range";
        return false;
    }
    selected_ = index;
    next_sample_ = 0;
    buildPresentationOrder(tracks_[(size_t)index]);
    return true;
}

void Mp4Demuxer::buildPresentationOrder(Track& tk) {
    if (!tk.by_pts.empty() || tk.samples.empty()) return;
    tk.by_pts.resize(tk.samples.size());
    for (uint32_t i = 0; i < (uint32_t)tk.samples.size(); i++) tk.by_pts[i] = i;
    std::stable_sort(tk.by_pts.begin(), tk.by_pts.end(),
                     [&tk](uint32_t a, uint32_t b) {
                         return tk.samples[a].pts < tk.samples[b].pts;
                     });
    tk.pts_rank.resize(tk.samples.size());
    for (uint32_t r = 0; r < (uint32_t)tk.by_pts.size(); r++)
        tk.pts_rank[tk.by_pts[r]] = r;
}

bool Mp4Demuxer::seekSync(int64_t index, int64_t& landed, std::string& error) {
    if (selected_ < 0) {
        error = "no track selected";
        return false;
    }
    Track& tk = tracks_[(size_t)selected_];
    if (tk.samples.empty()) return false;
    buildPresentationOrder(tk);
    int64_t r = std::min(index, (int64_t)tk.by_pts.size() - 1);
    while (r > 0 && !tk.samples[tk.by_pts[(size_t)r]].is_sync) --r;
    if (!tk.samples[tk.by_pts[(size_t)r]].is_sync) return false;
    next_sample_ = tk.by_pts[(size_t)r];
    landed = r;
    return true;
}

bool Mp4Demuxer::next(Packet& out, std::string& error) {
    const Track& tk = tracks_[(size_t)selected_];
    if (next_sample_ >= tk.samples.size()) return false;  // end of stream
    const Sample& s = tk.samples[next_sample_];

    out.data.resize(s.size);
    file_.clear();
    file_.seekg((std::streamoff)s.offset, std::ios::beg);
    file_.read((char*)out.data.data(), s.size);
    if ((uint32_t)file_.gcount() != s.size) {
        error = "truncated sample data at sample " + std::to_string(next_sample_);
        return false;
    }
    const double ts = tk.timescale ? (double)tk.timescale : 1.0;
    out.index = (int64_t)next_sample_;
    out.display_index = next_sample_ < tk.pts_rank.size()
                            ? (int64_t)tk.pts_rank[next_sample_]
                            : (int64_t)next_sample_;
    out.dts = (double)s.dts / ts;
    out.pts = (double)s.pts / ts;
    out.is_sync = s.is_sync;
    ++next_sample_;
    return true;
}

}  // namespace video
