// heif_test -- the HEIF container read on a file built here (no device), then
// a round trip on the GPU: tiles encoded by VideoEncoder, wrapped in that
// container, read back by decode_heif. `heif_test FILE.heic OUT.png` decodes
// one real file instead.

#include "external/stb_image_write.h"
#include "nn/core/Log.h"
#include "nn/vk/Context.h"
#include "video/Heif.h"
#include "video/Mp4Writer.h"
#include "video/Video.h"
#include "video/VideoEncoder.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

// ================
// A HEIF writer, just enough for the reader
// ================

struct Buf {
    std::vector<uint8_t> b;
    void u8(uint32_t v) { b.push_back((uint8_t)v); }
    void u16(uint32_t v) { u8(v >> 8); u8(v); }
    void u32(uint32_t v) { u16(v >> 16); u16(v); }
    void cc(const char* s) { b.insert(b.end(), s, s + 4); }
    void bytes(const std::vector<uint8_t>& v) { b.insert(b.end(), v.begin(), v.end()); }
    size_t open(const char* type) {
        const size_t at = b.size();
        u32(0);
        cc(type);
        return at;
    }
    size_t open_full(const char* type, int version, uint32_t flags = 0) {
        const size_t at = open(type);
        u8((uint32_t)version);
        u8(flags >> 16); u8(flags >> 8); u8(flags);
        return at;
    }
    void close(size_t at) {
        const uint32_t n = (uint32_t)(b.size() - at);
        for (int i = 0; i < 4; i++) b[at + (size_t)i] = (uint8_t)(n >> (24 - 8 * i));
    }
};

struct Spec {
    int cols = 3, rows = 2, tw = 256, th = 160;
    int out_w = 740, out_h = 300;
    std::vector<uint8_t> hvcc;
    std::vector<std::vector<uint8_t>> tiles;
    std::vector<uint8_t> tiff;            // the EXIF item's TIFF block
    bool unknown_essential = false;       // add an essential property nobody reads
    bool single = false;                  // item 1 alone, turned by an essential irot
    const char* brand = "heic";
};

