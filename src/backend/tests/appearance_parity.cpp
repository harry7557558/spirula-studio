// Parity tool for the fused appearance chain (appearance_chain_forward /
// _backward): background blend, PPISP either side of the display encode, and
// the encode itself, against the same stages run one kernel at a time. A
// masked draw is checked against both blends picked per pixel on the host.
//
//   ./appearance_parity check            fused vs per-stage, this backend
//   ./appearance_parity dump ref.bin     CUDA build
//   ./appearance_parity compare ref.bin  Vulkan build, per device
//
// The PPISP parameter gradient is block-reduced and atomically accumulated
// in a different order by the two paths, so it is compared loosely.

#include <kernels/pixelwise/PixelWise.cuh>
#include <engine/EngineInternal.h>
#include <core/Tensor.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <string>
#include <vector>

using backend::MemcpyKind;

namespace {

constexpr int64_t B = 2, H = 37, W = 70, N_CAM = 4;
constexpr int64_t PIX = B * H * W;
constexpr int MH = 19, MW = 35;  // the mask is nearest-sampled up

template <typename T>
T* upload(const std::vector<T>& host) {
    T* d = (T*)backend::device_malloc(((host.size() * sizeof(T) + 3) / 4) * 4);
    backend::memcpy_sync(d, host.data(), host.size() * sizeof(T),
                         MemcpyKind::HostToDevice);
    return d;
}

float* zeros(int64_t n) { return upload(std::vector<float>(n, 0.0f)); }

std::vector<float> read(const float* d, int64_t n) {
    std::vector<float> h(n);
    backend::memcpy_sync(h.data(), d, n * sizeof(float), MemcpyKind::DeviceToHost);
    return h;
}

TorchTensorView ttv(const void* p, std::vector<int64_t> shape) {
    return std::make_tuple((uint64_t)p, (uint32_t)4, std::move(shape));
}
TorchTensorView ttv_null() {
    return std::make_tuple((uint64_t)0, (uint32_t)4, std::vector<int64_t>{0});
}
DeviceTensor3D<float3> t3(const float* p) {
    return DeviceTensor3D<float3>(ttv(p, {B, H, W, 3}));
}
DeviceTensor3D<float> t1(const float* p) {
    return DeviceTensor3D<float>(ttv(p, {B, H, W, 1}));
}

struct Rng {
    std::mt19937 rng;
    explicit Rng(uint32_t s) : rng(s) {}
    float uf(float lo, float hi) {
        return lo + (hi - lo) * (float)(rng() & 0xffffff) / 16777215.0f;
    }
    std::vector<float> vec(int64_t n, float lo, float hi) {
        std::vector<float> v(n);
        for (auto& x : v) x = uf(lo, hi);
        return v;
    }
};

struct Case {
    AppearanceBg bg;
    bool noise;
    bool masked;
    int blocky;
    unsigned block_px;
    bool match_luma;
    AppearancePpisp ppisp;
    const char* ppisp_type;
    int cs_transfer;  // -1: no encode
    int cs_linear;
    float overexposure;
};

struct Inputs {
    float *raw, *T, *bg_image, *v_out, *cs_matrix, *intrins, *exponent;
    uint8_t* mask;
    std::vector<uint8_t> mask_host;
    int32_t* cams;
    float* params[6];
    int n_params[6];
};

const char* kPpispTypes[6] = {"original", "rqs", "no_crf", "no_crf_clamp",
                              "no_crf_no_vig", "no_crf_no_vig_clamp"};

int type_index(const char* name) {
    for (int i = 0; i < 6; i++)
        if (!std::strcmp(kPpispTypes[i], name)) return i;
    return 0;
}

AppearanceChainParams params_for(const Case& c, const Inputs& in) {
    AppearanceChainParams p;
    p.cam_indices = in.cams;
    p.bg = c.bg;
    p.bg_color[0] = 0.2f; p.bg_color[1] = 0.65f; p.bg_color[2] = 0.9f;
    p.bg_noise = c.noise ? 1 : 0;
    if (c.masked) {
        p.bg_mask = in.mask;
        p.bg_mask_h = MH;
        p.bg_mask_w = MW;
    }
    p.bg_transfer = c.cs_transfer < 0 ? 0 : c.cs_transfer;
    p.bg_is_linear = c.cs_linear;
    p.bg_blocky = c.blocky;
    p.bg_block_px = c.block_px;
    p.bg_seed = 4242u;
    p.bg_randomize_weight = 0.7f;
    p.bg_exponent_by_cam = c.match_luma ? in.exponent : nullptr;
    p.bg_image = in.bg_image;
    if (c.cs_transfer >= 0) {
        p.cs_enabled = 1;
        p.cs_transfer = c.cs_transfer;
        p.cs_is_linear = c.cs_linear;
        p.cs_matrix = in.cs_matrix;
    }
    if (c.ppisp != AppearancePpisp::Off) {
        const PpispParamSpec spec = ppisp_param_spec(c.ppisp_type);
        p.ppisp = c.ppisp;
        p.ppisp_layout = spec.layout;
        p.ppisp_clamp = spec.clamp_output ? 1 : 0;
        p.ppisp_params = in.params[type_index(c.ppisp_type)];
        p.intrins = in.intrins;
    }
    p.overexposure_weight = c.overexposure;
    return p;
}

struct Result {
    std::vector<float> out, v_raw, v_T, v_bg, v_params;
};

Result run_fused(const Case& c, const Inputs& in) {
    AppearanceChainParams p = params_for(c, in);
    const int np = c.ppisp != AppearancePpisp::Off
        ? in.n_params[type_index(c.ppisp_type)] : 0;
    float* out = zeros(PIX * 3);
    appearance_chain_forward(p, ttv(in.raw, {B, H, W, 3}), t1(in.T),
                             ttv(out, {B, H, W, 3}), ttv_null());
    float* v = upload(read(in.v_out, PIX * 3));
    float* v_T = upload(read(in.T, PIX));  // added into, so start non-zero
    float* v_bg = zeros(PIX * 3);
    float* v_params = zeros(std::max<int64_t>(N_CAM * np, 1));
    p.v_bg_image = v_bg;
    p.v_ppisp_params = v_params;
    appearance_chain_backward(p, ttv(in.raw, {B, H, W, 3}), t1(in.T), t3(v), t1(v_T));
    backend::device_synchronize();
    Result r{read(out, PIX * 3), read(v, PIX * 3), read(v_T, PIX),
             read(v_bg, PIX * 3), read(v_params, N_CAM * np)};
    for (float* d : {out, v, v_T, v_bg, v_params}) backend::device_free(d);
    return r;
}

Result run_stages(const Case& c, const Inputs& in) {
    const int np = c.ppisp != AppearancePpisp::Off
        ? in.n_params[type_index(c.ppisp_type)] : 0;
    const TorchTensorView params = c.ppisp != AppearancePpisp::Off
        ? ttv(in.params[type_index(c.ppisp_type)], {N_CAM, np}) : ttv_null();
    const TorchTensorView cams = ttv(in.cams, {B});
    const TorchTensorView intr = ttv(in.intrins, {B, 4});
    const DeviceTensor2D<float3> cm(ttv(in.cs_matrix, {3, 1, 3}));
    const int xf = c.cs_transfer < 0 ? 0 : c.cs_transfer;
    const bool lin = c.cs_linear != 0;

    // Per pixel: the draw, or the base (black when it is None).
    const bool has_bg = c.noise || c.bg != AppearanceBg::None;
    const float3 base_color = c.bg == AppearanceBg::Color
        ? make_float3(0.2f, 0.65f, 0.9f) : make_float3(0.0f, 0.0f, 0.0f);
    std::vector<uint8_t> drawn(PIX, 0);
    for (int64_t b = 0; b < B; b++)
        for (int64_t y = 0; y < H; y++)
            for (int64_t x = 0; x < W; x++) {
                const float u = ((float)x + 0.5f) * MW / W - 0.5f;
                const float v = ((float)y + 0.5f) * MH / H - 0.5f;
                const int mx = std::max(0, std::min(MW - 1, (int)std::floor(u + 0.5f)));
                const int my = std::max(0, std::min(MH - 1, (int)std::floor(v + 0.5f)));
                const bool seg = c.masked && (in.mask_host[(b * MH + my) * MW + mx] & 2);
                drawn[(b * H + y) * W + x] = c.noise && !seg;
            }
    auto pick = [&](const std::vector<float>& base, const std::vector<float>& draw,
                    int ch) {
        std::vector<float> out = base;
        for (int64_t i = 0; i < PIX; i++)
            if (drawn[i])
                for (int k = 0; k < ch; k++) out[i * ch + k] = draw[i * ch + k];
        return out;
    };

    float* s_bg = zeros(PIX * 3);
    if (c.bg == AppearanceBg::Image)
        blend_background_forward(t3(in.raw), t1(in.T), t3(in.bg_image), t3(s_bg));
    else if (has_bg)
        blend_background_color_forward(t3(in.raw), t1(in.T), base_color, t3(s_bg));
    else
        backend::memcpy_sync(s_bg, in.raw, PIX * 12, MemcpyKind::DeviceToDevice);
    if (c.noise) {
        float* s_draw = zeros(PIX * 3);
        blend_background_noise_forward(xf, lin, c.blocky != 0, c.block_px,
                                       t3(in.raw), t1(in.T), 0.7f, 4242u,
                                       c.match_luma ? in.exponent : nullptr,
                                       in.cams, t3(s_draw));
        const std::vector<float> sel = pick(read(s_bg, PIX * 3), read(s_draw, PIX * 3), 3);
        backend::memcpy_sync(s_bg, sel.data(), PIX * 12, MemcpyKind::HostToDevice);
        backend::device_free(s_draw);
    }
    float* s_p1 = s_bg;
    if (c.ppisp == AppearancePpisp::BeforeEncode) {
        s_p1 = zeros(PIX * 3);
        ppisp_forward(t3(s_bg), params, intr, (float)W, (float)H, c.ppisp_type,
                      cams, t3(s_p1));
    }
    float* s_enc = s_p1;
    if (c.cs_transfer >= 0) {
        s_enc = zeros(PIX * 3);
        working_to_display_forward(xf, lin, t3(s_p1), cm, t3(s_enc));
    }
    float* out = s_enc;
    if (c.ppisp == AppearancePpisp::AfterEncode) {
        out = zeros(PIX * 3);
        ppisp_forward(t3(s_enc), params, intr, (float)W, (float)H, c.ppisp_type,
                      cams, t3(out));
    }

    float* v = upload(read(in.v_out, PIX * 3));
    float* v_params = zeros(std::max<int64_t>(N_CAM * np, 1));
    if (c.ppisp == AppearancePpisp::AfterEncode)
        ppisp_backward(t3(s_enc), params, intr, (float)W, (float)H, t3(v),
                       c.ppisp_type, cams, t3(v), ttv(v_params, {N_CAM, np}));
    if (c.cs_transfer >= 0)
        working_to_display_backward(xf, lin, t3(s_p1), cm, t3(v), t3(v));
    if (c.ppisp == AppearancePpisp::BeforeEncode)
        ppisp_backward(t3(s_bg), params, intr, (float)W, (float)H, t3(v),
                       c.ppisp_type, cams, t3(v), ttv(v_params, {N_CAM, np}));
    float* v_T = upload(read(in.T, PIX));
    float* v_bg = zeros(PIX * 3);
    if (has_bg) {
        const std::vector<float> v_in = read(v, PIX * 3);
        float* v_T_base = zeros(PIX);
        float* v_base = upload(v_in);
        if (c.bg == AppearanceBg::Image)
            blend_background_backward(t3(in.raw), t1(in.T), t3(in.bg_image),
                                      c.overexposure, t3(v_base), t3(v_base),
                                      t1(v_T_base), t3(v_bg));
        else
            blend_background_color_backward(t3(in.raw), t1(in.T), base_color,
                                            c.overexposure, t3(v_base), t3(v_base),
                                            t1(v_T_base));
        std::vector<float> vr = read(v_base, PIX * 3), vt = read(v_T_base, PIX);
        if (c.noise) {
            float* v_draw = upload(v_in);
            float* v_T_draw = zeros(PIX);
            blend_background_noise_backward(xf, lin, c.blocky != 0, c.block_px,
                                            t3(in.raw), t1(in.T), 0.7f, 4242u,
                                            c.match_luma ? in.exponent : nullptr,
                                            in.cams, c.overexposure, t3(v_draw),
                                            t3(v_draw), t1(v_T_draw));
            vr = pick(vr, read(v_draw, PIX * 3), 3);
            vt = pick(vt, read(v_T_draw, PIX), 1);
            std::vector<float> vb = read(v_bg, PIX * 3);
            vb = pick(vb, std::vector<float>(PIX * 3, 0.0f), 3);
            backend::memcpy_sync(v_bg, vb.data(), PIX * 12, MemcpyKind::HostToDevice);
            backend::device_free(v_draw);
            backend::device_free(v_T_draw);
        }
        backend::memcpy_sync(v, vr.data(), PIX * 12, MemcpyKind::HostToDevice);
        std::vector<float> a = read(v_T, PIX);
        for (int64_t i = 0; i < PIX; i++) a[i] += vt[i];
        backend::memcpy_sync(v_T, a.data(), PIX * 4, MemcpyKind::HostToDevice);
        backend::device_free(v_T_base);
        backend::device_free(v_base);
    } else if (c.overexposure != 0.0f) {
        overexposure_grad_add(ttv(in.raw, {B, H, W, 3}), c.overexposure, t3(v));
    }
    backend::device_synchronize();
    Result r{read(out, PIX * 3), read(v, PIX * 3), read(v_T, PIX),
             read(v_bg, PIX * 3), read(v_params, N_CAM * np)};
    std::vector<float*> owned = {s_bg, s_p1, s_enc, out};
    std::sort(owned.begin(), owned.end());
    owned.erase(std::unique(owned.begin(), owned.end()), owned.end());
    for (float* d : owned) backend::device_free(d);
    for (float* d : {v, v_T, v_bg, v_params}) backend::device_free(d);
    return r;
}

std::vector<Case> cases() {
    std::vector<Case> cs;
    const AppearancePpisp pos[3] = {AppearancePpisp::Off,
                                    AppearancePpisp::BeforeEncode,
                                    AppearancePpisp::AfterEncode};
    struct Bg { AppearanceBg bg; bool noise, masked; int blocky; unsigned px; bool luma; };
    const Bg bgs[9] = {{AppearanceBg::None, false, false, 0, 0, false},
                       {AppearanceBg::Color, false, false, 0, 0, false},
                       {AppearanceBg::None, true, false, 0, 0, true},
                       {AppearanceBg::None, true, false, 0, 1, false},
                       {AppearanceBg::None, true, false, 1, 64, true},
                       {AppearanceBg::Image, false, false, 0, 0, false},
                       {AppearanceBg::None, true, true, 0, 1, false},
                       {AppearanceBg::Color, true, true, 1, 8, true},
                       {AppearanceBg::Image, true, true, 0, 0, false}};
    const int xfs[4][2] = {{-1, 0}, {0, 1}, {2, 0}, {4, 1}};
    int k = 0;
    for (const Bg& b : bgs)
        for (AppearancePpisp pp : pos)
            for (const auto& xf : xfs) {
                Case c{b.bg, b.noise, b.masked, b.blocky, b.px, b.luma, pp,
                       kPpispTypes[k % 6], xf[0], xf[1], (k % 3 == 0) ? 3.0f : 0.0f};
                cs.push_back(c);
                k++;
            }
    return cs;
}

}  // namespace

