// SensorView.cpp -- see SensorView.h.

#include "app/gui/SensorView.h"

#include "app/gui/Layout.h"
#include "app/gui/Subprocess.h"
#include "app/gui/Ui.h"
#include "i18n/catalog/SourcePreview.h"
#include "sfm/core/Attitude.h"
#include "sfm/core/Exif.h"
#ifdef SS_TOOL_SFM
#include "sfm/core/Telemetry.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <utility>

namespace pmsg = spirula::i18n::msg::preview;

namespace gui {

namespace {

constexpr double kEarthR = 6378137.0;
constexpr double kDeg = 3.14159265358979323846 / 180.0;
const ImVec4 kGood(0.40f, 0.80f, 0.45f, 1.0f);
const ImVec4 kWarn(0.95f, 0.75f, 0.30f, 1.0f);
const ImVec4 kBad(0.95f, 0.45f, 0.40f, 1.0f);
// How much the 3D track's heights may be stretched.
constexpr float kHeightScales[] = {1.0f, 2.0f, 5.0f, 10.0f, 20.0f};

std::string count_text(long long v) {
    std::string out;
    ui::detail::group_digits(std::to_string(v).c_str(), 0, out);
    return out;
}

std::string fixed(double v, int decimals) {
    char b[48];
    std::snprintf(b, sizeof b, "%.*f", decimals, v);
    return b;
}

std::string metres(double m) {
    if (m >= 1000.0)
        return spirula::i18n::format(pmsg::scale_km, {fixed(m / 1000.0, m >= 10000.0 ? 0 : 1)});
    return spirula::i18n::format(pmsg::scale_m, {fixed(m, m >= 10.0 ? 0 : 1)});
}

std::string clock_text(double s) {
    if (!(s >= 0.0)) s = 0.0;
    const long long ds = (long long)std::floor(s * 10.0 + 0.5);
    char b[48];
    std::snprintf(b, sizeof b, "%02lld:%02lld:%02lld.%lld", ds / 36000, (ds / 600) % 60,
                  (ds / 10) % 60, ds % 10);
    return b;
}

// 1, 2 or 5 times a power of ten: a scale bar and a grid read in round numbers.
double nice_step(double raw) {
    if (!(raw > 0.0)) return 1.0;
    const double p = std::pow(10.0, std::floor(std::log10(raw)));
    for (double m : {1.0, 2.0, 5.0, 10.0})
        if (m * p >= raw) return m * p;
    return 10.0 * p;
}

// Early to late along a capture: blue to amber, readable on the dark grid.
ImU32 time_colour(float f) {
    f = std::clamp(f, 0.0f, 1.0f);
    const float r = 0.25f + 0.75f * f, g = 0.55f + 0.20f * f, b = 1.0f - 0.85f * f;
    return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, 1.0f));
}

// Positions in metres about the mean of the kept ones (all of them, when
// none is kept): an equirectangular projection, exact enough over a capture.
void place(SensorData& d) {
    double la = 0.0, lo = 0.0;
    size_t n = 0;
    for (const SensorFix& f : d.fixes)
        if (!f.outlier) {
            la += f.lat;
            lo += f.lon;
            n++;
        }
    if (n == 0)
        for (const SensorFix& f : d.fixes) {
            la += f.lat;
            lo += f.lon;
            n++;
        }
    if (n == 0) return;
    d.lat0 = la / (double)n;
    d.lon0 = lo / (double)n;
    const double c = std::cos(d.lat0 * kDeg);
    for (SensorFix& f : d.fixes) {
        double dl = f.lon - d.lon0;
        if (dl > 180.0) dl -= 360.0;
        if (dl < -180.0) dl += 360.0;
        f.x = kEarthR * dl * kDeg * c;
        f.y = kEarthR * (f.lat - d.lat0) * kDeg;
    }
    std::set<std::pair<double, double>> seen;
    for (const SensorFix& f : d.fixes) seen.insert({f.lat, f.lon});
    d.distinct = seen.size();
}

// x, y and z, or a quantity's first, second and third reading.
const ImU32 kChannel[3] = {IM_COL32(224, 85, 85, 255), IM_COL32(78, 203, 113, 255),
                           IM_COL32(79, 142, 247, 255)};
const ImU32 kSecond = IM_COL32(224, 176, 85, 255);

double median_rate(const std::vector<float>& t) {
    if (t.size() < 3) return 0.0;
    std::vector<float> dt;
    dt.reserve(t.size() - 1);
    for (size_t i = 1; i < t.size(); i++)
        if (t[i] > t[i - 1]) dt.push_back(t[i] - t[i - 1]);
    if (dt.empty()) return 0.0;
    std::nth_element(dt.begin(), dt.begin() + dt.size() / 2, dt.end());
    return 1.0 / dt[dt.size() / 2];
}

// An angle carried across +-180 so a slow turn is a line, not a saw.
void unwrap_deg(std::vector<float>& a) {
    float offset = 0.0f;
    for (size_t i = 1; i < a.size(); i++) {
        const float raw = a[i] + offset;
        float d = raw - a[i - 1];
        while (d > 180.0f) {
            offset -= 360.0f;
            d -= 360.0f;
        }
        while (d < -180.0f) {
            offset += 360.0f;
            d += 360.0f;
        }
        a[i] = a[i - 1] + d;
    }
}

// Samples in time order, which every plot's search assumes. A carrier
// almost always writes them so; the odd one that does not is sorted.
void time_order(SensorSeries& s) {
    if (std::is_sorted(s.t.begin(), s.t.end())) return;
    std::vector<size_t> order(s.t.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return s.t[a] < s.t[b]; });
    auto apply = [&](std::vector<float>& v) {
        if (v.empty()) return;
        std::vector<float> out(v.size());
        for (size_t i = 0; i < order.size(); i++) out[i] = v[order[i]];
        v.swap(out);
    };
    apply(s.t);
    for (std::vector<float>& v : s.v) apply(v);
}

SensorSeries& add_series(SensorData& d, SensorSeries::Kind kind, int channels) {
    d.series.emplace_back();
    SensorSeries& s = d.series.back();
    s.kind = kind;
    s.channels = channels;
    return s;
}

const spirula::i18n::Msg& series_name(const SensorSeries& s, bool video) {
    switch (s.kind) {
        case SensorSeries::Speed: return pmsg::plot_speed;
        case SensorSeries::Altitude: return video ? pmsg::plot_altitude : pmsg::plot_altitude_photos;
        case SensorSeries::Dop: return pmsg::plot_dop;
        case SensorSeries::Rotation: return pmsg::plot_rotation;
        case SensorSeries::Acceleration: return pmsg::plot_acceleration;
        case SensorSeries::Gravity: return pmsg::plot_gravity;
        case SensorSeries::Attitude: return video ? pmsg::plot_attitude : pmsg::plot_gimbal;
        case SensorSeries::Magnet: return pmsg::plot_magnet;
        case SensorSeries::Step: return pmsg::plot_step;
        case SensorSeries::Exposure: return pmsg::plot_exposure;
        case SensorSeries::Shutter: return pmsg::plot_shutter;
        case SensorSeries::Iso: return pmsg::plot_iso;
        case SensorSeries::Aperture: return pmsg::plot_aperture;
    }
    return pmsg::plot_speed;
}

ImU32 channel_colour(const SensorSeries& s, int c) {
    if (s.ink[c]) return s.ink[c];
    if (s.channel[c] && (s.kind == SensorSeries::Speed || s.kind == SensorSeries::Altitude))
        return c == 0 ? kChannel[2] : kSecond;
    return kChannel[c % 3];
}

const char* channel_label(const SensorSeries& s, int c) {
    static const char* const xyz[3] = {"x", "y", "z"};
    return s.channel[c] ? s.channel[c]->get() : xyz[c % 3];
}

std::string shutter_text(double s) {
    if (s >= 0.3) return spirula::i18n::format(pmsg::shutter_seconds, {fixed(s, s < 10.0 ? 1 : 0)});
    return spirula::i18n::format(pmsg::shutter_fraction, {fixed(1.0 / s, 0)});
}

// A reading as the plot's quantity is read: an exposure time as 1/250 s.
std::string value_text(const SensorSeries& s, float v, int digits) {
    switch (s.kind) {
        case SensorSeries::Exposure: {
            const std::string n = fixed(std::fabs(v) < 0.05f ? 0.0f : v, 1);
            return spirula::i18n::format(pmsg::ev_value, {v >= 0.05f ? "+" + n : n});
        }
        case SensorSeries::Shutter: return shutter_text(std::exp2((double)v));
        case SensorSeries::Iso:
            return spirula::i18n::format(pmsg::iso_value, {fixed(std::exp2((double)v), 0)});
        case SensorSeries::Aperture:
            return spirula::i18n::format(pmsg::f_value, {fixed(std::exp2((double)v), 1)});
        default: return fixed(v, digits);
    }
}

bool is_exposure(const SensorSeries& s) {
    return s.kind == SensorSeries::Exposure || s.kind == SensorSeries::Shutter ||
           s.kind == SensorSeries::Iso || s.kind == SensorSeries::Aperture;
}

// The record at `t`: the nearest frame's, or exactly that photo's.
const SensorExposure* exposure_at(const SensorData& d, double t) {
    const std::vector<SensorExposure>& e = d.exposure;
    auto it = std::lower_bound(e.begin(), e.end(), (float)t,
                               [](const SensorExposure& a, float v) { return a.t < v; });
    if (it == e.end() || (it != e.begin() && t - it[-1].t < it->t - t)) {
        if (it == e.begin()) return nullptr;
        --it;
    }
    if (!d.video && std::fabs(it->t - t) > 0.5) return nullptr;
    return &*it;
}

std::string exposure_text(const SensorExposure& e) {
    std::string out;
    auto add = [&](const std::string& part) { out += (out.empty() ? "" : "  ·  ") + part; };
    if (e.shutter > 0.0f) add(shutter_text(e.shutter));
    if (e.iso > 0.0f) add(spirula::i18n::format(pmsg::iso_value, {fixed(e.iso, 0)}));
    if (e.fnum > 0.0f) add(spirula::i18n::format(pmsg::f_value, {fixed(e.fnum, 1)}));
    return out;
}

