// Telemetry: the four carriers on synthetic files, and the checks on
// synthetic readings. With file arguments it prints what each file carries.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "core/Env.h"
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

static bool no_declared_up(const Telemetry& tm) {
    return tm.attitude_world_up[0] == 0 && tm.attitude_world_up[1] == 0 && tm.attitude_world_up[2] == 0;
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
    check(tm.exposure.empty(), "dji: no exposure on the Osmo");
    check(no_declared_up(tm), "dji: no attitude vertical declared on the Osmo");
}

// An Avata 360 frame, numbered as the real ones read. 3.2.10 is Osmo-shaped
// on purpose: the real 4-byte payload does not parse, so it cannot catch an
// Avata reader that takes it as accel.
static Bytes avata_sample(const char* proto, bool with_clip, uint64_t ts_us) {
    Bytes out;
    if (with_clip) {
        Bytes hdr = cat({pb_str(1, proto), pb_str(5, "SN9"), pb_str(6, "01.00.06.00"), pb_str(10, "DJI Avata360")});
        Bytes clip = cat({pb_bytes(1, hdr), pb_bytes(4, pb_v(1, 21203282)), pb_bytes(8, pb_v(1, 4000)),
                          pb_bytes(9, pb_f32(1, 59.94f)), pb_bytes(10, pb_v(1, 18446744073709551061ull)),
                          pb_bytes(11, pb_v(1, 4207))});
        put_raw(out, pb_bytes(1, clip));
    }
    Bytes shutter; pb_varint(shutter, 1); pb_varint(shutter, 5000);
    Bytes cam = cat({pb_bytes(3, pb_f32(1, 800.0f)), pb_bytes(4, pb_bytes(1, shutter)), pb_bytes(6, pb_v(1, 6217)),
                     pb_bytes(10, cat({pb_f32(2, 0), pb_f32(3, 0), pb_f32(4, -1.0f)})),
                     pb_bytes(11, pb_f32(1, 325.291f))});
    Bytes quat = cat({pb_f32(1, 1.0f), pb_f32(2, 0), pb_f32(3, 0), pb_f32(4, 0)});
    Bytes att = cat({pb_v(1, ts_us - 770), pb_v(2, 10239), pb_bytes(3, quat), pb_bytes(3, quat), pb_bytes(3, quat)});
    Bytes coord = cat({pb_f64(2, 42.2174447), pb_f64(3, -83.6715616)});
    Bytes pos = cat({pb_bytes(4, cat({pb_bytes(1, coord), pb_v(2, 325291), pb_v(4, 1)})),
                     pb_bytes(5, cat({pb_f32(1, 36700.0f), pb_v(2, 1)}))});
    Bytes frame = cat({pb_bytes(1, pb_v(2, ts_us)), pb_bytes(2, cam), pb_bytes(3, pb_bytes(2, pb_bytes(1, att))),
                       pb_bytes(4, pos)});
    put_raw(out, pb_bytes(3, frame));
    return out;
}

static Telemetry read_samples(const std::vector<Bytes>& samples, const std::string& tag) {
    TrackSpec t{"djmd", "meta", "CAM meta", 60000, 1001, samples};
    const Bytes file = build_mp4({t}, 60000, (uint32_t)(1001 * samples.size()), 0);
    Telemetry tm;
    std::string err;
    check(telemetry_read(file.data(), file.size(), tm, err), tag + "read (" + err + ")");
    return tm;
}

static bool has_note(const Telemetry& tm, const std::string& needle, bool exact) {
    for (const std::string& n : tm.notes)
        if (exact ? n == needle : n.find(needle) != std::string::npos) return true;
    return false;
}

