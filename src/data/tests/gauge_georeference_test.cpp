// gauge_georeference_test -- a GPS origin in gauge.txt (sfm/Pipeline.cpp
// writeGauge) read back by dsparse::read_gauge and carried into the run's
// scene_transform.json, which is all a trained model has to be placed by.

#include "data/DatasetParser.h"
#include "data/Json.h"
#include "data/SceneTransform.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

ParsedDataset read(const fs::path& dir, const std::string& text) {
    std::ofstream(dir / "gauge.txt", std::ios::trunc) << text;
    ParsedDataset ds;
    dsparse::read_gauge(dir.string(), ds);
    return ds;
}

JsonValue transform_json(const ParsedDataset& ds) {
    const double centre[3] = {0, 0, 0};
    return json_parse(spirula::scene_transform_json(spirula::SceneTransform{}, "none", centre,
                                                    ds.gauge_geo));
}

}  // namespace

int main() {
    const fs::path dir = fs::temp_directory_path() / "gauge_georeference_test";
    fs::create_directories(dir);
    const std::string head =
        "# What this model's frame means, from spirula sfm.\n"
        "oriented 1\nmetric 1\nup sensors\nscale gps\n";

    {
        const ParsedDataset ds = read(dir, head +
            "# x east, y north of this WGS-84 point, in metres\n"
            "origin_lat 62.669093790\norigin_lon -13.967551870\n"
            "origin_alt 710.520\nalt_source dji\n"
            "point_gamut Rec.2020\npoint_linear 0\n");
        const spirula::GeoReference& g = ds.gauge_geo;
        check(ds.gauge_oriented && ds.gauge_metric, "the two bits still read");
        check(g.ok && g.lat_deg == 62.669093790 && g.lon_deg == -13.967551870 &&
                  g.alt_m == 710.520 && g.alt_source == "dji",
              "origin and its height");
        check(ds.point_color_gamut == "Rec.2020" && ds.point_color_is_linear == false,
              "the sparse point cloud declares its own gamut and transfer");
        const JsonValue j = transform_json(ds);
        const JsonValue* geo = j.find("georeference");
        check(geo && geo->is_object() && geo->get_double("origin_lat", 0) == g.lat_deg &&
                  geo->get_double("origin_lon", 0) == g.lon_deg &&
                  geo->get_double("origin_alt", 0) == g.alt_m && geo->find("alt_source") &&
                  geo->find("alt_source")->as_string() == "dji",
              "scene_transform.json carries it exactly");
    }
    {
        const ParsedDataset ds = read(dir, head + "origin_lat 1.5\norigin_lon 2.5\n");
        check(ds.gauge_geo.ok && ds.gauge_geo.alt_source.empty(), "an origin with no height");
        const JsonValue* geo = transform_json(ds).find("georeference");
        check(geo && !geo->find("origin_alt") && !geo->find("alt_source"),
              "... writes none to the json either");
    }
    {
        const ParsedDataset ds =
            read(dir, head + "origin_lat 1.5\norigin_lon 2.5\nalt_source exif\n");
        check(ds.gauge_geo.ok && ds.gauge_geo.alt_source.empty(),
              "a source with no origin_alt names no height");
    }
    {
        const ParsedDataset ds = read(dir, head + "origin_lat 1.5x\norigin_lon 2.5\n");
        check(!ds.gauge_geo.ok, "an unreadable latitude is no origin");
    }
    {
        const ParsedDataset ds = read(dir, head);
        check(!ds.gauge_geo.ok && !transform_json(ds).find("georeference"),
              "no origin, no georeference");
    }

    fs::remove_all(dir);
    std::printf("%s (%d failure%s)\n", g_failures ? "FAILED" : "OK", g_failures,
                g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
