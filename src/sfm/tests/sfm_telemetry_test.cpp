// Telemetry: the four carriers on synthetic files, and the checks on
// synthetic readings. With file arguments it prints what each file carries.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "sfm/core/Telemetry.h"
#include "sfm/tests/TestMain.h"

using namespace sfm;

static int fails = 0;

static void check(bool ok, const std::string& what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what.c_str());
        fails++;
    }
}

static bool close_to(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

// ================
// Byte builders
// ================

using Bytes = std::vector<uint8_t>;

static void put_u8(Bytes& b, uint8_t v) { b.push_back(v); }
static void put_be16(Bytes& b, uint16_t v) { b.push_back((uint8_t)(v >> 8)); b.push_back((uint8_t)v); }
static void put_be32(Bytes& b, uint32_t v) { for (int i = 3; i >= 0; i--) b.push_back((uint8_t)(v >> (8 * i))); }
static void put_be64(Bytes& b, uint64_t v) { for (int i = 7; i >= 0; i--) b.push_back((uint8_t)(v >> (8 * i))); }
static void put_le16(Bytes& b, uint16_t v) { b.push_back((uint8_t)v); b.push_back((uint8_t)(v >> 8)); }
static void put_le32(Bytes& b, uint32_t v) { for (int i = 0; i < 4; i++) b.push_back((uint8_t)(v >> (8 * i))); }
static void put_le64(Bytes& b, uint64_t v) { for (int i = 0; i < 8; i++) b.push_back((uint8_t)(v >> (8 * i))); }
static void put_lef32(Bytes& b, float f) { uint32_t u; std::memcpy(&u, &f, 4); put_le32(b, u); }
static void put_lef64(Bytes& b, double d) { uint64_t u; std::memcpy(&u, &d, 8); put_le64(b, u); }
static void put_bef32(Bytes& b, float f) { uint32_t u; std::memcpy(&u, &f, 4); put_be32(b, u); }
static void put_str(Bytes& b, const char* s) { while (*s) b.push_back((uint8_t)*s++); }
static void put_raw(Bytes& b, const Bytes& v) { b.insert(b.end(), v.begin(), v.end()); }

static Bytes box(const char* type, const Bytes& payload) {
    Bytes b;
    put_be32(b, (uint32_t)(8 + payload.size()));
    put_str(b, type);
    put_raw(b, payload);
    return b;
}

static Bytes full(const char* type, const Bytes& payload, uint8_t version = 0) {
    Bytes p;
    put_u8(p, version);
    put_u8(p, 0); put_u8(p, 0); put_u8(p, 0);
    put_raw(p, payload);
    return box(type, p);
}

struct TrackSpec {
    const char* sample_type;
    const char* handler;
    const char* handler_name;
    uint32_t timescale;
    uint32_t sample_delta;
    std::vector<Bytes> samples;
};

// ftyp, mdat (every track's samples in order), moov.
static Bytes build_mp4(const std::vector<TrackSpec>& tracks, uint32_t mv_timescale,
                       uint32_t mv_duration, uint32_t creation_1904) {
    Bytes ftyp_p;
    put_str(ftyp_p, "isom"); put_be32(ftyp_p, 0); put_str(ftyp_p, "isom");
    const Bytes ftyp = box("ftyp", ftyp_p);

    Bytes mdat_p;
    std::vector<uint32_t> first_offsets;
    for (const TrackSpec& t : tracks) {
        first_offsets.push_back((uint32_t)(ftyp.size() + 8 + mdat_p.size()));
        for (const Bytes& s : t.samples) put_raw(mdat_p, s);
    }
    const Bytes mdat = box("mdat", mdat_p);

    Bytes mvhd_p;
    put_be32(mvhd_p, creation_1904); put_be32(mvhd_p, creation_1904);
    put_be32(mvhd_p, mv_timescale); put_be32(mvhd_p, mv_duration);
    for (int i = 0; i < 20; i++) put_be32(mvhd_p, 0);
    Bytes moov_p = full("mvhd", mvhd_p);

    for (size_t ti = 0; ti < tracks.size(); ti++) {
        const TrackSpec& t = tracks[ti];
        Bytes tkhd_p(80, 0);
        put_be32(tkhd_p, 640 << 16); put_be32(tkhd_p, 480 << 16);
        Bytes mdhd_p;
        put_be32(mdhd_p, creation_1904); put_be32(mdhd_p, creation_1904);
        put_be32(mdhd_p, t.timescale);
        put_be32(mdhd_p, (uint32_t)(t.samples.size() * t.sample_delta));
        put_be16(mdhd_p, 0); put_be16(mdhd_p, 0);
        Bytes hdlr_p;
        put_be32(hdlr_p, 0); put_str(hdlr_p, t.handler);
        for (int i = 0; i < 3; i++) put_be32(hdlr_p, 0);
        put_str(hdlr_p, t.handler_name); put_u8(hdlr_p, 0);

        Bytes entry_p(8, 0);
        Bytes stsd_p; put_be32(stsd_p, 1); put_raw(stsd_p, box(t.sample_type, entry_p));
        Bytes stts_p; put_be32(stts_p, 1); put_be32(stts_p, (uint32_t)t.samples.size()); put_be32(stts_p, t.sample_delta);
        Bytes stsc_p; put_be32(stsc_p, 1); put_be32(stsc_p, 1); put_be32(stsc_p, (uint32_t)t.samples.size()); put_be32(stsc_p, 1);
        Bytes stsz_p; put_be32(stsz_p, 0); put_be32(stsz_p, (uint32_t)t.samples.size());
        for (const Bytes& s : t.samples) put_be32(stsz_p, (uint32_t)s.size());
        Bytes stco_p; put_be32(stco_p, 1); put_be32(stco_p, first_offsets[ti]);

        Bytes stbl_p;
        put_raw(stbl_p, full("stsd", stsd_p)); put_raw(stbl_p, full("stts", stts_p));
        put_raw(stbl_p, full("stsc", stsc_p)); put_raw(stbl_p, full("stsz", stsz_p));
        put_raw(stbl_p, full("stco", stco_p));
        Bytes minf_p = box("stbl", stbl_p);
        Bytes mdia_p;
        put_raw(mdia_p, full("mdhd", mdhd_p)); put_raw(mdia_p, full("hdlr", hdlr_p));
        put_raw(mdia_p, box("minf", minf_p));
        Bytes trak_p;
        put_raw(trak_p, full("tkhd", tkhd_p)); put_raw(trak_p, box("mdia", mdia_p));
        put_raw(moov_p, box("trak", trak_p));
    }
    Bytes out = ftyp;
    put_raw(out, mdat);
    put_raw(out, box("moov", moov_p));
    return out;
}

// GPMF key-length-value; payload padded to 4.
static Bytes klv(const char* key, char type, uint8_t size, uint16_t repeat, const Bytes& data) {
    Bytes b;
    put_str(b, key); put_u8(b, (uint8_t)type); put_u8(b, size); put_be16(b, repeat);
    put_raw(b, data);
    while (b.size() % 4) put_u8(b, 0);
    return b;
}
static Bytes klv_str(const char* key, const char* s) {
    Bytes d; put_str(d, s);
    return klv(key, 'c', 1, (uint16_t)d.size(), d);
}
static Bytes klv_nest(const char* key, const Bytes& inner) { return klv(key, 0, 1, (uint16_t)inner.size(), inner); }

// Protobuf fields.
static void pb_varint(Bytes& b, uint64_t v) {
    while (v >= 0x80) { b.push_back((uint8_t)(v | 0x80)); v >>= 7; }
    b.push_back((uint8_t)v);
}
static Bytes pb_v(uint32_t num, uint64_t v) { Bytes b; pb_varint(b, (num << 3) | 0); pb_varint(b, v); return b; }
static Bytes pb_f32(uint32_t num, float f) { Bytes b; pb_varint(b, (num << 3) | 5); put_lef32(b, f); return b; }
static Bytes pb_f64(uint32_t num, double d) { Bytes b; pb_varint(b, (num << 3) | 1); put_lef64(b, d); return b; }
static Bytes pb_bytes(uint32_t num, const Bytes& v) { Bytes b; pb_varint(b, (num << 3) | 2); pb_varint(b, v.size()); put_raw(b, v); return b; }
static Bytes pb_str(uint32_t num, const char* s) { Bytes v; put_str(v, s); return pb_bytes(num, v); }
static Bytes cat(std::initializer_list<Bytes> parts) { Bytes b; for (const Bytes& p : parts) put_raw(b, p); return b; }

// ================
// Carriers
// ================

static void test_camm() {
    TrackSpec t{"camm", "camm", "CAMM", 1000, 100, {}};
    for (int i = 0; i < 3; i++) {
        Bytes s; put_le16(s, 0); put_le16(s, 2);
        put_lef32(s, 0.1f * i); put_lef32(s, 0); put_lef32(s, 0);
        t.samples.push_back(s);
        Bytes a; put_le16(a, 0); put_le16(a, 3);
        put_lef32(a, 0); put_lef32(a, 0); put_lef32(a, 9.81f);
        t.samples.push_back(a);
    }
    Bytes g; put_le16(g, 0); put_le16(g, 6);
    put_lef64(g, 1571230484.0); put_le32(g, 3); put_lef64(g, 43.5); put_lef64(g, -79.4);
    put_lef32(g, 100.0f); put_lef32(g, 2.5f); put_lef32(g, 4.0f);
    put_lef32(g, 3.0f); put_lef32(g, 4.0f); put_lef32(g, 0); put_lef32(g, 0.5f);
    t.samples.push_back(g);
    const Bytes file = build_mp4({t}, 1000, 700, 3600u * 24 * 365);

    Telemetry tm;
    std::string err;
    check(telemetry_read(file.data(), file.size(), tm, err), "camm: read (" + err + ")");
    check(tm.carrier == TelemetryCarrier::Camm, "camm: carrier");
    check(tm.gyro.size() == 3 && tm.accel.size() == 3 && tm.gps.size() == 1, "camm: counts");
    check(tm.gyro.size() == 3 && close_to(tm.gyro[2].x, 0.2, 1e-6) && close_to(tm.gyro[2].t, 0.4, 1e-9), "camm: gyro value and time");
    check(tm.accel.size() == 3 && close_to(tm.accel[0].z, 9.81, 1e-5) && close_to(tm.accel[0].t, 0.1, 1e-9), "camm: accel");
    check(tm.gps.size() == 1 && tm.gps[0].fix && close_to(tm.gps[0].lon, -79.4, 1e-9) && close_to(tm.gps[0].speed, 5.0, 1e-5) &&
          close_to(tm.gps[0].alt, 100.0, 1e-4) && close_to(tm.gps[0].t, 0.6, 1e-9), "camm: gps");
    check(close_to(tm.video_duration, 0.7, 1e-9), "camm: movie duration");
}

static Bytes gpmf_payload(uint64_t stmp_us, int16_t a0, int16_t a1, int16_t a2, int32_t lat_e7, int32_t lon_e7) {
    Bytes accl;
    for (int i = 0; i < 4; i++) { put_be16(accl, (uint16_t)a0); put_be16(accl, (uint16_t)a1); put_be16(accl, (uint16_t)a2); }
    Bytes stmp; put_be64(stmp, stmp_us);
    Bytes scal; put_be16(scal, 417);
    Bytes strm_accl = cat({klv("STMP", 'J', 8, 1, stmp), klv_str("STNM", "Accelerometer"),
                           klv_str("ORIN", "XzY"), klv_str("SIUN", "m/s\xc2\xb2"),
                           klv("SCAL", 's', 2, 1, scal), klv("ACCL", 's', 6, 4, accl)});

    Bytes gpsf; put_be32(gpsf, 3);
    Bytes gpsp; put_be16(gpsp, 172);
    Bytes gpsu; put_str(gpsu, "191016125444.000");
    Bytes gscal;
    for (int32_t v : {10000000, 10000000, 1000, 1000, 100}) put_be32(gscal, (uint32_t)v);
    Bytes gps5;
    for (int i = 0; i < 2; i++) {
        put_be32(gps5, (uint32_t)(lat_e7 + i * 100)); put_be32(gps5, (uint32_t)lon_e7);
        put_be32(gps5, 100000); put_be32(gps5, 2500); put_be32(gps5, 260);
    }
    Bytes gstmp; put_be64(gstmp, stmp_us - 30000);
    Bytes strm_gps = cat({klv("STMP", 'J', 8, 1, gstmp), klv_str("STNM", "GPS (Lat., Long., Alt., 2D speed, 3D speed)"),
                          klv("GPSF", 'L', 4, 1, gpsf), klv("GPSU", 'U', 16, 1, gpsu), klv("GPSP", 'S', 2, 1, gpsp),
                          klv("SCAL", 'l', 4, 5, gscal), klv("GPS5", 'l', 20, 2, gps5)});

    Bytes gscal2; put_be16(gscal2, 32767);
    Bytes grav;
    for (int i = 0; i < 2; i++) { put_be16(grav, 0); put_be16(grav, 0); put_be16(grav, (uint16_t)(int16_t)-32767); }
    Bytes strm_grav = cat({klv("STMP", 'J', 8, 1, stmp), klv_str("STNM", "Gravity Vector"),
                           klv("SCAL", 's', 2, 1, gscal2), klv("GRAV", 's', 6, 2, grav)});

    Bytes dvid; put_be32(dvid, 1);
    Bytes devc = cat({klv("DVID", 'L', 4, 1, dvid), klv_str("DVNM", "GoPro Max"),
                      klv_nest("STRM", strm_accl), klv_nest("STRM", strm_gps), klv_nest("STRM", strm_grav)});
    return klv_nest("DEVC", devc);
}

static void test_gpmf() {
    TrackSpec t{"gpmd", "meta", "GoPro MET", 1000, 1000, {}};
    t.samples.push_back(gpmf_payload(1000000, 100, 200, 4170, 435000000, -794000000));
    t.samples.push_back(gpmf_payload(2000000, 100, 200, 4170, 435001000, -794000000));
    const Bytes file = build_mp4({t}, 1000, 2000, 0);

    Telemetry tm;
    std::string err;
    check(telemetry_read(file.data(), file.size(), tm, err), "gpmf: read (" + err + ")");
    check(tm.carrier == TelemetryCarrier::Gpmf, "gpmf: carrier");
    check(tm.camera == "GoPro Max", "gpmf: camera name");
    check(tm.accel.size() == 8, "gpmf: 8 accel readings");
    if (tm.accel.size() == 8) {
        // ORIN XzY: x = col0, z = -col1, y = col2; SCAL 417.
        check(close_to(tm.accel[0].x, 100.0 / 417, 1e-9) && close_to(tm.accel[0].y, 4170.0 / 417, 1e-9) &&
              close_to(tm.accel[0].z, -200.0 / 417, 1e-9), "gpmf: ORIN and SCAL");
        check(close_to(tm.accel[0].t, 0.0, 1e-9) && close_to(tm.accel[1].t, 0.25, 1e-9) && close_to(tm.accel[5].t, 1.25, 1e-9),
              "gpmf: readings spread by STMP");
    }
    check(tm.gps.size() == 4, "gpmf: 4 gps readings");
    if (tm.gps.size() == 4) {
        check(close_to(tm.gps[0].lat, 43.5, 1e-9) && close_to(tm.gps[0].lon, -79.4, 1e-9) && close_to(tm.gps[0].alt, 100.0, 1e-9) &&
              close_to(tm.gps[0].speed, 2.5, 1e-9), "gpmf: GPS5 scaling");
        check(tm.gps[0].fix && close_to(tm.gps[0].dop, 1.72, 1e-9), "gpmf: GPSF and GPSP");
        check(close_to(tm.gps[0].unix_time, 1571230484.0, 1e-3), "gpmf: GPSU to unix");
        check(close_to(tm.gps[0].t, 0.0, 1e-9) && close_to(tm.gps[2].t, 1.0, 1e-9), "gpmf: GPS anchored to its own first payload");
    }
    check(tm.gravity.size() == 4 && close_to(tm.gravity[0].y, -1.0, 1e-4) && tm.orientation_axes == "XYZ", "gpmf: GRAV swapped into the ORIN frame");
}

static Bytes insta_trailer(const Bytes& meta, const Bytes& gyro, const Bytes& gps) {
    Bytes out;
    auto rec = [&](uint8_t id, uint8_t format, const Bytes& data) {
        put_raw(out, data);
        put_u8(out, format); put_u8(out, id); put_le32(out, (uint32_t)data.size());
    };
    rec(7, 0, gps);
    rec(3, 0, gyro);
    rec(1, 1, meta);
    Bytes hdr(32, 0);
    put_le32(hdr, (uint32_t)(out.size() + 72));
    put_le32(hdr, 3);
    put_str(hdr, "8db42d694ccc418790edff439fe026bf");
    put_raw(out, hdr);
    return out;
}

static void test_insta360(bool raw) {
    const uint64_t creation_unix = 1762745253ull;
    Bytes cfg = cat({pb_v(1, 16), pb_v(2, 2000)});
    Bytes meta = cat({pb_str(1, "IAHEA2509USE64"), pb_str(2, "Insta360 X5"), pb_str(3, "v1.1.22"),
                      pb_v(7, 20251109222733ull), pb_v(24, raw ? 1000000 : 1000), pb_f64(25, 8.5),
                      pb_v(62, raw ? 1 : 0), pb_bytes(65, cfg)});
    Bytes gyro;
    for (int i = 0; i < 3; i++) {
        if (raw) {
            put_le64(gyro, 1000000 + 1000ull * i);
            put_le16(gyro, 32768); put_le16(gyro, 32768); put_le16(gyro, 32768 + 2048);   // 1 g on z
            put_le16(gyro, 32768 + 1638); put_le16(gyro, 32768); put_le16(gyro, 32768);   // ~100 deg/s on x
        } else {
            put_le64(gyro, 1000 + i);
            put_lef64(gyro, 0); put_lef64(gyro, 0); put_lef64(gyro, 1.0);
            put_lef64(gyro, 0.1); put_lef64(gyro, 0); put_lef64(gyro, 0);
        }
    }
    Bytes gps;
    for (int i = 0; i < 2; i++) {
        put_le64(gps, creation_unix + i); put_le16(gps, 500);
        put_u8(gps, i == 0 ? 'A' : 'V');
        put_lef64(gps, 43.5); put_u8(gps, 'N');
        put_lef64(gps, 79.4); put_u8(gps, 'W');
        put_lef64(gps, 1.5); put_lef64(gps, 270.0); put_lef64(gps, 64.8);
    }
    Bytes file = build_mp4({}, 24000, 24000 * 44, (uint32_t)(creation_unix + 2082844800ull));
    put_raw(file, insta_trailer(meta, gyro, gps));

    Telemetry tm;
    std::string err;
    const std::string tag = raw ? "insta360 raw: " : "insta360: ";
    check(telemetry_read(file.data(), file.size(), tm, err), tag + "read (" + err + ")");
    check(tm.carrier == TelemetryCarrier::Insta360, tag + "carrier");
    check(tm.camera == "Insta360 X5" && tm.serial == "IAHEA2509USE64", tag + "identity");
    check(close_to(tm.frame_readout, 0.0085, 1e-9), tag + "readout");
    check(tm.gyro.size() == 3 && tm.accel.size() == 3, tag + "imu counts");
    if (tm.gyro.size() == 3) {
        check(close_to(tm.accel[0].z, 9.80665, raw ? 0.01 : 1e-9) && close_to(tm.accel[0].x, 0, 1e-9), tag + "accel in m/s^2");
        check(close_to(tm.gyro[0].x, raw ? 1638.0 / 32768 * 2000 * 3.14159265358979 / 180 : 0.1, 1e-6), tag + "gyro rad/s");
        check(close_to(tm.gyro[0].t, 0.0, 1e-9) && close_to(tm.gyro[2].t, 0.002, 1e-9), tag + "time from the first frame");
    }
    check(tm.gps.size() == 2, tag + "gps count");
    if (tm.gps.size() == 2) {
        check(tm.gps[0].fix && !tm.gps[1].fix, tag + "fix flag");
        check(close_to(tm.gps[0].lon, -79.4, 1e-9) && close_to(tm.gps[0].alt, 64.8, 1e-9) && close_to(tm.gps[0].speed, 1.5, 1e-9), tag + "gps fields");
        check(close_to(tm.gps[0].t, 0.5, 1e-6) && close_to(tm.gps[1].t, 1.5, 1e-6), tag + "gps time from the movie header");
    }
    check(close_to(tm.video_duration, 44.0, 1e-9), tag + "duration from the movie header");
}

static Bytes dji_sample(bool with_clip, uint64_t ts_us, float az) {
    Bytes out;
    if (with_clip) {
        Bytes hdr = cat({pb_str(1, "dvtm_oq101.proto"), pb_str(5, "SN123"), pb_str(6, "10.00.14.27"), pb_str(10, "Osmo 360")});
        Bytes clip = cat({pb_bytes(1, hdr), pb_bytes(4, pb_v(1, 24020639)), pb_bytes(8, pb_f32(1, 1061.5f)),
                          pb_bytes(10, pb_v(1, 1000)), pb_bytes(11, pb_f32(1, 30.0f))});
        put_raw(out, pb_bytes(1, clip));
    }
    Bytes quat = cat({pb_f32(1, 1.0f), pb_f32(2, 0), pb_f32(3, 0), pb_f32(4, 0)});
    Bytes att = cat({pb_v(1, 123), pb_v(2, 5), pb_bytes(3, quat), pb_bytes(3, quat), pb_bytes(3, quat), pb_f32(4, 1.0f)});
    Bytes imu = pb_bytes(2, pb_bytes(1, att));
    Bytes acc = cat({pb_f32(2, 0), pb_f32(3, 0), pb_f32(4, az)});
    Bytes cam = pb_bytes(10, acc);
    Bytes frame = cat({pb_bytes(1, pb_v(2, ts_us)), pb_bytes(2, cam), pb_bytes(3, imu)});
    put_raw(out, pb_bytes(3, frame));
    return out;
}

static void test_dji() {
    TrackSpec t{"djmd", "meta", "CAM meta", 30000, 1001, {}};
    t.samples.push_back(dji_sample(true, 19260701001ull, -1.0f));
    t.samples.push_back(dji_sample(false, 19260734367ull, -1.0f));
    const Bytes file = build_mp4({t}, 30000, 2002, 0);

    Telemetry tm;
    std::string err;
    check(telemetry_read(file.data(), file.size(), tm, err), "dji: read (" + err + ")");
    check(tm.carrier == TelemetryCarrier::DjiDvtm, "dji: carrier");
    check(tm.camera == "DJI Osmo 360" && tm.serial == "SN123", "dji: identity");
    check(close_to(tm.frame_readout, 0.024020639, 1e-12), "dji: readout");
    check(tm.accel.size() == 2 && close_to(tm.accel[0].z, -9.80665, 1e-5) && close_to(tm.accel[1].t, 0.033366, 1e-6), "dji: accel per frame");
    check(tm.orientation.size() == 6, "dji: 3 quaternions per frame");
    if (tm.orientation.size() == 6) {
        check(close_to(tm.orientation[0].t, -1.0 / 30 / 3, 1e-9) && close_to(tm.orientation[1].t, 0.0, 1e-9), "dji: attitude times use the offset");
        check(close_to(tm.orientation[3].t, 0.033366 - 1.0 / 30 / 3, 1e-6), "dji: second frame");
    }
    check(tm.gps.empty(), "dji: no gps");
}

// Sample 0 of a djmd track: clip header (proto name) and StreamMeta, whose
// field 4 wraps color_mode. `color` < 0 writes an empty wrapper, as proto3
// does for 0; `stream4` false leaves field 4 out.
static Bytes dji_color_sample(const char* proto, int color, bool stream4 = true) {
    Bytes hdr = cat({pb_str(1, proto), pb_str(10, "cam")});
    Bytes stream = pb_bytes(3, cat({pb_v(1, 3840), pb_v(5, 10)}));
    if (stream4) put_raw(stream, pb_bytes(4, color < 0 ? Bytes{} : pb_v(1, (uint64_t)color)));
    return cat({pb_bytes(1, pb_bytes(1, hdr)), pb_bytes(2, stream)});
}

// The same with StreamMeta field 4 given as the raw field bytes, tag included.
static Bytes dji_color_raw(const Bytes& field4) {
    Bytes hdr = cat({pb_str(1, "dvtm_oq101.proto"), pb_str(10, "cam")});
    Bytes stream = cat({pb_bytes(3, pb_v(1, 3840)), field4});
    return cat({pb_bytes(1, pb_bytes(1, hdr)), pb_bytes(2, stream)});
}

// An Avata 360 sample 0, shaped as the real ones read: field 2.2.4 is
// `meta24` (tag included; empty leaves it out), and StreamMeta field 4 is
// fov_type, empty on every real clip.
static Bytes avata_color_sample(const Bytes& meta24, const Bytes& stream4 = pb_bytes(4, Bytes{})) {
    Bytes hdr = cat({pb_str(1, "dvtm_AVATA360.proto"), pb_str(10, "cam")});
    Bytes meta = cat({pb_bytes(3, pb_str(1, "SN")), meta24, pb_bytes(5, Bytes{})});
    Bytes stream = cat({pb_bytes(1, pb_str(3, "video")), pb_bytes(2, meta),
                        pb_bytes(3, pb_v(1, 3840)), stream4});
    return cat({pb_bytes(1, pb_bytes(1, hdr)), pb_bytes(2, stream)});
}

static VideoColor color_of(const Bytes& b) { return djmd_color(b.data(), b.size()); }

static void test_dji_color() {
    const VideoColor dlogm = color_of(dji_color_sample("dvtm_oq101.proto", 19));
    check(dlogm.mode == VideoColorMode::DlogM && dlogm.code == 19,
          "color: Osmo 360 color_mode 19 reads as D-Log M");
    const VideoColor normal = color_of(dji_color_sample("dvtm_oq101.proto", 0));
    check(normal.mode == VideoColorMode::Normal && normal.code == 0,
          "color: Osmo 360 color_mode 0 reads as Normal");
    check(color_of(dji_color_sample("dvtm_oq101.proto", -1)).mode == VideoColorMode::Normal,
          "color: Osmo 360 empty color_mode wrapper reads as Normal");
    for (int code : {2, 9, 22}) {
        const VideoColor c = color_of(dji_color_sample("dvtm_oq101.proto", code));
        check(c.mode == VideoColorMode::OtherLog && c.code == code,
              "color: Osmo 360 color_mode " + std::to_string(code) + " is log, unsupported");
    }
    const VideoColor no4 = color_of(dji_color_sample("dvtm_oq101.proto", 19, false));
    check(no4.mode == VideoColorMode::Unknown && no4.issue == VideoColorIssue::NoColorField,
          "color: Osmo 360 with no color_mode field is Unknown");

    // A reshaped wrapper must never fall through to Normal (review M1).
    const VideoColor wire0 = color_of(dji_color_raw(pb_v(4, 19)));
    check(wire0.mode == VideoColorMode::Unknown && wire0.issue == VideoColorIssue::MalformedColorField,
          "color: Osmo 360 color_mode wrapper sent as a varint is Unknown");
    check(color_of(dji_color_raw(pb_bytes(4, Bytes{0xff, 0xff}))).mode == VideoColorMode::Unknown,
          "color: Osmo 360 unparseable color_mode wrapper is Unknown");
    check(color_of(dji_color_raw(pb_bytes(4, pb_bytes(1, pb_v(1, 19))))).mode ==
              VideoColorMode::Unknown,
          "color: Osmo 360 color_mode sent length-delimited is Unknown");
    check(color_of(dji_color_raw(pb_bytes(4, pb_v(2, 19)))).mode == VideoColorMode::Unknown,
          "color: Osmo 360 wrapper holding only other fields is Unknown");

    // Kills reading the Avata 360 as Unknown, or at the Osmo's 2.4 (empty there: Normal).
    const VideoColor av19 = color_of(avata_color_sample(pb_bytes(4, pb_v(1, 19))));
    check(av19.mode == VideoColorMode::DlogM && av19.code == 19 && av19.proto == "dvtm_AVATA360.proto",
          "color: Avata 360 color_mode 19 at 2.2.4.1 reads as D-Log M");
    // Kills reading it at 2.4, where a fov_type of 2 would be log, unsupported.
    const VideoColor avn = color_of(avata_color_sample(pb_bytes(4, Bytes{}), pb_bytes(4, pb_v(1, 2))));
    check(avn.mode == VideoColorMode::Normal && avn.code == 0,
          "color: Avata 360 empty 2.2.4 reads as Normal");
    // Kills the Osmo's code table on the Avata: only 19 and empty were seen on samples.
    for (int code : {0, 2, 22}) {
        const VideoColor c = color_of(avata_color_sample(pb_bytes(4, pb_v(1, (uint64_t)code))));
        check(c.mode == VideoColorMode::Unknown && c.issue == VideoColorIssue::UnverifiedColorCode &&
                  c.code == code,
              "color: Avata 360 color_mode " + std::to_string(code) + " is Unknown");
    }
    // Kills a missing or reshaped 2.2.4 falling through to Normal.
    const VideoColor avmiss = color_of(avata_color_sample(Bytes{}, pb_bytes(4, pb_v(1, 19))));
    check(avmiss.mode == VideoColorMode::Unknown && avmiss.issue == VideoColorIssue::NoColorField,
          "color: Avata 360 without 2.2.4 is Unknown, whatever 2.4 holds");
    for (const Bytes& bad : {pb_v(4, 19), pb_bytes(4, Bytes{0xff, 0xff}), pb_bytes(4, pb_v(2, 19)),
                             pb_bytes(4, pb_bytes(1, pb_v(1, 19)))}) {
        const VideoColor c = color_of(avata_color_sample(bad));
        check(c.mode == VideoColorMode::Unknown && c.issue == VideoColorIssue::MalformedColorField,
              "color: Avata 360 malformed 2.2.4 is Unknown");
    }
    // Kills the Osmo taking the Avata path: an empty 2.4 is Normal though 2.2.4.1 says 19.
    const Bytes osmo_decoy = cat({pb_bytes(1, pb_bytes(1, cat({pb_str(1, "dvtm_oq101.proto")}))),
                                  pb_bytes(2, cat({pb_bytes(2, pb_bytes(4, pb_v(1, 19))),
                                                   pb_bytes(4, Bytes{})}))});
    check(color_of(osmo_decoy).mode == VideoColorMode::Normal,
          "color: Osmo 360 reads 2.4 only, never the Avata's 2.2.4");
    for (const char* proto : {"dvtm_wa530.proto", "dvtm_wm169.proto"}) {
        for (int code : {-1, 19}) {
            const VideoColor other = color_of(dji_color_sample(proto, code));
            check(other.mode == VideoColorMode::Unknown && other.proto == proto &&
                      other.issue == VideoColorIssue::UnknownLayout,
                  std::string("color: ") + proto + " is Unknown, never Normal (field 4 = " +
                      std::to_string(code) + ")");
        }
    }
    check(color_of(pb_bytes(3, pb_bytes(1, pb_v(2, 5)))).mode == VideoColorMode::NotRecorded,
          "color: a frame-only sample records nothing");

    TrackSpec t{"djmd", "meta", "CAM meta", 30000, 1001, {}};
    t.samples.push_back(dji_color_sample("dvtm_oq101.proto", 19));
    t.samples.push_back(dji_sample(false, 19260734367ull, -1.0f));
    const Bytes file = build_mp4({t}, 30000, 2002, 0);
    check(video_color(file.data(), file.size()).mode == VideoColorMode::DlogM,
          "color: video_color reads a file's first djmd sample");
    // A djmd track is DJI metadata even when no header can be read (review M2).
    TrackSpec frames_only{"djmd", "meta", "CAM meta", 30000, 1001, {}};
    frames_only.samples.push_back(dji_sample(false, 19260734367ull, -1.0f));
    const Bytes headless = build_mp4({frames_only}, 30000, 1001, 0);
    const VideoColor hc = video_color(headless.data(), headless.size());
    check(hc.mode == VideoColorMode::Unknown && hc.issue == VideoColorIssue::NoClipHeader,
          "color: a djmd track without a readable clip header is Unknown");
    TrackSpec other{"camm", "meta", "", 1000, 100, {Bytes(8, 0)}};
    const Bytes none = build_mp4({other}, 1000, 100, 0);
    check(video_color(none.data(), none.size()).mode == VideoColorMode::NotRecorded,
          "color: a file without djmd records nothing");
}

// ================
// Checks
// ================

static void test_checks() {
    Telemetry tm;
    tm.video_duration = 10;
    for (int i = 0; i < 1000; i++) {
        const double t = i * 0.01;
        const double th = 0.3 * std::sin(t);   // rocking about x
        // q maps sensor->world; the accelerometer reads world up (0,0,g)
        // brought into the sensor frame.
        tm.orientation.push_back({t, std::cos(th / 2), std::sin(th / 2), 0, 0});
        tm.accel.push_back({t, 0, 9.80665 * std::sin(th), 9.80665 * std::cos(th)});
        tm.gyro.push_back({t, 0.3 * std::cos(t), 0, 0});
    }
    for (int i = 0; i < 100; i++) tm.gps.push_back({i * 0.1, 0, 43.5 + i * 1e-5, -79.4, 100, true, true, 1.0, 0, 1});
    TelemetryCheck c = telemetry_check(tm);
    check(c.imu_usable, "check: synthetic IMU usable");
    check(close_to(c.accel.rate_hz, 100, 0.5) && close_to(c.accel_norm_median, 9.80665, 0.05), "check: rate and gravity norm");
    check(c.accel_in_world_spread_deg >= 0 && c.accel_in_world_spread_deg < 0.5 && c.attitude_is_sensor_to_world,
          "check: attitude sense found from gravity");
    check(c.gps_usable && c.gps_frozen_fraction == 0 && c.gps_spread_m > 5, "check: moving GPS usable");
    check(c.warnings.empty(), "check: no warnings on clean data");

    // A 1 Hz receiver logged at 10 Hz repeats 90% of its samples and is fine;
    // one jump of a kilometre is not.
    Telemetry slow = tm;
    slow.gps.clear();
    for (int i = 0; i < 100; i++) slow.gps.push_back({i * 0.1, 0, 43.5 + (i / 10) * 1e-4, -79.4, 100, true, true, 1.0, 0, 1});
    slow.gps[55].lat += 0.01;
    c = telemetry_check(slow);
    check(c.gps_usable && c.gps_distinct == 12 && c.gps_outliers == 1 && close_to(c.gps_longest_hold, 0.9, 1e-6),
          "check: slow receiver kept, jump dropped");

    // The other quaternion sense.
    for (TelemetryQuat& q : tm.orientation) { q.x = -q.x; }
    c = telemetry_check(tm);
    check(c.accel_in_world_spread_deg < 0.5 && !c.attitude_is_sensor_to_world, "check: conjugate sense found");

    // A stale fix repeated, in g, with a gap.
    Telemetry bad;
    bad.video_duration = 10;
    for (int i = 0; i < 500; i++) {
        const double t = i * 0.01 + (i > 250 ? 3.0 : 0.0);
        bad.accel.push_back({t, 0, 0, 1.0});
        bad.gyro.push_back({t, 0, 0, 0});
    }
    for (int i = 0; i < 50; i++) bad.gps.push_back({i * 0.2, 0, 43.5, -79.4, 100, true, true, 0, 0, 1});
    c = telemetry_check(bad);
    check(!c.imu_usable && !c.gps_usable, "check: bad data refused");
    check(close_to(c.gps_frozen_fraction, 1.0, 1e-9) && c.gps_distinct == 1 && c.gps_spread_m < 0.01, "check: frozen GPS measured");
    bool saw_g = false, saw_gap = false, saw_frozen = false;
    for (const std::string& w : c.warnings) {
        if (w.find("in g") != std::string::npos) saw_g = true;
        if (w.find("gap") != std::string::npos) saw_gap = true;
        if (w.find("stale") != std::string::npos) saw_frozen = true;
    }
    check(saw_g && saw_gap && saw_frozen, "check: warnings name the problems");
    const std::string report = telemetry_report(bad, c);
    check(report.find("verdict") != std::string::npos && report.find("WARNING") != std::string::npos, "report: renders");
}

static void test_rejects() {
    Telemetry tm;
    std::string err;
    const Bytes junk(64, 0x55);
    check(!telemetry_read(junk.data(), junk.size(), tm, err) && !err.empty(), "junk is refused with a reason");
    const Bytes plain = build_mp4({}, 1000, 1000, 0);
    err.clear();
    const bool ok = telemetry_read(plain.data(), plain.size(), tm, err);
    check(ok && tm.carrier == TelemetryCarrier::None && tm.empty(),
          "an MP4 without telemetry reads as empty, not as an error (" + err + ")");
}

// ================
// Entry
// ================

// `--head N` also prints the first N readings of every stream.
static void print_head(const Telemetry& tm, int n) {
    auto vecs = [&](const char* name, const std::vector<TelemetryVec>& v) {
        for (int i = 0; i < n && i < (int)v.size(); i++)
            std::printf("  %s[%d] t=%.6f  %.6f %.6f %.6f\n", name, i, v[i].t, v[i].x, v[i].y, v[i].z);
    };
    vecs("gyro", tm.gyro);
    vecs("accel", tm.accel);
    vecs("gravity", tm.gravity);
    for (int i = 0; i < n && i < (int)tm.orientation.size(); i++) {
        const TelemetryQuat& q = tm.orientation[i];
        std::printf("  quat[%d] t=%.6f  w=%.6f x=%.6f y=%.6f z=%.6f\n", i, q.t, q.w, q.x, q.y, q.z);
    }
    for (int i = 0; i < n && i < (int)tm.gps.size(); i++) {
        const TelemetryGps& g = tm.gps[i];
        std::printf("  gps[%d] t=%.3f unix=%.3f fix=%d  %.7f %.7f alt=%.2f speed=%.2f track=%.1f dop=%.2f\n",
                    i, g.t, g.unix_time, (int)g.fix, g.lat, g.lon, g.alt, g.speed, g.track, g.dop);
    }
}

static int cmdTelemetryTest(int argc, char** argv) {
    if (argc > 1) {
        int rc = 0, head = 0;
        for (int i = 1; i < argc; i++) {
            if (std::strcmp(argv[i], "--head") == 0 && i + 1 < argc) { head = std::atoi(argv[++i]); continue; }
            Telemetry tm;
            std::string err;
            std::printf("==== %s\n", argv[i]);
            if (!telemetry_read(argv[i], tm, err)) {
                std::printf("error: %s\n", err.c_str());
                rc = 1;
                continue;
            }
            std::printf("%s", telemetry_report(tm, telemetry_check(tm)).c_str());
            const VideoColor vc = video_color(argv[i]);
            std::printf("color: mode %d code %d proto %s issue %d\n", (int)vc.mode, vc.code, vc.proto.c_str(),
                        (int)vc.issue);
            if (head > 0) print_head(tm, head);
            if (tm.carrier == TelemetryCarrier::None) rc = 1;
        }
        return rc;
    }
    test_camm();
    test_gpmf();
    test_insta360(false);
    test_insta360(true);
    test_dji();
    test_dji_color();
    test_checks();
    test_rejects();
    if (fails) {
        std::printf("FAIL (%d)\n", fails);
        return 1;
    }
    std::printf("PASS\n");
    return 0;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, cmdTelemetryTest); }
