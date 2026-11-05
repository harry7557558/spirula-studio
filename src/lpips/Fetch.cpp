#include "lpips/Fetch.h"

#include "nn/core/Error.h"

#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

namespace lpips {
namespace {

// Each SHA-256 is the file's own; ModelScope stores LFS objects under it, so
// the mirror cannot serve anything else.
const nn::FetchFile kBackbones[] = {
    {"alexnet-owt-7be5be79.pth", "https://download.pytorch.org/models/alexnet-owt-7be5be79.pth",
     "7be5be791159472b1fbf3c69796f7cb30dca7ad8466c2df70058c37116cdee02", 244408911ull,
     "https://modelscope.cn/models/AI-ModelScope/alexnet/resolve/master/alexnet-owt-7be5be79.pth"},
    {"vgg16-397923af.pth", "https://download.pytorch.org/models/vgg16-397923af.pth",
     "397923af8e79cdbb6a7127f12361acd7a2f83e06b05044ddf496e83de57a5bf0", 553433881ull,
     "https://modelscope.cn/models/VoyagerX/vgg16-imagenet1k/resolve/master/vgg16-397923af.pth"},
};
const nn::FetchFile kLinear[] = {
    {"lpips-v0.1-alex.pth",
     "https://raw.githubusercontent.com/richzhang/PerceptualSimilarity/master/lpips/weights/v0.1/alex.pth",
     "df73285e35b22355a2df87cdb6b70b343713b667eddbda73e1977e0c860835c0", 6009ull},
    {"lpips-v0.1-vgg.pth",
     "https://raw.githubusercontent.com/richzhang/PerceptualSimilarity/master/lpips/weights/v0.1/vgg.pth",
     "a78928a0af1e5f0fcb1f3b9e8f8c3a2a5a3de244d830ad5c1feddc79b8432868", 7289ull},
};

std::string url_basename(const char* url) {
    const std::string u(url);
    return u.substr(u.rfind('/') + 1);
}

std::string find_in(const fs::path& dir, const nn::FetchFile& f) {
    std::error_code ec;
    for (const std::string& name : {std::string(f.file), url_basename(f.url)})
        if (fs::is_regular_file(dir / name, ec)) return (dir / name).string();
    nn::fail("%s holds neither %s nor %s (from %s)", dir.string().c_str(), f.file,
             url_basename(f.url).c_str(), f.url);
}

}  // namespace

const nn::FetchFile& backbone_file(Net net) { return kBackbones[net == Net::Vgg]; }
const nn::FetchFile& linear_file(Net net) { return kLinear[net == Net::Vgg]; }

std::vector<nn::FetchFile> missing_files(const std::vector<Net>& nets) {
    std::vector<nn::FetchFile> out;
    for (Net net : nets)
        for (const nn::FetchFile* f : {&backbone_file(net), &linear_file(net)}) {
            std::error_code ec;
            const fs::path p = nn::cached_path(*f);
            if (!fs::is_regular_file(p, ec) || fs::file_size(p, ec) != f->bytes)
                out.push_back(*f);
        }
    return out;
}

Checkpoints resolve(Net net, const std::string& dir) {
    if (dir.empty())
        return {nn::ensure_file(backbone_file(net), "lpips"),
                nn::ensure_file(linear_file(net), "lpips")};
    return {find_in(dir, backbone_file(net)), find_in(dir, linear_file(net))};
}

}  // namespace lpips