// What one fix says, under the pointer on the map or the 3D track.
void fix_tooltip(const SensorData& d, const SensorFix& f) {
    if (!ImGui::BeginTooltip()) return;
    if (d.video) ui::TextRaw(clock_text(f.t));
    else ui::Text(pmsg::tip_photo, {count_text(f.photo + 1)});
    ui::Text(pmsg::tip_position, {fixed(f.lat, 6), fixed(f.lon, 6)});
    if (f.has_alt) ui::Text(pmsg::tip_altitude, {fixed(f.alt, 1)});
    if (f.speed >= 0.0f) ui::Text(pmsg::tip_speed, {fixed(f.speed, 1)});
    if (f.dop > 0.0f) ui::Text(pmsg::tip_dop, {fixed(f.dop, 2)});
    if (f.has_yaw) {
        ui::Text(pmsg::tip_heading, {fixed(f.yaw, 1)});
        ui::Text(pmsg::tip_pitch, {fixed(f.pitch, 1)});
    }
    if (f.no_fix) ui::TextColored(kWarn, pmsg::tip_no_fix);
    else if (f.outlier) ui::TextColored(kBad, d.video ? pmsg::tip_outlier : pmsg::tip_far);
    ui::TextDisabled(d.video ? pmsg::tip_click_seek : pmsg::tip_click_photo);
    ImGui::EndTooltip();
}

// The 3D track's height stretch: as chosen, or by default the least that
// makes the climb a fifth of the track's width, so drift is not a hair.
float height_stretch(const SensorData& d, int choice) {
    if (choice > 0) return kHeightScales[std::min(choice, (int)std::size(kHeightScales)) - 1];
    double x0 = 1e300, x1 = -1e300, y0 = 1e300, y1 = -1e300, a0 = 1e300, a1 = -1e300;
    for (const SensorFix& f : d.fixes) {
        if (f.outlier) continue;
        x0 = std::min(x0, f.x);
        x1 = std::max(x1, f.x);
        y0 = std::min(y0, f.y);
        y1 = std::max(y1, f.y);
        if (f.has_alt) {
            a0 = std::min(a0, f.alt);
            a1 = std::max(a1, f.alt);
        }
    }
    const double rise = a1 - a0, width = std::max({x1 - x0, y1 - y0, 20.0});
    if (!(rise > 0.0)) return 1.0f;
    for (float k : kHeightScales)
        if (rise * k >= 0.2 * width) return k;
    return kHeightScales[std::size(kHeightScales) - 1];
}

// How long a stream may go quiet before it is a gap: a few of its own
// intervals. Any channel's reading counts -- derived speed has none while a
// position repeats, and that is not the receiver falling silent.
void set_gap(SensorSeries& s) {
    std::vector<float> dt;
    float last = -1.0f;
    for (size_t i = 0; i < s.t.size(); i++) {
        bool any = false;
        for (int ch = 0; ch < s.channels; ch++) any = any || std::isfinite(s.v[ch][i]);
        if (!any) continue;
        if (last >= 0.0f && s.t[i] > last) dt.push_back(s.t[i] - last);
        last = s.t[i];
    }
    if (dt.size() < 3) return;
    std::nth_element(dt.begin(), dt.begin() + dt.size() / 2, dt.end());
    s.gap = std::max(5.0f * dt[dt.size() / 2], 0.5f);
}

// Each factor as the stops of light it lets in (log2 t, log2 ISO, -2 log2 N),
// and their sum over the factors every record has: for one source, what
// --ppisp-exposure-from-exif compares (app/TrainerCore.cpp). 0 is the mean.
void add_exposure(SensorData& d) {
    const std::vector<SensorExposure>& in = d.exposure;
    if (in.empty()) return;
    const float none = std::numeric_limits<float>::quiet_NaN();
    const SensorSeries::Kind kinds[3] = {SensorSeries::Shutter, SensorSeries::Iso, SensorSeries::Aperture};
    const spirula::i18n::Msg* names[3] = {&pmsg::ch_shutter, &pmsg::ch_iso, &pmsg::ch_aperture};
    std::vector<float> stops[3];
    unsigned common = 7, any = 0;
    for (const SensorExposure& e : in) {
        const float f[3] = {e.shutter > 0.0f ? std::log2(e.shutter) : none,
                            e.iso > 0.0f ? std::log2(e.iso) : none,
                            e.fnum > 0.0f ? -2.0f * std::log2(e.fnum) : none};
        unsigned has = 0;
        for (int k = 0; k < 3; k++) {
            stops[k].push_back(f[k]);
            if (std::isfinite(f[k])) has |= 1u << k;
        }
        common &= has;
        any |= has;
    }
    double mean[3] = {0, 0, 0};
    float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
    for (int k = 0; k < 3; k++) {
        size_t n = 0;
        for (float v : stops[k])
            if (std::isfinite(v)) {
                mean[k] += v;
                lo[k] = std::min(lo[k], v);
                hi[k] = std::max(hi[k], v);
                n++;
            }
        if (n) mean[k] /= (double)n;
    }
    if (common) {
        SensorSeries& s = add_series(d, SensorSeries::Exposure, 1);
        const int factors = (common & 1) + (common >> 1 & 1) + (common >> 2 & 1);
        std::vector<int> shown;   // factors drawn beside the sum: those that moved
        for (int k = 0; k < 3; k++)
            if (common >> k & 1u && (factors == 1 || hi[k] - lo[k] > 0.05f)) shown.push_back(k);
        if (factors == 1) {
            s.channel[0] = names[shown[0]];
            s.ink[0] = kChannel[shown[0]];
        } else {
            s.channels = 1 + (int)shown.size();
            s.channel[0] = &pmsg::ch_total;
            s.ink[0] = IM_COL32(232, 232, 232, 255);
            for (size_t c = 0; c < shown.size(); c++) {
                s.channel[c + 1] = names[shown[c]];
                s.ink[c + 1] = kChannel[shown[c]];
            }
        }
        for (size_t i = 0; i < in.size(); i++) {
            s.t.push_back(in[i].t);
            float total = 0.0f;
            for (int k = 0; k < 3; k++)
                if (common >> k & 1u) total += stops[k][i] - (float)mean[k];
            s.v[0].push_back(total);
            if (factors > 1)
                for (size_t c = 0; c < shown.size(); c++)
                    s.v[c + 1].push_back(stops[shown[c]][i] - (float)mean[shown[c]]);
        }
        s.rate_hz = d.video ? median_rate(s.t) : 0.0;
    }
    for (int k = 0; k < 3; k++) {
        if (!(any >> k & 1u)) continue;
        SensorSeries& s = add_series(d, kinds[k], 1);
        s.ink[0] = kChannel[k];
        for (size_t i = 0; i < in.size(); i++) {
            s.t.push_back(in[i].t);
            s.v[0].push_back(k == 2 ? stops[k][i] / -2.0f : stops[k][i]);
        }
        s.rate_hz = d.video ? median_rate(s.t) : 0.0;
    }
}

#ifdef SS_TOOL_SFM
void add_vectors(SensorData& d, SensorSeries::Kind kind, const std::vector<sfm::TelemetryVec>& in,
                 double scale, double rate) {
    if (in.empty()) return;
    SensorSeries& s = add_series(d, kind, 3);
    s.rate_hz = rate;
    for (const sfm::TelemetryVec& v : in) {
        s.t.push_back((float)v.t);
        s.v[0].push_back((float)(v.x * scale));
        s.v[1].push_back((float)(v.y * scale));
        s.v[2].push_back((float)(v.z * scale));
    }
    time_order(s);
    if (s.rate_hz <= 0.0) s.rate_hz = median_rate(s.t);
}

