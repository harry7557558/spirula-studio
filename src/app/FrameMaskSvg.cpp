// FrameMaskSvg.cpp -- see FrameMaskSvg.h.

#include "app/FrameMaskSvg.h"

#include "core/CubicBezier.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace app {

namespace {

constexpr float kPi = 3.14159265358979f;
// Segments per flattened curve; in normalized units a frame-wide arc is still
// under a pixel off at 4K.
constexpr int kCurveSteps = 24;

std::string num(float v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.6g", v);
    return buf;
}

std::string escape_xml(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += c;
        }
    }
    return out;
}

std::string unescape_xml(const std::string& s) {
    static const std::pair<const char*, char> kEntities[] = {
        {"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&apos;", '\''}};
    std::string out;
    for (size_t i = 0; i < s.size();) {
        bool hit = false;
        if (s[i] == '&')
            for (const auto& [name, c] : kEntities) {
                const size_t n = std::char_traits<char>::length(name);
                if (s.compare(i, n, name) == 0) {
                    out += c;
                    i += n;
                    hit = true;
                    break;
                }
            }
        if (!hit) out += s[i++];
    }
    return out;
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) a++;
    while (b > a && std::isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}

std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// A number with an optional unit; only px and none are meaningful here.
bool parse_length(const std::string& s, float& v) {
    const std::string t = trim(s);
    if (t.empty()) return false;
    char* end = nullptr;
    v = std::strtof(t.c_str(), &end);
    if (end == t.c_str()) return false;
    const std::string unit = trim(std::string(end));
    return unit.empty() || unit == "px";
}

// Numbers separated by commas and/or whitespace, as points="" and viewBox write them.
std::vector<float> parse_list(const std::string& s) {
    std::vector<float> out;
    const char* p = s.c_str();
    while (*p) {
        while (*p && (std::isspace((unsigned char)*p) || *p == ',')) p++;
        if (!*p) break;
        char* end = nullptr;
        const float v = std::strtof(p, &end);
        if (end == p) break;
        out.push_back(v);
        p = end;
    }
    return out;
}

// ---------------------------------------------------------------------------
// Paint
// ---------------------------------------------------------------------------

enum class Ink { None, Remove, Keep };

// Dark paint removes and light paint keeps; data-op, when present, overrides.
bool parse_ink(const std::string& raw, Ink& out) {
    const std::string c = lower(trim(raw));
    if (c.empty() || c == "inherit") return false;
    if (c == "none" || c == "transparent") { out = Ink::None; return true; }
    float r = 0, g = 0, b = 0;
    if (c[0] == '#') {
        const std::string hex = c.substr(1);
        const unsigned long v = std::strtoul(hex.c_str(), nullptr, 16);
        if (hex.size() == 3) {
            r = (float)((v >> 8) & 0xF) / 15.0f;
            g = (float)((v >> 4) & 0xF) / 15.0f;
            b = (float)(v & 0xF) / 15.0f;
        } else if (hex.size() >= 6) {
            const unsigned long w = hex.size() == 8 ? v >> 8 : v;
            r = (float)((w >> 16) & 0xFF) / 255.0f;
            g = (float)((w >> 8) & 0xFF) / 255.0f;
            b = (float)(w & 0xFF) / 255.0f;
        }
    } else if (c.rfind("rgb", 0) == 0) {
        const size_t open = c.find('(');
        const std::vector<float> v = parse_list(open == std::string::npos ? "" : c.substr(open + 1));
        if (v.size() >= 3) {
            r = v[0] / 255.0f;
            g = v[1] / 255.0f;
            b = v[2] / 255.0f;
        }
    } else if (c == "white") {
        r = g = b = 1.0f;
    }
    // Any other named colour is a mark someone drew, and a mark removes.
    out = 0.2126f * r + 0.7152f * g + 0.0722f * b >= 0.5f ? Ink::Keep : Ink::Remove;
    return true;
}

// What a <g> hands down to its children; SVG's own defaults are black fill
// and no stroke, so a bare <rect> removes.
struct Style {
    Ink fill = Ink::Remove;
    Ink stroke = Ink::None;
    float stroke_width = 1.0f;
    std::string op;          // data-op: "remove" | "keep" | ""
    float rx = 0.0f, ry = 0.0f;  // data-rx / data-ry, normalized; not inherited
    bool hidden = false;
};

