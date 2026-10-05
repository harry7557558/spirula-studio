// Heif.cpp -- see Heif.h. decode_heif() and probe_heif() are Video.h's.

#include "video/Heif.h"

#include "core/ImageOrient.h"
#include "video/Demuxer.h"
#include "video/Video.h"
#include "video/VideoPipeline.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>

namespace video {

namespace {

constexpr uint32_t fourcc(const char s[5]) {
    return ((uint32_t)(uint8_t)s[0] << 24) | ((uint32_t)(uint8_t)s[1] << 16) |
           ((uint32_t)(uint8_t)s[2] << 8) | (uint32_t)(uint8_t)s[3];
}

std::string fourcc_str(uint32_t t) {
    std::string s(4, ' ');
    for (int i = 0; i < 4; i++) {
        const char c = (char)(t >> (24 - 8 * i));
        s[(size_t)i] = c >= 0x20 && c < 0x7f ? c : '?';
    }
    return s;
}

// Big-endian reads over one box's payload. Running off the end latches `bad`
// and reads zeros, so a box is checked once rather than per field.
struct Span {
    const uint8_t* p = nullptr;
    size_t n = 0, at = 0;
    bool bad = false;

    uint64_t be(int bytes) {
        if (bytes <= 0) return 0;
        if (at + (size_t)bytes > n) {
            bad = true;
            at = n;
            return 0;
        }
        uint64_t v = 0;
        for (int i = 0; i < bytes; i++) v = v << 8 | p[at++];
        return v;
    }
    uint8_t  u8() { return (uint8_t)be(1); }
    uint16_t u16() { return (uint16_t)be(2); }
    uint32_t u32() { return (uint32_t)be(4); }
    void skip(size_t k) {
        if (at + k > n) bad = true;
        at = std::min(at + k, n);
    }
};

struct Box {
    uint32_t type = 0;
    Span body;
};

bool children(const uint8_t* p, size_t n, std::vector<Box>& out) {
    size_t at = 0;
    while (at + 8 <= n) {
        Span h{p + at, n - at};
        uint64_t size = h.u32();
        const uint32_t type = h.u32();
        size_t hdr = 8;
        if (size == 1) {
            size = h.be(8);
            hdr = 16;
        } else if (size == 0) {
            size = n - at;
        }
        if (h.bad || size < hdr || size > n - at) return false;
        out.push_back({type, Span{p + at + hdr, (size_t)size - hdr}});
        at += (size_t)size;
    }
    return true;
}

struct Extent {
    uint64_t offset = 0, length = 0;
};

struct Item {
    uint32_t type = 0;
    bool protected_ = false;
    int method = -1;                 // iloc construction_method; -1 = not located
    uint64_t base = 0;
    std::vector<Extent> extents;
    struct Prop {
        uint16_t index = 0;          // into ipco, 1-based
        bool essential = false;
    };
    std::vector<Prop> props;
};

struct Ref {
    uint32_t type = 0, from = 0;
    std::vector<uint32_t> to;
};

// Properties that change nothing about the pixels decode_heif() produces, or
// that it applies itself. An essential property outside this list means the
// file wants something done that is not done here.
bool understood(uint32_t t) {
    for (const char* k : {"hvcC", "ispe", "irot", "imir", "clap", "colr", "pixi",
                          "auxC", "pasp", "clli", "mdcv", "cclv", "amve", "rloc"})
        if (t == fourcc(k)) return true;
    return false;
}

constexpr uint64_t kMaxMeta = 64ull << 20;
constexpr uint64_t kMaxItem = 512ull << 20;
constexpr uint64_t kMaxPixels = 1ull << 28;

class Parser {
public:
    Parser(const HeifRead& read, uint64_t file_size) : read_(read), size_(file_size) {}