std::shared_ptr<SensorData> read_video(const std::string& path) {
    auto d = std::make_shared<SensorData>();
    d->video = true;
    d->done = true;
    sfm::Telemetry t;
    std::string error;
    if (!sfm::telemetry_read(path, t, error)) {
        d->error = error;
        return d;
    }
    const sfm::TelemetryCheck c = sfm::telemetry_check(t);
    d->duration = t.video_duration;
    if (t.carrier != sfm::TelemetryCarrier::None)
        d->carrier = sfm::telemetry_carrier_name(t.carrier);
    d->camera = t.camera;
    d->zero = t.gps_zero;
    d->warnings = c.warnings;
    d->report = sfm::telemetry_report(t, c);
    d->has_gps = !t.gps.empty();
    d->has_imu = !t.gyro.empty() || !t.accel.empty() || !t.orientation.empty();
    d->gps_usable = c.gps_usable;
    d->imu_usable = c.imu_usable;
    d->path_m = c.gps_path_m;
    d->spread_m = c.gps_spread_m;
    d->rate_hz = c.gps.rate_hz;
    d->imu_rate_hz = c.gyro.rate_hz > 0.0 ? c.gyro.rate_hz : c.accel.rate_hz;

    std::vector<sfm::TelemetryGps> kept;
    sfm::telemetry_gps_filter(t, kept);
    std::vector<const sfm::TelemetryGps*> source;   // per fix, what it was read from
    size_t k = 0;
    for (const sfm::TelemetryGps& g : t.gps) {
        const bool valid = sfm::gps_valid(g);
        if (!valid && (std::fabs(g.lat) > 90.0 || std::fabs(g.lon) > 180.0)) continue;
        if (!valid) d->no_fix++;
        SensorFix f;
        f.t = g.t;
        f.lat = g.lat;
        f.lon = g.lon;
        f.alt = g.alt;
        f.has_alt = g.has_alt;
        f.speed = (float)g.speed;
        f.dop = (float)g.dop;
        if (!valid)
            f.outlier = f.no_fix = true;
        else if (k < kept.size() && kept[k].t == g.t && kept[k].lat == g.lat && kept[k].lon == g.lon)
            k++;
        else {
            f.outlier = true;
            d->outliers++;
        }
        d->fixes.push_back(f);
        source.push_back(&g);
    }
    place(*d);

    // The GPS streams, on the kept fixes' times. A channel with nothing at a
    // fix holds NaN there, so two readings of one quantity share an axis.
    const float none = std::numeric_limits<float>::quiet_NaN();
    SensorSeries speed, alt, dop;
    speed.kind = SensorSeries::Speed;
    alt.kind = SensorSeries::Altitude;
    dop.kind = SensorSeries::Dop;
    bool reported = false, above = false, any_alt = false, any_dop = false;
    const SensorFix* last = nullptr;   // the last position that changed
    for (size_t i = 0; i < d->fixes.size(); i++) {
        const SensorFix& f = d->fixes[i];
        if (f.outlier) continue;
        const float ft = (float)f.t;
        // Between positions that changed: a receiver logging faster than it
        // fixes repeats each one, and a zero between two jumps means nothing.
        const bool moved = !last || f.x != last->x || f.y != last->y;
        const float derived = last && moved && f.t > last->t
                                  ? (float)(std::hypot(f.x - last->x, f.y - last->y) / (f.t - last->t))
                                  : none;
        if (moved) last = &f;
        speed.t.push_back(ft);
        speed.v[0].push_back(derived);
        speed.v[1].push_back(f.speed >= 0.0f ? f.speed : none);
        reported = reported || f.speed >= 0.0f;
        alt.t.push_back(ft);
        alt.v[0].push_back(f.has_alt ? (float)f.alt : none);
        alt.v[1].push_back(source[i]->has_rel_alt ? (float)source[i]->rel_alt : none);
        any_alt = any_alt || f.has_alt;
        above = above || source[i]->has_rel_alt;
        if (f.dop > 0.0f) {
            dop.t.push_back(ft);
            dop.v[0].push_back(f.dop);
            any_dop = true;
        }
    }
    if (!speed.t.empty()) {
        speed.channels = reported ? 2 : 1;
        speed.channel[0] = &pmsg::ch_from_positions;
        speed.channel[1] = &pmsg::ch_reported;
        speed.rate_hz = d->rate_hz;
        d->series.push_back(std::move(speed));
    }
    if (any_alt || above) {
        alt.channels = above ? 2 : 1;
        alt.channel[0] = &pmsg::ch_gps;
        alt.channel[1] = &pmsg::ch_above_takeoff;
        alt.rate_hz = d->rate_hz;
        d->series.push_back(std::move(alt));
    }
    if (any_dop) {
        dop.rate_hz = d->rate_hz;
        d->series.push_back(std::move(dop));
    }
    for (SensorSeries& s : d->series) time_order(s);

    add_vectors(*d, SensorSeries::Rotation, t.gyro, 1.0 / kDeg, c.gyro.rate_hz);
    add_vectors(*d, SensorSeries::Acceleration, t.accel, 1.0, c.accel.rate_hz);
    if (!t.accel.empty()) d->series.back().reference = 9.81f;
    add_vectors(*d, SensorSeries::Gravity, t.gravity, 1.0, c.gravity.rate_hz);
    add_vectors(*d, SensorSeries::Magnet, t.magnet, 1.0, 0.0);
    if (!t.orientation.empty()) {
        // Z-Y-X angles of the attitude as the camera wrote it.
        SensorSeries& s = add_series(*d, SensorSeries::Attitude, 3);
        s.channel[0] = &pmsg::ch_yaw;
        s.channel[1] = &pmsg::ch_pitch;
        s.channel[2] = &pmsg::ch_roll;
        s.rate_hz = c.orientation.rate_hz;
        for (const sfm::TelemetryQuat& q : t.orientation) {
            s.t.push_back((float)q.t);
            const double sinp = std::clamp(2.0 * (q.w * q.y - q.z * q.x), -1.0, 1.0);
            s.v[0].push_back((float)(std::atan2(2.0 * (q.w * q.z + q.x * q.y),
                                                1.0 - 2.0 * (q.y * q.y + q.z * q.z)) / kDeg));
            s.v[1].push_back((float)(std::asin(sinp) / kDeg));
            s.v[2].push_back((float)(std::atan2(2.0 * (q.w * q.x + q.y * q.z),
                                                1.0 - 2.0 * (q.x * q.x + q.y * q.y)) / kDeg));
        }
        time_order(s);
        unwrap_deg(s.v[0]);
        unwrap_deg(s.v[2]);
    }
    for (const sfm::TelemetryExposure& e : t.exposure) {
        SensorExposure x;
        x.t = (float)e.t;
        x.shutter = (float)e.shutter;
        x.iso = (float)e.iso;
        x.fnum = (float)e.fnum;
        if (x.shutter > 0.0f || x.iso > 0.0f || x.fnum > 0.0f) d->exposure.push_back(x);
    }
    std::stable_sort(d->exposure.begin(), d->exposure.end(),
                     [](const SensorExposure& a, const SensorExposure& b) { return a.t < b.t; });
    add_exposure(*d);
    for (SensorSeries& s : d->series) set_gap(s);
    return d;
}
#endif

// Photos far from where the rest were taken: a sign dropped, a fix left over
// from the last place the camera saw the sky.
void mark_far_photos(SensorData& d) {
    if (d.fixes.size() < 3) return;
    auto median = [](std::vector<double> v) {
        std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
        return v[v.size() / 2];
    };
    std::vector<double> lat, lon;
    for (const SensorFix& f : d.fixes) {
        lat.push_back(f.lat);
        lon.push_back(f.lon);
    }
    const double mlat = median(lat), mlon = median(lon);
    const double c = std::cos(mlat * kDeg);
    std::vector<double> dist;
    for (const SensorFix& f : d.fixes)
        dist.push_back(kEarthR * kDeg * std::hypot((f.lat - mlat), (f.lon - mlon) * c));
    const double typical = median(dist);
    const double limit = std::max(200.0, 10.0 * typical);
    d.outliers = 0;
    for (size_t i = 0; i < d.fixes.size(); i++) {
        d.fixes[i].outlier = dist[i] > limit;
        d.outliers += d.fixes[i].outlier;
    }
}

}  // namespace

bool SensorView::reads_video() {
#ifdef SS_TOOL_SFM
    return true;
#else
    return false;
#endif
}

SensorView::~SensorView() { join(); }

void SensorView::join() {
    _cancel = true;
    if (_worker.joinable()) _worker.join();
    _cancel = false;
}

void SensorView::clear() {
    join();
    std::lock_guard<std::mutex> lk(_mu);
    _key.clear();
    _data.reset();
    _fit = true;
}

std::shared_ptr<const SensorData> SensorView::data() const {
    std::lock_guard<std::mutex> lk(_mu);
    return _data;
}

void SensorView::load_video(const std::string& key, const std::string& path) {
    if (key == _key) return;
    clear();
    _key = key;
#ifdef SS_TOOL_SFM
    _worker = std::thread([this, path] {
        std::shared_ptr<SensorData> d = read_video(path);
        std::lock_guard<std::mutex> lk(_mu);
        if (!_cancel.load()) _data = std::move(d);
    });
#else
    (void)path;
    auto d = std::make_shared<SensorData>();
    d->video = true;
    d->done = true;
    _data = std::move(d);
#endif
}

void SensorView::load_photos(const std::string& key, std::vector<std::string> files) {
    if (key == _key) return;
    clear();
    _key = key;
    _worker = std::thread([this, files = std::move(files)] {
        SensorData d;
        d.photos = (int)files.size();
        // Per photo, NaN where it says nothing: the plots run by photo.
        const float none = std::numeric_limits<float>::quiet_NaN();
        SensorSeries alt, look;
        alt.kind = SensorSeries::Altitude;
        alt.channel[0] = &pmsg::ch_gps;
        alt.channel[1] = &pmsg::ch_above_takeoff;
        look.kind = SensorSeries::Attitude;
        look.channels = 3;
        look.channel[0] = &pmsg::ch_yaw;
        look.channel[1] = &pmsg::ch_pitch;
        look.channel[2] = &pmsg::ch_roll;
        bool above = false;
        auto publish = [&](bool done) {
            auto snap = std::make_shared<SensorData>(d);
            snap->done = done;
            place(*snap);
            mark_far_photos(*snap);
            // How far each photo is from the one before it: a jump stands out
            // of a survey's even steps.
            SensorSeries step;
            step.kind = SensorSeries::Step;
            const SensorFix* prev = nullptr;
            for (const SensorFix& f : snap->fixes) {
                if (prev) {
                    step.t.push_back((float)f.t);
                    step.v[0].push_back((float)std::hypot(f.x - prev->x, f.y - prev->y));
                }
                prev = &f;
            }
            if (!alt.empty()) {
                SensorSeries a = alt;
                a.channels = above ? 2 : 1;
                snap->series.push_back(std::move(a));
            }
            if (!step.empty()) snap->series.push_back(std::move(step));
            // Yaw as written: a survey turns about between its lines, and
            // unwrapping those turns would wind it up by thousands of degrees.
            if (!look.empty()) snap->series.push_back(look);
            add_exposure(*snap);
            std::lock_guard<std::mutex> lk(_mu);
            if (!_cancel.load()) _data = std::move(snap);
        };
        for (size_t i = 0; i < files.size() && !_cancel.load(); i++) {
            const sfm::ExifData e = sfm::readExif(files[i]);
            const std::string xmp = sfm::readXmpPacket(files[i]);
            const sfm::CameraAttitude a = sfm::parseCameraAttitude(xmp);
            double rel = 0.0;
            const bool has_rel = sfm::detail::xmpNumber(xmp, "drone-dji:RelativeAltitude", rel);
            if (e.has_gps) {
                SensorFix f;
                f.t = (double)i;
                f.lat = e.lat_deg;
                f.lon = e.lon_deg;
                f.alt = e.alt_m;
                f.has_alt = e.has_alt;
                f.photo = (int)i;
                f.has_yaw = a.valid;
                f.yaw = (float)a.yaw_deg;
                f.pitch = (float)a.pitch_deg;
                d.fixes.push_back(f);
                d.with_gps++;
            }
            if (e.has_alt || has_rel) {
                alt.t.push_back((float)i);
                alt.v[0].push_back(e.has_gps && e.has_alt ? (float)e.alt_m : none);
                alt.v[1].push_back(has_rel ? (float)rel : none);
                above = above || has_rel;
            }
            if (a.valid) {
                look.t.push_back((float)i);
                look.v[0].push_back((float)a.yaw_deg);
                look.v[1].push_back((float)a.pitch_deg);
                look.v[2].push_back((float)a.roll_deg);
                d.with_attitude++;
            }
            SensorExposure x;
            x.t = (float)i;
            x.shutter = (float)(e.exposure_time > 0 ? e.exposure_time
                                : std::isfinite(e.shutter_apex) ? std::exp2(-e.shutter_apex) : -1.0);
            x.fnum = (float)(e.f_number > 0 ? e.f_number
                             : std::isfinite(e.aperture_apex) ? std::exp2(e.aperture_apex / 2) : -1.0);
            x.iso = e.iso > 0 ? (float)e.iso : -1.0f;
            if (x.shutter > 0.0f || x.iso > 0.0f || x.fnum > 0.0f) {
                d.exposure.push_back(x);
                d.with_exposure++;
            }
            d.zero += e.gps_zero;
            d.scanned = (int)i + 1;
            if ((i + 1) % 200 == 0) publish(false);
        }
        d.has_gps = d.with_gps > 0;
        publish(true);
    });
}

