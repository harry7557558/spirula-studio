// DeDoDe's forward pass: a VGG encoder, a ConvRefiner cascade summing a dense
// description grid at the input resolution, and one bilinear grid sample at
// the keypoints.
//
// Read against LoMa's dedode.py. Two things it does NOT do, both
// deliberate and both different from the detector: the image is not
// mean/std-normalized (only 1/255), and the head is upsampled bilinearly.

#include "loma/model/Model.h"

#include "loma/Common.h"
#include "loma/model/Dump.h"
#include "loma/model/Fetch.h"
#include "nn/Ops.h"
#include "nn/vk/Memory.h"
#include "nn/vk/Stream.h"

#include <algorithm>
#include <vector>

namespace loma {

using nn::DType;
using nn::Tensor;

void Descriptor::load(const std::string& path, vk::Arena* arena) {
    pyr_.arena = arena;
    pyr_.prefix = "desc";
    const std::string file = resolve_model(path, "descriptor");
    pyr_.w.load(file, "desc");
    NN_CHECK(pyr_.hp().input_size > 0,
             "'%s' left its input size dynamic; a DeDoDe descriptor is exported at a "
             "fixed square and there is nothing sensible to guess",
             file.c_str());
}

int64_t Descriptor::planBytes() const {
    const PyramidHparams& p = pyr_.hp();
    const int64_t S = p.input_size;
    const std::vector<Level> levels = Pyramid::planLevels(p, S, S);
    int64_t live = S * S * (3 + p.head);   // the image and the description grid
    int64_t transient = 0;
    if (p.dino_width) {
        live += levels[0].h * levels[0].w * levels[0].c;
        transient = std::max(transient, Dino::planBytes(p, S) / 4);
    }
    Pyramid::planPyramid(p, levels, live, transient);
    return (live + transient) * 4 + (96ll << 20);
}

void Descriptor::run(const Tensor& img, const float* xy, int n, float* out) {
    vk::Arena& arena = *pyr_.arena;
    const int64_t S = pyr_.hp().input_size;
    const int64_t D = pyr_.hp().head;

    dump_tensor("desc_input", img, {S, S, 3});
    std::vector<Level> taps;
    pyr_.encode(taps, img, S, S);

    // Coarse first, and for DeDoDe-G the DINOv2 map is coarser than every VGG
    // tap: at patch 14 on a 784 square it is 56x56, between the 98 and 49 the
    // VGG would have produced, which is why it leads rather than replaces.
    std::vector<Level> levels;
    if (pyr_.hp().dino_width) {
        const int64_t g = S / pyr_.hp().dino_patch;
        Level lv;
        lv.h = lv.w = g;
        lv.c = pyr_.hp().dino_width;
        lv.map = nn::arena_tensor(arena, DType::F32, g, g, lv.c);
        dino_.run(pyr_.w, arena, img, lv.map, g, g);
        levels.push_back(lv);
    }
    levels.insert(levels.end(), taps.rbegin(), taps.rend());

    Tensor grid = nn::arena_tensor(arena, DType::F32, S, S, D);
    pyr_.cascade(grid, levels, /*bicubic_head=*/false);
    if (dump_enabled()) dump_tensor("desc_grid_row0", grid.view(1, S * D), {S, D});
    if (n == 0) return;

    vk::ArenaScope scope(arena);
    Tensor pos = nn::arena_tensor(arena, DType::F32, n, 2);
    vk::Stream::get().upload(pos.ptr, xy, (uint64_t)n * 8);
    Tensor desc = nn::arena_tensor(arena, DType::F32, n, D);
    nn::grid_sample_points(desc, grid, pos, /*align_corners=*/false);
    vk::Stream::get().download(out, desc.ptr, (uint64_t)n * D * 4);
}

}  // namespace loma
