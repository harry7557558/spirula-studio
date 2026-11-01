#include "marigold/model/Files.h"

#include "marigold/Common.h"
#include "marigold/model/HostMath.h"

#include <cstring>
#include <fstream>

namespace marigold {

std::vector<uint8_t> read_raw(const nn::SafetensorsFile& f, const std::string& name) {
    const nn::SafetensorsFile::Entry& e = f.entry(name);
    std::vector<uint8_t> out((size_t)(e.end - e.begin));
    std::ifstream in(f.path(), std::ios::binary);
    in.seekg((std::streamoff)(f.dataOffset() + e.begin));
    in.read(reinterpret_cast<char*>(out.data()), (std::streamsize)out.size());
    NN_CHECK(in.good(), "%s: short read of '%s'", f.path().c_str(), name.c_str());
    return out;
}

std::vector<float> read_f32(const nn::SafetensorsFile& f, const std::string& name) {
    const std::vector<uint8_t> b = read_raw(f, name);
    std::vector<float> out(b.size() / 4);
    std::memcpy(out.data(), b.data(), out.size() * 4);
    return out;
}

std::vector<float> read_nf4(const nn::SafetensorsFile& f, const std::string& prefix,
                            bool gemv) {
    const std::vector<uint8_t> codes = read_raw(f, prefix + ".q4");
    const std::vector<uint8_t> am = read_raw(f, prefix + ".absmax");
    std::vector<float> out(codes.size() * 2);
    nf4_dequantize(codes.data(), reinterpret_cast<const uint16_t*>(am.data()),
                   (int64_t)out.size(), out.data(), gemv);
    return out;
}

}  // namespace marigold