    bool run(bool with_tiles, HeifImage& out, std::string& error);

private:
    bool top_level(std::string& error);
    bool parse_meta(std::string& error);
    bool parse_iloc(Span s, std::string& error);
    bool parse_iprp(Span s, std::string& error);
    bool item_data(uint32_t id, std::vector<uint8_t>& out, std::string& error) const;
    const Item* item(uint32_t id) const;
    std::vector<uint32_t> refs(uint32_t type, uint32_t from) const;
    const Box* prop(const Item& it, uint32_t type) const;

    const HeifRead& read_;
    uint64_t size_;
    std::vector<uint8_t> meta_;
    Span idat_;
    uint32_t primary_ = 0;
    bool have_primary_ = false;
    std::map<uint32_t, Item> items_;
    std::vector<Ref> refs_;
    std::vector<Box> ipco_;
};

bool Parser::top_level(std::string& error) {
    uint64_t at = 0;
    bool first = true;
    while (at + 8 <= size_) {
        uint8_t h[16];
        if (!read_(at, 8, h)) break;
        uint64_t box = ((uint64_t)h[0] << 24) | ((uint64_t)h[1] << 16) |
                       ((uint64_t)h[2] << 8) | h[3];
        const uint32_t type = ((uint32_t)h[4] << 24) | ((uint32_t)h[5] << 16) |
                              ((uint32_t)h[6] << 8) | h[7];
        uint64_t hdr = 8;
        if (box == 1) {
            if (!read_(at + 8, 8, h + 8)) break;
            box = 0;
            for (int i = 8; i < 16; i++) box = box << 8 | h[i];
            hdr = 16;
        } else if (box == 0) {
            box = size_ - at;
        }
        if (box < hdr || box > size_ - at) {
            error = "truncated '" + fourcc_str(type) + "' box";
            return false;
        }
        if (first) {
            if (type != fourcc("ftyp") || box - hdr < 8 || box - hdr > 4096) {
                error = "not a HEIF file (no ftyp box first)";
                return false;
            }
            std::vector<uint8_t> f((size_t)(box - hdr));
            if (!read_(at + hdr, f.size(), f.data())) break;
            bool heif = false;
            for (size_t i = 0; i + 4 <= f.size(); i += 4) {
                if (i == 4) continue;   // minor_version
                const uint32_t b = ((uint32_t)f[i] << 24) | ((uint32_t)f[i + 1] << 16) |
                                   ((uint32_t)f[i + 2] << 8) | f[i + 3];
                for (const char* k : {"mif1", "mif2", "miaf", "heic", "heix", "heim",
                                      "heis"})
                    heif = heif || b == fourcc(k);
            }
            if (!heif) {
                error = "not a HEIF image: no HEIF brand in ftyp";
                return false;
            }
            first = false;
        } else if (type == fourcc("meta")) {
            if (box - hdr > kMaxMeta) {
                error = "meta box too large";
                return false;
            }
            meta_.resize((size_t)(box - hdr));
            if (!read_(at + hdr, meta_.size(), meta_.data())) {
                error = "truncated meta box";
                return false;
            }
            return true;
        }
        at += box;
    }
    error = first ? "not a HEIF file" : "HEIF file has no meta box";
    return false;
}

bool Parser::parse_iloc(Span s, std::string& error) {
    const uint8_t version = s.u8();
    s.skip(3);
    if (version > 2) {
        error = "iloc version " + std::to_string(version) + " is not supported";
        return false;
    }
    const uint8_t a = s.u8(), b = s.u8();
    const int offset_size = a >> 4, length_size = a & 15, base_size = b >> 4;
    const int index_size = version >= 1 ? (b & 15) : 0;
    const uint32_t count = version < 2 ? s.u16() : s.u32();
    for (uint32_t i = 0; i < count && !s.bad; i++) {
        const uint32_t id = version < 2 ? s.u16() : s.u32();
        int method = 0;
        if (version >= 1) method = s.u16() & 15;
        const uint16_t data_ref = s.u16();
        const uint64_t base = s.be(base_size);
        const uint16_t n_ext = s.u16();
        std::vector<Extent> ext;
        for (uint16_t e = 0; e < n_ext && !s.bad; e++) {
            s.be(index_size);
            Extent x;
            x.offset = s.be(offset_size);
            x.length = s.be(length_size);
            ext.push_back(x);
        }
        auto it = items_.find(id);
        if (it != items_.end()) {
            // Another file (data_reference_index) is as unreadable as an
            // unknown method, and says so the same way.
            it->second.method = data_ref != 0 ? 99 : method;
            it->second.base = base;
            it->second.extents = std::move(ext);
        }
    }
    if (s.bad) error = "truncated iloc box";
    return !s.bad;
}

bool Parser::parse_iprp(Span s, std::string& error) {
    std::vector<Box> kids;
    if (!children(s.p, s.n, kids)) {
        error = "malformed iprp box";
        return false;
    }
    for (const Box& k : kids)
        if (k.type == fourcc("ipco") && !children(k.body.p, k.body.n, ipco_)) {
            error = "malformed ipco box";
            return false;
        }
    for (Box k : kids) {
        if (k.type != fourcc("ipma")) continue;
        Span& m = k.body;
        const uint8_t version = m.u8();
        const uint32_t flags = (uint32_t)m.be(3);
        const uint32_t count = m.u32();
        for (uint32_t i = 0; i < count && !m.bad; i++) {
            const uint32_t id = version < 1 ? m.u16() : m.u32();
            const uint8_t n = m.u8();
            std::vector<Item::Prop> props;
            for (uint8_t j = 0; j < n && !m.bad; j++) {
                Item::Prop pr;
                if (flags & 1) {
                    const uint16_t v = m.u16();
                    pr.essential = (v >> 15) != 0;
                    pr.index = v & 0x7fff;
                } else {
                    const uint8_t v = m.u8();
                    pr.essential = (v >> 7) != 0;
                    pr.index = v & 0x7f;
                }
                props.push_back(pr);
            }
            auto it = items_.find(id);
            if (it != items_.end())
                it->second.props.insert(it->second.props.end(), props.begin(), props.end());
        }
        if (m.bad) {
            error = "truncated ipma box";
            return false;
        }
    }
    return true;
}

bool Parser::parse_meta(std::string& error) {
    std::vector<Box> kids;
    if (meta_.size() < 4 || !children(meta_.data() + 4, meta_.size() - 4, kids)) {
        error = "malformed meta box";
        return false;
    }
    bool pict = false;
    for (Box k : kids) {
        Span& s = k.body;
        if (k.type == fourcc("hdlr")) {
            s.skip(8);
            pict = s.u32() == fourcc("pict");
        } else if (k.type == fourcc("pitm")) {
            const uint8_t version = s.u8();
            s.skip(3);
            primary_ = version == 0 ? s.u16() : s.u32();
            have_primary_ = !s.bad;
        } else if (k.type == fourcc("iinf")) {
            const uint8_t version = s.u8();
            s.skip(3);
            s.skip(version == 0 ? 2 : 4);
            std::vector<Box> infe;
            if (s.bad || !children(s.p + s.at, s.n - s.at, infe)) {
                error = "malformed iinf box";
                return false;
            }
            for (Box e : infe) {
                if (e.type != fourcc("infe")) continue;
                Span& f = e.body;
                const uint8_t v = f.u8();
                f.skip(3);
                if (v < 2) continue;   // no item_type before version 2
                const uint32_t id = v == 2 ? f.u16() : f.u32();
                Item it;
                it.protected_ = f.u16() != 0;
                it.type = f.u32();
                if (!f.bad) items_[id] = std::move(it);
            }
        } else if (k.type == fourcc("iref")) {
            const uint8_t version = s.u8();
            s.skip(3);
            std::vector<Box> rs;
            if (s.bad || !children(s.p + s.at, s.n - s.at, rs)) {
                error = "malformed iref box";
                return false;
            }
            const int w = version == 0 ? 2 : 4;
            for (Box r : rs) {
                Ref ref;
                ref.type = r.type;
                ref.from = (uint32_t)r.body.be(w);
                const uint16_t n = r.body.u16();
                for (uint16_t i = 0; i < n; i++) ref.to.push_back((uint32_t)r.body.be(w));
                if (!r.body.bad) refs_.push_back(std::move(ref));
            }
        } else if (k.type == fourcc("idat")) {
            idat_ = s;
        }
    }
    // After iinf whatever the box order: iloc and ipma attach to known items.
    for (const Box& k : kids) {
        if (k.type == fourcc("iloc") && !parse_iloc(k.body, error)) return false;
        if (k.type == fourcc("iprp") && !parse_iprp(k.body, error)) return false;
    }
    if (!pict) {
        error = "HEIF meta box is not an image ('pict') handler";
        return false;
    }
    if (!have_primary_) {
        error = "HEIF file names no primary item";
        return false;
    }
    return true;
}

const Item* Parser::item(uint32_t id) const {
    auto it = items_.find(id);
    return it == items_.end() ? nullptr : &it->second;
}

std::vector<uint32_t> Parser::refs(uint32_t type, uint32_t from) const {
    for (const Ref& r : refs_)
        if (r.type == type && r.from == from) return r.to;
    return {};
}

const Box* Parser::prop(const Item& it, uint32_t type) const {
    for (const Item::Prop& p : it.props)
        if (p.index >= 1 && p.index <= ipco_.size() && ipco_[p.index - 1].type == type)
            return &ipco_[p.index - 1];
    return nullptr;
}

bool Parser::item_data(uint32_t id, std::vector<uint8_t>& out,
                       std::string& error) const {
    out.clear();
    const Item* it = item(id);
    if (!it || it->method < 0) {
        error = "HEIF item " + std::to_string(id) + " has no location";
        return false;
    }
    if (it->method > 1) {
        error = "HEIF item " + std::to_string(id) +
                " is stored in a way this reader does not follow (construction "
                "method or external file)";
        return false;
    }
    for (const Extent& e : it->extents) {
        const uint64_t limit = it->method == 0 ? size_ : (uint64_t)idat_.n;
        const uint64_t off = it->base + e.offset;
        const uint64_t len = e.length ? e.length : (off < limit ? limit - off : 0);
        if (off > limit || len > limit - off || out.size() + len > kMaxItem) {
            error = "HEIF item " + std::to_string(id) + " lies outside the file";
            return false;
        }
        const size_t at = out.size();
        out.resize(at + (size_t)len);
        if (it->method == 1) {
            std::memcpy(out.data() + at, idat_.p + off, (size_t)len);
        } else if (!read_(off, (size_t)len, out.data() + at)) {
            error = "HEIF item " + std::to_string(id) + " could not be read";
            return false;
        }
    }
    return true;
}

bool Parser::run(bool with_tiles, HeifImage& out, std::string& error) {
    out = HeifImage{};
    if (!top_level(error) || !parse_meta(error)) return false;

    uint32_t id = primary_;
    const Item* img = item(id);
    // An ISO 21496-1 gain-map image: its first input is the base picture.
    if (img && img->type == fourcc("tmap")) {
        const std::vector<uint32_t> in = refs(fourcc("dimg"), id);
        if (!in.empty()) {
            id = in[0];
            img = item(id);
        }
    }
    if (!img) {
        error = "HEIF primary item " + std::to_string(id) + " is not described";
        return false;
    }
    if (img->protected_) {
        error = "HEIF primary image is encrypted";
        return false;
    }

    std::vector<uint32_t> tile_ids;
    if (img->type == fourcc("grid")) {
        std::vector<uint8_t> g;
        if (!item_data(id, g, error)) return false;
        Span s{g.data(), g.size()};
        s.u8();   // version
        const uint8_t flags = s.u8();
        out.rows = s.u8() + 1;
        out.cols = s.u8() + 1;
        const int w = (flags & 1) ? 4 : 2;
        out.width = (int)std::min<uint64_t>(s.be(w), INT32_MAX);
        out.height = (int)std::min<uint64_t>(s.be(w), INT32_MAX);
        if (s.bad) {
            error = "truncated HEIF grid descriptor";
            return false;
        }
        tile_ids = refs(fourcc("dimg"), id);
        if ((int)tile_ids.size() != out.rows * out.cols) {
            error = "HEIF grid of " + std::to_string(out.cols) + "x" +
                    std::to_string(out.rows) + " names " +
                    std::to_string(tile_ids.size()) + " tiles";
            return false;
        }
    } else if (img->type == fourcc("hvc1")) {
        tile_ids = {id};
    } else {
        error = "HEIF primary image is of type '" + fourcc_str(img->type) +
                "'; only H.265 ('hvc1') and grids of it are decoded here";
        return false;
    }

    for (size_t i = 0; i < tile_ids.size(); i++) {
        const Item* t = item(tile_ids[i]);
        if (!t || t->type != fourcc("hvc1") || t->protected_) {
            error = "HEIF tile " + std::to_string(tile_ids[i]) + " is not an H.265 image";
            return false;
        }
        // A lone coded image is its own tile, and its turns are applied below.
        for (const Item::Prop& p : t->props) {
            if (tile_ids[i] == id) break;
            if (!p.essential || p.index == 0 || p.index > ipco_.size()) continue;
            const uint32_t pt = ipco_[p.index - 1].type;
            if (!understood(pt) || pt == fourcc("irot") || pt == fourcc("imir") ||
                pt == fourcc("clap")) {
                error = "HEIF tile carries an essential '" + fourcc_str(pt) +
                        "' property, which this reader does not apply";
                return false;
            }
        }
        const Box* hv = prop(*t, fourcc("hvcC"));
        if (!hv || hv->body.n < 23) {
            error = "HEIF tile " + std::to_string(tile_ids[i]) + " has no hvcC";
            return false;
        }
        std::vector<uint8_t> cfg(hv->body.p, hv->body.p + hv->body.n);
        int tw = 0, th = 0;
        if (const Box* ispe = prop(*t, fourcc("ispe"))) {
            Span s = ispe->body;
            s.skip(4);
            tw = (int)std::min<uint32_t>(s.u32(), INT32_MAX);
            th = (int)std::min<uint32_t>(s.u32(), INT32_MAX);
        }
        if (i == 0) {
            out.hvcc = std::move(cfg);
            out.tile_width = tw;
            out.tile_height = th;
        } else if (cfg != out.hvcc || tw != out.tile_width || th != out.tile_height) {
            error = "HEIF grid tiles differ in size or decoder configuration";
            return false;
        }
    }
    if (img->type == fourcc("hvc1")) {
        out.width = out.tile_width;
        out.height = out.tile_height;
    }
    if (out.width <= 0 || out.height <= 0) {
        error = "HEIF image has no size (no ispe)";
        return false;
    }
    if ((uint64_t)out.width * (uint64_t)out.height > kMaxPixels) {
        error = "HEIF image of " + std::to_string(out.width) + "x" +
                std::to_string(out.height) + " is larger than this reader takes";
        return false;
    }
    if (img->type == fourcc("grid") && out.tile_width > 0 &&
        ((int64_t)out.tile_width * out.cols < out.width ||
         (int64_t)out.tile_height * out.rows < out.height)) {
        error = "HEIF grid tiles do not cover its output size";
        return false;
    }

    int cw = out.width, ch = out.height;
    for (const Item::Prop& p : img->props) {
        if (p.index == 0 || p.index > ipco_.size()) continue;
        const Box& b = ipco_[p.index - 1];
        Span s = b.body;
        if (b.type == fourcc("irot")) {
            HeifOp op;
            op.kind = HeifOp::Rotate;
            op.ccw = s.u8() & 3;
            if (op.ccw & 1) std::swap(cw, ch);
            out.ops.push_back(op);
        } else if (b.type == fourcc("imir")) {
            HeifOp op;
            op.kind = HeifOp::Mirror;
            op.left_right = (s.u8() & 1) != 0;
            out.ops.push_back(op);
        } else if (b.type == fourcc("clap")) {
            const double wn = s.u32(), wd = s.u32(), hn = s.u32(), hd = s.u32();
            const double xn = (int32_t)s.u32(), xd = s.u32();
            const double yn = (int32_t)s.u32(), yd = s.u32();
            if (s.bad || wd == 0 || hd == 0 || xd == 0 || yd == 0) {
                error = "malformed HEIF clap property";
                return false;
            }
            // ISO/IEC 14496-12 12.1.4: the aperture is centred on
            // (w-1)/2 + horizOff, and its first column rounds down.
            const double aw = wn / wd, ah = hn / hd;
            const double left = std::floor(xn / xd + (cw - 1) / 2.0 - (aw - 1) / 2.0);
            const double top = std::floor(yn / yd + (ch - 1) / 2.0 - (ah - 1) / 2.0);
            HeifOp op;
            op.kind = HeifOp::Crop;
            op.x = (int)std::clamp(left, 0.0, (double)cw - 1);
            op.y = (int)std::clamp(top, 0.0, (double)ch - 1);
            op.w = std::clamp((int)std::lround(aw), 1, cw - op.x);
            op.h = std::clamp((int)std::lround(ah), 1, ch - op.y);
            cw = op.w;
            ch = op.h;
            out.ops.push_back(op);
        } else if (b.type == fourcc("colr")) {
            if (s.u32() == fourcc("nclx")) {
                s.u16();   // colour_primaries
                s.u16();   // transfer_characteristics
                out.matrix_coefficients = s.u16();
                out.full_range = (s.u8() & 0x80) != 0;
                out.has_nclx = !s.bad;
            }
        } else if (p.essential && !understood(b.type)) {
            error = "HEIF image carries an essential '" + fourcc_str(b.type) +
                    "' property, which this reader does not apply";
            return false;
        }
    }
    // The grid's own colr is the one that counts; a tile's stands in for none.
    if (!out.has_nclx && !tile_ids.empty())
        if (const Box* c = prop(*item(tile_ids[0]), fourcc("colr"))) {
            Span s = c->body;
            if (s.u32() == fourcc("nclx")) {
                s.skip(4);
                out.matrix_coefficients = s.u16();
                out.full_range = (s.u8() & 0x80) != 0;
                out.has_nclx = !s.bad;
            }
        }

    // The EXIF that describes this image, else the file's only one.
    uint32_t exif_id = 0;
    bool found = false;
    for (const Ref& r : refs_) {
        const Item* e = item(r.from);
        if (r.type != fourcc("cdsc") || !e || e->type != fourcc("Exif")) continue;
        if (std::find(r.to.begin(), r.to.end(), id) != r.to.end() ||
            std::find(r.to.begin(), r.to.end(), primary_) != r.to.end()) {
            exif_id = r.from;
            found = true;
            break;
        }
    }
    if (!found)
        for (const auto& [iid, it] : items_)
            if (it.type == fourcc("Exif")) {
                exif_id = iid;
                found = true;
                break;
            }
    std::vector<uint8_t> ex;
    std::string exif_error;
    if (found && item_data(exif_id, ex, exif_error) && ex.size() > 4) {
        // A 4-byte offset to the TIFF header, which is usually 6: past a
        // JPEG-style "Exif\0\0" the writer kept.
        const uint64_t off = 4 + (((uint64_t)ex[0] << 24) | ((uint64_t)ex[1] << 16) |
                                  ((uint64_t)ex[2] << 8) | ex[3]);
        if (off + 8 <= ex.size() &&
            (std::memcmp(ex.data() + off, "II*\0", 4) == 0 ||
             std::memcmp(ex.data() + off, "MM\0*", 4) == 0)) {
            static const uint8_t kSig[6] = {'E', 'x', 'i', 'f', 0, 0};
            out.exif.assign(kSig, kSig + 6);
            out.exif.insert(out.exif.end(), ex.begin() + (ptrdiff_t)off, ex.end());
        }
    }

    if (with_tiles) {
        out.tiles.resize(tile_ids.size());
        for (size_t i = 0; i < tile_ids.size(); i++)
            if (!item_data(tile_ids[i], out.tiles[i], error)) return false;
    }
    return true;
}

// A HEIF image's tiles as a stream of intra pictures, which is how
// VideoPipeline decodes them.
class TileDemuxer : public Demuxer {
public:
    TileDemuxer(const TrackInfo& t, std::vector<std::vector<uint8_t>> tiles)
        : tracks_{t}, tiles_(std::move(tiles)) {}