int main(int argc, char** argv) {
    const bool check = argc == 2 && !std::strcmp(argv[1], "check");
    const bool dumping = argc == 3 && !std::strcmp(argv[1], "dump");
    const bool comparing = argc == 3 && !std::strcmp(argv[1], "compare");
    if (!check && !dumping && !comparing) {
        std::fprintf(stderr, "usage: %s check | dump|compare <ref.bin>\n", argv[0]);
        return 2;
    }

    Rng r(310777u);
    Inputs in;
    in.raw = upload(r.vec(PIX * 3, -0.1f, 1.6f));
    in.T = upload(r.vec(PIX, 0.0f, 1.0f));
    in.bg_image = upload(r.vec(PIX * 3, 0.0f, 1.0f));
    in.mask_host.resize((size_t)B * MH * MW);
    for (auto& m : in.mask_host) m = (uint8_t)(r.rng() & 3u);
    in.mask = upload(in.mask_host);
    std::vector<float> v_out = r.vec(PIX * 3, -1.0f, 1.0f);
    for (int64_t i = 0; i < PIX; i += 7)   // masked pixels: zero gradient
        v_out[3 * i] = v_out[3 * i + 1] = v_out[3 * i + 2] = 0.0f;
    in.v_out = upload(v_out);
    in.cs_matrix = upload<float>({0.9f, 0.08f, 0.02f, 0.05f, 0.9f, 0.05f,
                                  0.02f, 0.08f, 0.9f});
    in.intrins = upload<float>({80.f, 80.f, 33.f, 17.f, 75.f, 79.f, 36.f, 20.f});
    in.exponent = upload<float>({1.0f, 3.9f, 0.6f, 2.5f});
    in.cams = upload<int32_t>({3, 1});
    for (int i = 0; i < 6; i++) {
        in.n_params[i] = ppisp_param_spec(kPpispTypes[i]).num_params;
        in.params[i] = upload(r.vec(N_CAM * in.n_params[i], -0.3f, 0.3f));
    }

    std::vector<float> tight, loose;
    int fails = 0;
    double worst = 0.0;
    for (const Case& c : cases()) {
        Result f = run_fused(c, in);
        if (check) {
            Result s = run_stages(c, in);
            auto cmp = [&](const std::vector<float>& a, const std::vector<float>& b,
                           double rel, const char* what) {
                double m = 0.0;
                for (size_t i = 0; i < a.size(); i++) {
                    double d = std::fabs((double)a[i] - b[i]) /
                               (1e-4 + rel * std::fabs((double)b[i]));
                    m = std::max(m, d);
                }
                worst = std::max(worst, m);
                if (m > 1.0) {
                    fails++;
                    std::fprintf(stderr,
                                 "FAIL %s: bg=%d draw=%d mask=%d px=%u ppisp=%d (%s) "
                                 "xf=%d lin=%d oe=%g -- %.3g of tolerance\n",
                                 what, (int)c.bg, (int)c.noise, (int)c.masked,
                                 c.block_px, (int)c.ppisp,
                                 c.ppisp_type, c.cs_transfer, c.cs_linear,
                                 c.overexposure, m);
                }
            };
            cmp(f.out, s.out, 1e-5, "out");
            cmp(f.v_raw, s.v_raw, 1e-4, "v_rgb");
            cmp(f.v_T, s.v_T, 1e-4, "v_T");
            cmp(f.v_bg, s.v_bg, 1e-4, "v_bg");
            cmp(f.v_params, s.v_params, 1e-3, "v_params");
        } else {
            for (auto* v : {&f.out, &f.v_raw, &f.v_T, &f.v_bg})
                tight.insert(tight.end(), v->begin(), v->end());
            loose.insert(loose.end(), f.v_params.begin(), f.v_params.end());
        }
    }

    if (check) {
        std::printf("appearance_parity check: %zu cases, worst %.3g of tolerance, "
                    "%d failing\n", cases().size(), worst, fails);
        return fails ? 1 : 0;
    }
    if (dumping) {
        std::ofstream f(argv[2], std::ios::binary);
        int64_t nt = (int64_t)tight.size(), nl = (int64_t)loose.size();
        f.write((const char*)&nt, 8);
        f.write((const char*)&nl, 8);
        f.write((const char*)tight.data(), nt * 4);
        f.write((const char*)loose.data(), nl * 4);
        std::printf("appearance_parity: dumped %lld + %lld floats to %s\n",
                    (long long)nt, (long long)nl, argv[2]);
        return 0;
    }
    std::ifstream f(argv[2], std::ios::binary);
    int64_t nt = 0, nl = 0;
    f.read((char*)&nt, 8);
    f.read((char*)&nl, 8);
    if (!f || nt != (int64_t)tight.size() || nl != (int64_t)loose.size()) {
        std::fprintf(stderr, "reference size mismatch\n");
        return 1;
    }
    std::vector<float> rt(nt), rl(nl);
    f.read((char*)rt.data(), nt * 4);
    f.read((char*)rl.data(), nl * 4);
    auto count = [](const std::vector<float>& got, const std::vector<float>& ref,
                    double abs_tol, double rel_tol, double& max_abs) {
        int64_t viol = 0;
        for (size_t i = 0; i < ref.size(); i++) {
            double d = std::fabs((double)got[i] - ref[i]);
            max_abs = std::max(max_abs, d);
            if (d > abs_tol + rel_tol * std::fabs((double)ref[i])) viol++;
        }
        return viol;
    };
    double mt = 0.0, ml = 0.0;
    const int64_t vt = count(tight, rt, 5e-4, 5e-4, mt);
    const int64_t vl = count(loose, rl, 1e-2, 1e-3, ml);
    std::printf("appearance_parity: tight %lld/%lld out of tolerance (max_abs %.3g), "
                "loose %lld/%lld (max_abs %.3g)\n",
                (long long)vt, (long long)nt, mt, (long long)vl, (long long)nl, ml);
    // A threshold-crossing pixel (a clamp, the sRGB toe) may flip between
    // compilers; a layout or indexing break moves whole images.
    return (vt * 1000 > nt || vl * 1000 > nl) ? 1 : 0;
}
