#include "app/EvalLpips.h"

#include "app/TrainerCore.h"
#include "backend/api/BackendRuntime.h"
#include "lpips/Fetch.h"
#include "lpips/Lpips.h"
#include "nn/Device.h"

#include <exception>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace spirula {
namespace {

// alex with normalize=True, vgg with normalize=False: what the benchmarks this
// is compared against were recorded with, so vgg reads [0, 1] as the top half
// of its [-1, 1] range. Changing it means rebaselining every one of them.
struct NetCall {
    lpips::Net net;
    bool normalize;
};
constexpr NetCall kNets[] = {{lpips::Net::Alex, true}, {lpips::Net::Vgg, false}};

// The GUI pins the inference device to the engine's; the CLI leaves it unset.
void match_engine_device() {
    if (!nn::configured_device_selector().empty()) return;
    const int cur = backend::device_current();
    if (cur < 0) return;
    const std::string uuid = backend::device_info(cur).uuid;
    if (uuid.empty()) return;
    try {
        nn::configure_device(uuid);
    } catch (const std::exception&) {
        // A CUDA device Vulkan does not list: the inference layer picks its own.
    }
}

class Scorer final : public LpipsScorer {
public:
    explicit Scorer(const TrainConfig& cfg) {
        match_engine_device();
        for (size_t i = 0; i < std::size(kNets); ++i)
            _nets[i].load(kNets[i].net, lpips::resolve(kNets[i].net, cfg.lpips_checkpoints));
    }

    std::vector<std::string> names() const override {
        std::vector<std::string> out;
        for (const NetCall& n : kNets) out.push_back(std::string("lpips_") + lpips::net_name(n.net));
        return out;
    }

    std::vector<std::vector<float>> score(const float* gt, const std::vector<const float*>& preds,
                                          int width, int height) override {
        std::vector<std::vector<float>> out;
        for (size_t i = 0; i < std::size(kNets); ++i) {
            std::vector<float>& row = out.emplace_back();
            for (double v : _nets[i].score(gt, preds, width, height, kNets[i].normalize))
                row.push_back((float)v);
        }
        return out;
    }

private:
    lpips::Scorer _nets[std::size(kNets)];
};

}  // namespace

void install_eval_lpips() {
    LpipsHooks hooks;
    hooks.prepare = [](const TrainConfig& cfg) {
        for (const NetCall& n : kNets) lpips::resolve(n.net, cfg.lpips_checkpoints);
    };
    hooks.open = [](const TrainConfig& cfg) -> std::unique_ptr<LpipsScorer> {
        return std::make_unique<Scorer>(cfg);
    };
    set_lpips_hooks(std::move(hooks));
}

}  // namespace spirula
