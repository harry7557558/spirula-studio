#pragma once

// What to mask out of a capture, and how hard. Owned by the dataset screen so
// that what SegmentPanel tries on one frame is what the run writes over the
// whole capture, and saved into a dataset preset -- everything here except the
// clicks, which belong to the frames they were drawn on.

#include "app/gui/DatasetPrep.h"   // MaskClick

#include <string>
#include <vector>

namespace gui {

struct MaskSettings {
    std::string prompt;              // "people; cars"
    std::string negative_prompt;
    bool keep_subject = false;       // prompt names what to KEEP
    // How far the boundary moves from where the model drew it, as a share of
    // the object's own size. Two, because the polarities want opposite
    // directions and opposite defaults -- see boundary_ratio().
    float dilate_ratio = 0.05f;      // removing: outward, over the object's rim
    float shrink_ratio = 0.0f;       // keeping: inward, off unless asked for
    int  max_image_size = 1600;
    float threshold = 0.5f;
    float nms = 0.1f;
    float box_threshold = 0.3f;      // Grounding DINO's, for a Grounded model
    // Clicked objects, across every frame and every input the user visited;
    // each carries the input it was drawn on (MaskClick::source).
    std::vector<MaskClick> clicks;
    int object_count = 1;            // how many the user has opened
    int current_object = 0;          // which one a new click joins

    // What the masker takes (sam::MaskOptions::dilate_ratio, signed). The
    // margin always grows what is THROWN AWAY: the named object when it is
    // being removed, everything else when it is the one being kept.
    float boundary_ratio() const {
        return keep_subject ? -shrink_ratio : dilate_ratio;
    }
};

}  // namespace gui
