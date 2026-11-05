#pragma once

// MatchMatrix -- which images matched which, as a picture.
//
// The one thing about matching that a progress bar cannot say. A capture shot
// as a walk gives a bright diagonal band; a walk that came back on itself and
// closed the loop gives corners as well, and a capture that did NOT close shows
// the band alone -- which is exactly the failure that turns into a
// reconstruction in two pieces, and the reason the sequential pairing has a
// loop-closure switch at all.
//
// Fed live from the run's progress directory while matching, and from its
// matches.bin afterwards (SfmProgress.h).

#include "app/gui/GlLoader.h"
#include "app/gui/SfmProgress.h"

namespace gui {

class MatchMatrix {
public:
    ~MatchMatrix();

    // Replaces what is shown. Cheap; the texture is rebuilt on the next draw.
    void set(const PairMatrix& m);
    bool empty() const { return _m.empty(); }
    void clear();

    // Draws it `size` pixels square, with a legend and a hover readout of the
    // image range under the cursor. True while a cell is hovered, with `block`
    // the images it stands for. GL context must be current.
    bool draw(float size, PairBlock& block);
    void destroy_gl();

    const PairMatrix& matrix() const { return _m; }
    // The matrix as a bins x bins RGB texture, built if stale. GL context must be current.
    GLuint texture() { ensure_texture(); return _tex; }

private:
    void ensure_texture();

    PairMatrix _m;
    GLuint _tex = 0;
    bool _dirty = false;
};

}  // namespace gui