void SensorView::draw_status(const SensorData& d) {
    // A coloured word per sensor, and the checks behind it on hover.
    auto chip = [&](const ImVec4& c, const std::string& text, const std::string& tip) {
        ui::TextColoredRaw(c, text);
        if (!tip.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) &&
            ImGui::BeginTooltip()) {
            ImGui::PushTextWrapPos(px(460.0f));
            ui::TextRaw(tip);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
        ImGui::SameLine(0.0f, px(16.0f));
    };
    std::string gps_tip, imu_tip;
    for (const std::string& w : d.warnings) {
        std::string& tip = w.rfind("GPS", 0) == 0 ? gps_tip : imu_tip;
        tip += (tip.empty() ? "" : "\n") + w;
    }
    if (d.video) {
        if (!d.has_gps) chip(ImGui::GetStyle().Colors[ImGuiCol_TextDisabled], pmsg::gps_none.get(), "");
        else if (d.gps_usable) chip(kGood, pmsg::gps_usable.get(), gps_tip);
        else chip(kWarn, pmsg::gps_unusable.get(),
                  gps_tip.empty() ? std::string(pmsg::gps_unusable_help.get()) : gps_tip);
        if (!d.has_imu) chip(ImGui::GetStyle().Colors[ImGuiCol_TextDisabled], pmsg::imu_none.get(), "");
        else if (d.imu_usable) chip(kGood, pmsg::imu_usable.get(), imu_tip);
        else chip(kWarn, pmsg::imu_unusable.get(),
                  imu_tip.empty() ? std::string(pmsg::imu_unusable_help.get()) : imu_tip);
        if (!d.exposure.empty())
            chip(kGood, spirula::i18n::format(pmsg::exposure_records,
                                              {count_text((long long)d.exposure.size())}),
                 pmsg::exposure_records_help.get());
    } else {
        const ImVec4 c = d.with_gps == 0 ? ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]
                         : d.with_gps < d.photos ? kWarn : kGood;
        chip(c, spirula::i18n::format(pmsg::gps_photos,
                                      {count_text(d.with_gps), count_text(d.photos)}),
             pmsg::gps_photos_help.get());
        if (d.with_attitude > 0)
            chip(kGood, spirula::i18n::format(pmsg::attitude_photos,
                                              {count_text(d.with_attitude), count_text(d.photos)}),
                 pmsg::attitude_photos_help.get());
        if (d.with_exposure > 0)
            chip(d.with_exposure < d.photos ? kWarn : kGood,
                 spirula::i18n::format(pmsg::exposure_photos,
                                       {count_text(d.with_exposure), count_text(d.photos)}),
                 pmsg::exposure_photos_help.get());
        if (d.with_gps > 0 && d.distinct * 4 < (size_t)d.with_gps)
            chip(kWarn, spirula::i18n::format(pmsg::gps_distinct, {count_text((long long)d.distinct)}),
                 pmsg::gps_distinct_help.get());
        if (!d.done)
            chip(ImGui::GetStyle().Colors[ImGuiCol_TextDisabled],
                 spirula::i18n::format(pmsg::reading_photos,
                                       {count_text(d.scanned), count_text(d.photos)}),
                 "");
    }
    if (d.outliers > 0)
        chip(kBad,
             spirula::i18n::format(d.video ? pmsg::gps_outliers : pmsg::gps_far,
                                   {count_text((long long)d.outliers)}),
             d.video ? pmsg::gps_outliers_help.get() : pmsg::gps_far_help.get());
    if (d.no_fix > 0)
        chip(kWarn, spirula::i18n::format(pmsg::gps_no_fix, {count_text((long long)d.no_fix)}),
             pmsg::gps_no_fix_help.get());
    if (d.zero > 0)
        chip(ImGui::GetStyle().Colors[ImGuiCol_TextDisabled],
             spirula::i18n::format(pmsg::gps_zero, {count_text((long long)d.zero)}),
             pmsg::gps_zero_help.get());
    ImGui::NewLine();
}

SensorView::Result SensorView::draw(float height, double playhead, double clip_start,
                                    double clip_end) {
    Result r;
    const std::shared_ptr<const SensorData> held = data();
    if (!held) {
        ui::TextDisabled(pmsg::reading_sensors);
        return r;
    }
    const SensorData& d = *held;
    if (!d.error.empty()) {
        ui::TextColoredWrappedRaw(kBad, d.error);
        return r;
    }
    if (d.video && !reads_video()) {
        ui::TextDisabledWrapped(pmsg::sensors_not_in_build);
        return r;
    }
    if (d.video && !d.has_gps && !d.has_imu && d.exposure.empty()) {
        ui::TextDisabledWrapped(pmsg::sensors_none_video);
        return r;
    }
    if (!d.video && d.done && d.with_gps == 0 && d.with_attitude == 0 && d.with_exposure == 0) {
        draw_status(d);
        ui::TextDisabledWrapped(pmsg::sensors_none_photos);
        return r;
    }

    const float y0 = ImGui::GetCursorPosY();
    draw_status(d);
    if (!d.carrier.empty() || !d.camera.empty()) {
        ui::TextDisabledRaw(d.camera.empty() ? d.carrier
                            : d.carrier.empty() ? d.camera : d.camera + " · " + d.carrier);
    }
    const float used = ImGui::GetCursorPosY() - y0;
    const float avail = std::max(px(120.0f), height - used);

    // The plots' span: the whole capture until a plot is zoomed.
    double full = d.video ? d.duration : std::max(1.0, (double)d.photos - 1.0);
    for (const SensorSeries& s : d.series)
        if (!s.empty() && !(full > 0.0)) full = std::max(full, (double)s.t.back());
    _full = std::max(full, 1e-3);
    if (!_zoomed || !(_win1 > _win0)) {
        _win0 = 0.0;
        _win1 = _full;
    }

    // The track above, the plots below, each plot an equal share of what
    // the track leaves; the pane scrolls when there are too many to fit.
    std::vector<const SensorSeries*> shown;
    for (const SensorSeries& s : d.series)
        if (!s.empty() && !_hidden.count(s.kind)) shown.push_back(&s);
    const bool track = !d.fixes.empty();
    const float row = ImGui::GetFrameHeightWithSpacing();
    const float gap = ImGui::GetStyle().ItemSpacing.y;
    const float fixed_h = (track ? row : 0.0f) + (d.series.empty() ? 0.0f : row) +
                          (d.report.empty() ? 0.0f : row);
    const int n = (int)shown.size();
    const float plot_h =
        n == 0 ? 0.0f
               : std::clamp((track ? avail * 0.5f : avail - fixed_h) / (float)n - gap, px(56.0f),
                            px(track ? 120.0f : 240.0f));
    const float view_h =
        std::max(px(180.0f), avail - fixed_h - (float)n * (plot_h + gap) - 2.0f * gap);

    ImGui::BeginChild("##sensorsbody", ImVec2(0, avail));
    if (track) {
        if (ui::RadioButton(pmsg::track_map, !_track3d)) _track3d = false;
        ui::help_on_hover(pmsg::track_map_help);
        ImGui::SameLine();
        if (ui::RadioButton(pmsg::track_3d, _track3d)) _track3d = true;
        ui::help_on_hover(pmsg::track_3d_help);
        ImGui::SameLine(0.0f, px(16.0f));
        if (ui::SmallButton(pmsg::map_fit)) {
            _fit = true;
            _fit3d = true;
        }
        ui::help_on_hover(_track3d ? pmsg::track_3d_help : pmsg::map_fit_help);
        if (_track3d) {
            // A few metres of drift over a long walk is a hair at true scale.
            ImGui::SameLine();
            ui::TextDisabled(pmsg::height_scale);
            ImGui::SameLine();
            std::vector<std::string> names;
            std::vector<const char*> items;
            names.push_back(spirula::i18n::format(
                pmsg::height_auto,
                {spirula::i18n::format(pmsg::height_times, {fixed(height_stretch(d, 0), 0)})}));
            for (float k : kHeightScales)
                names.push_back(spirula::i18n::format(pmsg::height_times, {fixed(k, 0)}));
            for (const std::string& s : names) items.push_back(s.c_str());
            ImGui::SetNextItemWidth(ImGui::CalcTextSize(names[0].c_str()).x + ImGui::GetFrameHeight() +
                                    ImGui::GetStyle().FramePadding.x * 3.0f);
            if (ui::ComboRaw("##heightscale", &_height_scale, items.data(), (int)items.size()))
                _fit3d = true;
            ui::help_on_hover(pmsg::height_scale_help);
        }
        ImGui::SameLine();
        if (ui::SmallButton(pmsg::map_open_osm)) {
            char url[160];
            std::snprintf(url, sizeof url,
                          "https://www.openstreetmap.org/?mlat=%.6f&mlon=%.6f#map=17/%.6f/%.6f",
                          d.lat0, d.lon0, d.lat0, d.lon0);
            open_url(url);
        }
        ui::help_on_hover(pmsg::map_open_osm_help);
        ImGui::SameLine();
        ui::TextDisabledRaw(fixed(d.lat0, 6) + ", " + fixed(d.lon0, 6));
        const ImVec2 size(ImGui::GetContentRegionAvail().x, view_h);
        if (_track3d) draw_track3d(d, size, playhead, r);
        else draw_map(d, size, playhead, r);
    }
    if (!d.series.empty()) {
        if (ui::SmallButton(pmsg::plots_button)) ImGui::OpenPopup("##plotpick");
        ui::help_on_hover(pmsg::plots_help);
        if (ImGui::BeginPopup("##plotpick")) {
            for (const SensorSeries& s : d.series) {
                bool on = !_hidden.count(s.kind);
                if (ui::Checkbox(series_name(s, d.video), &on)) {
                    if (on) _hidden.erase(s.kind);
                    else _hidden.insert(s.kind);
                }
            }
            ImGui::EndPopup();
        }
        if (_zoomed) {
            ImGui::SameLine();
            if (ui::SmallButton(pmsg::whole_length)) _zoomed = false;
            ui::help_on_hover(pmsg::whole_length_help);
        }
        for (const SensorSeries* s : shown)
            draw_series(d, *s, plot_h, playhead, clip_start, clip_end, r);
    }
    if (!d.report.empty() && ui::CollapsingHeader(pmsg::diagnostics)) {
        std::string text = d.report;
        ui::InputTextMultilineRaw("##report", &text,
                                  ImVec2(-1.0f, ImGui::GetTextLineHeight() * 16.0f),
                                  ImGuiInputTextFlags_ReadOnly);
    }
    ImGui::EndChild();
    return r;
}

