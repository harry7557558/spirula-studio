#pragma once
// The names loma/ borrows from the inference layer (src/nn/), exactly as
// aliked/Common.h does for src/aliked/. Include this at the top of a loma/
// translation unit instead of qualifying nn:: at every call site.
//
// Nothing model-specific belongs here -- that is what loma/Loma.h is for.

#include "nn/core/Error.h"
#include "nn/core/Half.h"
#include "nn/core/Log.h"
#include "nn/core/Parallel.h"
#include "nn/io/Onnx.h"

namespace nn { namespace vk {} }

namespace loma {

namespace vk = ::nn::vk;
using ::nn::Error;
using ::nn::fail;
using ::nn::log_level;
using ::nn::now_ms;
using ::nn::ScopedTimer;
using ::nn::parallel_for;
using ::nn::OnnxFile;
using ::nn::OnnxNode;
using ::nn::OnnxTensor;
using ::nn::read_onnx;

}  // namespace loma
