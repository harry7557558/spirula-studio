#pragma once

// A synthetic E57 file for data/E57Reader.h and app/E57Dataset.h tests: paged
// with zero checksums (the reader does not verify them), one scan whose fields
// have odd bit widths and whose streams are cut unevenly across packets, so
// values straddle packet boundaries, and whatever images a test describes.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace e57test {

// cartesianX is ScaledInteger at 1e-4 m, so `x` is taken to that grid.
struct TestPoint {
    double x, y, z;
    int r, g, b;    // red and blue 0..255, green 0..1023
    int invalid;    // cartesianInvalidState, 0..2
};

struct Spec {
    std::vector<TestPoint> points;
    double scan_q[4] = {1, 0, 0, 0};
    double scan_t[3] = {0, 0, 0};
    // Each is the inside of one <vectorChild> of images2D; "{BLOB}" becomes a
    // <jpegImage> and "{PNGBLOB}" a <pngImage> element pointing at `blob`.
    std::vector<std::string> images;
    std::string blob;
};

inline uint64_t to_physical(uint64_t logical) {
    return logical / 1020 * 1024 + logical % 1020;
}

struct Bits {
    std::vector<uint8_t> bytes;
    uint64_t bit = 0;
    void put(uint64_t v, int n) {
        for (int i = 0; i < n; i++, bit++) {
            if ((bit >> 3) >= bytes.size()) bytes.push_back(0);
            if ((v >> i) & 1) bytes[bit >> 3] |= (uint8_t)(1u << (bit & 7));
        }
    }
};

template <typename T>
void append(std::string& s, T v) { s.append(reinterpret_cast<const char*>(&v), sizeof v); }

inline std::string num(double v) {
    char b[40];
    std::snprintf(b, sizeof b, "%.17g", v);
    return b;
}