// Item 100 is a grid of items 1..N, 200 its EXIF. The grid carries clap, then
// irot 1, then imir 1, then an nclx colr: BT.709, studio range.
std::vector<uint8_t> build_heif(const Spec& s) {
    const int n = s.single ? 1 : s.cols * s.rows;
    const uint32_t primary = s.single ? 1 : 100;
    std::vector<uint8_t> exif_item;
    {
        Buf e;
        e.u32(6);
        e.b.insert(e.b.end(), {'E', 'x', 'i', 'f', 0, 0});
        e.bytes(s.tiff);
        exif_item = e.b;
    }
    auto build = [&](uint32_t mdat_payload) {
        Buf f;
        size_t box = f.open("ftyp");
        f.cc(s.brand); f.u32(0); f.cc("mif1"); f.cc(s.brand);
        f.close(box);

        const size_t meta = f.open_full("meta", 0);
        box = f.open_full("hdlr", 0);
        f.u32(0); f.cc("pict"); f.u32(0); f.u32(0); f.u32(0); f.u8(0);
        f.close(box);
        box = f.open_full("pitm", 0);
        f.u16(primary);
        f.close(box);

        box = f.open_full("iinf", 0);
        f.u16((uint32_t)n + (s.single ? 1 : 2));
        auto infe = [&](uint32_t id, const char* type) {
            const size_t e = f.open_full("infe", 2);
            f.u16(id); f.u16(0); f.cc(type); f.u8(0);
            f.close(e);
        };
        if (!s.single) infe(100, "grid");
        for (int i = 1; i <= n; i++) infe((uint32_t)i, "hvc1");
        infe(200, "Exif");
        f.close(box);

        box = f.open_full("iref", 0);
        size_t r = 0;
        if (!s.single) {
            r = f.open("dimg");
            f.u16(100); f.u16((uint32_t)n);
            for (int i = 1; i <= n; i++) f.u16((uint32_t)i);
            f.close(r);
        }
        r = f.open("cdsc");
        f.u16(200); f.u16(1); f.u16(primary);
        f.close(r);
        f.close(box);

        const size_t iprp = f.open("iprp");
        const size_t ipco = f.open("ipco");
        box = f.open("hvcC");                       // 1
        f.bytes(s.hvcc);
        f.close(box);
        box = f.open_full("ispe", 0);               // 2
        f.u32((uint32_t)s.tw); f.u32((uint32_t)s.th);
        f.close(box);
        box = f.open_full("ispe", 0);               // 3
        f.u32((uint32_t)s.out_w); f.u32((uint32_t)s.out_h);
        f.close(box);
        box = f.open("clap");                       // 4: 700x280, off (-10, 2)
        f.u32(700); f.u32(1); f.u32(280); f.u32(1);
        f.u32((uint32_t)-10); f.u32(1); f.u32(4); f.u32(2);
        f.close(box);
        box = f.open("irot");                       // 5
        f.u8(1);
        f.close(box);
        box = f.open("imir");                       // 6
        f.u8(1);
        f.close(box);
        box = f.open("colr");                       // 7
        f.cc("nclx"); f.u16(1); f.u16(1); f.u16(1); f.u8(0);
        f.close(box);
        box = f.open("xtra");                       // 8
        f.u32(0);
        f.close(box);
        f.close(ipco);
        box = f.open_full("ipma", 0);
        if (s.single) {
            f.u32(1);
            f.u16(1); f.u8(3); f.u8(0x80 | 1); f.u8(2); f.u8(0x80 | 5);
        } else {
            f.u32((uint32_t)n + 1);
            for (int i = 1; i <= n; i++) {
                f.u16((uint32_t)i); f.u8(2); f.u8(0x80 | 1); f.u8(2);
            }
            f.u16(100);
            f.u8(s.unknown_essential ? 6 : 5);
            f.u8(3); f.u8(0x80 | 4); f.u8(0x80 | 5); f.u8(0x80 | 6); f.u8(7);
            if (s.unknown_essential) f.u8(0x80 | 8);
        }
        f.close(box);
        f.close(iprp);

        box = f.open_full("iloc", 1);
        f.u8(0x44); f.u8(0x00);
        f.u16((uint32_t)n + (s.single ? 1 : 2));
        auto loc = [&](uint32_t id, int method, uint32_t off, uint32_t len) {
            f.u16(id); f.u16((uint32_t)method); f.u16(0); f.u16(1); f.u32(off); f.u32(len);
        };
        if (!s.single) loc(100, 1, 0, 8);
        uint32_t at = mdat_payload;
        for (int i = 0; i < n; i++) {
            loc((uint32_t)i + 1, 0, at, (uint32_t)s.tiles[(size_t)i].size());
            at += (uint32_t)s.tiles[(size_t)i].size();
        }
        loc(200, 0, at, (uint32_t)exif_item.size());
        f.close(box);

        if (!s.single) {
            box = f.open("idat");
            f.u8(0); f.u8(0); f.u8((uint32_t)s.rows - 1); f.u8((uint32_t)s.cols - 1);
            f.u16((uint32_t)s.out_w); f.u16((uint32_t)s.out_h);
            f.close(box);
        }
        f.close(meta);

        box = f.open("mdat");
        for (int i = 0; i < n; i++) f.bytes(s.tiles[(size_t)i]);
        f.bytes(exif_item);
        f.close(box);
        return f.b;
    };
    size_t payload = exif_item.size();
    for (int i = 0; i < n; i++) payload += s.tiles[(size_t)i].size();
    return build((uint32_t)(build(0).size() - payload));
}

std::vector<uint8_t> small_tiff() {
    // Little-endian, one IFD entry: Orientation = 6.
    return {'I', 'I', 42, 0, 8, 0, 0, 0, 1, 0, 0x12, 0x01, 3, 0, 1, 0, 0, 0,
            6, 0, 0, 0, 0, 0, 0, 0};
}

bool parse_bytes(const std::vector<uint8_t>& f, bool tiles, video::HeifImage& im,
                 std::string& error) {
    const video::HeifRead read = [&f](uint64_t off, size_t n, uint8_t* dst) {
        if (off > f.size() || n > f.size() - off) return false;
        std::memcpy(dst, f.data() + off, n);
        return true;
    };
    return video::parse_heif(read, f.size(), tiles, im, error);
}