static void test_dji_avata() {
    const Telemetry tm = read_samples({avata_sample("dvtm_AVATA360.proto", true, 200830583ull),
                                       avata_sample("dvtm_AVATA360.proto", false, 200847283ull)}, "avata: ");
    check(tm.camera == "DJI Avata360" && tm.serial == "SN9" && close_to(tm.frame_readout, 0.021203282, 1e-12),
          "avata: identity and readout");
    // Kills the radians rule on a coord with no unit field (2418.9 deg), and GPS read at 3.4.2.
    check(tm.gps.size() == 2 && close_to(tm.gps[0].lat, 42.2174447, 1e-9) && close_to(tm.gps[0].lon, -83.6715616, 1e-9),
          "avata: GPS at 3.4.4.1, in degrees");
    check(!tm.gps.empty() && tm.gps[0].has_alt && close_to(tm.gps[0].alt, 325.291, 1e-9), "avata: abs alt in mm");
    check(!tm.gps.empty() && tm.gps[0].has_rel_alt && close_to(tm.gps[0].rel_alt, 36.7, 1e-9),
          "avata: rel alt at 3.4.5.1, f32 mm");
    check(tm.accel.empty(), "avata: 3.2.10 is not accel");
    check(tm.attitude_world_up[0] == 0 && tm.attitude_world_up[1] == 0 && tm.attitude_world_up[2] == -1,
          "avata: attitude world declared z-down");
    // Kills sensor fps read at 1.11 (every quaternion of a frame on one time).
    check(tm.orientation.size() == 6 &&
              close_to(tm.orientation[1].t - tm.orientation[0].t, 1.0 / (double)59.94f / 3, 1e-9),
          "avata: attitude spread over the frame at 1.9.1 fps");
    check(has_note(tm, "IMU fusion rate 4000 Hz, sensor 59.940 fps", true),
          "avata: rate from 1.8.1, fps from 1.9.1, no focal");
    check(!has_note(tm, "not verified", false), "avata: layout verified");
    const TelemetryCheck c = telemetry_check(tm);
    check(c.gps.count == 2 && c.gps_fix_fraction == 1, "avata: 3.4.4.4 is not a fix status");
    check(tm.exposure.size() == 2, "avata: one exposure per frame");
    if (tm.exposure.size() == 2) {
        const TelemetryExposure& e = tm.exposure[1];
        check(close_to(e.t, 0.0167, 1e-9), "avata: exposure on the frame time");
        check(e.iso == 800, "avata: ISO at 3.2.3.1, f32");
        check(close_to(e.shutter, 2e-4, 1e-12), "avata: shutter at 3.2.4.1 is n/d seconds");
        check(e.color_temp == 6217, "avata: colour temperature at 3.2.6.1, varint K");
        check(e.fnum == -1 && !e.has_ev, "avata: fnum and EV not located, left unknown");
    }
    const std::string report = telemetry_report(tm, c);
    check(report.find("exposure     : 2 frames, ISO 800..800, shutter 1/5000..1/5000 s, ct 6217..6217 K\n") !=
                  std::string::npos && report.find(", rel alt 36.7..36.7 m\n") != std::string::npos,
          "avata: report shows exposure and rel alt");
}

// An Osmo frame carrying the Avata's numbers must not be read at them.
static void test_dji_osmo_ignores_avata_fields() {
    Bytes coord = cat({pb_f64(2, 42.2174447), pb_f64(3, -83.6715616)});
    Bytes shutter; pb_varint(shutter, 1); pb_varint(shutter, 5000);
    Bytes cam = cat({pb_bytes(3, pb_f32(1, 800.0f)), pb_bytes(4, pb_bytes(1, shutter)), pb_bytes(6, pb_v(1, 6217))});
    Bytes pos = cat({pb_bytes(4, cat({pb_bytes(1, coord), pb_v(2, 325291)})), pb_bytes(5, pb_f32(1, 36700.0f))});
    Bytes decoy = pb_bytes(3, cat({pb_bytes(1, pb_v(2, 19260767733ull)), pb_bytes(2, cam), pb_bytes(4, pos)}));
    const Telemetry tm = read_samples({dji_sample(true, 19260701001ull, -1.0f), dji_sample(false, 19260734367ull, -1.0f),
                                       decoy}, "osmo decoy: ");
    check(tm.gps.empty(), "osmo decoy: Avata 3.4.4 and 3.4.5 not read on the Osmo");
    check(tm.exposure.empty(), "osmo decoy: Avata 3.2.3, 3.2.4, 3.2.6 not read on the Osmo");
    const std::string report = telemetry_report(tm, telemetry_check(tm));
    check(report.find("exposure") == std::string::npos && report.find("rel alt") == std::string::npos,
          "osmo decoy: report has no exposure or rel alt line");
    check(has_note(tm, "IMU fusion rate 1000 Hz, sensor 30.000 fps, focal 1061.5 px", true) &&
              has_note(tm, "dvtm proto dvtm_oq101.proto", true),
          "osmo decoy: Osmo clip fields unchanged");
}