    const std::vector<TrackInfo>& tracks() const override { return tracks_; }
    bool selectTrack(int index, std::string& error) override {
        if (index != 0) {
            error = "a HEIF image is one track";
            return false;
        }
        next_ = 0;
        return true;
    }
    bool next(Packet& out, std::string& error) override {
        error.clear();
        if (next_ >= tiles_.size()) return false;
        out.data = tiles_[next_];
        out.index = out.display_index = (int64_t)next_;
        out.pts = out.dts = 0.0;
        out.is_sync = true;
        ++next_;
        return true;
    }

private:
    std::vector<TrackInfo> tracks_;
    std::vector<std::vector<uint8_t>> tiles_;
    size_t next_ = 0;
};

}  // namespace

bool parse_heif(const HeifRead& read, uint64_t file_size, bool with_tiles,
                HeifImage& out, std::string& error) {
    Parser p(read, file_size);
    return p.run(with_tiles, out, error);
}

bool read_heif(const std::string& path, bool with_tiles, HeifImage& out,
               std::string& error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        error = "cannot open '" + path + "'";
        return false;
    }
    f.seekg(0, std::ios::end);
    const uint64_t size = (uint64_t)f.tellg();
    const HeifRead read = [&f, size](uint64_t off, size_t n, uint8_t* dst) {
        if (off > size || n > size - off) return false;
        f.clear();
        f.seekg((std::streamoff)off);
        f.read((char*)dst, (std::streamsize)n);
        return (bool)f;
    };
    if (!parse_heif(read, size, with_tiles, out, error)) {
        error = "'" + path + "': " + error;
        return false;
    }
    return true;
}