void test_container() {
    Spec s;
    s.hvcc.assign(23, 0);
    s.hvcc[21] = 0xff;
    for (int i = 0; i < s.cols * s.rows; i++)
        s.tiles.push_back(std::vector<uint8_t>((size_t)(17 + i), (uint8_t)(i + 1)));
    s.tiff = small_tiff();
    const std::vector<uint8_t> file = build_heif(s);

    video::HeifImage im;
    std::string err;
    const bool ok = parse_bytes(file, true, im, err);
    check(ok, "a grid HEIF parses" + (err.empty() ? "" : " (" + err + ")"));
    if (!ok) return;
    check(im.cols == 3 && im.rows == 2, "grid is 3x2");
    check(im.width == 740 && im.height == 300, "output size from the grid descriptor");
    check(im.tile_width == 256 && im.tile_height == 160, "tile size from ispe");
    check(im.hvcc == s.hvcc, "hvcC carried whole");
    check(im.tiles == s.tiles, "tile data read from mdat, in dimg order");
    check(im.has_nclx && im.matrix_coefficients == 1 && !im.full_range, "nclx colr read");
    check(im.ops.size() == 3, "three transformative properties");
    if (im.ops.size() == 3) {
        const video::HeifOp& c = im.ops[0];
        // Centre (739/2 - 10, 299/2 + 2); a 700x280 aperture's first pixel.
        check(c.kind == video::HeifOp::Crop && c.x == 10 && c.y == 12 && c.w == 700 &&
                  c.h == 280, "clap centred on the offset");
        check(im.ops[1].kind == video::HeifOp::Rotate && im.ops[1].ccw == 1, "irot read");
        check(im.ops[2].kind == video::HeifOp::Mirror && im.ops[2].left_right,
              "imir mode 1 swaps left and right");
    }
    int w = 0, h = 0;
    video::heif_display_size(im, w, h);
    check(w == 280 && h == 700, "display size follows crop and turn");
    std::vector<uint8_t> want{'E', 'x', 'i', 'f', 0, 0};
    want.insert(want.end(), s.tiff.begin(), s.tiff.end());
    check(im.exif == want, "EXIF item behind its 4-byte offset");

    video::HeifImage head;
    check(parse_bytes(file, false, head, err) && head.tiles.empty() && head.width == 740,
          "without tiles: the size, no coded data");

    for (size_t cut : {(size_t)12, file.size() / 3, file.size() - 40}) {
        std::vector<uint8_t> t(file.begin(), file.begin() + (ptrdiff_t)cut);
        video::HeifImage x;
        std::string e;
        check(!parse_bytes(t, true, x, e) && !e.empty(),
              "truncated at " + std::to_string(cut) + " fails with a reason");
    }
    Spec iso = s;
    iso.brand = "isom";
    {
        std::vector<uint8_t> f = build_heif(iso);
        // Both brand slots carry "isom"; strip the mif1 one too.
        for (size_t i = 0; i + 4 <= 24; i++)
            if (std::memcmp(f.data() + i, "mif1", 4) == 0) std::memcpy(f.data() + i, "isom", 4);
        video::HeifImage x;
        std::string e;
        check(!parse_bytes(f, true, x, e) && e.find("HEIF") != std::string::npos,
              "an MP4 is not taken for a HEIF");
    }
    Spec one = s;
    one.single = true;
    {
        video::HeifImage x;
        std::string e;
        const bool ok1 = parse_bytes(build_heif(one), true, x, e);
        check(ok1 && x.cols == 1 && x.width == 256 && x.height == 160 &&
                  x.tiles.size() == 1 && x.tiles[0] == s.tiles[0] && x.ops.size() == 1 &&
                  x.ops[0].kind == video::HeifOp::Rotate && x.exif == want,
              "a lone image carries its own essential irot" +
                  (e.empty() ? std::string() : " (" + e + ")"));
    }
    Spec ess = s;
    ess.unknown_essential = true;
    {
        video::HeifImage x;
        std::string e;
        check(!parse_bytes(build_heif(ess), true, x, e) &&
                  e.find("xtra") != std::string::npos,
              "an essential property it cannot apply is refused by name");
    }
}