// A proto nobody has checked keeps the Osmo numbers and says so.
static void test_dji_unknown_proto() {
    const Telemetry tm = read_samples({avata_sample("dvtm_wm169.proto", true, 200830583ull),
                                       avata_sample("dvtm_wm169.proto", false, 200847283ull)}, "wm169: ");
    check(tm.gps.empty() && tm.exposure.empty(), "wm169: no Avata GPS or exposure on an unknown proto");
    check(has_note(tm, "(layout not verified on a sample)", false), "wm169: layout flagged unverified");
    check(no_declared_up(tm), "wm169: no attitude vertical declared");
}

// ================
// Real Avata 360 clips, when SS_TEST_AVATA_* name them
// ================

static bool same_bits(double a, double b) { return std::memcmp(&a, &b, sizeof a) == 0; }

static bool read_file(const char* path, Telemetry& tm, const std::string& tag) {
    std::string err;
    const bool ok = telemetry_read(path, tm, err);
    check(ok, tag + "read (" + err + ")");
    return ok;
}

// Expected values are from the flight's SRT (FrameCnt 1 = djmd packet 0).
static void test_avata_flight() {
    const char* path = spirula::env("TEST_AVATA_OSV");
    if (!path) { std::printf("SKIP avata real clip (SS_TEST_AVATA_OSV unset)\n"); return; }
    Telemetry tm;
    if (!read_file(path, tm, "avata flight: ")) return;
    const TelemetryCheck c = telemetry_check(tm);
    check(tm.gps.size() == 8354, "avata flight: 8354 GPS fixes");
    if (!tm.gps.empty()) {
        const TelemetryGps& g = tm.gps[0];
        check(close_to(g.lat, 42.217445, 1e-5) && close_to(g.lon, -83.671562, 1e-5), "avata flight: first fix");
        check(close_to(g.alt, 325.291, 0.002) && close_to(g.rel_alt, 36.700, 0.001), "avata flight: altitudes");
    }
    check(!tm.exposure.empty() && tm.exposure[0].iso == 800 && tm.exposure[0].color_temp == 6217 &&
              std::fabs(std::log(5000 * tm.exposure[0].shutter)) <= std::log(1.19),
          "avata flight: first exposure ISO 800, 1/5000, 6217 K");
    check(c.orientation.rate_hz >= 3950 && c.orientation.rate_hz <= 4050, "avata flight: attitude at 4 kHz");
    check(c.gps_usable && c.gps_distinct >= 1000 && c.gps_spread_m >= 55 && c.gps_spread_m <= 70,
          "avata flight: GPS usable");
    check(!has_note(tm, "not verified", false), "avata flight: layout verified");
    check(telemetry_report(tm, c).find("GPS usable") != std::string::npos, "avata flight: verdict GPS usable");
}

