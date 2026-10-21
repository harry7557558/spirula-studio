#include "app/SkyBake.h"
#include "app/SkyModel.h"
#include "app/SkySampling.h"
#include "core/ImageFile.h"
#include "core/ImageOrient.h"
#include "data/DatasetParser.h"
#include "external/stb_image.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace spirula::sky {
namespace {

struct Image {
    const uint8_t* pixels = nullptr;
    int width = 0, height = 0;
};

class ResizeMap {
public:
    void apply(Image source, int width, int height, std::vector<uint8_t>& destination) {
        const std::array<int, 4> shape{source.width, source.height, width, height};
        if (shape != shape_) {
            build_axis(source.width, width, columns_);
            build_axis(source.height, height, rows_);
            shape_ = shape;
        }
        destination.resize((size_t)width * height * 3);
        auto* output = destination.data();
        for (const auto y : rows_) {
            for (const auto x : columns_) {
                const auto value = sampling::rgb(source.pixels, source.width, x, y);
                for (float channel : value) *output++ = (uint8_t)std::lround(channel);
            }
        }
    }

private:
    static void build_axis(int source, int destination, std::vector<sampling::Axis>& axis) {
        axis.resize(destination);
        for (int pixel = 0; pixel < destination; ++pixel) {
            const float position = ((float)pixel + 0.5f) * (float)source /
                                     (float)destination - 0.5f;
            axis[pixel] = sampling::bracket(position, source, sampling::Border::LastPixel);
        }
    }

    std::array<int, 4> shape_{};
    std::vector<sampling::Axis> columns_, rows_;
};

class ImageReader {
public:
    bool prepare(const std::string& path, int turns, int width, int height, Image& result) {
        Image source = decode(path);
        if (!source.pixels || source.width <= 0 || source.height <= 0) return false;
        if ((turns & 3) != 0) {
            secondary_.resize((size_t)source.width * source.height * 3);
            orient_pixels(source.pixels, source.width, source.height, 3, turns, false,
                           secondary_.data());
            oriented_size(turns, source.width, source.height);
            source.pixels = secondary_.data();
        }
        if (source.width != width || source.height != height) {
            auto& output = source.pixels == primary_.data() ? secondary_ : primary_;
            resize_.apply(source, width, height, output);
            source = {output.data(), width, height};
        }
        if (source.pixels != raw_.get()) raw_.reset();
        result = source;
        return true;
    }

private:
    Image decode(const std::string& path) {
        raw_.reset();
        if (imagefile::handles(path)) {
            imagefile::Info info;
            imagefile::Options options;
            options.channels = 3;
            options.threads = 1;
            if (!imagefile::decode_srgb8(path, options, info, primary_).empty() ||
                primary_.size() < (size_t)info.width * info.height * 3)
                return {};
            return {primary_.data(), info.width, info.height};
        }
        int width = 0, height = 0, channels = 0;
        raw_.reset(stbi_load(path.c_str(), &width, &height, &channels, 3));
        return {raw_.get(), width, height};
    }

    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> raw_{nullptr, stbi_image_free};
    std::vector<uint8_t> primary_, secondary_;
    ResizeMap resize_;
};

View registered_view(const ParsedDataset& dataset, size_t index, Image image) {
    const auto* intrinsic = dataset.intrins.data() + index * 4;
    View result{image.pixels, dataset.widths[index], dataset.heights[index],
                dataset.camera_models.empty() ? 0 : dataset.camera_models[index],
                dataset.camera_distortions.empty() ? 0 : dataset.camera_distortions[index],
                intrinsic[0], intrinsic[1], intrinsic[2], intrinsic[3]};
    if (!dataset.dist_coeffs.empty())
        std::copy_n(dataset.dist_coeffs.data() + index * 8, 8, result.dist);
    std::copy_n(dataset.c2w.data() + index * 12, 12, result.c2w);
    return result;
}

}  // namespace

int64_t bake_dataset(Rows& rows, const ParsedDataset& dataset,
                     const std::atomic<bool>& stop_requested) {
    ImageReader reader;
    Bake colors;
    int64_t images_used = 0;
    for (int32_t frame : dataset.train_indices) {
        if (stop_requested.load(std::memory_order_relaxed)) break;
        if (frame < 0 || frame >= dataset.num_cameras) continue;
        const auto index = (size_t)frame;
        const int turns = index < dataset.exif_quarter_turns.size()
            ? (int)dataset.exif_quarter_turns[index] : 0;
        Image image;
        if (!reader.prepare(dataset.image_filenames[index], turns,
                              dataset.widths[index], dataset.heights[index], image))
            throw std::runtime_error("sky bake: cannot load " + dataset.image_filenames[index]);
        const auto view = registered_view(dataset, index, image);
        bake_accumulate(rows, view, colors);
        ++images_used;
    }
    bake_finalize(rows, colors);
    return images_used;
}

}  // namespace spirula::sky