using Attrs = std::map<std::string, std::string>;

float attr_num(const Attrs& a, const char* key, float fallback = 0.0f) {
    const auto it = a.find(key);
    float v;
    return it != a.end() && parse_length(it->second, v) ? v : fallback;
}


void apply_style(const Attrs& a, Style& st) {
    std::map<std::string, std::string> props;
    for (const char* k : {"fill", "stroke", "stroke-width", "display", "visibility"}) {
        const auto it = a.find(k);
        if (it != a.end()) props[k] = it->second;
    }
    // style="" wins over the presentation attributes, as in CSS.
    const auto sit = a.find("style");
    if (sit != a.end()) {
        std::stringstream ss(sit->second);
        std::string decl;
        while (std::getline(ss, decl, ';')) {
            const size_t colon = decl.find(':');
            if (colon == std::string::npos) continue;
            props[lower(trim(decl.substr(0, colon)))] = trim(decl.substr(colon + 1));
        }
    }
    Ink ink;
    if (props.count("fill") && parse_ink(props["fill"], ink)) st.fill = ink;
    if (props.count("stroke") && parse_ink(props["stroke"], ink)) st.stroke = ink;
    float w;
    if (props.count("stroke-width") && parse_length(props["stroke-width"], w)) st.stroke_width = w;
    if (lower(props["display"]) == "none" || lower(props["visibility"]) == "hidden")
        st.hidden = true;
    const auto op = a.find("data-op");
    if (op != a.end()) st.op = lower(trim(op->second));
    st.rx = attr_num(a, "data-rx");
    st.ry = attr_num(a, "data-ry");
}

// ---------------------------------------------------------------------------
// The XML subset
// ---------------------------------------------------------------------------

struct Tag {
    std::string name;
    Attrs attrs;
    bool closing = false;
    bool self_closing = false;
};

