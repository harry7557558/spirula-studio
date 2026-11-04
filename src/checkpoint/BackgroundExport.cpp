#include "checkpoint/BackgroundExport.h"

#include "core/CheckpointIO.h"
#include "core/ColorSpace.h"
#include "external/stb_image_write.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace ckpt {

float3 eval_sh_direction(int degree, float3 dir, float3 dc, const float3* coeffs) {
    float3 colors = make_float3(0.2820947917738781f * dc.x,
                                0.2820947917738781f * dc.y,
                                0.2820947917738781f * dc.z);
    if (degree <= 0) return colors;

    float norm_sq = dir.x * dir.x + dir.y * dir.y + dir.z * dir.z;
    if (norm_sq <= 1e-12f) return colors;
    float inv_norm = 1.0f / std::sqrt(norm_sq);
    float x = dir.x * inv_norm;
    float y = dir.y * inv_norm;
    float z = dir.z * inv_norm;

    auto add = [](float3& c, float weight, float3 coeff) {
        c.x += weight * coeff.x;
        c.y += weight * coeff.y;
        c.z += weight * coeff.z;
    };

    float fTmp0A = 0.48860251190292f;
    add(colors, fTmp0A * -y, coeffs[0]);
    add(colors, fTmp0A *  z, coeffs[1]);
    add(colors, fTmp0A * -x, coeffs[2]);
    if (degree <= 1) return colors;

    float z2 = z * z;
    float fTmp0B = -1.092548430592079f * z;
    float fTmp1A = 0.5462742152960395f;
    float fC1 = x * x - y * y;
    float fS1 = 2.0f * x * y;
    float pSH6 = (0.9461746957575601f * z2 - 0.3153915652525201f);
    float pSH7 = fTmp0B * x;
    float pSH5 = fTmp0B * y;
    float pSH8 = fTmp1A * fC1;
    float pSH4 = fTmp1A * fS1;
    add(colors, pSH4, coeffs[3]);
    add(colors, pSH5, coeffs[4]);
    add(colors, pSH6, coeffs[5]);
    add(colors, pSH7, coeffs[6]);
    add(colors, pSH8, coeffs[7]);
    if (degree <= 2) return colors;

    float fTmp0C = -2.285228997322329f * z2 + 0.4570457994644658f;
    float fTmp1B = 1.445305721320277f * z;
    float fTmp2A = -0.5900435899266435f;
    float fC2 = x * fC1 - y * fS1;
    float fS2 = x * fS1 + y * fC1;
    float pSH12 = z * (1.865881662950577f * z2 - 1.119528997770346f);
    float pSH13 = fTmp0C * x;
    float pSH11 = fTmp0C * y;
    float pSH14 = fTmp1B * fC1;
    float pSH10 = fTmp1B * fS1;
    float pSH15 = fTmp2A * fC2;
    float pSH9  = fTmp2A * fS2;
    add(colors, pSH9,  coeffs[8]);
    add(colors, pSH10, coeffs[9]);
    add(colors, pSH11, coeffs[10]);
    add(colors, pSH12, coeffs[11]);
    add(colors, pSH13, coeffs[12]);
    add(colors, pSH14, coeffs[13]);
    add(colors, pSH15, coeffs[14]);
    if (degree <= 3) return colors;

    float fTmp0D = z * (-4.683325804901025f * z2 + 2.007139630671868f);
    float fTmp1C = 3.31161143515146f * z2 - 0.47308734787878f;
    float fTmp2B = -1.770130769779931f * z;
    float fC3 = x * fC2 - y * fS2;
    float fS3 = x * fS2 + y * fC2;
    float pSH20 = (1.984313483298443f * z * pSH12 - 1.006230589874905f * pSH6);
    float pSH21 = fTmp0D * x;
    float pSH19 = fTmp0D * y;
    float pSH22 = fTmp1C * fC1;
    float pSH18 = fTmp1C * fS1;
    float pSH23 = fTmp2B * fC2;
    float pSH17 = fTmp2B * fS2;
    float pSH24 = 0.6258357354491763f * fC3;
    float pSH16 = 0.6258357354491763f * fS3;
    add(colors, pSH16, coeffs[15]);
    add(colors, pSH17, coeffs[16]);
    add(colors, pSH18, coeffs[17]);
    add(colors, pSH19, coeffs[18]);
    add(colors, pSH20, coeffs[19]);
    add(colors, pSH21, coeffs[20]);
    add(colors, pSH22, coeffs[21]);
    add(colors, pSH23, coeffs[22]);
    add(colors, pSH24, coeffs[23]);

    return colors;
}

