// Pairs a rig implies. When a/i and b/j verified, frames i and j face the same
// way through those two lenses, and the members' known rotations then say
// which other lens of i faces which lens of j: those pairs are matched too, so
// a frame link never rests on whichever lens the pair source happened to rank
// (docs/notes/sfm-rig-constraints.md, "Known lens geometry").
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

#include "sfm/core/Rig.h"

namespace sfm {

// (a'/i, b'/j), a' facing away from a, for each seed (a/i, b/j) whose frame
// pair has at most a quarter of the members in seeds (the weak links; all of
// them unless `weak_only`), where a'/i and b'/j face within `max_angle_deg`.
inline std::vector<std::pair<uint32_t, uint32_t>> rigMatePairs(
    const RigTable& rigs, const std::vector<std::pair<uint32_t, uint32_t>>& seeds,
    double max_angle_deg, bool weak_only = true) {
    const double cos_max = std::cos(max_angle_deg * M_PI / 180.0);
    // implied[r][a * n + b]: the member pairs a chosen (a, b) brings.
    std::vector<std::vector<std::vector<std::pair<uint32_t, uint32_t>>>> implied(rigs.rigs.size());
    for (size_t r = 0; r < rigs.rigs.size(); r++) {
        const std::vector<RigMemberDef>& mem = rigs.rigs[r].members;
        const size_t n = mem.size();
        implied[r].resize(n * n);
        // "Faced alike" holds for a seed of two ~190-degree fisheyes; two 90-degree
        // views overlap at 60 degrees apart, and on a .360 6% of mates verified.
        if (rigs.rigs[r].kind != "dual-fisheye") continue;
        for (size_t a = 0; a < n; a++)
            for (size_t b = 0; b < n; b++) {
                if (!mem[a].has_ext || !mem[b].has_ext) continue;
                // Frame j's rig frame, in frame i's, when a/i and b/j agree.
                const Mat3 H = mul(transpose(mem[a].ext.R), mem[b].ext.R);
                for (size_t a2 = 0; a2 < n; a2++)
                    for (size_t b2 = 0; b2 < n; b2++) {
                        if ((a2 == a && b2 == b) || !mem[a2].has_ext || !mem[b2].has_ext) continue;
                        const Mat3& Ra = mem[a2].ext.R;
                        // Only the lenses facing away from the seed's: those
                        // beside it share its content, which found them already.
                        const Mat3& Rs = mem[a].ext.R;
                        if (Ra[6] * Rs[6] + Ra[7] * Rs[7] + Ra[8] * Rs[8] > 0) continue;
                        const Mat3 Rb = mul(H, transpose(mem[b2].ext.R));
                        // Optical axes: a row of cam_from_rig, a column of rig_from_cam.
                        const double c = Ra[6] * Rb[2] + Ra[7] * Rb[5] + Ra[8] * Rb[8];
                        if (c >= cos_max) implied[r][a * n + b].push_back({(uint32_t)a2, (uint32_t)b2});
                    }
            }
    }
    auto frameKey = [](const RigSlot& a, const RigSlot& b) {
        const uint64_t lo = std::min(a.frame, b.frame), hi = std::max(a.frame, b.frame);
        return std::make_pair(a.rig, (lo << 32) | hi);
    };
    std::map<std::pair<uint32_t, uint64_t>, size_t> links;
    for (const auto& s : seeds) {
        const RigSlot si = rigs.slot(s.first), sj = rigs.slot(s.second);
        if (si.valid() && sj.valid() && si.rig == sj.rig && si.frame != sj.frame)
            links[frameKey(si, sj)]++;
    }
    std::vector<std::pair<uint32_t, uint32_t>> out;
    for (const auto& s : seeds) {
        const RigSlot si = rigs.slot(s.first), sj = rigs.slot(s.second);
        if (!si.valid() || !sj.valid() || si.rig != sj.rig || si.frame == sj.frame) continue;
        const RigSpec& spec = rigs.rigs[si.rig];
        const size_t n = spec.members.size();
        if (weak_only && links[frameKey(si, sj)] > std::max<size_t>(1, n / 4)) continue;
        for (const auto& mm : implied[si.rig][si.member * n + sj.member]) {
            const uint32_t p = spec.frames[si.frame][mm.first];
            const uint32_t q = spec.frames[sj.frame][mm.second];
            if (p != kNoImage && q != kNoImage) out.emplace_back(std::min(p, q), std::max(p, q));
        }
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    std::vector<std::pair<uint32_t, uint32_t>> sorted_seeds = seeds;
    std::sort(sorted_seeds.begin(), sorted_seeds.end());
    out.erase(std::remove_if(out.begin(), out.end(),
                             [&](const std::pair<uint32_t, uint32_t>& q) {
                                 return std::binary_search(sorted_seeds.begin(),
                                                           sorted_seeds.end(), q);
                             }),
              out.end());
    return out;
}

}  // namespace sfm