// A hover, with its .LRF proxy as the oracle: the same metadata at 30 fps,
// paired to the OSV by exact frame time (both anchor on one timestamp).
static void test_avata_hover() {
    const char* osv = spirula::env("TEST_AVATA_OSV_HOVER");
    const char* lrf = spirula::env("TEST_AVATA_LRF_HOVER");
    if (!osv || !lrf) {
        std::printf("SKIP avata hover (SS_TEST_AVATA_OSV_HOVER or SS_TEST_AVATA_LRF_HOVER unset)\n");
        return;
    }
    Telemetry tm, px;
    if (!read_file(osv, tm, "avata hover: ") || !read_file(lrf, px, "avata hover lrf: ")) return;
    const TelemetryCheck c = telemetry_check(tm);
    check(tm.gps.size() == 634 && tm.exposure.size() == 634, "avata hover: 634 fixes and exposures");
    check(c.gps_distinct >= 100 && c.gps_spread_m < 1, "avata hover: GPS present, standing still");
    if (!tm.gps.empty() && !tm.exposure.empty())
        check(close_to(tm.gps[0].rel_alt, 2.600, 0.001) && close_to(tm.gps[0].alt, 237.503, 0.001) &&
                  tm.exposure[0].iso == 400,
              "avata hover: rel alt 2.6 m, alt 237.503 m, ISO 400");
    // A hover has no GPS spread: not usable is the right answer, not a reader defect.
    check(!c.gps_usable, "avata hover: GPS not usable");

    std::map<double, size_t> gps_at, exp_at;
    for (size_t i = 0; i < tm.gps.size(); i++) gps_at[tm.gps[i].t] = i;
    for (size_t i = 0; i < tm.exposure.size(); i++) exp_at[tm.exposure[i].t] = i;
    size_t pairs = 0, differ = 0;
    for (size_t j = 0; j < px.gps.size() && j < px.exposure.size(); j++) {
        const auto g = gps_at.find(px.gps[j].t);
        const auto e = exp_at.find(px.exposure[j].t);
        if (g == gps_at.end() || e == exp_at.end()) continue;
        pairs++;
        const TelemetryGps &a = tm.gps[g->second], &b = px.gps[j];
        const TelemetryExposure &x = tm.exposure[e->second], &y = px.exposure[j];
        if (!a.has_rel_alt || !b.has_rel_alt || x.iso < 0 || x.shutter < 0 || x.color_temp < 0 ||
            !same_bits(a.lat, b.lat) || !same_bits(a.lon, b.lon) || !same_bits(a.alt, b.alt) ||
            !same_bits(a.rel_alt, b.rel_alt) || !same_bits(x.iso, y.iso) || !same_bits(x.shutter, y.shutter) ||
            !same_bits(x.color_temp, y.color_temp))
            differ++;
    }
    check(px.gps.size() == 317 && pairs == 317, "avata hover: 317 LRF frames pair by exact time, got " +
                                                    std::to_string(pairs));
    check(pairs > 0 && differ == 0, "avata hover: paired frames bit-identical, " + std::to_string(differ) + " differ");
}

// ================
// Checks
// ================

// ================
// Lens calibration: StreamMeta.5, numbered as real Avata 360 clips read
// ================

struct DewarpSpec { float fx, fy, cx, cy, k[5], p[2], w, h, model; };

// The two lenses of a real clip; distinct in every field, so a swap shows.
static const DewarpSpec kSlave = {1046.979f, 1047.15088f, 1906.91724f, 1911.23059f,
                                  {0.0674323887f, -0.0157264061f, 0.0115848193f, -0.00665603066f, 0.000904370507f},
                                  {0.000813371211f, 0.000434809597f}, 3840, 3840, 8};
static const DewarpSpec kMaster = {1038.1936f, 1038.35229f, 1926.75354f, 1923.78381f,
                                   {0.0909626037f, -0.0467612185f, 0.0301645789f, -0.0117680114f, 0.00143191358f},
                                   {-0.000353124284f, -0.000570476695f}, 3840, 3840, 8};