// Each op alone against ISO/IEC 23008-12's definition. A turn the wrong way and
// a flip on the wrong axis compose to the same picture, so the round trip
// below cannot tell them apart; this can.
void test_ops() {
    const int W = 5, H = 3;
    std::vector<uint8_t> src((size_t)W * H * 3);
    for (int i = 0; i < W * H; i++)
        for (int k = 0; k < 3; k++) src[(size_t)i * 3 + k] = (uint8_t)(i * 3 + k);
    auto at = [&](const std::vector<uint8_t>& im, int w, int x, int y) {
        return im[((size_t)y * w + x) * 3];
    };
    auto run = [&](video::HeifOp op, int& w, int& h) {
        std::vector<uint8_t> px = src;
        w = W;
        h = H;
        video::heif_apply_ops({op}, px, w, h);
        return px;
    };
    int w = 0, h = 0;
    video::HeifOp rot;
    rot.kind = video::HeifOp::Rotate;
    rot.ccw = 1;
    std::vector<uint8_t> r = run(rot, w, h);
    // Anticlockwise: the top-right corner becomes the top-left one.
    bool ok = w == H && h == W;
    for (int y = 0; ok && y < h; y++)
        for (int x = 0; x < w; x++) ok = ok && at(r, w, x, y) == at(src, W, W - 1 - y, x);
    check(ok, "irot 1 turns a quarter anticlockwise");

    video::HeifOp mir;
    mir.kind = video::HeifOp::Mirror;
    std::vector<uint8_t> m = run(mir, w, h);
    ok = w == W && h == H;
    for (int y = 0; ok && y < h; y++)
        for (int x = 0; x < w; x++) ok = ok && at(m, w, x, y) == at(src, W, W - 1 - x, y);
    check(ok, "imir mode 1 swaps left and right");
    mir.left_right = false;
    m = run(mir, w, h);
    ok = w == W && h == H;
    for (int y = 0; ok && y < h; y++)
        for (int x = 0; x < w; x++) ok = ok && at(m, w, x, y) == at(src, W, x, H - 1 - y);
    check(ok, "imir mode 0 swaps top and bottom");

    video::HeifOp crop;
    crop.x = 1;
    crop.y = 1;
    crop.w = 3;
    crop.h = 2;
    std::vector<uint8_t> c = run(crop, w, h);
    ok = w == 3 && h == 2;
    for (int y = 0; ok && y < h; y++)
        for (int x = 0; x < w; x++) ok = ok && at(c, w, x, y) == at(src, W, x + 1, y + 1);
    check(ok, "clap keeps its rectangle");
}

// ================
// The round trip on the device
// ================

std::vector<uint8_t> nal_arrays_hvcc(const std::vector<uint8_t>& headers) {
    std::vector<uint8_t> c(22, 0);
    c[0] = 1;
    c[21] = 0xfc | 3;
    std::vector<std::pair<const uint8_t*, size_t>> nals =
        video::split_annexb(headers.data(), headers.size());
    c.push_back((uint8_t)nals.size());
    for (const auto& [p, n] : nals) {
        c.push_back((uint8_t)(0x80 | ((p[0] >> 1) & 0x3f)));
        c.push_back(0);
        c.push_back(1);
        c.push_back((uint8_t)(n >> 8));
        c.push_back((uint8_t)n);
        c.insert(c.end(), p, p + n);
    }
    return c;
}

// The picture the grid encodes: smooth ramps plus a bright bar near the
// top-left corner, so a wrong turn or flip cannot score well.
uint8_t source_px(int x, int y, int c) {
    if (x >= 40 && x < 200 && y >= 30 && y < 60) return c == 0 ? 240 : 30;
    const int v = c == 0 ? x / 3 : c == 1 ? y * 2 / 3 : (x + y) / 5;
    return (uint8_t)std::min(v, 255);
}