// Advances `i` past one tag or run of text. Returns false at the end.
bool next_tag(const std::string& s, size_t& i, Tag& tag, std::string& text, bool& is_text) {
    tag = Tag{};
    text.clear();
    if (i >= s.size()) return false;
    if (s[i] != '<') {
        const size_t lt = s.find('<', i);
        text = s.substr(i, lt == std::string::npos ? std::string::npos : lt - i);
        i = lt == std::string::npos ? s.size() : lt;
        is_text = true;
        return true;
    }
    is_text = false;
    auto skip_to = [&](const char* close) {
        const size_t e = s.find(close, i);
        i = e == std::string::npos ? s.size() : e + std::char_traits<char>::length(close);
    };
    if (s.compare(i, 4, "<!--") == 0) { skip_to("-->"); return true; }
    if (s.compare(i, 9, "<![CDATA[") == 0) { skip_to("]]>"); return true; }
    if (s.compare(i, 2, "<?") == 0) { skip_to("?>"); return true; }
    if (s.compare(i, 2, "<!") == 0) { skip_to(">"); return true; }
    i++;
    if (i < s.size() && s[i] == '/') {
        tag.closing = true;
        i++;
    }
    size_t a = i;
    while (i < s.size() && !std::isspace((unsigned char)s[i]) && s[i] != '>' && s[i] != '/') i++;
    tag.name = s.substr(a, i - a);
    // A namespace prefix (svg:rect) names the same element.
    const size_t colon = tag.name.find(':');
    if (colon != std::string::npos) tag.name = tag.name.substr(colon + 1);
    while (i < s.size()) {
        while (i < s.size() && std::isspace((unsigned char)s[i])) i++;
        if (i >= s.size()) break;
        if (s[i] == '>') { i++; break; }
        if (s[i] == '/') {
            tag.self_closing = true;
            i++;
            continue;
        }
        a = i;
        while (i < s.size() && s[i] != '=' && s[i] != '>' && s[i] != '/' &&
               !std::isspace((unsigned char)s[i]))
            i++;
        const std::string key = s.substr(a, i - a);
        while (i < s.size() && std::isspace((unsigned char)s[i])) i++;
        if (i >= s.size() || s[i] != '=') continue;
        i++;
        while (i < s.size() && std::isspace((unsigned char)s[i])) i++;
        if (i >= s.size() || (s[i] != '"' && s[i] != '\'')) continue;
        const char q = s[i++];
        const size_t e = s.find(q, i);
        const std::string val = s.substr(i, e == std::string::npos ? std::string::npos : e - i);
        i = e == std::string::npos ? s.size() : e + 1;
        tag.attrs[key] = unescape_xml(val);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Path data
// ---------------------------------------------------------------------------

class PathCursor {
public:
    explicit PathCursor(const std::string& d) : _d(d) {}
    void skip() {
        while (_i < _d.size() && (std::isspace((unsigned char)_d[_i]) || _d[_i] == ',')) _i++;
    }
    bool at_end() { skip(); return _i >= _d.size(); }
    bool at_command() {
        skip();
        return _i < _d.size() && std::isalpha((unsigned char)_d[_i]) && _d[_i] != 'e' &&
               _d[_i] != 'E';
    }
    char command() { return _d[_i++]; }
    bool number(float& v) {
        skip();
        if (_i >= _d.size()) return false;
        const char* p = _d.c_str() + _i;
        char* end = nullptr;
        v = std::strtof(p, &end);
        if (end == p) return false;
        _i += (size_t)(end - p);
        return true;
    }
    // Arc flags are one digit and need no separator: "a1 1 0 00 1 1".
    bool flag(bool& f) {
        skip();
        if (_i >= _d.size() || (_d[_i] != '0' && _d[_i] != '1')) return false;
        f = _d[_i++] == '1';
        return true;
    }

private:
    const std::string& _d;
    size_t _i = 0;
};

void cubic(std::vector<float>& pts, float x0, float y0, float x1, float y1, float x2, float y2,
           float x3, float y3) {
    for (int k = 1; k <= kCurveSteps; k++) {
        const float t = (float)k / kCurveSteps, u = 1.0f - t;
        pts.push_back(u * u * u * x0 + 3 * u * u * t * x1 + 3 * u * t * t * x2 + t * t * t * x3);
        pts.push_back(u * u * u * y0 + 3 * u * u * t * y1 + 3 * u * t * t * y2 + t * t * t * y3);
    }
}

void quad(std::vector<float>& pts, float x0, float y0, float x1, float y1, float x2, float y2) {
    for (int k = 1; k <= kCurveSteps; k++) {
        const float t = (float)k / kCurveSteps, u = 1.0f - t;
        pts.push_back(u * u * x0 + 2 * u * t * x1 + t * t * x2);
        pts.push_back(u * u * y0 + 2 * u * t * y1 + t * t * y2);
    }
}

// SVG 1.1 F.6.5: endpoint to centre parameterization.
void arc(std::vector<float>& pts, float x1, float y1, float rx, float ry, float phi_deg,
         bool large, bool sweep, float x2, float y2) {
    rx = std::fabs(rx);
    ry = std::fabs(ry);
    if (rx == 0.0f || ry == 0.0f || (x1 == x2 && y1 == y2)) {
        pts.push_back(x2);
        pts.push_back(y2);
        return;
    }
    const float phi = phi_deg * kPi / 180.0f, c = std::cos(phi), s = std::sin(phi);
    const float dx = 0.5f * (x1 - x2), dy = 0.5f * (y1 - y2);
    const float xp = c * dx + s * dy, yp = -s * dx + c * dy;
    const float lam = xp * xp / (rx * rx) + yp * yp / (ry * ry);
    if (lam > 1.0f) {
        rx *= std::sqrt(lam);
        ry *= std::sqrt(lam);
    }
    const float num_ = rx * rx * ry * ry - rx * rx * yp * yp - ry * ry * xp * xp;
    const float den = rx * rx * yp * yp + ry * ry * xp * xp;
    float co = den > 0.0f ? std::sqrt(std::max(0.0f, num_ / den)) : 0.0f;
    if (large == sweep) co = -co;
    const float cxp = co * rx * yp / ry, cyp = -co * ry * xp / rx;
    const float cx = c * cxp - s * cyp + 0.5f * (x1 + x2);
    const float cy = s * cxp + c * cyp + 0.5f * (y1 + y2);
    auto angle = [](float ux, float uy, float vx, float vy) {
        return std::atan2(ux * vy - uy * vx, ux * vx + uy * vy);
    };
    const float t1 = angle(1, 0, (xp - cxp) / rx, (yp - cyp) / ry);
    float dt = angle((xp - cxp) / rx, (yp - cyp) / ry, (-xp - cxp) / rx, (-yp - cyp) / ry);
    if (!sweep && dt > 0) dt -= 2 * kPi;
    if (sweep && dt < 0) dt += 2 * kPi;
    for (int k = 1; k <= kCurveSteps; k++) {
        const float t = t1 + dt * (float)k / kCurveSteps;
        const float ex = rx * std::cos(t), ey = ry * std::sin(t);
        pts.push_back(c * ex - s * ey + cx);
        pts.push_back(s * ex + c * ey + cy);
    }
}

// A path's last anchor sitting on its first is the same anchor, reached
// again: it keeps the first's out-handle and brings its own in-handle.
void close_anchors(std::vector<float>& a) {
    constexpr size_t k = bezier::kAnchorFloats;
    const size_t n = a.size() / k;
    if (n < 2) return;
    const float* last = &a[(n - 1) * k];
    const float tol = 1e-6f * std::max({1.0f, std::fabs(a[2]), std::fabs(a[3])});
    if (std::fabs(last[2] - a[2]) > tol || std::fabs(last[3] - a[3]) > tol) return;
    a[0] = last[0];
    a[1] = last[1];
    a.resize((n - 1) * k);
}

}  // namespace

bool parse_svg_path(const std::string& d, std::vector<SvgSubpath>& out, std::string& error) {
    out.clear();
    PathCursor p(d);
    float x = 0, y = 0, sx = 0, sy = 0;
    // The reflected control point S and T mirror; reset by any other command.
    float cx2 = 0, cy2 = 0;
    char prev = 0, cmd = 0;
    SvgSubpath* cur = nullptr;
    // Per subpath in `out`: whether it had a curve, and an arc.
    std::vector<char> curved, arced;
    auto begin = [&](float nx, float ny) {
        out.push_back({});
        curved.push_back(0);
        arced.push_back(0);
        cur = &out.back();
        cur->pts = {nx, ny};
        cur->anchors = {nx, ny, nx, ny, nx, ny};
        x = sx = nx;
        y = sy = ny;
    };
    auto need = [&]() -> SvgSubpath* {
        if (!cur || cur->closed) begin(x, y);
        return cur;
    };
    auto line_anchor = [&](float px, float py) {
        cur->anchors.insert(cur->anchors.end(), {px, py, px, py, px, py});
    };
    auto curve_anchor = [&](float x1, float y1, float x2, float y2, float ex, float ey) {
        cur->anchors[cur->anchors.size() - 2] = x1;
        cur->anchors[cur->anchors.size() - 1] = y1;
        cur->anchors.insert(cur->anchors.end(), {x2, y2, ex, ey, ex, ey});
        curved.back() = 1;
    };
    while (!p.at_end()) {
        if (p.at_command()) cmd = p.command();
        else if (!cmd) {
            error = d;
            return false;
        }
        const bool rel = std::islower((unsigned char)cmd) != 0;
        const char up = (char)std::toupper((unsigned char)cmd);
        const float ox = rel ? x : 0.0f, oy = rel ? y : 0.0f;
        float v[7];
        auto read = [&](int n) {
            for (int k = 0; k < n; k++)
                if (!p.number(v[k])) return false;
            return true;
        };
        switch (up) {
            case 'M':
                if (!read(2)) { error = d; return false; }
                begin(ox + v[0], oy + v[1]);
                // Pairs after a moveto are lineto, relative if it was.
                cmd = rel ? 'l' : 'L';
                break;
            case 'L':
                if (!read(2)) { error = d; return false; }
                x = ox + v[0];
                y = oy + v[1];
                need()->pts.insert(cur->pts.end(), {x, y});
                line_anchor(x, y);
                break;
            case 'H':
                if (!read(1)) { error = d; return false; }
                x = (rel ? x : 0.0f) + v[0];
                need()->pts.insert(cur->pts.end(), {x, y});
                line_anchor(x, y);
                break;
            case 'V':
                if (!read(1)) { error = d; return false; }
                y = (rel ? y : 0.0f) + v[0];
                need()->pts.insert(cur->pts.end(), {x, y});
                line_anchor(x, y);
                break;
            case 'C':
            case 'S': {
                float x1, y1;
                if (up == 'C') {
                    if (!read(6)) { error = d; return false; }
                    x1 = ox + v[0];
                    y1 = oy + v[1];
                    v[0] = v[2]; v[1] = v[3]; v[2] = v[4]; v[3] = v[5];
                } else {
                    if (!read(4)) { error = d; return false; }
                    const bool follows = prev == 'C' || prev == 'S';
                    x1 = follows ? 2 * x - cx2 : x;
                    y1 = follows ? 2 * y - cy2 : y;
                }
                cx2 = ox + v[0];
                cy2 = oy + v[1];
                const float ex = ox + v[2], ey = oy + v[3];
                cubic(need()->pts, x, y, x1, y1, cx2, cy2, ex, ey);
                curve_anchor(x1, y1, cx2, cy2, ex, ey);
                x = ex;
                y = ey;
                break;
            }
            case 'Q':
            case 'T': {
                if (up == 'Q') {
                    if (!read(4)) { error = d; return false; }
                    cx2 = ox + v[0];
                    cy2 = oy + v[1];
                    v[0] = v[2]; v[1] = v[3];
                } else {
                    if (!read(2)) { error = d; return false; }
                    const bool follows = prev == 'Q' || prev == 'T';
                    cx2 = follows ? 2 * x - cx2 : x;
                    cy2 = follows ? 2 * y - cy2 : y;
                }
                const float ex = ox + v[0], ey = oy + v[1];
                quad(need()->pts, x, y, cx2, cy2, ex, ey);
                // Degree elevation: the cubic with these handles IS the quadratic.
                curve_anchor(x + 2.0f / 3.0f * (cx2 - x), y + 2.0f / 3.0f * (cy2 - y),
                             ex + 2.0f / 3.0f * (cx2 - ex), ey + 2.0f / 3.0f * (cy2 - ey),
                             ex, ey);
                x = ex;
                y = ey;
                break;
            }
            case 'A': {
                bool large = false, sweep = false;
                if (!p.number(v[0]) || !p.number(v[1]) || !p.number(v[2]) || !p.flag(large) ||
                    !p.flag(sweep) || !p.number(v[3]) || !p.number(v[4])) {
                    error = d;
                    return false;
                }
                const float ex = ox + v[3], ey = oy + v[4];
                arc(need()->pts, x, y, v[0], v[1], v[2], large, sweep, ex, ey);
                line_anchor(ex, ey);
                arced.back() = 1;
                x = ex;
                y = ey;
                break;
            }
            case 'Z':
                if (cur) cur->closed = true;
                x = sx;
                y = sy;
                break;
            default:
                error = d;
                return false;
        }
        prev = up;
    }
    for (size_t k = 0; k < out.size(); k++) {
        if (curved[k] && !arced[k]) close_anchors(out[k].anchors);
        if (!curved[k] || arced[k] || out[k].anchors.size() < 2 * bezier::kAnchorFloats)
            out[k].anchors.clear();
    }
    return true;
}

// ---------------------------------------------------------------------------
// Writing
// ---------------------------------------------------------------------------

std::string write_mask_svg(const std::vector<MaskShape>& shapes, const std::string& title,
                           const std::string& camera) {
    std::string o;
    o += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    // width/height only size a viewer's window; the geometry is the viewBox.
    o += "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 1 1\" width=\"1024\" "
         "height=\"1024\" preserveAspectRatio=\"none\"";
    if (!camera.empty()) o += " data-camera=\"" + escape_xml(camera) + "\"";
    o += ">\n";
    if (!title.empty()) o += "  <title>" + escape_xml(title) + "</title>\n";
    o += "  <desc>Spirula Studio frame stencil. Coordinates are normalized to the image, "
         "(0,0) top left to (1,1) bottom right. Shapes paint in order: black removes, "
         "white keeps.</desc>\n";
    // rasterize_frame_mask's base, so the file renders as the mask it makes.
    const bool base_keep = shapes.empty() || shapes.front().remove;
    o += std::string("  <rect data-role=\"base\" x=\"0\" y=\"0\" width=\"1\" height=\"1\" fill=\"") +
         (base_keep ? "#fff" : "#000") + "\"/>\n";
    for (const MaskShape& s : shapes) {
        const std::string op = s.remove ? "remove" : "keep";
        const std::string ink = s.remove ? "#000" : "#fff";
        const std::string head = "  <";
        switch (s.kind) {
            case MaskShape::Kind::Rect:
                o += head + "rect data-op=\"" + op + "\" x=\"" + num(std::min(s.cx, s.rx)) +
                     "\" y=\"" + num(std::min(s.cy, s.ry)) + "\" width=\"" +
                     num(std::fabs(s.rx - s.cx)) + "\" height=\"" + num(std::fabs(s.ry - s.cy)) +
                     "\" fill=\"" + ink + "\"/>\n";
                break;
            case MaskShape::Kind::Ellipse:
                o += head + "ellipse data-op=\"" + op + "\" cx=\"" + num(s.cx) + "\" cy=\"" +
                     num(s.cy) + "\" rx=\"" + num(s.rx) + "\" ry=\"" + num(s.ry) + "\" fill=\"" +
                     ink + "\"/>\n";
                break;
            case MaskShape::Kind::Path: {
                std::string d;
                for (size_t i = 0; i + 1 < s.pts.size(); i += 2)
                    d += (i ? " L" : "M") + num(s.pts[i]) + " " + num(s.pts[i + 1]);
                o += head + "path data-op=\"" + op + "\" d=\"" + d + " Z\" fill=\"" + ink +
                     "\" fill-rule=\"evenodd\"/>\n";
                break;
            }
            case MaskShape::Kind::Bezier: {
                // Every segment a C, straight ones too, so it reads back as a
                // Bezier; the last ends on the first anchor, which reading merges.
                const size_t n = s.pts.size() / bezier::kAnchorFloats;
                if (n == 0) break;
                std::string d = "M" + num(s.pts[2]) + " " + num(s.pts[3]);
                for (size_t i = 0; i < n; i++) {
                    float c[8];
                    bezier::segment(s.pts.data(), n, i, c);
                    d += " C" + num(c[2]) + " " + num(c[3]) + " " + num(c[4]) + " " +
                         num(c[5]) + " " + num(c[6]) + " " + num(c[7]);
                }
                o += head + "path data-op=\"" + op + "\" d=\"" + d + " Z\" fill=\"" + ink +
                     "\" fill-rule=\"evenodd\"/>\n";
                break;
            }
            case MaskShape::Kind::Stroke: {
                std::string pts;
                for (size_t i = 0; i + 1 < s.pts.size(); i += 2)
                    pts += (i ? " " : "") + num(s.pts[i]) + "," + num(s.pts[i + 1]);
                // A single point still draws its round cap in a viewer.
                if (s.pts.size() == 2) pts += " " + num(s.pts[0]) + "," + num(s.pts[1]);
                // SVG strokes are rx = ry in the viewBox; a brush round in
                // PIXELS is not, so a viewer gets the mean and data-r* the exact.
                std::string radii;
                if (s.rx != s.ry)
                    radii = "\" data-rx=\"" + num(s.rx) + "\" data-ry=\"" + num(s.ry);
                o += head + "polyline data-op=\"" + op + radii + "\" points=\"" + pts +
                     "\" fill=\"none\" stroke=\"" + ink + "\" stroke-width=\"" +
                     num(2.0f * std::sqrt(s.rx * s.ry)) +
                     "\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/>\n";
                break;
            }
        }
    }
    o += "</svg>\n";
    return o;
}

// ---------------------------------------------------------------------------
// Reading
// ---------------------------------------------------------------------------

bool read_mask_svg(const std::string& text, std::vector<MaskShape>& out, std::string& title,
                   std::string& error, std::string* camera) {
    out.clear();
    title.clear();
    if (camera) camera->clear();
    float vx = 0, vy = 0, vw = 1, vh = 1;
    bool seen_svg = false;
    std::vector<Style> stack{Style{}};
    // Everything under <defs>, <clipPath> and the like is not drawn.
    int hidden_depth = 0;
    bool in_title = false;
    size_t i = 0;
    Tag tag;
    std::string chunk;
    bool is_text = false;

    auto nx = [&](float x) { return (x - vx) / vw; };
    auto ny = [&](float y) { return (y - vy) / vh; };

    // `closed`: the outline's stroke returns to its start. A fill always closes.
    auto emit = [&](const Style& st, const std::vector<float>& outline, bool closed,
                    const MaskShape* filled) {
        const auto op_of = [&](Ink ink) {
            if (st.op == "remove") return true;
            if (st.op == "keep") return false;
            return ink == Ink::Remove;
        };
        if (st.fill != Ink::None && (filled || outline.size() >= 6)) {
            MaskShape s;
            if (filled) s = *filled;
            else {
                s.kind = MaskShape::Kind::Path;
                s.pts = outline;
            }
            s.remove = op_of(st.fill);
            out.push_back(s);
        }
        if (st.stroke != Ink::None && st.stroke_width > 0.0f && outline.size() >= 2) {
            MaskShape s;
            s.kind = MaskShape::Kind::Stroke;
            s.pts = outline;
            if (closed && outline.size() >= 4) s.pts.insert(s.pts.end(), {outline[0], outline[1]});
            // What a stroke of that width is once the viewBox is stretched onto the frame.
            s.rx = st.rx > 0.0f ? st.rx : 0.5f * st.stroke_width / vw;
            s.ry = st.ry > 0.0f ? st.ry : 0.5f * st.stroke_width / vh;
            s.remove = op_of(st.stroke);
            out.push_back(s);
        }
    };
    auto to_norm = [&](std::vector<float> pts) {
        for (size_t k = 0; k + 1 < pts.size(); k += 2) {
            pts[k] = nx(pts[k]);
            pts[k + 1] = ny(pts[k + 1]);
        }
        return pts;
    };
    auto ellipse_outline = [&](float cx, float cy, float rx, float ry) {
        std::vector<float> pts;
        for (int k = 0; k < 4 * kCurveSteps; k++) {
            const float t = 2.0f * kPi * (float)k / (4 * kCurveSteps);
            pts.push_back(nx(cx + rx * std::cos(t)));
            pts.push_back(ny(cy + ry * std::sin(t)));
        }
        return pts;
    };

    while (next_tag(text, i, tag, chunk, is_text)) {
        if (is_text) {
            if (in_title) title += unescape_xml(chunk);
            continue;
        }
        if (tag.name.empty()) continue;
        if (tag.closing) {
            if (tag.name == "title") in_title = false;
            if (hidden_depth > 0) {
                hidden_depth--;
                continue;
            }
            if ((tag.name == "g" || tag.name == "svg" || tag.name == "a") && stack.size() > 1)
                stack.pop_back();
            continue;
        }
        if (hidden_depth > 0) {
            if (!tag.self_closing) hidden_depth++;
            continue;
        }
        static const char* kNotDrawn[] = {"defs", "clipPath", "mask", "symbol", "pattern",
                                          "marker", "metadata", "linearGradient",
                                          "radialGradient", "style", "text", "sodipodi:namedview",
                                          "namedview", "desc"};
        if (std::find_if(std::begin(kNotDrawn), std::end(kNotDrawn),
                         [&](const char* n) { return tag.name == n; }) != std::end(kNotDrawn)) {
            if (!tag.self_closing) hidden_depth = 1;
            continue;
        }
        if (tag.name == "title") {
            // The document's, not a shape's tooltip.
            in_title = !tag.self_closing && stack.size() == 2 && title.empty();
            if (!in_title && !tag.self_closing) hidden_depth = 1;
            continue;
        }
        if (tag.attrs.count("transform")) {
            error = "<" + tag.name + " transform=\"" + tag.attrs["transform"] +
                    "\">: transforms are not supported";
            return false;
        }
        Style st = stack.back();
        apply_style(tag.attrs, st);
        if (tag.name == "svg") {
            if (!seen_svg) {
                seen_svg = true;
                if (camera) *camera = trim(tag.attrs["data-camera"]);
                const std::vector<float> vb = parse_list(tag.attrs["viewBox"]);
                if (vb.size() == 4 && vb[2] > 0 && vb[3] > 0) {
                    vx = vb[0]; vy = vb[1]; vw = vb[2]; vh = vb[3];
                } else {
                    vw = attr_num(tag.attrs, "width", 1.0f);
                    vh = attr_num(tag.attrs, "height", 1.0f);
                    if (!(vw > 0) || !(vh > 0)) vw = vh = 1.0f;
                }
            }
            if (!tag.self_closing) stack.push_back(st);
            continue;
        }
        if (tag.name == "g" || tag.name == "a") {
            if (!tag.self_closing) stack.push_back(st);
            continue;
        }
        if (tag.name == "use" || tag.name == "image") {
            error = "<" + tag.name + ">: not supported in a stencil";
            return false;
        }
        const bool drawn = !st.hidden && tag.attrs["data-role"] != "base";
        if (tag.name == "rect" && drawn) {
            const float x = attr_num(tag.attrs, "x"), y = attr_num(tag.attrs, "y");
            const float w = attr_num(tag.attrs, "width"), h = attr_num(tag.attrs, "height");
            if (w > 0 && h > 0) {
                MaskShape r;
                r.kind = MaskShape::Kind::Rect;
                r.cx = nx(x);
                r.cy = ny(y);
                r.rx = nx(x + w);
                r.ry = ny(y + h);
                emit(st, to_norm({x, y, x + w, y, x + w, y + h, x, y + h}), true, &r);
            }
        } else if ((tag.name == "circle" || tag.name == "ellipse") && drawn) {
            const float cx = attr_num(tag.attrs, "cx"), cy = attr_num(tag.attrs, "cy");
            float rx, ry;
            if (tag.name == "circle") rx = ry = attr_num(tag.attrs, "r");
            else {
                rx = attr_num(tag.attrs, "rx");
                ry = attr_num(tag.attrs, "ry");
            }
            if (rx > 0 && ry > 0) {
                MaskShape e;
                e.kind = MaskShape::Kind::Ellipse;
                e.cx = nx(cx);
                e.cy = ny(cy);
                e.rx = rx / vw;
                e.ry = ry / vh;
                emit(st, ellipse_outline(cx, cy, rx, ry), true, &e);
            }
        } else if ((tag.name == "polygon" || tag.name == "polyline") && drawn) {
            std::vector<float> pts = parse_list(tag.attrs["points"]);
            if (pts.size() % 2) pts.pop_back();
            emit(st, to_norm(pts), tag.name == "polygon", nullptr);
        } else if (tag.name == "line" && drawn) {
            Style line = st;
            line.fill = Ink::None;
            emit(line, to_norm({attr_num(tag.attrs, "x1"), attr_num(tag.attrs, "y1"),
                                attr_num(tag.attrs, "x2"), attr_num(tag.attrs, "y2")}),
                 false, nullptr);
        } else if (tag.name == "path" && drawn) {
            std::vector<SvgSubpath> subs;
            if (!parse_svg_path(tag.attrs["d"], subs, error)) {
                error = "<path d=\"" + error + "\">: unreadable path data";
                return false;
            }
            for (const SvgSubpath& sp : subs) {
                MaskShape curve;
                curve.kind = MaskShape::Kind::Bezier;
                curve.pts = to_norm(sp.anchors);
                emit(st, to_norm(sp.pts), sp.closed, sp.anchors.empty() ? nullptr : &curve);
            }
        }
        if (!tag.self_closing && tag.name != "rect" && tag.name != "circle" &&
            tag.name != "ellipse" && tag.name != "polygon" && tag.name != "polyline" &&
            tag.name != "line" && tag.name != "path")
            hidden_depth = 1;
    }
    if (!seen_svg) {
        error = "not an SVG document";
        return false;
    }
    title = trim(title);
    return true;
}

bool load_mask_svg(const std::string& path, std::vector<MaskShape>& out, std::string& title,
                   std::string& error, std::string* camera) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        error = path;
        return false;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    if (!read_mask_svg(ss.str(), out, title, error, camera)) {
        error = path + ": " + error;
        return false;
    }
    return true;
}

