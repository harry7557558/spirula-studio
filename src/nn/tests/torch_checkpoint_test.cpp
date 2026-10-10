#include "nn/io/TorchCheckpoint.h"

#include "external/miniz.h"
#include "nn/core/Error.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<uint8_t>;
int failures = 0;

void check(bool ok, const char* name) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    failures += !ok;
}

struct Fixture {
    Bytes pickle{0x80, 2, '}', '('};
    std::vector<std::pair<std::string, Bytes>> storages;

    void integer(int64_t value) {
        if (value >= 0 && value <= 255) { pickle.push_back('K'); pickle.push_back((uint8_t)value); }
        else { pickle.push_back('J'); for (int i = 0; i < 4; ++i) pickle.push_back((uint8_t)((uint64_t)value >> (8 * i))); }
    }
    void text(const std::string& s) {
        pickle.push_back('X');
        for (int i = 0; i < 4; ++i) pickle.push_back((uint8_t)(s.size() >> (8 * i)));
        pickle.insert(pickle.end(), s.begin(), s.end());
    }
    void global(const std::string& module, const std::string& name) {
        pickle.push_back('c');
        for (const std::string& s : {module, name}) {
            pickle.insert(pickle.end(), s.begin(), s.end()); pickle.push_back('\n');
        }
    }
    void tuple(const std::vector<int64_t>& dims) {
        pickle.push_back('('); for (int64_t d : dims) integer(d); pickle.push_back('t');
    }
    void tensor(const std::string& name, const std::string& dtype, const std::string& key,
                int64_t storage_elements, int64_t offset, const std::vector<int64_t>& shape,
                const std::vector<int64_t>& stride) {
        text(name); global("torch._utils", "_rebuild_tensor_v2"); pickle.push_back('(');
        pickle.push_back('('); text("storage"); global("torch", dtype); text(key); text("cuda:0");
        integer(storage_elements); pickle.push_back('t'); pickle.push_back('Q'); integer(offset);
        tuple(shape); tuple(stride); pickle.push_back(0x89);
        global("collections", "OrderedDict"); pickle.push_back(')'); pickle.push_back('R');
        pickle.push_back('t'); pickle.push_back('R');
    }
    template<class T> void storage(const std::string& key, const std::vector<T>& values) {
        Bytes raw(values.size() * sizeof(T));
        if (!raw.empty()) std::memcpy(raw.data(), values.data(), raw.size());
        storages.emplace_back(key, std::move(raw));
    }
    void write(const std::filesystem::path& path, int compression = 0,
               const std::string& order = "little", bool stop = true) const {
        mz_zip_archive zip{};
        NN_CHECK(mz_zip_writer_init_file(&zip, path.string().c_str(), 0), "cannot create fixture");
        Bytes data = pickle;
        data.push_back('u'); if (stop) data.push_back('.');
        bool ok = mz_zip_writer_add_mem(&zip, "weights/data.pkl", data.data(), data.size(), compression) &&
                  mz_zip_writer_add_mem(&zip, "weights/byteorder", order.data(), order.size(), compression);
        for (const auto& s : storages)
            ok = ok && mz_zip_writer_add_mem(&zip, ("weights/data/" + s.first).c_str(),
                                             s.second.data(), s.second.size(), compression);
        ok = ok && mz_zip_writer_finalize_archive(&zip);
        mz_zip_writer_end(&zip);
        NN_CHECK(ok, "cannot finish fixture");
    }
};

bool equal(const nn::OnnxTensor& tensor, const std::vector<float>& expected) {
    if (tensor.data.size() != expected.size()) return false;
    for (size_t i = 0; i < expected.size(); ++i)
        if (tensor.data[i] != expected[i]) return false;
    return true;
}

void rejects(const std::function<void()>& f, const char* name) {
    bool rejected = false;
    try { f(); } catch (const nn::Error&) { rejected = true; }
    check(rejected, name);
}