void SensorView::draw_map(const SensorData& d, const ImVec2& size, double playhead, Result& r) {
    const ImVec2 at = ImGui::GetCursorScreenPos();
    ui::InvisibleButtonRaw("##map", size);
    // The wheel zooms the map and stays here, not scrolling the pane.
    ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const ImGuiIO& io = ImGui::GetIO();
    const ImVec2 moved = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
    const bool clicked = ImGui::IsItemDeactivated() && moved.x * moved.x + moved.y * moved.y < 9.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(at, ImVec2(at.x + size.x, at.y + size.y), true);
    dl->AddRectFilled(at, ImVec2(at.x + size.x, at.y + size.y), IM_COL32(22, 24, 28, 255));

    // Fitted to the data until the user moves the view, so a folder still
    // being read keeps growing into the frame.
    if (_fit || !(_scale > 0.0)) {
        double x0 = 1e300, x1 = -1e300, y0 = 1e300, y1 = -1e300;
        bool any_kept = false;
        for (const SensorFix& f : d.fixes) any_kept = any_kept || !f.outlier;
        for (const SensorFix& f : d.fixes) {
            if (any_kept && f.outlier) continue;
            x0 = std::min(x0, f.x);
            x1 = std::max(x1, f.x);
            y0 = std::min(y0, f.y);
            y1 = std::max(y1, f.y);
        }
        // Never closer than 20 m across: a stuck receiver is one dot, not a
        // magnified pixel.
        const double w = std::max(x1 - x0, 20.0), h = std::max(y1 - y0, 20.0);
        _cx = 0.5 * (x0 + x1);
        _cy = 0.5 * (y0 + y1);
        _scale = std::min(size.x / w, size.y / h) * 0.88;
    }
    auto to_screen = [&](double x, double y) {
        return ImVec2(at.x + size.x * 0.5f + (float)((x - _cx) * _scale),
                      at.y + size.y * 0.5f - (float)((y - _cy) * _scale));
    };

    if (hovered && io.MouseWheel != 0.0f) {
        const double mx = _cx + (io.MousePos.x - (at.x + size.x * 0.5f)) / _scale;
        const double my = _cy - (io.MousePos.y - (at.y + size.y * 0.5f)) / _scale;
        const double f = std::pow(1.2, (double)io.MouseWheel);
        _scale = std::clamp(_scale * f, 1e-6, 1e4);
        _cx = mx - (io.MousePos.x - (at.x + size.x * 0.5f)) / _scale;
        _cy = my + (io.MousePos.y - (at.y + size.y * 0.5f)) / _scale;
        _fit = false;
    }
    if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f)) {
        _cx -= io.MouseDelta.x / _scale;
        _cy += io.MouseDelta.y / _scale;
        _fit = false;
    }
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) _fit = true;

    // A metric grid at a round spacing, and a scale bar of the same.
    const double step = nice_step(70.0 / _scale);
    const ImU32 grid = IM_COL32(255, 255, 255, 18);
    const double gx0 = _cx - size.x * 0.5 / _scale, gx1 = _cx + size.x * 0.5 / _scale;
    const double gy0 = _cy - size.y * 0.5 / _scale, gy1 = _cy + size.y * 0.5 / _scale;
    for (double x = std::floor(gx0 / step) * step; x <= gx1; x += step) {
        const float sx = to_screen(x, 0).x;
        dl->AddLine(ImVec2(sx, at.y), ImVec2(sx, at.y + size.y), grid);
    }
    for (double y = std::floor(gy0 / step) * step; y <= gy1; y += step) {
        const float sy = to_screen(0, y).y;
        dl->AddLine(ImVec2(at.x, sy), ImVec2(at.x + size.x, sy), grid);
    }

    // The track: kept fixes joined in order and coloured by time. A long log
    // is thinned to a few thousand segments, which is what a pane can show.
    std::vector<size_t> kept;
    for (size_t i = 0; i < d.fixes.size(); i++)
        if (!d.fixes[i].outlier) kept.push_back(i);
    const size_t stride = std::max<size_t>(1, kept.size() / 4000);
    const float n = (float)std::max<size_t>(1, d.fixes.size() - 1);
    const float th = px(d.video ? 2.0f : 1.0f);
    for (size_t k = stride; k < kept.size(); k += stride) {
        const SensorFix& a = d.fixes[kept[k - stride]];
        const SensorFix& b = d.fixes[kept[k]];
        dl->AddLine(to_screen(a.x, a.y), to_screen(b.x, b.y),
                    time_colour((float)kept[k] / n), th);
    }
    if (!d.video)
        for (size_t i : kept) {
            const SensorFix& f = d.fixes[i];
            const ImVec2 p = to_screen(f.x, f.y);
            const ImU32 col = time_colour((float)i / n);
            // Which way the camera faced: a drone's photos fly a pattern, and
            // one turned the wrong way stands out of it.
            if (f.has_yaw) {
                const float a = f.yaw * (float)kDeg;
                dl->AddLine(p, ImVec2(p.x + std::sin(a) * px(11.0f), p.y - std::cos(a) * px(11.0f)),
                            col, px(1.5f));
            }
            dl->AddCircleFilled(p, px(3.0f), col);
        }
    // Dropped fixes, where they are -- or pinned to the edge, pointing the
    // way, when they are off the map.
    const ImU32 red = IM_COL32(240, 80, 70, 255), amber = ImGui::ColorConvertFloat4ToU32(kWarn);
    for (const SensorFix& f : d.fixes) {
        if (!f.outlier) continue;
        const ImU32 col = f.no_fix ? amber : red;
        ImVec2 p = to_screen(f.x, f.y);
        const bool inside = p.x >= at.x && p.x <= at.x + size.x && p.y >= at.y && p.y <= at.y + size.y;
        if (!inside) {
            p.x = std::clamp(p.x, at.x + px(6.0f), at.x + size.x - px(6.0f));
            p.y = std::clamp(p.y, at.y + px(6.0f), at.y + size.y - px(6.0f));
            dl->AddCircle(p, px(5.0f), col, 0, px(2.0f));
            continue;
        }
        const float s = px(4.0f);
        dl->AddLine(ImVec2(p.x - s, p.y - s), ImVec2(p.x + s, p.y + s), col, px(2.0f));
        dl->AddLine(ImVec2(p.x - s, p.y + s), ImVec2(p.x + s, p.y - s), col, px(2.0f));
    }
    if (!kept.empty()) {
        const SensorFix& a = d.fixes[kept.front()];
        const SensorFix& b = d.fixes[kept.back()];
        dl->AddCircleFilled(to_screen(a.x, a.y), px(5.5f), IM_COL32(70, 185, 90, 255));
        const ImVec2 e = to_screen(b.x, b.y);
        dl->AddRectFilled(ImVec2(e.x - px(5.0f), e.y - px(5.0f)), ImVec2(e.x + px(5.0f), e.y + px(5.0f)),
                          IM_COL32(220, 70, 60, 255));
    }
    // Where the camera was at the video's playhead.
    if (d.video && playhead >= 0.0 && kept.size() >= 2) {
        size_t hi = 0;
        while (hi < kept.size() && d.fixes[kept[hi]].t < playhead) hi++;
        const SensorFix& b = d.fixes[kept[std::min(hi, kept.size() - 1)]];
        const SensorFix& a = d.fixes[kept[hi > 0 ? hi - 1 : 0]];
        const double span = b.t - a.t;
        const double f = span > 0.0 ? std::clamp((playhead - a.t) / span, 0.0, 1.0) : 0.0;
        const ImVec2 p = to_screen(a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f);
        dl->AddCircle(p, px(8.0f), IM_COL32(0, 0, 0, 255), 0, px(4.0f));
        dl->AddCircle(p, px(8.0f), IM_COL32(255, 255, 255, 255), 0, px(2.0f));
    }

    // Scale bar and north, bottom left and top right.
    const double bar = nice_step(90.0 / _scale);
    const float bar_px = (float)(bar * _scale);
    const ImVec2 b0(at.x + px(10.0f), at.y + size.y - px(12.0f));
    dl->AddLine(b0, ImVec2(b0.x + bar_px, b0.y), IM_COL32(230, 230, 230, 255), px(2.0f));
    dl->AddLine(ImVec2(b0.x, b0.y - px(4.0f)), ImVec2(b0.x, b0.y + px(1.0f)), IM_COL32(230, 230, 230, 255), px(2.0f));
    dl->AddLine(ImVec2(b0.x + bar_px, b0.y - px(4.0f)), ImVec2(b0.x + bar_px, b0.y + px(1.0f)),
                IM_COL32(230, 230, 230, 255), px(2.0f));
    const std::string bar_label = metres(bar);
    dl->AddText(ImVec2(b0.x, b0.y - ImGui::GetTextLineHeight() - px(4.0f)),
                IM_COL32(230, 230, 230, 255), bar_label.c_str());
    const ImVec2 nn(at.x + size.x - px(18.0f), at.y + px(10.0f));
    dl->AddTriangleFilled(ImVec2(nn.x, nn.y), ImVec2(nn.x - px(5.0f), nn.y + px(12.0f)),
                          ImVec2(nn.x + px(5.0f), nn.y + px(12.0f)), IM_COL32(230, 230, 230, 255));
    dl->AddText(ImVec2(nn.x - ImGui::CalcTextSize("N").x * 0.5f, nn.y + px(13.0f)),
                IM_COL32(230, 230, 230, 255), "N");

    // The fix under the pointer: what it says, and a click goes to it.
    if (hovered && !ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f)) {
        int best = -1;
        float best_d = px(9.0f) * px(9.0f);
        const size_t probe_stride = std::max<size_t>(1, d.fixes.size() / 20000);
        for (size_t i = 0; i < d.fixes.size(); i += probe_stride) {
            const ImVec2 p = to_screen(d.fixes[i].x, d.fixes[i].y);
            const float dx = p.x - io.MousePos.x, dy = p.y - io.MousePos.y;
            if (dx * dx + dy * dy < best_d) {
                best_d = dx * dx + dy * dy;
                best = (int)i;
            }
        }
        if (best >= 0) {
            const SensorFix& f = d.fixes[(size_t)best];
            const ImVec2 p = to_screen(f.x, f.y);
            dl->AddCircle(p, px(7.0f), IM_COL32(255, 255, 255, 230), 0, px(1.5f));
            fix_tooltip(d, f);
            if (clicked) {
                if (d.video) r.seek = f.t;
                else r.open_photo = f.photo;
            }
        }
    }
    dl->PopClipRect();
}

