// MarkerTypes.h -- types for fiducial marker detection, 3D triangulation and alignment.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace spirula {

enum class MarkerFamily { Auto = -1, Tag36h10 = 0, Tag36h11, Aruco4x4, Aruco5x5, Aruco6x6, Aruco7x7, ArucoOriginal };

const char* marker_family_name(MarkerFamily family);
bool parse_marker_family(const std::string& name, MarkerFamily& family);

struct MarkerDetection {
    int id = -1;
    MarkerFamily family = MarkerFamily::Tag36h10;
    double center[2]{0.0, 0.0};
    double corners[4][2]{{0.0, 0.0}};  // CCW: c0, c1, c2, c3
    double hamming = 0.0;
    double margin = 0.0;
};

struct MarkerObservation {
    uint32_t image_id = 0;
    std::string image_name;
    MarkerDetection det;
};

struct Marker3D {
    int id = -1;
    MarkerFamily family = MarkerFamily::Tag36h10;
    double center[3]{0.0, 0.0, 0.0};
    double corners[4][3]{{0.0, 0.0, 0.0}};
    double normal[3]{0.0, 0.0, 1.0};
    int num_views = 0;
    double reproj_error_px = 0.0;
};

struct Marker3Def {
    int origin_id = -1;
    int x_axis_id = -1;
    int xy_plane_id = -1;
    double dist_x = 0.0;  // metres along origin->X; <= 0 keeps SfM scale
};

struct MarkerGcp {
    int id = -1;
    double pos[3]{0.0, 0.0, 0.0};
};

struct MarkerSystemConfig {
    MarkerFamily family = MarkerFamily::Tag36h10;
    Marker3Def three_markers;
    std::vector<MarkerGcp> gcp_table;
    bool has_three_markers() const {
        return three_markers.origin_id >= 0 &&
               three_markers.x_axis_id >= 0 &&
               three_markers.xy_plane_id >= 0;
    }
};

}  // namespace spirula