void heif_apply_ops(const std::vector<HeifOp>& ops, std::vector<uint8_t>& rgb, int& w,
                    int& h) {
    std::vector<uint8_t> tmp;
    for (const HeifOp& op : ops) {
        if (op.kind == HeifOp::Crop) {
            tmp.resize((size_t)op.w * op.h * 3);
            for (int y = 0; y < op.h; y++)
                std::memcpy(tmp.data() + (size_t)y * op.w * 3,
                            rgb.data() + (((size_t)(op.y + y) * w) + op.x) * 3,
                            (size_t)op.w * 3);
            w = op.w;
            h = op.h;
        } else {
            // A top-bottom flip is a half turn and then the left-right one.
            const int cw = op.kind == HeifOp::Rotate ? (4 - op.ccw) & 3
                                                     : (op.left_right ? 0 : 2);
            const bool mirror = op.kind == HeifOp::Mirror;
            if (cw == 0 && !mirror) continue;
            tmp.resize(rgb.size());
            spirula::orient_pixels(rgb.data(), w, h, 3, cw, mirror, tmp.data());
            spirula::oriented_size(cw, w, h);
        }
        rgb.swap(tmp);
    }
}

void heif_display_size(const HeifImage& im, int& w, int& h) {
    w = im.width;
    h = im.height;
    for (const HeifOp& op : im.ops) {
        if (op.kind == HeifOp::Crop) {
            w = op.w;
            h = op.h;
        } else if (op.kind == HeifOp::Rotate && (op.ccw & 1)) {
            std::swap(w, h);
        }
    }
}