bool load_mask_svg_set(const std::string& path, MaskSet& out, std::string& title,
                       std::string& error) {
    out = MaskSet{};
    std::string camera;
    std::vector<MaskShape> shapes;
    if (!load_mask_svg(path, shapes, title, error, &camera)) return false;
    if (camera.empty()) {
        out.shapes = std::move(shapes);
        return true;
    }
    out.cameras[camera] = std::move(shapes);
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path self = fs::path(path).lexically_normal();
    for (fs::directory_iterator it(self.parent_path().empty() ? fs::path(".") : self.parent_path(), ec),
         end;
         !ec && it != end; it.increment(ec)) {
        if (it->path().extension() != ".svg" || it->path().lexically_normal() == self) continue;
        std::string t, c, err;
        std::vector<MaskShape> other;
        // A sibling that does not read is not this set's business.
        if (!load_mask_svg(it->path().string(), other, t, err, &c)) continue;
        if (!c.empty() && t == title && !out.cameras.count(c)) out.cameras[c] = std::move(other);
    }
    return true;
}

bool save_mask_svg(const std::string& path, const std::vector<MaskShape>& shapes,
                   const std::string& title, std::string& error, const std::string& camera) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (f) f << write_mask_svg(shapes, title, camera);
    if (!f) {
        error = path;
        return false;
    }
    return true;
}

}  // namespace app