void test_round_trip() {
    nn::vk::ContextOptions co;
    co.want_encode = true;
    try {
        nn::vk::Context::get(co);
    } catch (const std::exception& e) {
        std::printf("skip round trip: %s\n", e.what());
        return;
    }
    const std::string why = video::VideoReader::availability();
    if (!why.empty()) {
        std::printf("skip round trip: %s\n", why.c_str());
        return;
    }
    Spec s;
    video::EncodeOptions eo;
    eo.codec = video::Codec::H265;
    eo.width = s.tw;
    eo.height = s.th;
    eo.fps = 0.5;   // a key frame every frame: each tile stands alone
    eo.quality = 0;
    video::VideoEncoder enc;
    std::string err;
    if (!enc.open(eo, err)) {
        std::printf("skip round trip: %s\n", err.c_str());
        return;
    }
    std::vector<uint8_t> tile((size_t)s.tw * s.th * 3);
    for (int r = 0; r < s.rows; r++)
        for (int c = 0; c < s.cols; c++) {
            for (int y = 0; y < s.th; y++)
                for (int x = 0; x < s.tw; x++)
                    for (int k = 0; k < 3; k++)
                        tile[((size_t)y * s.tw + x) * 3 + k] =
                            source_px(c * s.tw + x, r * s.th + y, k);
            std::vector<uint8_t> au;
            bool sync = false;
            if (!enc.encode(tile.data(), au, sync, err)) {
                check(false, "encode a tile: " + err);
                return;
            }
            std::vector<uint8_t> sample;
            for (const auto& [p, n] : video::split_annexb(au.data(), au.size())) {
                const int type = (p[0] >> 1) & 0x3f;
                if (type >= 32 && type <= 34) continue;
                for (int i = 0; i < 4; i++) sample.push_back((uint8_t)(n >> (24 - 8 * i)));
                sample.insert(sample.end(), p, p + n);
            }
            if (!sync) check(false, "every tile is a key frame");
            s.tiles.push_back(std::move(sample));
        }
    s.hvcc = nal_arrays_hvcc(enc.headers());
    s.tiff = small_tiff();

    const fs::path tmp = fs::temp_directory_path() / "spirula_heif_test.heic";
    {
        const std::vector<uint8_t> f = build_heif(s);
        std::ofstream(tmp, std::ios::binary).write((const char*)f.data(), (std::streamsize)f.size());
    }
    nn::Image out;
    std::vector<uint8_t> exif;
    const bool ok = video::decode_heif(tmp.string(), out, &exif, err);
    std::error_code ec;
    fs::remove(tmp, ec);
    check(ok, "decode_heif on an encoded grid" + (err.empty() ? "" : " (" + err + ")"));
    if (!ok) return;
    check(out.width == 280 && out.height == 700, "decoded to the display size");
    check(exif.size() == 6 + s.tiff.size(), "EXIF handed back");

    // The expected picture, from the definitions rather than the reader's
    // helpers: crop (10, 12, 700, 280) of the grid's output, a quarter turn
    // anticlockwise, then a left-right flip.
    double se = 0;
    for (int y = 0; y < out.height; y++)
        for (int x = 0; x < out.width; x++) {
            const int rx = out.width - 1 - x;         // undo the flip
            const int cx = 700 - 1 - y, cy = rx;      // undo the turn
            const int sx = cx + 10, sy = cy + 12;     // undo the crop
            for (int k = 0; k < 3; k++) {
                const double d = (double)out.data[((size_t)y * out.width + x) * 3 + k] -
                                 (double)source_px(sx, sy, k);
                se += d * d;
            }
        }
    const double mse = se / ((double)out.width * out.height * 3);
    const double psnr = mse > 0 ? 10.0 * std::log10(255.0 * 255.0 / mse) : 99.0;
    char line[96];
    std::snprintf(line, sizeof line, "round trip matches the source (PSNR %.1f dB)", psnr);
    check(psnr > 32.0, line);

    // The first tile on its own, a quarter turn anticlockwise.
    Spec one = s;
    one.single = true;
    {
        const std::vector<uint8_t> f = build_heif(one);
        std::ofstream(tmp, std::ios::binary).write((const char*)f.data(), (std::streamsize)f.size());
    }
    const bool ok1 = video::decode_heif(tmp.string(), out, nullptr, err);
    fs::remove(tmp, ec);
    check(ok1 && out.width == s.th && out.height == s.tw,
          "a lone image decodes, turned" + (err.empty() ? "" : " (" + err + ")"));
    if (!ok1) return;
    se = 0;
    for (int y = 0; y < out.height; y++)
        for (int x = 0; x < out.width; x++)
            for (int k = 0; k < 3; k++) {
                const double d = (double)out.data[((size_t)y * out.width + x) * 3 + k] -
                                 (double)source_px(s.tw - 1 - y, x, k);
                se += d * d;
            }
    const double mse1 = se / ((double)out.width * out.height * 3);
    const double psnr1 = mse1 > 0 ? 10.0 * std::log10(255.0 * 255.0 / mse1) : 99.0;
    std::snprintf(line, sizeof line, "the lone image matches its tile (PSNR %.1f dB)", psnr1);
    check(psnr1 > 32.0, line);
}

int decode_one(const char* in, const char* out_png) {
    nn::Image im;
    std::vector<uint8_t> exif;
    std::string err;
    const double t0 = nn::now_ms();
    if (!video::decode_heif(in, im, &exif, err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }
    std::printf("%dx%d, %zu bytes of EXIF, %.0f ms\n", im.width, im.height, exif.size(),
                nn::now_ms() - t0);
    return stbi_write_png(out_png, im.width, im.height, 3, im.data.data(), im.width * 3)
               ? 0
               : 1;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 3) return decode_one(argv[1], argv[2]);
    test_container();
    test_ops();
    test_round_trip();
    std::printf("%s\n", g_failures ? "FAILED" : "passed");
    return g_failures ? 1 : 0;
}