void fixtures(const std::filesystem::path& path) {
    Fixture f;
    f.storage<float>("0", {0, 1, 2, 3, 4, 5, 6, 7});
    f.tensor("transpose", "FloatStorage", "0", 8, 1, {2, 3}, {1, 2});
    f.tensor("shared", "FloatStorage", "0", 8, 4, {3}, {1});
    f.tensor("broadcast", "FloatStorage", "0", 8, 2, {3}, {0});
    f.tensor("scalar", "FloatStorage", "0", 8, 7, {}, {});
    f.tensor("empty", "FloatStorage", "0", 8, 8, {0}, {1});
    f.storage<uint16_t>("1", {0x3c00, 0xc000, 0x3800});
    f.tensor("half", "HalfStorage", "1", 3, 0, {3}, {1});
    f.storage<uint16_t>("2", {0x3f80, 0xc000, 0x3f00});
    f.tensor("bfloat", "BFloat16Storage", "2", 3, 0, {3}, {1});
    f.storage<int64_t>("3", {123});
    f.tensor("counter", "LongStorage", "3", 1, 0, {}, {});
    f.storage<double>("4", {1.25, -7});
    f.tensor("double", "DoubleStorage", "4", 2, 0, {2}, {1});
    f.storage<int32_t>("5", {-3, 17});
    f.tensor("int", "IntStorage", "5", 2, 0, {2}, {1});
    f.storage<int16_t>("6", {-123, 17});
    f.tensor("short", "ShortStorage", "6", 2, 0, {2}, {1});
    f.storage<int8_t>("7", {-3, 17});
    f.tensor("char", "CharStorage", "7", 2, 0, {2}, {1});
    f.storage<uint8_t>("8", {0, 255});
    f.tensor("byte", "ByteStorage", "8", 2, 0, {2}, {1});
    f.storage<uint8_t>("9", {0, 1});
    f.tensor("bool", "BoolStorage", "9", 2, 0, {2}, {1});
    for (int compression : {0, 6}) {
        f.write(path, compression);
        nn::TorchCheckpoint reader(path.string());
        check(reader.names().size() == 14, compression ? "compressed ZIP state dictionary" : "stored ZIP state dictionary");
        check(equal(reader.read("transpose"), {1, 3, 5, 2, 4, 6}), "offset and transposed strides");
        check(equal(reader.read("shared"), {4, 5, 6}), "shared storage view");
        check(equal(reader.read("broadcast"), {2, 2, 2}), "zero-stride view");
        check(equal(reader.read("scalar"), {7}) && reader.read("empty").data.empty(), "scalar and empty tensors");
        check(equal(reader.read("half"), {1, -2, 0.5f}) && reader.read("half").was_f16, "float16 conversion");
        check(equal(reader.read("bfloat"), {1, -2, 0.5f}), "bfloat16 conversion");
        check(equal(reader.read("counter"), {123}), "integer batch counter");
        check(equal(reader.read("double"), {1.25f, -7}), "float64 conversion");
        check(equal(reader.read("int"), {-3, 17}) && equal(reader.read("short"), {-123, 17}) &&
              equal(reader.read("char"), {-3, 17}) && equal(reader.read("byte"), {0, 255}) &&
              equal(reader.read("bool"), {0, 1}), "integer storage types");
        rejects([&] { reader.read("missing"); }, "missing tensor rejected");
    }
    {
        // MoGe's layout: {"model_config": {...}, "model": {state dict}}.
        Fixture w;
        auto ops = [&](std::initializer_list<uint8_t> b) { w.pickle.insert(w.pickle.end(), b); };
        w.storage<float>("0", {1, 2});
        w.text("model_config"); ops({'}', '('});
        w.text("refiner"); ops({'}', '('});
        w.text("factors"); ops({']', '('}); w.integer(2); w.integer(4); ops({'e'});
        w.text("dims"); ops({']', '('}); w.integer(3); ops({'N', 'e'});
        w.text("bins"); w.integer(256);
        ops({'u'});
        w.text("scale"); ops({'G', 0x3f, 0xe0, 0, 0, 0, 0, 0, 0});
        ops({'u'});
        w.text("model"); ops({'}', '('});
        w.tensor("w", "FloatStorage", "0", 2, 0, {2}, {1});
        ops({'u'});
        w.write(path);
        nn::TorchCheckpoint reader(path.string());
        check(reader.names() == std::vector<std::string>{"w"} && equal(reader.read("w"), {1, 2}),
              "state dictionary under \"model\"");
        const auto dims = reader.config("model_config.refiner.dims");
        check(reader.config("model_config.refiner.factors") == std::vector<double>({2, 4}) &&
                  reader.config("model_config.refiner.bins") == std::vector<double>({256}) &&
                  reader.config("model_config.scale") == std::vector<double>({0.5}) &&
                  dims.size() == 2 && dims[0] == 3 && std::isnan(dims[1]) &&
                  reader.config("model_config.missing").empty(),
              "config beside the weights, by dotted path");
    }
    auto bad = [&](const Fixture& input, const char* name) {
        input.write(path); rejects([&] { nn::TorchCheckpoint r(path.string()); }, name);
    };
    Fixture past;
    past.storage<float>("0", {1, 2});
    past.tensor("bad", "FloatStorage", "0", 2, 1, {2}, {1});
    bad(past, "view beyond storage rejected");
    Fixture negative;
    negative.storage<float>("0", {1, 2});
    negative.tensor("bad", "FloatStorage", "0", 2, 0, {-1}, {1});
    bad(negative, "negative dimension rejected");
    Fixture rank;
    rank.storage<float>("0", {1, 2});
    rank.tensor("bad", "FloatStorage", "0", 2, 0, {2}, {});
    bad(rank, "shape/stride mismatch rejected");
    Fixture absent;
    absent.tensor("bad", "FloatStorage", "0", 2, 0, {2}, {1});
    bad(absent, "missing storage rejected");
    Fixture short_storage;
    short_storage.storage<float>("0", {1});
    short_storage.tensor("bad", "FloatStorage", "0", 2, 0, {2}, {1});
    bad(short_storage, "storage length mismatch rejected");
    Fixture executable;
    executable.text("bad"); executable.global("os", "system");
    bad(executable, "executable pickle global rejected");
    Fixture duplicate;
    duplicate.storage<float>("0", {1});
    duplicate.tensor("same", "FloatStorage", "0", 1, 0, {}, {});
    duplicate.tensor("same", "FloatStorage", "0", 1, 0, {}, {});
    bad(duplicate, "duplicate tensor name rejected");
    Fixture traversal;
    traversal.tensor("bad", "FloatStorage", "../0", 1, 0, {}, {});
    bad(traversal, "non-numeric storage key rejected");
    f.write(path, 0, "big");
    rejects([&] { nn::TorchCheckpoint r(path.string()); }, "big-endian checkpoint rejected");
    f.write(path, 0, "little", false);
    rejects([&] { nn::TorchCheckpoint r(path.string()); }, "truncated pickle rejected");
    f.write(path);
    std::filesystem::resize_file(path, 40);
    rejects([&] { nn::TorchCheckpoint r(path.string()); }, "truncated ZIP rejected");
}

void official(const char* path) {
    nn::TorchCheckpoint reader(path);
    check(reader.names().size() == 907, "official RoMa v2.0.1 tensor count");
    check(reader.entry("f.patch_embed.proj.weight").shape == std::vector<int64_t>({1024, 3, 16, 16}),
          "official DINOv3 patch embedding");
    check(reader.entry("f.blocks.0.attn.qkv.weight").dtype == "BFloat16Storage", "official bfloat16 descriptor");
    uint64_t values = 0;
    bool finite = true;
    for (const auto& name : reader.names()) {
        const nn::OnnxTensor t = reader.read(name);
        values += t.data.size();
        for (float v : t.data) finite = finite && std::isfinite(v);
    }
    std::printf("official checkpoint: %llu values decoded\n", (unsigned long long)values);
    check(finite, "every official tensor decoded and finite");
}

}  // namespace

int main(int argc, char** argv) {
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::temp_directory_path() / ("spirula-torch-" + std::to_string(nonce) + ".pt");
    try { fixtures(path); if (argc > 1) official(argv[1]); }
    catch (const std::exception& e) { std::printf("FAIL %s\n", e.what()); ++failures; }
    std::error_code ec;
    std::filesystem::remove(path, ec);
    return failures ? 1 : 0;
}