void SensorView::draw_track3d(const SensorData& d, const ImVec2& size, double playhead,
                              Result& r) {
    const ImVec2 at = ImGui::GetCursorScreenPos();
    ui::InvisibleButtonRaw("##track3d", size,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const ImGuiIO& io = ImGui::GetIO();
    const ImVec2 moved = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
    const bool clicked = ImGui::IsItemDeactivated() && ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
                         moved.x * moved.x + moved.y * moved.y < 9.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 br(at.x + size.x, at.y + size.y);
    dl->PushClipRect(at, br, true);
    dl->AddRectFilled(at, br, IM_COL32(22, 24, 28, 255));

    // Heights above the lowest kept fix, stretched as asked; a fix with no
    // altitude sits on the ground.
    const double zs = height_stretch(d, _height_scale);
    std::vector<size_t> kept;
    double ground = 1e300, peak = -1e300;
    for (size_t i = 0; i < d.fixes.size(); i++) {
        if (d.fixes[i].outlier) continue;
        kept.push_back(i);
        if (d.fixes[i].has_alt) {
            ground = std::min(ground, d.fixes[i].alt);
            peak = std::max(peak, d.fixes[i].alt);
        }
    }
    const bool any_alt = peak >= ground;
    if (!any_alt) ground = peak = 0.0;
    struct P3 {
        double x, y, z;
    };
    auto pos = [&](const SensorFix& f) {
        return P3{f.x, f.y, f.has_alt ? (f.alt - ground) * zs : 0.0};
    };
    double lo[3] = {1e300, 1e300, 0.0}, hi[3] = {-1e300, -1e300, 0.0};
    for (size_t i : kept) {
        const P3 p = pos(d.fixes[i]);
        lo[0] = std::min(lo[0], p.x);
        lo[1] = std::min(lo[1], p.y);
        hi[0] = std::max(hi[0], p.x);
        hi[1] = std::max(hi[1], p.y);
        hi[2] = std::max(hi[2], p.z);
    }
    if (kept.empty()) {
        lo[0] = lo[1] = -10.0;
        hi[0] = hi[1] = 10.0;
    }
    const double extent = std::max({hi[0] - lo[0], hi[1] - lo[1], hi[2], 20.0});

    // An orbit about a point: degrees from north and above the horizon.
    const double fov = 45.0 * kDeg;
    const double focal = 0.5 * size.y / std::tan(0.5 * fov);
    if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f)) {
        _yaw += io.MouseDelta.x * 0.35;
        _pitch = std::clamp(_pitch + io.MouseDelta.y * 0.35, 0.0, 89.5);
        _fit3d = false;
    }
    const double ya = _yaw * kDeg, pa = _pitch * kDeg;
    const P3 f{std::sin(ya) * std::cos(pa), std::cos(ya) * std::cos(pa), -std::sin(pa)};
    const P3 rt{std::cos(ya), -std::sin(ya), 0.0};
    const P3 up{rt.y * f.z - rt.z * f.y, rt.z * f.x - rt.x * f.z, rt.x * f.y - rt.y * f.x};
    if (_fit3d || !(_dist > 0.0)) {
        // Near enough that the track's own points, as this view sees them,
        // fill most of the pane: a long or round track is not lost in its box.
        _target[0] = 0.5 * (lo[0] + hi[0]);
        _target[1] = 0.5 * (lo[1] + hi[1]);
        _target[2] = 0.5 * hi[2];
        const double sx = 0.45 * size.x / focal, sy = 0.45 * size.y / focal;
        _dist = 10.0;
        const size_t every = std::max<size_t>(1, kept.size() / 2000);
        for (size_t k = 0; k < kept.size(); k += every) {
            const P3 p = pos(d.fixes[kept[k]]);
            const double dx = p.x - _target[0], dy = p.y - _target[1], dz = p.z - _target[2];
            const double cx = dx * rt.x + dy * rt.y + dz * rt.z, cy = dx * up.x + dy * up.y + dz * up.z;
            const double cz = dx * f.x + dy * f.y + dz * f.z;
            _dist = std::max({_dist, std::fabs(cx) / sx - cz, std::fabs(cy) / sy - cz});
        }
    }
    if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Right, 1.0f)) {
        const double mx = io.MouseDelta.x * _dist / focal, my = io.MouseDelta.y * _dist / focal;
        _target[0] -= rt.x * mx - up.x * my;
        _target[1] -= rt.y * mx - up.y * my;
        _target[2] -= rt.z * mx - up.z * my;
        _fit3d = false;
    }
    if (hovered && io.MouseWheel != 0.0f) {
        _dist = std::clamp(_dist * std::pow(1.2, -(double)io.MouseWheel), 0.5, 1e8);
        _fit3d = false;
    }
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) _fit3d = true;
    const P3 eye{_target[0] - f.x * _dist, _target[1] - f.y * _dist, _target[2] - f.z * _dist};
    const double near_z = _dist * 0.01;
    const ImVec2 c(at.x + size.x * 0.5f, at.y + size.y * 0.5f);
    struct Cam {
        double x, y, z;
    };
    auto view = [&](const P3& p) {
        const double dx = p.x - eye.x, dy = p.y - eye.y, dz = p.z - eye.z;
        return Cam{dx * rt.x + dy * rt.y + dz * rt.z, dx * up.x + dy * up.y + dz * up.z,
                   dx * f.x + dy * f.y + dz * f.z};
    };
    auto screen = [&](const Cam& v) {
        return ImVec2(c.x + (float)(v.x * focal / v.z), c.y - (float)(v.y * focal / v.z));
    };
    auto project = [&](const P3& p, ImVec2& out) {
        const Cam v = view(p);
        if (v.z < near_z) return false;
        out = screen(v);
        return true;
    };
    auto line = [&](const P3& a, const P3& b, ImU32 col, float th) {
        Cam va = view(a), vb = view(b);
        if (va.z < near_z && vb.z < near_z) return;
        // Cut at the near plane, so a line passing beside the eye still draws.
        if (va.z < near_z || vb.z < near_z) {
            Cam& in = va.z < near_z ? va : vb;
            const Cam& out = va.z < near_z ? vb : va;
            const double s = (near_z - out.z) / (in.z - out.z);
            in = Cam{out.x + (in.x - out.x) * s, out.y + (in.y - out.y) * s, near_z};
        }
        dl->AddLine(screen(va), screen(vb), col, th);
    };

    // The ground: a grid at the lowest fix, north marked on it.
    const double step = nice_step(extent / 8.0);
    const double gx0 = std::floor((lo[0] - 0.1 * extent) / step) * step;
    const double gx1 = std::ceil((hi[0] + 0.1 * extent) / step) * step;
    const double gy0 = std::floor((lo[1] - 0.1 * extent) / step) * step;
    const double gy1 = std::ceil((hi[1] + 0.1 * extent) / step) * step;
    const ImU32 grid = IM_COL32(255, 255, 255, 22);
    for (double x = gx0; x <= gx1 + 1e-9; x += step) line({x, gy0, 0}, {x, gy1, 0}, grid, 1.0f);
    for (double y = gy0; y <= gy1 + 1e-9; y += step) line({gx0, y, 0}, {gx1, y, 0}, grid, 1.0f);
    ImVec2 np;
    if (project({0.5 * (gx0 + gx1), gy1 + 0.5 * step, 0.0}, np))
        dl->AddText(ImVec2(np.x - ImGui::CalcTextSize("N").x * 0.5f, np.y - ImGui::GetTextLineHeight() * 0.5f),
                    IM_COL32(230, 230, 230, 200), "N");

    // A ruler in true metres, whatever the stretch, at the grid's far corner
    // where perspective leaves it the size of the track.
    if (any_alt && peak > ground) {
        const double rise = peak - ground;
        const double tick = nice_step(rise / 4.0);
        double rx = gx0, ry = gy0, deepest = -1e300;
        for (int i = 0; i < 4; i++) {
            const double x = (i & 1) ? gx1 : gx0, y = (i & 2) ? gy1 : gy0;
            const double depth = view({x, y, 0.0}).z;
            if (depth > deepest) {
                deepest = depth;
                rx = x;
                ry = y;
            }
        }
        const double inward = rx == gx0 ? 0.03 * extent : -0.03 * extent;
        line({rx, ry, 0.0}, {rx, ry, rise * zs}, IM_COL32(230, 230, 230, 160), px(1.5f));
        float last_y = 1e30f;
        for (double h = 0.0; h <= rise + 1e-9; h += tick) {
            ImVec2 a, b;
            if (!project({rx, ry, h * zs}, a) || !project({rx + inward, ry, h * zs}, b)) continue;
            dl->AddLine(a, b, IM_COL32(230, 230, 230, 160), px(1.5f));
            // Seen edge-on the ticks bunch up; a label only where there is room.
            if (std::fabs(a.y - last_y) < ImGui::GetTextLineHeight()) continue;
            last_y = a.y;
            const std::string label = metres(ground + h);
            dl->AddText(ImVec2(a.x - ImGui::CalcTextSize(label.c_str()).x - px(4.0f),
                               a.y - ImGui::GetTextLineHeight() * 0.5f),
                        IM_COL32(220, 220, 220, 200), label.c_str());
        }
    }

    // The track's shadow on the ground and lines down to it, so height reads
    // as height; then the track itself, early to late.
    const float n = (float)std::max<size_t>(1, d.fixes.size() - 1);
    const size_t stride = std::max<size_t>(1, kept.size() / 4000);
    const size_t drop_every = std::max<size_t>(1, kept.size() / 60);
    for (size_t k = 0; k < kept.size(); k += drop_every) {
        const P3 p = pos(d.fixes[kept[k]]);
        if (p.z > 0.0)
            line({p.x, p.y, 0.0}, p, (time_colour((float)kept[k] / n) & ~IM_COL32_A_MASK) | IM_COL32(0, 0, 0, 80),
                 1.0f);
    }
    if (any_alt)
        for (size_t k = stride; k < kept.size(); k += stride) {
            const P3 a = pos(d.fixes[kept[k - stride]]), b = pos(d.fixes[kept[k]]);
            line({a.x, a.y, 0.0}, {b.x, b.y, 0.0}, IM_COL32(255, 255, 255, 40), px(1.5f));
        }
    for (size_t k = stride; k < kept.size(); k += stride)
        line(pos(d.fixes[kept[k - stride]]), pos(d.fixes[kept[k]]), time_colour((float)kept[k] / n),
             px(d.video ? 2.0f : 1.0f));
    if (!d.video)
        for (size_t i : kept) {
            const SensorFix& fx = d.fixes[i];
            const P3 p = pos(fx);
            ImVec2 s;
            if (!project(p, s)) continue;
            const ImU32 col = time_colour((float)i / n);
            // Where the camera looked, pitch included: a survey's nadir shots
            // point straight down, and one that does not stands out.
            if (fx.has_yaw) {
                const double yw = fx.yaw * kDeg, pt = fx.pitch * kDeg, len = 0.04 * extent;
                line(p, {p.x + std::sin(yw) * std::cos(pt) * len, p.y + std::cos(yw) * std::cos(pt) * len,
                         p.z + std::sin(pt) * len},
                     col, px(1.5f));
            }
            dl->AddCircleFilled(s, px(3.0f), col);
        }
    const ImU32 red = IM_COL32(240, 80, 70, 255), amber = ImGui::ColorConvertFloat4ToU32(kWarn);
    for (const SensorFix& fx : d.fixes) {
        if (!fx.outlier) continue;
        const P3 p = pos(fx);
        if (std::fabs(p.x - _target[0]) > 3.0 * extent || std::fabs(p.y - _target[1]) > 3.0 * extent) continue;
        ImVec2 s;
        if (!project(p, s)) continue;
        const float k = px(4.0f);
        const ImU32 col = fx.no_fix ? amber : red;
        dl->AddLine(ImVec2(s.x - k, s.y - k), ImVec2(s.x + k, s.y + k), col, px(2.0f));
        dl->AddLine(ImVec2(s.x - k, s.y + k), ImVec2(s.x + k, s.y - k), col, px(2.0f));
    }
    if (!kept.empty()) {
        ImVec2 s;
        if (project(pos(d.fixes[kept.front()]), s)) dl->AddCircleFilled(s, px(5.5f), IM_COL32(70, 185, 90, 255));
        if (project(pos(d.fixes[kept.back()]), s))
            dl->AddRectFilled(ImVec2(s.x - px(5.0f), s.y - px(5.0f)), ImVec2(s.x + px(5.0f), s.y + px(5.0f)),
                              IM_COL32(220, 70, 60, 255));
    }
    if (d.video && playhead >= 0.0 && kept.size() >= 2) {
        size_t hi_k = 0;
        while (hi_k < kept.size() && d.fixes[kept[hi_k]].t < playhead) hi_k++;
        const SensorFix& b = d.fixes[kept[std::min(hi_k, kept.size() - 1)]];
        const SensorFix& a = d.fixes[kept[hi_k > 0 ? hi_k - 1 : 0]];
        const double sp = b.t - a.t;
        const double t = sp > 0.0 ? std::clamp((playhead - a.t) / sp, 0.0, 1.0) : 0.0;
        const P3 pa3 = pos(a), pb3 = pos(b);
        const P3 p{pa3.x + (pb3.x - pa3.x) * t, pa3.y + (pb3.y - pa3.y) * t, pa3.z + (pb3.z - pa3.z) * t};
        ImVec2 s;
        if (project(p, s)) {
            dl->AddCircle(s, px(8.0f), IM_COL32(0, 0, 0, 255), 0, px(4.0f));
            dl->AddCircle(s, px(8.0f), IM_COL32(255, 255, 255, 255), 0, px(2.0f));
        }
    }
    if (!any_alt) {
        const char* note = pmsg::no_altitude.get();
        dl->AddText(ImVec2(at.x + px(10.0f), at.y + px(8.0f)), IM_COL32(230, 200, 120, 230), note);
    }

    if (hovered && !ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f) &&
        !ImGui::IsMouseDragging(ImGuiMouseButton_Right, 1.0f)) {
        int best = -1;
        float best_d = px(9.0f) * px(9.0f);
        const size_t probe = std::max<size_t>(1, d.fixes.size() / 20000);
        for (size_t i = 0; i < d.fixes.size(); i += probe) {
            ImVec2 s;
            if (!project(pos(d.fixes[i]), s)) continue;
            const float dx = s.x - io.MousePos.x, dy = s.y - io.MousePos.y;
            if (dx * dx + dy * dy < best_d) {
                best_d = dx * dx + dy * dy;
                best = (int)i;
            }
        }
        if (best >= 0) {
            const SensorFix& fx = d.fixes[(size_t)best];
            ImVec2 s;
            if (project(pos(fx), s)) dl->AddCircle(s, px(7.0f), IM_COL32(255, 255, 255, 230), 0, px(1.5f));
            fix_tooltip(d, fx);
            if (clicked) {
                if (d.video) r.seek = fx.t;
                else r.open_photo = fx.photo;
            }
        }
    }
    dl->PopClipRect();
}

