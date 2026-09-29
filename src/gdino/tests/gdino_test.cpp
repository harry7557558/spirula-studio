// Grounding DINO gate: a checkpoint loads, the tokenizer agrees with BERT's on
// a fixed sentence, and a frame produces boxes. The network's numbers are
// tools/gdino/compare_ort.py's job.
//
//   ./build_vulkan/gdino_test                                # cached checkpoints, or SKIP
//   ./build_vulkan/gdino_test --model ID|PATH --image IMG --text "cat; dog" [--repeat N]
//   ./build_vulkan/gdino_test --model ID|PATH --npy in.npy --ids 101,1037,102 --out DIR

#include "gdino/GroundingDino.h"

#include "nn/core/Error.h"
#include "nn/core/Log.h"
#include "nn/io/Image.h"
#include "nn/io/StageDump.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    std::string model, image, text = "person; car", npy_in, out, ids_arg;
    int repeat = 1;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&] { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        if (a == "--model") model = next();
        else if (a == "--image") image = next();
        else if (a == "--text") text = next();
        else if (a == "--npy") npy_in = next();
        else if (a == "--ids") ids_arg = next();
        else if (a == "--out") out = next();
        else if (a == "--repeat") repeat = std::atoi(next().c_str());
    }
    nn::set_log_level(2);
    int failures = 0;
    try {
        if (model.empty()) {
            for (const char* id : {"gdino-tiny", "gdino-base"}) {
                const gdino::ModelSource* src = gdino::find_model_source(id);
                if (fs::exists(nn::cached_path(src->weights)) &&
                    fs::exists(nn::cached_path(src->vocab))) {
                    model = id;
                    break;
                }
            }
            if (model.empty()) {
                std::printf("SKIP: no Grounding DINO checkpoint cached\n");
                return 0;
            }
        }
        gdino::Detector det;
        det.load(model);

        // bert-base-uncased on this sentence, from Hugging Face's tokenizer.
        const std::vector<int32_t> want = {101,   1037,  3756, 2082, 3902,  1012, 2019, 14925,
                                           19771, 2099,  1011, 16985, 9286, 2102, 1029, 102};
        const std::vector<int32_t> got = det.tokenize("A yellow SCHOOL bus. An Éclair-tartlet?");
        const bool tok_ok = got == want;
        std::printf("  %s tokenizer\n", tok_ok ? "ok  " : "FAIL");
        if (!tok_ok) {
            ++failures;
            for (int32_t v : got) std::printf(" %d", v);
            std::printf("\n");
        }

        if (!npy_in.empty()) {
            std::vector<float> chw;
            std::vector<int64_t> shape;
            NN_CHECK(nn::read_npy_f32(npy_in, chw, shape) && shape.size() == 4 && shape[1] == 3,
                     "%s is not a [1, 3, H, W] float32 array", npy_in.c_str());
            const int H = (int)shape[2], W = (int)shape[3];
            std::vector<float> hwc(chw.size());
            for (int c = 0; c < 3; ++c)
                for (int k = 0; k < H * W; ++k) hwc[(size_t)k * 3 + c] = chw[(size_t)c * H * W + k];
            std::vector<int32_t> ids;
            std::stringstream ss(ids_arg);
            for (std::string t; std::getline(ss, t, ',');) ids.push_back(std::atoi(t.c_str()));
            std::vector<float> logits, boxes;
            for (int r = 0; r < repeat; ++r) {
                const double t0 = nn::now_ms();
                det.forwardNormalized(hwc, H, W, ids, logits, boxes);
                std::printf("forward %d: %.1f ms\n", r, nn::now_ms() - t0);
            }
            if (!out.empty()) {
                fs::create_directories(out);
                const int64_t Q = (int64_t)boxes.size() / 4;
                nn::write_npy_f32(out + "/logits.npy", logits.data(), {1, Q, (int64_t)ids.size()});
                nn::write_npy_f32(out + "/boxes.npy", boxes.data(), {1, Q, 4});
            }
            return failures ? 1 : 0;
        }

        if (!image.empty()) {
            const nn::Image img = nn::load_image(image);
            NN_CHECK(!img.empty(), "cannot read %s", image.c_str());
            std::vector<std::string> phrases;
            std::stringstream ss(text);
            for (std::string p; std::getline(ss, p, ';');)
                if (p.find_first_not_of(' ') != std::string::npos) phrases.push_back(p);
            std::vector<gdino::Detection> dets;
            for (int r = 0; r < repeat; ++r) {
                const double t0 = nn::now_ms();
                dets = det.detect(img, phrases);
                std::printf("detect %d: %.1f ms\n", r, nn::now_ms() - t0);
            }
            for (const gdino::Detection& d : dets)
                std::printf("  %-12s %.3f  [%.0f %.0f %.0f %.0f]\n",
                            phrases[(size_t)d.phrase].c_str(), d.score, d.x0, d.y0, d.x1, d.y1);
        }
    } catch (const std::exception& e) {
        std::printf("FAIL: %s\n", e.what());
        return 1;
    }
    return failures ? 1 : 0;
}
