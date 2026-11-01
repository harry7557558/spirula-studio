#pragma once
// The four checkpoints: torchvision's own AlexNet and VGG16 releases, and the
// LPIPS repository's v0.1 linear layers -- the files torchmetrics runs. The
// backbones' fallbacks are ModelScope repositories holding the same bytes; the
// linear layers fall back to the project's mirror (core/ModelMirror.h).

#include "lpips/Lpips.h"
#include "nn/io/Fetch.h"

#include <string>
#include <vector>

namespace lpips {

const nn::FetchFile& backbone_file(Net net);
const nn::FetchFile& linear_file(Net net);

// The files `nets` need that the model cache does not hold yet, judged by name
// and size; nn::ensure_file still verifies the hash before anything loads.
std::vector<nn::FetchFile> missing_files(const std::vector<Net>& nets);

// `dir` empty: the model cache, downloading what is missing. Otherwise the
// files in `dir`, under the cache name or the upstream one (torch's hub cache
// keeps the linear layers as alex.pth / vgg.pth), read as they are.
Checkpoints resolve(Net net, const std::string& dir);

}  // namespace lpips