// One DewarpParams: k1..k4 at 5..8, k5 at 15, p packed at 20, lens_model at 24.
static Bytes dewarp(const DewarpSpec& d) {
    Bytes pp;
    put_lef32(pp, d.p[0]);
    put_lef32(pp, d.p[1]);
    return cat({pb_f32(1, d.fx), pb_f32(2, d.fy), pb_f32(3, d.cx), pb_f32(4, d.cy), pb_f32(5, d.k[0]),
                pb_f32(6, d.k[1]), pb_f32(7, d.k[2]), pb_f32(8, d.k[3]), pb_f32(10, d.w), pb_f32(11, d.h),
                pb_f32(12, -179.489f), pb_f32(13, 90.159f), pb_f32(14, 0.334f), pb_f32(15, d.k[4]),
                pb_bytes(20, pp), pb_f32(24, d.model), pb_f32(25, 34.0f)});
}

// Entries 1 and 2 hold only a temperature on every real clip.
static Bytes lens_sample(const char* proto, const Bytes& slave, const Bytes& master) {
    Bytes hdr = cat({pb_str(1, proto), pb_str(10, "cam")});
    Bytes pano = cat({pb_bytes(1, pb_f32(25, 34.0f)), pb_bytes(2, pb_f32(25, 38.0f)), pb_bytes(3, slave),
                      pb_bytes(4, master)});
    Bytes stream = cat({pb_bytes(1, pb_str(3, "video")), pb_bytes(3, pb_v(1, 3840)), pb_bytes(5, pano)});
    return cat({pb_bytes(1, pb_bytes(1, hdr)), pb_bytes(2, stream)});
}

static bool lens_is(const LensCalibration& l, const DewarpSpec& d) {
    bool ok = l.fx == (double)d.fx && l.fy == (double)d.fy && l.cx == (double)d.cx && l.cy == (double)d.cy &&
              l.p1 == (double)d.p[0] && l.p2 == (double)d.p[1] && l.width == (int)d.w && l.height == (int)d.h;
    for (int i = 0; i < 5; i++) ok = ok && l.k[i] == (double)d.k[i];
    return ok;
}

static const LensCalibration* on_track(const std::vector<LensCalibration>& ls, int track) {
    for (const LensCalibration& l : ls)
        if (l.track == track) return &l;
    return nullptr;
}

static std::vector<LensCalibration> lenses_of(const Bytes& b) { return djmd_lenses(b.data(), b.size()); }

static void test_dji_lenses() {
    const auto ls = lenses_of(lens_sample("dvtm_AVATA360.proto", dewarp(kSlave), dewarp(kMaster)));
    check(ls.size() == 2, "lens: two lenses in an Avata 360 header");
    const LensCalibration *t0 = on_track(ls, 0), *t1 = on_track(ls, 1);
    check(t0 && t0->lens == "master" && lens_is(*t0, kMaster), "lens: 2.5.4 is the master lens, on track 0");
    check(t1 && t1->lens == "slave" && lens_is(*t1, kSlave), "lens: 2.5.3 is the slave lens, on track 1");

    check(lenses_of(lens_sample("dvtm_oq101.proto", dewarp(kSlave), dewarp(kMaster))).empty(),
          "lens: none read from an Osmo 360 header");
    check(lenses_of(lens_sample("dvtm_new.proto", dewarp(kSlave), dewarp(kMaster))).empty(),
          "lens: none read from an unknown proto");
    DewarpSpec other = kSlave;
    other.model = 7;
    const auto one = lenses_of(lens_sample("dvtm_AVATA360.proto", dewarp(other), dewarp(kMaster)));
    check(one.size() == 1 && one[0].track == 0, "lens: a lens_model other than 8 is not read");
    DewarpSpec zero = kMaster;
    zero.fx = 0;
    check(lenses_of(lens_sample("dvtm_AVATA360.proto", dewarp(kSlave), dewarp(zero))).size() == 1,
          "lens: a lens with no focal is not read");

    // Through a file: the header is looked for in the first samples, as for the colour.
    TrackSpec t{"djmd", "meta", "CAM meta", 60000, 1001,
                {lens_sample("dvtm_AVATA360.proto", dewarp(kSlave), dewarp(kMaster)),
                 avata_sample("dvtm_AVATA360.proto", false, 200847283ull)}};
    const Bytes file = build_mp4({t}, 60000, 2002, 0);
    const auto fl = video_lenses(file.data(), file.size());
    check(fl.size() == 2 && on_track(fl, 0) && lens_is(*on_track(fl, 0), kMaster),
          "lens: read from a file's clip header");
}