bool render_background_sh_equirectangular(const float3* coeffs, int degree,
                                          int width, int height,
                                          std::vector<uint8_t>& out_rgb8,
                                          int transfer,
                                          bool is_linear) {
    if (!coeffs || degree < 0 || degree > 4 || width <= 0 || height <= 0)
        return false;

    constexpr float kPi = 3.14159265358979323846f;
    out_rgb8.resize((size_t)width * height * 3);

    const float3 dc = coeffs[0];
    const float3* higher = coeffs + 1;

#pragma omp parallel for schedule(static)
    for (int py = 0; py < height; ++py) {
        float lat = (0.5f - ((float)py + 0.5f) / (float)height) * kPi;
        float cl = std::cos(lat);
        float sl = std::sin(lat);

        for (int px = 0; px < width; ++px) {
            float lon = (((float)px + 0.5f) / (float)width - 0.5f) * (2.0f * kPi);
            float3 dir = make_float3(cl * std::sin(lon), sl, cl * std::cos(lon));

            float3 col = eval_sh_direction(degree, dir, dc, higher);
            col.x = std::max(col.x + 0.5f, 0.0f);
            col.y = std::max(col.y + 0.5f, 0.0f);
            col.z = std::max(col.z + 0.5f, 0.0f);

            float r = col.x, g = col.y, b = col.z;
            if (is_linear) {
                r = colorspace::working_to_display(r, (colorspace::Transfer)transfer, true);
                g = colorspace::working_to_display(g, (colorspace::Transfer)transfer, true);
                b = colorspace::working_to_display(b, (colorspace::Transfer)transfer, true);
            }

            size_t idx = ((size_t)py * width + px) * 3;
            out_rgb8[idx + 0] = (uint8_t)(std::clamp(r, 0.0f, 1.0f) * 255.0f + 0.5f);
            out_rgb8[idx + 1] = (uint8_t)(std::clamp(g, 0.0f, 1.0f) * 255.0f + 0.5f);
            out_rgb8[idx + 2] = (uint8_t)(std::clamp(b, 0.0f, 1.0f) * 255.0f + 0.5f);
        }
    }
    return true;
}

bool export_background_sh_webp(const fs::path& out_path,
                               const float3* coeffs, int degree,
                               int width, int height,
                               int transfer, bool is_linear) {
    std::vector<uint8_t> rgb8;
    if (!render_background_sh_equirectangular(coeffs, degree, width, height,
                                              rgb8, transfer, is_linear))
        return false;

    fs::path target = out_path;
    if (target.extension() != ".webp") target.replace_extension(".webp");
    std::error_code ec;
    fs::create_directories(target.parent_path(), ec);

    fs::path temp_png = target;
    temp_png.replace_extension(".png");

    if (!stbi_write_png(temp_png.string().c_str(), width, height, 3,
                        rgb8.data(), width * 3))
        return false;

    std::string cmd = "ffmpeg -y -hide_banner -loglevel error -i \"" +
                      temp_png.string() + "\" -q:v 90 \"" +
                      target.string() + "\" 2>&1";
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        std::string cmd2 = "cwebp -q 90 \"" + temp_png.string() +
                           "\" -o \"" + target.string() + "\" 2>&1";
        ret = std::system(cmd2.c_str());
    }

    if (ret == 0 && fs::is_regular_file(target, ec)) {
        fs::remove(temp_png, ec);
        return true;
    }

    return true;
}

bool load_background_sh(const fs::path& ckpt_dir,
                        int& out_degree,
                        std::vector<float3>& out_coeffs,
                        int& out_transfer,
                        bool& out_is_linear) {
    fs::path tarpath;
    std::error_code ec;
    if (fs::is_regular_file(ckpt_dir / "state.tar", ec))
        tarpath = ckpt_dir / "state.tar";
    else if (fs::is_regular_file(ckpt_dir, ec) && ckpt_dir.filename() == "state.tar")
        tarpath = ckpt_dir;
    else if (fs::is_directory(ckpt_dir, ec)) {
        for (const auto& entry : fs::directory_iterator(ckpt_dir, ec)) {
            if (entry.is_directory(ec) && fs::is_regular_file(entry.path() / "state.tar", ec)) {
                tarpath = entry.path() / "state.tar";
                break;
            }
        }
    }
    if (tarpath.empty() || !fs::is_regular_file(tarpath, ec)) return false;

    std::ifstream in(tarpath.string(), std::ios::binary);
    if (!in) return false;

    auto members = tar_index(in);
    out_transfer = 0;
    out_is_linear = false;

    for (const auto& m : members) {
        if (m.name == "state.json") {
            std::string sj = tar_read_member(in, m);
            size_t pos = sj.find("\"background\":");
            if (pos != std::string::npos) {
                size_t tr = sj.find("\"splat_transfer\":", pos);
                if (tr != std::string::npos)
                    out_transfer = std::atoi(sj.c_str() + tr + 17);
                size_t lin = sj.find("\"splat_is_linear\":", pos);
                if (lin != std::string::npos)
                    out_is_linear = (std::atoi(sj.c_str() + lin + 18) != 0);
            }
            break;
        }
    }

    for (const auto& m : members) {
        if (m.name == "eng.bg_sky.sh_coeffs.npy") {
            auto info = npy_locate(in, m.data_offset, m.size);
            int64_t n_coeffs = (int64_t)(info.data_bytes / (3 * sizeof(float)));
            int deg = (int)std::round(std::sqrt((double)n_coeffs)) - 1;
            if (deg < 0 || deg > 4 || (deg + 1) * (deg + 1) != n_coeffs)
                return false;
            out_degree = deg;
            out_coeffs.resize((size_t)n_coeffs);
            in.clear();
            in.seekg((std::streamoff)info.data_offset, std::ios::beg);
            in.read(reinterpret_cast<char*>(out_coeffs.data()),
                    (std::streamsize)info.data_bytes);
            return in.good();
        }
    }
    return false;
}

bool export_background_from_checkpoint(const fs::path& ckpt_dir,
                                       const fs::path& out_path,
                                       int width, int height) {
    int degree = 0, transfer = 0;
    bool is_linear = false;
    std::vector<float3> coeffs;
    if (!load_background_sh(ckpt_dir, degree, coeffs, transfer, is_linear))
        return false;

    fs::path dst = out_path;
    if (dst.empty()) {
        std::error_code ec;
        if (fs::is_directory(ckpt_dir, ec))
            dst = ckpt_dir / "background.webp";
        else
            dst = ckpt_dir.parent_path() / "background.webp";
    }
    return export_background_sh_webp(dst, coeffs.data(), degree, width, height,
                                     transfer, is_linear);
}

}  // namespace ckpt
