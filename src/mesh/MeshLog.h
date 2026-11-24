#pragma once

// What the meshing pipeline says while it runs, in the language the user
// picked.
//
// `spirula mesh` is not a third-party tool whose output we pass through: it is
// this program, and when the GUI runs it as a child process (with --lang) the
// lines land in the GUI's terminal panel, read by the person who chose the
// language. So every line goes through here and comes out as
//
//     [meshing] <stage>: <message>
//
// with all three parts translated -- see i18n/catalog/Mesh.h.
//
// The GUI's progress bar reads that shape back (src/app/gui/MeshRunner.cpp).
// It does NOT match English: it compares the stage against the very
// `msg::mesh::stage_*` entries printed here, and pulls counts out of the
// bodies with i18n::scan(), which matches a line against the message it came
// from. That is why this file exposes stage_label() and tag_prefix() as inline
// helpers -- the runner builds exactly the same strings without linking the
// meshing module.
//
// What stays as it is: paths, file names, numbers, and the moment dumps behind
// the debug hooks. out_raw() is for those, named so that using it is a visible
// decision, as ui::*Raw is in the GUI.

#include "i18n/Message.h"
#include "i18n/catalog/Mesh.h"

#include <initializer_list>
#include <string>

namespace meshing {
namespace mlog {

// One per line of the pipeline a user can name. The order is the order the
// pipeline reaches them, which is also the order MeshRunner's progress table
// is written in.
enum class Stage {
    Loading,
    Tsdf,
    PointCloud,
    Delaunay,
    Occupancy,
    CutEdges,
    Bisection,
    Marching,
    Merge,
    Cleanup,
    Cull,
    Quality,
    Orient,
    Color,
    Ground,
    Uv,
    Bake,
    Texture,
    TexelDensity,
    Stats,
    Wrote,
    Done,
    Debug,
};

inline const spirula::i18n::Msg& stage_msg(Stage s) {
    namespace m = spirula::i18n::msg::mesh;
    switch (s) {
        case Stage::Loading:      return m::stage_loading;
        case Stage::Tsdf:         return m::stage_tsdf;
        case Stage::PointCloud:   return m::stage_point_cloud;
        case Stage::Delaunay:     return m::stage_delaunay;
        case Stage::Occupancy:    return m::stage_occupancy;
        case Stage::CutEdges:     return m::stage_cut_edges;
        case Stage::Bisection:    return m::stage_bisection;
        case Stage::Marching:     return m::stage_marching;
        case Stage::Merge:        return m::stage_merge;
        case Stage::Cleanup:      return m::stage_cleanup;
        case Stage::Cull:         return m::stage_cull;
        case Stage::Quality:      return m::stage_quality;
        case Stage::Orient:       return m::stage_orient;
        case Stage::Color:        return m::stage_color;
        case Stage::Ground:       return m::stage_ground;
        case Stage::Uv:           return m::stage_uv;
        case Stage::Bake:         return m::stage_bake;
        case Stage::Texture:      return m::stage_texture;
        case Stage::TexelDensity: return m::stage_texel_density;
        case Stage::Stats:        return m::stage_stats;
        case Stage::Wrote:        return m::stage_wrote;
        case Stage::Done:         return m::stage_done;
        case Stage::Debug:        return m::stage_debug;
    }
    return m::stage_loading;
}

// "[meshing] " -- the tag every line opens with, localized.
inline std::string tag_prefix() {
    return "[" + std::string(spirula::i18n::msg::mesh::tag_meshing.get()) + "] ";
}

// "<stage>: " -- what follows the tag. Inline so the GUI's runner can build
// the same string and match on it. The separator is an ASCII colon-space in
// every language, including the ones whose own punctuation is fullwidth: it is
// where MeshRunner cuts the stage column off the body, and a body that used a
// fullwidth colon of its own must not be mistaken for it.
inline std::string stage_label(Stage s) {
    return std::string(stage_msg(s).get()) + ": ";
}

// One line to stdout / stderr: the tag, the stage, and the message with its
// {0}, {1}, ... filled in.
void out(Stage s, const spirula::i18n::Msg& m,
         std::initializer_list<spirula::i18n::Arg> args = {});
void err(Stage s, const spirula::i18n::Msg& m,
         std::initializer_list<spirula::i18n::Arg> args = {});
// As err(), with the localized word for WARNING / error in front. Both go to
// stderr: a warning is not a result.
void warn(Stage s, const spirula::i18n::Msg& m,
          std::initializer_list<spirula::i18n::Arg> args = {});
void fail(Stage s, const spirula::i18n::Msg& m,
          std::initializer_list<spirula::i18n::Arg> args = {});

// Text that is already what it is: a path, a dump of numbers, a line the
// caller built. Same tag and stage column, no translation.
void out_raw(Stage s, const std::string& text);
void err_raw(Stage s, const std::string& text);

// An error that belongs to no stage, because the run never reached one: a
// missing file, a refused output path. "[meshing] error: <text>".
void fail_raw(const std::string& text);

// A number rounded for display: num(1.14962, 2) -> "1.15". Needed wherever a
// printf said %.2f, because i18n::Arg's double conversion is %g and a timing
// of 1.14962 seconds is not what anyone wanted to read.
std::string num(double v, int decimals);

}  // namespace mlog
}  // namespace meshing