// Real clips: values as an independent protobuf walker read them.
static void test_avata_lenses() {
    const char* flight = spirula::env("TEST_AVATA_OSV");
    const char* hover = spirula::env("TEST_AVATA_OSV_HOVER");
    if (!flight && !hover) {
        std::printf("SKIP avata lenses (SS_TEST_AVATA_OSV and SS_TEST_AVATA_OSV_HOVER unset)\n");
        return;
    }
    if (hover) {
        const auto ls = video_lenses(std::string(hover));
        check(ls.size() == 2 && on_track(ls, 0) && lens_is(*on_track(ls, 0), kMaster) && on_track(ls, 1) &&
                  lens_is(*on_track(ls, 1), kSlave),
              "avata hover: factory lenses");
    }
    if (flight) {
        const DewarpSpec m = {1038.18347f, 1038.34192f, 1927.07397f, 1924.07776f,
                              {kMaster.k[0], kMaster.k[1], kMaster.k[2], kMaster.k[3], kMaster.k[4]},
                              {kMaster.p[0], kMaster.p[1]}, 3840, 3840, 8};
        const DewarpSpec sl = {1046.1012f, 1046.27332f, 1905.30188f, 1911.14246f,
                               {kSlave.k[0], kSlave.k[1], kSlave.k[2], kSlave.k[3], kSlave.k[4]},
                               {kSlave.p[0], kSlave.p[1]}, 3840, 3840, 8};
        const auto ls = video_lenses(std::string(flight));
        check(ls.size() == 2 && on_track(ls, 0) && lens_is(*on_track(ls, 0), m) && on_track(ls, 1) &&
                  lens_is(*on_track(ls, 1), sl),
              "avata flight: factory lenses");
    }
}

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
        std::printf("  gps[%d] t=%.3f unix=%.3f fix=%d  %.7f %.7f alt=%.3f speed=%.2f track=%.1f dop=%.2f",
                    i, g.t, g.unix_time, (int)g.fix, g.lat, g.lon, g.alt, g.speed, g.track, g.dop);
        if (g.has_rel_alt) std::printf(" rel_alt=%.3f", g.rel_alt);
        std::printf("\n");
    }
    for (int i = 0; i < n && i < (int)tm.exposure.size(); i++) {
        const TelemetryExposure& e = tm.exposure[i];
        std::printf("  exposure[%d] t=%.6f iso=%.1f shutter=%.9g ct=%.0f fnum=%.2f ev=%s\n", i, e.t, e.iso,
                    e.shutter, e.color_temp, e.fnum, e.has_ev ? std::to_string(e.ev).c_str() : "-");
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
            for (const LensCalibration& l : video_lenses(std::string(argv[i])))
                std::printf("lens track %d (%s): %dx%d fx %.4f fy %.4f cx %.4f cy %.4f k %.7g %.7g %.7g %.7g %.7g "
                            "p %.7g %.7g\n", l.track, l.lens.c_str(), l.width, l.height, l.fx, l.fy, l.cx, l.cy,
                            l.k[0], l.k[1], l.k[2], l.k[3], l.k[4], l.p1, l.p2);
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
    test_dji_avata();
    test_dji_osmo_ignores_avata_fields();
    test_dji_unknown_proto();
    test_avata_flight();
    test_avata_hover();
    test_dji_lenses();
    test_avata_lenses();
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