void SensorView::draw_series(const SensorData& d, const SensorSeries& s, float height,
                             double playhead, double clip_start, double clip_end, Result& r) {
    const float w = std::max(ImGui::GetContentRegionAvail().x, 1.0f);
    const ImVec2 at = ImGui::GetCursorScreenPos();
    ImGui::PushID((int)s.kind);
    ui::InvisibleButtonRaw("##plot", ImVec2(w, height),
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    ImGui::PopID();
    // The wheel zooms the plot's time and stays here, not scrolling the pane.
    ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 br(at.x + w, at.y + height);
    dl->AddRectFilled(at, br, ImGui::GetColorU32(ImGuiCol_FrameBg));
    dl->PushClipRect(at, br, true);

    // Zoom about the pointer, pan with the right button, all of it on a double-click.
    const double min_span = d.video ? 0.05 : 3.0;
    auto keep_in = [&](double a, double span) {
        span = std::clamp(span, std::min(min_span, _full), _full);
        a = std::clamp(a, 0.0, _full - span);
        _win0 = a;
        _win1 = a + span;
        _zoomed = span < _full * 0.999;
    };
    if (hovered && io.MouseWheel != 0.0f) {
        const double span = _win1 - _win0;
        const double tm = _win0 + (double)(io.MousePos.x - at.x) / w * span;
        const double ns = span * std::pow(1.25, -(double)io.MouseWheel);
        keep_in(tm - (tm - _win0) * ns / std::max(span, 1e-9), ns);
    }
    if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Right, 1.0f))
        keep_in(_win0 - (double)io.MouseDelta.x / w * (_win1 - _win0), _win1 - _win0);
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) _zoomed = false;
    const double t0 = _win0, span = std::max(_win1 - _win0, 1e-9);
    auto x_of = [&](double t) { return at.x + (float)((t - t0) / span) * w; };
    auto t_of = [&](float x) { return t0 + (double)(x - at.x) / w * span; };

    // Per pixel column and channel, the range of what fell in it: a 400 Hz
    // gyro over an hour is a million samples and a few hundred columns.
    struct Col {
        float lo = 1e30f, hi = -1e30f, first = 0.0f, last = 0.0f;
        bool any = false;
    };
    const int cols = std::max(1, (int)w);
    std::vector<Col> col[4];
    for (int c = 0; c < s.channels; c++) col[c].assign((size_t)cols, Col{});
    const size_t i0 = (size_t)std::max<std::ptrdiff_t>(
        0, std::lower_bound(s.t.begin(), s.t.end(), (float)t0) - s.t.begin() - 1);
    const size_t i1 = std::min(
        s.t.size(), (size_t)(std::upper_bound(s.t.begin(), s.t.end(), (float)(t0 + span)) - s.t.begin()) + 1);
    float vmin = 1e30f, vmax = -1e30f;
    for (size_t i = i0; i < i1; i++) {
        const double t = s.t[i];
        const int c = std::clamp((int)((t - t0) / span * cols), 0, cols - 1);
        const bool inside = t >= t0 && t <= t0 + span;
        for (int ch = 0; ch < s.channels; ch++) {
            const float v = s.v[ch][i];
            if (!std::isfinite(v)) continue;
            Col& k = col[ch][(size_t)c];
            if (!k.any) k.first = s.t[i];
            k.any = true;
            k.last = s.t[i];
            k.lo = std::min(k.lo, v);
            k.hi = std::max(k.hi, v);
            if (inside) {
                vmin = std::min(vmin, v);
                vmax = std::max(vmax, v);
            }
        }
    }
    if (s.reference >= 0.0f && vmax >= vmin) {
        vmin = std::min(vmin, s.reference);
        vmax = std::max(vmax, s.reference);
    }
    const bool flat = vmax == vmin;
    if (!(vmax > vmin)) {
        const float mid = vmax >= vmin ? vmin : 0.0f;
        vmin = mid - 1.0f;
        vmax = mid + 1.0f;
    }
    const float top = at.y + ImGui::GetTextLineHeight() + px(4.0f), pad = px(4.0f);
    auto y_of = [&](float v) {
        return br.y - pad - (v - vmin) / (vmax - vmin) * (br.y - pad - top);
    };

    if (d.video && (clip_start > 0.0 || clip_end > 0.0)) {
        const ImU32 veil = IM_COL32(0, 0, 0, 110);
        const float xs = x_of(clip_start), xe = clip_end > 0.0 ? x_of(clip_end) : br.x;
        if (xs > at.x) dl->AddRectFilled(at, ImVec2(xs, br.y), veil);
        if (xe < br.x) dl->AddRectFilled(ImVec2(xe, at.y), br, veil);
    }
    auto dashed = [&](float v, ImU32 c) {
        const float y = y_of(v);
        for (float x = at.x; x < br.x; x += px(8.0f))
            dl->AddLine(ImVec2(x, y), ImVec2(std::min(x + px(4.0f), br.x), y), c);
    };
    const bool log2_axis = is_exposure(s) && s.kind != SensorSeries::Exposure;
    if (vmin < 0.0f && vmax > 0.0f && !log2_axis) dashed(0.0f, IM_COL32(255, 255, 255, 40));
    if (s.reference >= 0.0f) dashed(s.reference, IM_COL32(255, 255, 255, 70));

    // Where a stream stops reporting for longer than its own rhythm: shaded,
    // and the line not drawn across it.
    const ImU32 hole = IM_COL32(240, 80, 70, 38);
    std::vector<std::pair<float, float>> holes;
    if (d.video && s.gap > 0.0f) {
        float prev = -1.0f;
        for (int c = 0; c < cols; c++) {
            float first = 1e30f, last = -1e30f;
            for (int ch = 0; ch < s.channels; ch++)
                if (col[ch][(size_t)c].any) {
                    first = std::min(first, col[ch][(size_t)c].first);
                    last = std::max(last, col[ch][(size_t)c].last);
                }
            if (last < first) continue;
            if (prev >= 0.0f && first - prev > s.gap) holes.push_back({prev, first});
            prev = last;
        }
        if (!s.t.empty()) {
            if (s.t.front() > s.gap) holes.push_back({0.0f, s.t.front()});
            if (_full - s.t.back() > s.gap) holes.push_back({s.t.back(), (float)_full});
        }
        for (const auto& [a, b] : holes) {
            const float xa = std::max(x_of(a), at.x), xb = std::min(x_of(b), br.x);
            if (xb > xa) dl->AddRectFilled(ImVec2(xa, top), ImVec2(std::max(xb, xa + 1.0f), br.y), hole);
        }
    }
    for (int ch = s.channels - 1; ch >= 0; ch--) {
        const ImU32 ink = channel_colour(s, ch);
        const ImU32 band = (ink & ~IM_COL32_A_MASK) | IM_COL32(0, 0, 0, 120);
        const float limit = s.gap > 0.0f ? s.gap : 1e30f;
        ImVec2 prev(-1.0f, 0.0f);
        float prev_t = 0.0f;
        for (int c = 0; c < cols; c++) {
            const Col& k = col[ch][(size_t)c];
            if (!k.any) continue;
            const float x = at.x + (float)c + 0.5f;
            const float ya = y_of(k.hi), yb = y_of(k.lo);
            if (yb - ya > 1.0f) dl->AddLine(ImVec2(x, ya), ImVec2(x, yb), band, px(1.0f));
            const ImVec2 mid(x, (ya + yb) * 0.5f);
            if (prev.x >= 0.0f && k.first - prev_t <= limit) dl->AddLine(prev, mid, ink, px(1.3f));
            else if (!d.video || s.channels == 1) dl->AddCircleFilled(mid, px(1.6f), ink);
            prev = mid;
            prev_t = k.last;
        }
    }
    if (d.video && playhead >= 0.0) {
        const float x = x_of(playhead);
        dl->AddLine(ImVec2(x, at.y), ImVec2(x, br.y), IM_COL32(255, 255, 255, 200), px(1.5f));
    }

    // The name, the channels in their colours, how often it was sampled, and
    // the range shown, over a band that keeps them readable on the lines.
    const ImU32 text = ImGui::GetColorU32(ImGuiCol_Text), dim = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    dl->AddRectFilled(at, ImVec2(br.x, top - px(2.0f)), IM_COL32(0, 0, 0, 70));
    ImVec2 cur(at.x + px(5.0f), at.y + px(1.0f));
    auto put = [&](const char* str, ImU32 c, float after) {
        dl->AddText(cur, c, str);
        cur.x += ImGui::CalcTextSize(str).x + after;
    };
    put(series_name(s, d.video).get(), text, px(10.0f));
    if (s.channels > 1 || s.channel[0])
        for (int ch = 0; ch < s.channels; ch++) put(channel_label(s, ch), channel_colour(s, ch), px(8.0f));
    if (d.video && s.rate_hz > 0.0)
        put(spirula::i18n::format(pmsg::hz_value, {fixed(s.rate_hz, s.rate_hz < 10.0 ? 1 : 0)}).c_str(),
            dim, 0.0f);
    const int digits = vmax - vmin < 10.0f ? 2 : vmax - vmin < 100.0f ? 1 : 0;
    const std::string range = flat ? value_text(s, vmin + 1.0f, digits)
                                   : value_text(s, vmin, digits) + " – " + value_text(s, vmax, digits);
    const float rw = ImGui::CalcTextSize(range.c_str()).x;
    if (cur.x + px(10.0f) < br.x - rw - px(5.0f))
        dl->AddText(ImVec2(br.x - rw - px(5.0f), at.y + px(1.0f)), dim, range.c_str());
    dl->PopClipRect();

    // What the streams read under the pointer, and a click to go there.
    if (hovered && !ImGui::IsMouseDragging(ImGuiMouseButton_Right, 1.0f)) {
        const double tm = std::clamp(t_of(io.MousePos.x), 0.0, _full);
        dl->AddLine(ImVec2(io.MousePos.x, at.y), ImVec2(io.MousePos.x, br.y), IM_COL32(255, 255, 255, 70));
        const size_t j = (size_t)std::clamp<std::ptrdiff_t>(
            std::lower_bound(s.t.begin(), s.t.end(), (float)tm) - s.t.begin(), 0,
            (std::ptrdiff_t)s.t.size() - 1);
        if (ImGui::BeginTooltip()) {
            if (d.video) ui::TextRaw(clock_text(tm));
            else ui::Text(pmsg::tip_photo, {count_text((long long)std::llround(tm) + 1)});
            double sq = 0.0;
            bool all = true;
            for (int ch = 0; ch < s.channels; ch++) {
                // The nearest sample this channel has, which may not be at j.
                float v = std::numeric_limits<float>::quiet_NaN();
                double dt = 1e30;
                for (size_t a = j > 64 ? j - 64 : 0, b = std::min(s.t.size(), j + 64); a < b; a++)
                    if (std::isfinite(s.v[ch][a]) && std::fabs(s.t[a] - tm) < dt) {
                        dt = std::fabs(s.t[a] - tm);
                        v = s.v[ch][a];
                    }
                if (!std::isfinite(v)) {
                    all = false;
                    continue;
                }
                sq += (double)v * v;
                const ImVec4 c = ImGui::ColorConvertU32ToFloat4(channel_colour(s, ch));
                const bool named = s.channels > 1 || s.channel[ch];
                ui::TextColoredRaw(c, (named ? std::string(channel_label(s, ch)) + "  " : std::string()) +
                                          value_text(s, v, 2));
            }
            const bool vector = s.kind == SensorSeries::Rotation || s.kind == SensorSeries::Acceleration ||
                                s.kind == SensorSeries::Gravity || s.kind == SensorSeries::Magnet;
            if (vector && all) ui::Text(pmsg::tip_magnitude, {fixed(std::sqrt(sq), 2)});
            if (is_exposure(s))
                if (const SensorExposure* e = exposure_at(d, tm)) ui::TextRaw(exposure_text(*e));
            if (s.kind == SensorSeries::Exposure) ui::TextDisabled(pmsg::tip_exposure_mean);
            for (const auto& [a, b] : holes)
                if (tm >= a && tm <= b) ui::TextColored(kBad, pmsg::tip_gap, {fixed(b - a, 1)});
            ui::TextDisabled(d.video ? pmsg::plot_help : pmsg::plot_help_photos);
            ImGui::EndTooltip();
        }
        if (d.video && active && ImGui::IsMouseDown(ImGuiMouseButton_Left)) r.seek = tm;
        const ImVec2 moved = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
        if (!d.video && ImGui::IsItemDeactivated() && ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
            moved.x * moved.x + moved.y * moved.y < 9.0f) {
            const int photo = (int)std::llround(tm);
            for (const SensorFix& f : d.fixes)
                if (f.photo == photo) r.open_photo = photo;
            if (r.open_photo < 0 && photo >= 0 && photo < d.photos) r.open_photo = photo;
        }
    }
}

}  // namespace gui
