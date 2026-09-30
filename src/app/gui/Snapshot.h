#pragma once

#include "data/SceneTransform.h"

namespace gui {

enum class SnapshotCoordinates { ZUp, YUp, SuperSplat };

inline constexpr SnapshotCoordinates snapshot_default_coordinates() {
#ifdef SS_DEFAULT_SNAPSHOT_SUPERSPLAT
    return SnapshotCoordinates::SuperSplat;
#else
    return SnapshotCoordinates::YUp;
#endif
}

inline const char* snapshot_coordinates_name(SnapshotCoordinates coordinates) {
    switch (coordinates) {
        case SnapshotCoordinates::YUp: return "y-up";
        case SnapshotCoordinates::SuperSplat: return "supersplat";
        default: return "z-up";
    }
}

struct SnapshotExport {
    spirula::SceneTransform transform;
    SnapshotCoordinates coordinates = snapshot_default_coordinates();
};

}  // namespace gui
