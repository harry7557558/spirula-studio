#include "data/NavigationHint.h"
#include "data/DatasetParser.h"

#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {
std::vector<double> arc(double angle, double elevation = 0, bool outward = false) {
    std::vector<double> poses(120*12, 0);
    for (int i = 0; i < 120; i++) {
        const double t = angle*i/119;
        const double p[3] = {std::cos(t), std::sin(t), elevation};
        const double norm = std::sqrt(1+elevation*elevation);
        for (int k = 0; k < 3; k++) {
            poses[i*12+k*4+3] = p[k];
            poses[i*12+k*4+2] = (outward ? -1 : 1)*p[k]/norm;
        }
    }
    return poses;
}
void check(const char* name, const std::vector<double>& poses, bool orbit) {
    const auto result = dsparse::navigation_hint(poses.data(), poses.size()/12);
    if (result.orbit != orbit) throw std::runtime_error(name);
    std::printf("PASS %s\n", name);
}
}

int main(int argc, char** argv) {
    try {
        for (int i = 1; i < argc; i++) {
            DatasetParserConfig cfg;
            cfg.require_image_files = false;
            const auto ds = parse_dataset(argv[i], cfg, "");
            const auto hint = dsparse::navigation_hint(ds.c2w.data(), ds.num_cameras);
            std::printf("%s: %s (%lld cameras)\n", argv[i], hint.orbit ? "orbit" : "fly", (long long)ds.num_cameras);
        }
        if (argc > 1) return 0;
        check("no poses", {}, true);
        check("full orbit", arc(6.2831853), true);
        check("half orbit", arc(3.1415927), true);
        check("quarter orbit", arc(1.5707963), true);
        check("pitched orbit", arc(6.2831853, 0.7), true);
        check("pitched quarter orbit", arc(1.5707963, 0.7), true);
        auto hemisphere = arc(6.2831853);
        for (int i = 0; i < 120; i++) {
            double z = (i%12)/12.0, t = i*2.39996323;
            const double p[3] = {std::sqrt(1-z*z)*std::cos(t), std::sqrt(1-z*z)*std::sin(t), z};
            for (int k = 0; k < 3; k++) {
                hemisphere[i*12+4*k+3] = p[k];
                hemisphere[i*12+4*k+2] = p[k];
            }
        }
        check("hemisphere", hemisphere, true);
        const auto focus = dsparse::navigation_hint(hemisphere.data(), 120);
        for (double x : focus.focus) if (std::abs(x) > 1e-6) throw std::runtime_error("focus position");
        check("outward panorama", arc(6.2831853, 0, true), false);
        check("small forward arc", arc(0.1), false);
        auto poses = arc(6.2831853);
        for (int i = 0; i < 12; i++) {
            poses[i*12+2] *= -1; poses[i*12+6] *= -1;
            poses[i*12+3] += 5;
        }
        check("ten percent outliers", poses, true);
        poses = arc(6.2831853);
        for (int i = 0; i < 120; i++) for (int k = 0; k < 3; k++)
            poses[i*12+k*4+3] = 2000 + 0.001*poses[i*12+k*4+3];
        check("translated and scaled orbit", poses, true);
        poses = arc(6.2831853);
        for (int i = 0; i < 120; i++) {
            poses[i*12+3] = 0; poses[i*12+7] = 0; poses[i*12+11] = i;
            poses[i*12+2] = 0; poses[i*12+6] = 0; poses[i*12+10] = -1;
        }
        check("corridor", poses, false);
        for (int i = 0; i < 120; i++) poses[i*12+11] = 0;
        check("coincident cameras", poses, false);
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