// The whole file, ready to write.
inline std::string build(const Spec& spec) {
    std::string L(48, '\0');   // logical bytes; the header is filled last

    Bits streams[7];
    for (const TestPoint& p : spec.points) {
        streams[0].put((uint64_t)(std::llround(p.x * 1e4) + 2000000), 22);
        const float y = (float)p.y;
        uint32_t yu; std::memcpy(&yu, &y, 4);
        streams[1].put(yu, 32);
        uint64_t zu; std::memcpy(&zu, &p.z, 8);
        streams[2].put(zu, 64);
        streams[3].put((uint64_t)p.r, 8);
        streams[4].put((uint64_t)p.g, 10);
        streams[5].put((uint64_t)p.b, 8);
        streams[6].put((uint64_t)p.invalid, 2);
    }

    const uint64_t section = L.size();
    L.append(32, '\0');
    const uint64_t data_start = L.size();
    // Uneven cuts on purpose, and enough of them to keep each packet under
    // the 64 KiB a packet length can say.
    size_t total = 0;
    for (const Bits& b : streams) total += b.bytes.size();
    const int packets = std::max<int>(3, (int)(total / 30000) + 1);
    std::vector<double> cuts = {0.0};
    for (int k = 1; k < packets; k++) cuts.push_back((k - (k % 2 ? 0.63 : 0.0)) / packets);
    cuts.push_back(1.0);
    for (int k = 0; k < packets; k++) {
        if (k == packets - 1) {   // an empty packet between two data packets
            L.push_back(2); L.push_back(0); append<uint16_t>(L, 3);
        }
        std::string body;
        uint16_t lens[7];
        for (int f = 0; f < 7; f++) {
            const size_t n = streams[f].bytes.size();
            const size_t a = (size_t)(n * cuts[k]);
            const size_t b = k == packets - 1 ? n : (size_t)(n * cuts[k + 1]);
            lens[f] = (uint16_t)(b - a);
            body.append(reinterpret_cast<const char*>(streams[f].bytes.data()) + a, b - a);
        }
        const size_t len = 6 + 2 * 7 + body.size();
        L.push_back(1); L.push_back(0);
        append<uint16_t>(L, (uint16_t)(len - 1));
        append<uint16_t>(L, 7);
        for (uint16_t n : lens) append<uint16_t>(L, n);
        L += body;
    }
    const uint64_t section_len = L.size() - section;
    L[section] = 1;
    std::memcpy(&L[section + 8], &section_len, 8);
    const uint64_t data_phys = to_physical(data_start);
    std::memcpy(&L[section + 16], &data_phys, 8);

    const uint64_t blob_at = L.size();
    L.push_back(0); L.append(7, '\0');
    append<uint64_t>(L, 16 + spec.blob.size());
    L += spec.blob;

    const std::string blob_ref = " type=\"Blob\" fileOffset=\"" +
        std::to_string(to_physical(blob_at)) + "\" length=\"" +
        std::to_string(spec.blob.size()) + "\"/>";
    std::string images;
    for (std::string im : spec.images) {
        size_t at = im.find("{BLOB}");
        if (at != std::string::npos) im.replace(at, 6, "<jpegImage" + blob_ref);
        at = im.find("{PNGBLOB}");
        if (at != std::string::npos) im.replace(at, 9, "<pngImage" + blob_ref);
        images += "<vectorChild type=\"Structure\">" + im + "</vectorChild>\n";
    }
    const double* q = spec.scan_q;
    const double* t = spec.scan_t;
    const std::string xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<e57Root type=\"Structure\" xmlns=\"http://www.astm.org/COMMIT/E57/2010-e57-v1.0\">\n"
        "<formatName type=\"String\"><![CDATA[ASTM E57 3D Imaging Data File]]></formatName>\n"
        "<versionMinor type=\"Integer\"/>\n"
        "<data3D type=\"Vector\" allowHeterogeneousChildren=\"1\"><vectorChild type=\"Structure\">\n"
        "<guid type=\"String\"><![CDATA[scan-0]]></guid>\n"
        "<name type=\"String\"><![CDATA[station 0]]></name>\n"
        "<colorLimits type=\"Structure\"><colorRedMinimum type=\"Integer\"/>"
        "<colorRedMaximum type=\"Integer\">255</colorRedMaximum>"
        "<colorGreenMinimum type=\"Integer\"/><colorGreenMaximum type=\"Integer\">1023</colorGreenMaximum>"
        "<colorBlueMinimum type=\"Integer\"/><colorBlueMaximum type=\"Integer\">255</colorBlueMaximum>"
        "</colorLimits>\n"
        "<pose type=\"Structure\"><rotation type=\"Structure\">"
        "<w type=\"Float\">" + num(q[0]) + "</w><x type=\"Float\">" + num(q[1]) +
        "</x><y type=\"Float\">" + num(q[2]) + "</y><z type=\"Float\">" + num(q[3]) + "</z>"
        "</rotation><translation type=\"Structure\"><x type=\"Float\">" + num(t[0]) +
        "</x><y type=\"Float\">" + num(t[1]) + "</y><z type=\"Float\">" + num(t[2]) + "</z>"
        "</translation></pose>\n"
        "<points type=\"CompressedVector\" fileOffset=\"" + std::to_string(to_physical(section)) +
        "\" recordCount=\"" + std::to_string(spec.points.size()) + "\">\n"
        "<prototype type=\"Structure\">"
        "<cartesianX type=\"ScaledInteger\" minimum=\"-2000000\" maximum=\"2000000\" scale=\"1e-4\"/>"
        "<cartesianY type=\"Float\" precision=\"single\"/>"
        "<cartesianZ type=\"Float\"/>"
        "<colorRed type=\"Integer\" minimum=\"0\" maximum=\"255\"/>"
        "<colorGreen type=\"Integer\" minimum=\"0\" maximum=\"1023\"/>"
        "<colorBlue type=\"Integer\" minimum=\"0\" maximum=\"255\"/>"
        "<cartesianInvalidState type=\"Integer\" minimum=\"0\" maximum=\"2\"/>"
        "</prototype><codecs type=\"Vector\" allowHeterogeneousChildren=\"1\"/>\n"
        "</points></vectorChild></data3D>\n"
        "<images2D type=\"Vector\" allowHeterogeneousChildren=\"1\">\n" + images +
        "</images2D></e57Root>\n";
    const uint64_t xml_at = L.size();
    L += xml;

    while (L.size() % 1020) L.push_back('\0');
    const uint64_t physical = L.size() / 1020 * 1024;
    std::memcpy(&L[0], "ASTM-E57", 8);
    const uint32_t major = 1, minor = 0;
    std::memcpy(&L[8], &major, 4);
    std::memcpy(&L[12], &minor, 4);
    std::memcpy(&L[16], &physical, 8);
    const uint64_t xml_phys = to_physical(xml_at), xml_len = xml.size(), page = 1024;
    std::memcpy(&L[24], &xml_phys, 8);
    std::memcpy(&L[32], &xml_len, 8);
    std::memcpy(&L[40], &page, 8);

    std::string file;
    for (size_t p = 0; p < L.size(); p += 1020) {
        file.append(L, p, 1020);
        file.append(4, '\0');
    }
    return file;
}

// An image's <pose> element.
inline std::string pose_xml(const double q[4], const double t[3]) {
    return "<pose type=\"Structure\"><rotation type=\"Structure\"><w type=\"Float\">" +
           num(q[0]) + "</w><x type=\"Float\">" + num(q[1]) + "</x><y type=\"Float\">" +
           num(q[2]) + "</y><z type=\"Float\">" + num(q[3]) +
           "</z></rotation><translation type=\"Structure\"><x type=\"Float\">" +
           num(t[0]) + "</x><y type=\"Float\">" + num(t[1]) + "</y><z type=\"Float\">" +
           num(t[2]) + "</z></translation></pose>";
}

}  // namespace e57test