bool probe_heif(const std::string& path, int& width, int& height,
                std::string& error) {
    HeifImage im;
    if (!read_heif(path, false, im, error)) return false;
    heif_display_size(im, width, height);
    return true;
}

bool decode_heif(const std::string& path, nn::Image& out, std::vector<uint8_t>* exif,
                 std::string& error) {
    HeifImage im;
    if (!read_heif(path, true, im, error)) return false;

    TrackInfo t;
    t.codec = Codec::H265;
    t.width = im.tile_width;
    t.height = im.tile_height;
    t.frame_count = (int64_t)im.tiles.size();
    t.nal_length_size = (im.hvcc[21] & 3) + 1;
    t.codec_config = im.hvcc;
    const size_t n_tiles = im.tiles.size();

    auto named = [&]() {
        if (error.find(path) == std::string::npos) error = "'" + path + "': " + error;
        return false;
    };
    // One device queue and one compute stream serve every caller.
    static std::mutex mu;
    std::lock_guard<std::mutex> lock(mu);
    VideoPipeline pipe;
    if (!pipe.open(std::make_unique<TileDemuxer>(t, std::move(im.tiles)), path, 0, 1,
                   error))
        return named();

    ConvertOpts co;
    const StreamFormat& f = pipe.format();
    // Unspecified is BT.601, as libheif and ffmpeg's swscale read it; the
    // pipeline's own fallback guesses from the height, a tile's here.
    co.matrix_coefficients = im.has_nclx ? im.matrix_coefficients : f.matrix_coefficients;
    if (co.matrix_coefficients == 2) co.matrix_coefficients = 6;
    co.full_range = (im.has_nclx ? im.full_range : f.full_range) ? 1 : 0;
    if (co.matrix_coefficients == 0) {
        error = "'" + path + "' is coded as RGB (matrix_coefficients 0), which is not "
                "converted here";
        return false;
    }

    out = nn::Image{};
    out.width = im.width;
    out.height = im.height;
    out.channels = 3;
    out.data.assign((size_t)im.width * im.height * 3, 0);
    size_t got = 0;
    nn::Image tile;
    for (;;) {
        FrameHandle h;
        if (!pipe.next(h, error)) {
            if (!error.empty()) return named();
            break;
        }
        const bool ok = pipe.toImage(h, co, tile, error);
        pipe.release(h);
        if (!ok) return named();
        if ((im.tile_width > 0 && tile.width != im.tile_width) ||
            (im.tile_height > 0 && tile.height != im.tile_height)) {
            error = "'" + path + "': a tile decoded to " + std::to_string(tile.width) +
                    "x" + std::to_string(tile.height) + ", not the " +
                    std::to_string(im.tile_width) + "x" + std::to_string(im.tile_height) +
                    " its container states";
            return false;
        }
        const int64_t k = h.index;
        if (k < 0 || k >= (int64_t)n_tiles) continue;
        const int x0 = (int)(k % im.cols) * tile.width;
        const int y0 = (int)(k / im.cols) * tile.height;
        const int cw = std::min(tile.width, im.width - x0);
        const int rows = std::min(tile.height, im.height - y0);
        for (int y = 0; cw > 0 && y < rows; y++)
            std::memcpy(out.data.data() + (((size_t)(y0 + y) * im.width) + x0) * 3,
                        tile.data.data() + (size_t)y * tile.width * 3, (size_t)cw * 3);
        ++got;
    }
    if (got != n_tiles) {
        error = "'" + path + "': decoded " + std::to_string(got) + " of " +
                std::to_string(n_tiles) + " tiles";
        return false;
    }
    heif_apply_ops(im.ops, out.data, out.width, out.height);
    if (exif) *exif = std::move(im.exif);
    return true;
}

}  // namespace video
