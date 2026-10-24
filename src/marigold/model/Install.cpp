#include "marigold/model/Install.h"

#include "core/AtomicFile.h"
#include "core/Sha256.h"
#include "marigold/Common.h"
#include "marigold/model/Files.h"
#include "marigold/model/HostMath.h"
#include "marigold/model/Layout.h"
#include "marigold/model/Remote.h"
#include "nn/io/Safetensors.h"
#include "nn/io/SafetensorsWriter.h"
#include "nn/io/TorchCheckpoint.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <unordered_map>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>   // NOMINMAX comes from cmake/SsOptions.cmake
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace marigold {
namespace {

using Entry = nn::SafetensorsFile::Entry;
using Bytes = std::vector<uint8_t>;

uint64_t numel(const std::vector<int64_t>& s) {
    uint64_t n = 1;
    for (int64_t d : s) n *= (uint64_t)d;
    return n;
}

std::vector<float> bf16_floats(const uint8_t* p, uint64_t n) {
    std::vector<float> out((size_t)n);
    for (uint64_t i = 0; i < n; ++i) {
        uint16_t h;
        std::memcpy(&h, p + 2 * i, 2);
        out[(size_t)i] = bf16_to_f32(h);
    }
    return out;
}

// The weight a bitsandbytes Linear4bit computes with, on one row (gemv) or many.
std::vector<float> nf4_roundtrip(const uint8_t* bf16, uint64_t n, bool gemv) {
    Bytes codes((size_t)n / 2);
    std::vector<uint16_t> absmax((size_t)n / 64);
    nf4_quantize(reinterpret_cast<const uint16_t*>(bf16), (int64_t)n, codes.data(), absmax.data());
    std::vector<float> out((size_t)n);
    nf4_dequantize(codes.data(), absmax.data(), (int64_t)n, out.data(), gemv);
    return out;
}

std::vector<float> silu_bf16(const std::vector<float>& x) {
    std::vector<float> y(x.size());
    for (size_t i = 0; i < x.size(); ++i) y[i] = bf16r(x[i] / (1.0f + std::exp(-x[i])));
    return y;
}

void write_floats(nn::SafetensorsWriter& w, const std::string& name, const std::vector<float>& v,
                  uint64_t at = 0) {
    w.write(name, v.data(), v.size() * 4, at);
}

void write_nf4(nn::SafetensorsWriter& w, const std::string& prefix, const uint8_t* bf16,
               uint64_t n, uint64_t first_elem) {
    Bytes codes((size_t)n / 2);
    std::vector<uint16_t> absmax((size_t)n / 64);
    nf4_quantize(reinterpret_cast<const uint16_t*>(bf16), (int64_t)n, codes.data(), absmax.data());
    w.write(prefix + ".q4", codes.data(), codes.size(), first_elem / 2);
    w.write(prefix + ".absmax", absmax.data(), absmax.size() * 2, first_elem / 64 * 2);
}

void write_f16(nn::SafetensorsWriter& w, const std::string& name, const std::vector<float>& v,
               uint64_t first_elem = 0) {
    std::vector<uint16_t> h(v.size());
    for (size_t i = 0; i < v.size(); ++i) {
        NN_CHECK(std::fabs(v[i]) < 65504.0f, "'%s' holds %g, past fp16's range", name.c_str(),
                 (double)v[i]);
        h[i] = nn::float_to_half(v[i]);
    }
    w.write(name, h.data(), h.size() * 2, first_elem * 2);
}

// ================
// The VAE: a video autoencoder run on one frame
// ================

// A causal 3D convolution sees one frame through its last temporal tap -- the
// others read the zero padding in front -- so it is stored as that 2D conv.
bool vae_kept(const std::string& name) { return name.find("time_conv") == std::string::npos; }

void vae_plan(nn::SafetensorsWriter& w, const std::string& out, const Entry& e) {
    const std::vector<int64_t>& s = e.shape;
    if (s.size() == 5) w.add(out, "F16", {s[0], s[1], s[3], s[4]});
    else if (s.size() == 4 && out.size() > 7 && out.compare(out.size() - 7, 7, ".weight") == 0)
        w.add(out, "F16", s);
    else w.add(out, "F32", {(int64_t)numel(s)});
}

void vae_write(nn::SafetensorsWriter& w, const std::string& out, const Entry& e,
               const Bytes& d) {
    const std::vector<int64_t>& s = e.shape;
    std::vector<float> v = bf16_floats(d.data(), numel(s));
    if (s.size() == 5) {
        const int64_t taps = s[2], plane = s[3] * s[4];
        std::vector<float> last((size_t)(s[0] * s[1] * plane));
        for (int64_t oc = 0; oc < s[0] * s[1]; ++oc)
            std::copy_n(&v[(size_t)((oc * taps + taps - 1) * plane)], plane,
                        &last[(size_t)(oc * plane)]);
        write_f16(w, out, last);
    } else if (w.bytes(out) == v.size() * 2) {
        write_f16(w, out, v);
    } else {
        write_floats(w, out, v);
    }
}

// ================
// Sources
// ================

struct Source {
    const RemoteFile* file = nullptr;
    std::string header;
    uint64_t data_offset = 0;
    std::vector<std::pair<std::string, Entry>> tensors;   // in file order
    std::unordered_map<std::string, Entry> by_name;

    void expect(const std::string& name, const std::vector<int64_t>& shape) const {
        auto it = by_name.find(name);
        NN_CHECK(it != by_name.end(), "%s has no tensor '%s'", file->name, name.c_str());
        NN_CHECK(it->second.dtype == "BF16" && it->second.shape == shape,
                 "%s: '%s' is not the bf16 tensor this build expects", file->name,
                 name.c_str());
    }
};

Source probe(const RemoteFile& f, const std::atomic<bool>* cancel) {
    Source s;
    s.file = &f;
    s.header = remote_safetensors_header(f, &s.data_offset, cancel);
    s.by_name = nn::parse_safetensors_header(s.header, f.name);
    for (const auto& kv : s.by_name) s.tensors.emplace_back(kv.first, kv.second);
    std::sort(s.tensors.begin(), s.tensors.end(),
              [](const auto& a, const auto& b) { return a.second.begin < b.second.begin; });
    NN_CHECK(!s.tensors.empty() && s.data_offset + s.tensors.back().second.end == f.bytes,
             "%s: its header does not describe a %llu-byte file", f.name,
             (unsigned long long)f.bytes);
    return s;
}

const Source& holding(const std::vector<Source>& srcs, const std::string& name) {
    for (const Source& s : srcs)
        if (s.by_name.count(name)) return s;
    fail("no published shard holds '%s'", name.c_str());
}

// ================
// Resuming
// ================

// Where a build stopped: which source, the byte in it, and the SHA-256 state
// there, so one hash spans the interruption.
struct Resume {
    size_t source = 0;
    uint64_t pos = 0;
    spirula::Sha256 sha;
};

std::string to_hex(const void* p, size_t n) {
    static const char* k = "0123456789abcdef";
    std::string s;
    for (size_t i = 0; i < n; ++i) {
        const uint8_t b = static_cast<const uint8_t*>(p)[i];
        s += k[b >> 4];
        s += k[b & 15];
    }
    return s;
}

bool from_hex(const std::string& s, void* p, size_t n) {
    if (s.size() != 2 * n) return false;
    for (size_t i = 0; i < n; ++i)
        static_cast<uint8_t*>(p)[i] = (uint8_t)std::stoi(s.substr(2 * i, 2), nullptr, 16);
    return true;
}

void save_resume(const std::string& path, const std::string& plan, const Resume& r) {
    const std::string tmp = path + ".tmp";
    {
        std::ofstream o(tmp, std::ios::trunc);
        o << plan << ' ' << r.source << ' ' << r.pos << ' ' << to_hex(r.sha.h, sizeof r.sha.h)
          << ' ' << to_hex(r.sha.buf, sizeof r.sha.buf) << ' ' << r.sha.buf_len << ' '
          << r.sha.total << '\n';
        NN_CHECK(o.good(), "cannot write '%s'", tmp.c_str());
    }
    spirula::replace_file(tmp, path);
}

// `plan` empty: any plan, for an estimate.
bool load_resume(const std::string& path, const std::string& plan, Resume& r) {
    std::ifstream in(path);
    std::string id, h, buf;
    if (!(in >> id >> r.source >> r.pos >> h >> buf >> r.sha.buf_len >> r.sha.total))
        return false;
    if (!plan.empty() && id != plan) return false;
    return from_hex(h, r.sha.h, sizeof r.sha.h) && from_hex(buf, r.sha.buf, sizeof r.sha.buf) &&
           r.sha.buf_len < 64;
}

std::string plan_id(const std::string& header) {
    spirula::Sha256 sha;
    sha.update(reinterpret_cast<const uint8_t*>(header.data()), header.size());
    return sha.hex().substr(0, 16);
}

// ================
// Streaming one source through a builder
// ================

struct Sink {
    virtual ~Sink() = default;
    virtual void tensor(const std::string& name, const Entry& e, const Bytes& data) = 0;
    // No half-finished pair of tensors: a place a resume may start from.
    virtual bool settled() const = 0;
};

void stream(const Source& s, Resume& r, Sink& sink,
            const std::function<void(const Resume&)>& checkpoint,
            const std::function<void(uint64_t)>& moved, const std::atomic<bool>* cancel) {
    RemoteRange rr(*s.file, r.pos, s.file->bytes, cancel);
    Bytes buf;
    auto take = [&](uint64_t n) {
        buf.resize((size_t)n);
        for (uint64_t at = 0; at < n;) {
            const uint64_t k = std::min<uint64_t>(n - at, 16ull << 20);
            rr.read(buf.data() + at, (size_t)k);
            r.sha.update(buf.data() + at, (size_t)k);
            at += k;
            r.pos += k;
            moved(k);
        }
    };
    if (r.pos == 0) {
        take(s.data_offset);
        NN_CHECK(std::memcmp(buf.data() + 8, s.header.data(), s.header.size()) == 0,
                 "%s changed on the server while it was read; start the install again",
                 s.file->name);
    }
    for (const auto& [name, e] : s.tensors) {
        const uint64_t begin = s.data_offset + e.begin, end = s.data_offset + e.end;
        if (end <= r.pos) continue;
        NN_CHECK(begin >= r.pos, "%s: a resume point inside '%s'", s.file->name, name.c_str());
        if (begin > r.pos) take(begin - r.pos);
        take(end - begin);
        sink.tensor(name, e, buf);
        if (sink.settled()) checkpoint(r);
    }
    if (r.pos < s.file->bytes) take(s.file->bytes - r.pos);
    spirula::Sha256 fin = r.sha;
    const std::string got = fin.hex();
    if (got != s.file->sha256) {
        const size_t at = r.source;
        r = Resume{};
        r.source = at;
        checkpoint(r);
        fail("%s arrived with SHA-256 %s where %s was published; run the install "
             "again to fetch it anew", s.file->name, got.c_str(), s.file->sha256);
    }
}

// Every source of one output file, resumed, into its `.part`, renamed at the end.
void build(const std::string& path, nn::SafetensorsWriter& w, const std::vector<Source>& srcs,
           Sink& sink, const std::function<void(uint64_t)>& moved,
           const std::atomic<bool>* cancel) {
    const std::string part = path + ".part", progress = path + ".progress";
    const std::string plan = plan_id(w.header());
    Resume r;
    std::error_code ec;
    const bool resume = load_resume(progress, plan, r) && r.source <= srcs.size() &&
                        fs::exists(part, ec);
    if (!resume) r = Resume{};
    w.open(part, !resume);
    for (size_t i = 0; i < r.source; ++i) moved(srcs[i].file->bytes);
    moved(r.pos);
    auto checkpoint = [&](const Resume& at) {
        w.flush();
        save_resume(progress, plan, at);
    };
    for (; r.source < srcs.size(); ++r.source, r.pos = 0, r.sha = spirula::Sha256{}) {
        NN_LOG_INFO("[marigold] %s\n", srcs[r.source].file->name);
        stream(srcs[r.source], r, sink, checkpoint, moved, cancel);
        Resume next;
        next.source = r.source + 1;
        checkpoint(next);
    }
    w.close();
    fs::remove(path, ec);
    fs::rename(part, path, ec);
    NN_CHECK(!ec, "cannot move %s into place: %s", path.c_str(), ec.message().c_str());
    fs::remove(progress, ec);
}

// ================
// The base: the NF4 transformer and the VAE encoder
// ================

const char* const kMods[2] = {"img_mod.1", "txt_mod.1"};

int64_t global_n(int g) { return g == 2 ? 2 * kDim : g == 3 ? 4 * kLatent : kDim; }
int64_t global_k(int g) { return g == 0 ? 4 * kLatent : g == 1 ? kTextDim : kDim; }

// diffusers' Timesteps(256, True, 0, scale=1000) at Marigold v2's one timestep,
// bf16(bf16(499) / 1000) = 0.5. The frequencies stay fp32 (embeddings.py, not the
// transformer's own copy): at 500 rad one bf16 step is a different angle.
std::vector<float> timestep_projection() {
    const float t = bf16r(bf16r(499.0f) / 1000.0f);
    const float c = (float)-std::log(10000.0);
    std::vector<float> out(256);
    for (int i = 0; i < 128; ++i) {
        const float arg = 1000.0f * (t * std::exp(c * (float)i / 128.0f));
        out[(size_t)i] = bf16r(std::cos(arg));
        out[(size_t)(128 + i)] = bf16r(std::sin(arg));
    }
    return out;
}

class BaseBuilder : public Sink {
public:
    explicit BaseBuilder(nn::SafetensorsWriter& w) : w_(w) {}

    static void plan(nn::SafetensorsWriter& w, const std::vector<Source>& shards,
                     const Source& vae) {
        w.metadata("format", kBaseFormat);
        w.add("temb", "F32", {kDim});
        w.add("txt_norm", "F32", {kTextDim});
        holding(shards, "txt_norm.weight").expect("txt_norm.weight", {kTextDim});
        for (int g = 0; g < 4; ++g) {
            const int64_t N = global_n(g), K = global_k(g);
            const std::string p = kGlobalLinears[g];
            holding(shards, p + ".weight").expect(p + ".weight", {N, K});
            w.add(p + ".q4", "U8", {N * K / 2});
            w.add(p + ".absmax", "BF16", {N * K / 64});
            w.add(p + ".bias", "F32", {N});
        }
        for (int l = 1; l <= 2; ++l) {
            const std::string p = "time_text_embed.timestep_embedder.linear_" + std::to_string(l);
            holding(shards, p + ".weight").expect(p + ".weight", {kDim, l == 1 ? 256 : kDim});
        }
        for (int i = 0; i < kLayers; ++i)
            for (const char* m : kMods) {
                const std::string src = "transformer_blocks." + std::to_string(i) + "." + m;
                holding(shards, src + ".weight").expect(src + ".weight", {6 * kDim, kDim});
                w.add(block_tensor(i, m == kMods[0] ? "mod_img" : "mod_txt"), "F32", {6, kDim});
            }
        for (const auto& [name, e] : vae.tensors)
            if ((name.rfind("encoder.", 0) == 0 || name.rfind("quant_conv.", 0) == 0) &&
                vae_kept(name))
                vae_plan(w, "vae." + name, e);
        for (int i = 0; i < kLayers; ++i) {
            const std::string blk = "transformer_blocks." + std::to_string(i) + ".";
            for (const BlockLinear& L : kBlockLinears) {
                for (int p = 0; p < L.parts; ++p) {
                    const std::string src = blk + L.src[p];
                    holding(shards, src + ".weight").expect(src + ".weight", {L.N / L.parts, L.K});
                    holding(shards, src + ".bias").expect(src + ".bias", {L.N / L.parts});
                }
                w.add(block_tensor(i, L.name, ".q4"), "U8", {L.N * L.K / 2});
                w.add(block_tensor(i, L.name, ".absmax"), "BF16", {L.N * L.K / 64});
                w.add(block_tensor(i, L.name, ".bias"), "F32", {L.N});
            }
            for (const char* n : kBlockNorms) {
                holding(shards, blk + n + ".weight").expect(blk + n + ".weight", {kHeadDim});
                w.add(block_tensor(i, n), "F32", {kHeadDim});
            }
        }
    }

    void tensor(const std::string& name, const Entry& e, const Bytes& d) override {
        const uint64_t n = numel(e.shape);
        if (name.rfind("time_text_embed.", 0) == 0) {
            pending_[name] = d;
            if (pending_.size() == 4) finish_temb();
            return;
        }
        static const std::string kBlocks = "transformer_blocks.";
        if (name.rfind(kBlocks, 0) == 0) {
            const size_t dot = name.find('.', kBlocks.size());
            const int i = std::stoi(name.substr(kBlocks.size(), dot - kBlocks.size()));
            const std::string rest = name.substr(dot + 1);
            for (const char* m : kMods) {
                if (rest == std::string(m) + ".bias") pending_[name] = d;
                if (rest == std::string(m) + ".weight") modulation(i, m, d);
            }
            for (const BlockLinear& L : kBlockLinears)
                for (int p = 0; p < L.parts; ++p) {
                    const int64_t rows = L.N / L.parts;
                    if (rest == std::string(L.src[p]) + ".weight")
                        write_nf4(w_, block_tensor(i, L.name), d.data(), n,
                                  (uint64_t)(p * rows * L.K));
                    if (rest == std::string(L.src[p]) + ".bias")
                        write_floats(w_, block_tensor(i, L.name, ".bias"),
                                     bf16_floats(d.data(), n), (uint64_t)(p * rows * 4));
                }
            for (const char* norm : kBlockNorms)
                if (rest == std::string(norm) + ".weight")
                    write_floats(w_, block_tensor(i, norm), bf16_floats(d.data(), n));
            return;
        }
        for (const char* g : kGlobalLinears) {
            if (name == std::string(g) + ".weight") write_nf4(w_, g, d.data(), n, 0);
            if (name == std::string(g) + ".bias")
                write_floats(w_, std::string(g) + ".bias", bf16_floats(d.data(), n));
        }
        if (name == "txt_norm.weight") write_floats(w_, "txt_norm", bf16_floats(d.data(), n));
        if ((name.rfind("encoder.", 0) == 0 || name.rfind("quant_conv.", 0) == 0) &&
            vae_kept(name))
            vae_write(w_, "vae." + name, e, d);
    }

    bool settled() const override { return pending_.empty(); }

private:
    std::vector<float> pending_floats(const std::string& name) {
        const Bytes& b = pending_.at(name);
        return bf16_floats(b.data(), b.size() / 2);
    }

    void finish_temb() {
        const std::string p = "time_text_embed.timestep_embedder.linear_";
        const std::vector<float> w1 =
            nf4_roundtrip(pending_.at(p + "1.weight").data(), kDim * 256ull, true);
        const std::vector<float> w2 =
            nf4_roundtrip(pending_.at(p + "2.weight").data(), (uint64_t)kDim * kDim, true);
        const std::vector<float> b1 = pending_floats(p + "1.bias"), b2 = pending_floats(p + "2.bias");
        const std::vector<float> h = silu_bf16(linear_bf16(timestep_projection(), 1, w1, kDim,
                                                           256, b1.data(), Rounding::Gemv4bit));
        temb_ = linear_bf16(h, 1, w2, kDim, kDim, b2.data(), Rounding::Gemv4bit);
        write_floats(w_, "temb", temb_);
        pending_.clear();
    }

    // Block 0's img_mod is the one projection Marigold v2 left unquantized.
    void modulation(int i, const char* m, const Bytes& weight) {
        if (temb_.empty()) {
            temb_.resize(kDim);
            w_.read("temb", temb_.data(), kDim * 4);
        }
        const bool img = m == kMods[0];
        const std::string bias = "transformer_blocks." + std::to_string(i) + "." + m + ".bias";
        const uint64_t n = 6ull * kDim * kDim;
        const bool exact = i == 0 && img;
        const std::vector<float> W = exact ? bf16_floats(weight.data(), n)
                                           : nf4_roundtrip(weight.data(), n, true);
        const std::vector<float> b = pending_floats(bias);
        pending_.erase(bias);
        write_floats(w_, block_tensor(i, img ? "mod_img" : "mod_txt"),
                     linear_bf16(silu_bf16(temb_), 1, W, 6 * kDim, kDim, b.data(),
                                 exact ? Rounding::Gemm : Rounding::Gemv4bit));
    }

    nn::SafetensorsWriter& w_;
    std::map<std::string, Bytes> pending_;
    std::vector<float> temb_;
};

std::vector<Source> base_sources(const std::atomic<bool>* cancel) {
    std::vector<Source> s;
    for (const RemoteFile& f : transformer_shards()) s.push_back(probe(f, cancel));
    s.push_back(probe(vae_file(), cancel));
    return s;
}

void build_base(const std::function<void(uint64_t)>& moved, const std::atomic<bool>* cancel) {
    const std::vector<Source> srcs = base_sources(cancel);
    nn::SafetensorsWriter w;
    BaseBuilder::plan(w, std::vector<Source>(srcs.begin(), srcs.end() - 1), srcs.back());
    BaseBuilder b(w);
    build(base_path(), w, srcs, b, moved, cancel);
}

// ================
// A task: its LoRA, its VAE decoder, and the constants they change
// ================

class TaskBuilder : public Sink {
public:
    TaskBuilder(nn::SafetensorsWriter& w, const nn::SafetensorsFile& base,
                std::vector<float> prompt, int64_t tokens)
        : w_(w), base_(base), prompt_(std::move(prompt)), tokens_(tokens) {}

    static void plan(nn::SafetensorsWriter& w, Task t, const Source& tr, int64_t tokens) {
        w.metadata("format", kTaskFormat);
        w.metadata("task", task_name(t));
        w.add("txt", "F32", {tokens, kDim});
        w.add("norm_out", "F32", {2, kDim});
        w.add("img_in.weight", "F32", {kDim, 4 * kLatent});
        w.add("img_in.bias", "F32", {kDim});
        for (const char* g : {"img_in", "txt_in", "norm_out.linear"}) {
            const std::string p = std::string("Diffuser.") + g;
            const int gi = g[0] == 'i' ? 0 : g[0] == 't' ? 1 : 2;
            tr.expect(p + ".lora_A.default.weight", {kRank, global_k(gi)});
            tr.expect(p + ".lora_B.default.weight", {global_n(gi), kRank});
        }
        for (const auto& [name, e] : tr.tensors)
            if ((name.rfind("VAE.decoder.", 0) == 0 || name.rfind("VAE.post_quant_conv.", 0) == 0) &&
                vae_kept(name))
                vae_plan(w, "vae." + name.substr(4), e);
        for (int i = 0; i < kLayers; ++i)
            for (const BlockLinear& L : kBlockLinears) {
                for (int p = 0; p < L.parts; ++p) {
                    const std::string src = "Diffuser.transformer_blocks." + std::to_string(i) +
                                            "." + L.src[p];
                    tr.expect(src + ".lora_A.default.weight", {kRank, L.K});
                    tr.expect(src + ".lora_B.default.weight", {L.N / L.parts, kRank});
                }
                w.add(block_tensor(i, L.name, ".lora_a"), "F16", {L.parts, L.K, kRank});
                w.add(block_tensor(i, L.name, ".lora_b"), "F16", {L.N, kRank});
            }
    }

    void tensor(const std::string& name, const Entry& e, const Bytes& d) override {
        const uint64_t n = numel(e.shape);
        static const std::string kBlocks = "Diffuser.transformer_blocks.";
        if (name.rfind(kBlocks, 0) == 0) {
            const size_t dot = name.find('.', kBlocks.size());
            const int i = std::stoi(name.substr(kBlocks.size(), dot - kBlocks.size()));
            const std::string rest = name.substr(dot + 1);
            for (const BlockLinear& L : kBlockLinears)
                for (int p = 0; p < L.parts; ++p) {
                    const std::string src = L.src[p];
                    if (rest.rfind(src + ".lora_", 0) != 0) continue;
                    const std::vector<float> v = bf16_floats(d.data(), n);
                    if (rest == src + ".lora_A.default.weight") {
                        // Stored transposed, [K, rank]: the GEMM's weight operand.
                        std::vector<float> t(v.size());
                        for (int64_t r = 0; r < kRank; ++r)
                            for (int64_t k = 0; k < L.K; ++k) t[(size_t)(k * kRank + r)] = v[(size_t)(r * L.K + k)];
                        write_f16(w_, block_tensor(i, L.name, ".lora_a"), t,
                                  (uint64_t)(p * L.K * kRank));
                    }
                    if (rest == src + ".lora_B.default.weight")
                        write_f16(w_, block_tensor(i, L.name, ".lora_b"), v,
                                  (uint64_t)(p * (L.N / L.parts) * kRank));
                }
            return;
        }
        if (name.rfind("Diffuser.img_in.", 0) == 0 || name.rfind("Diffuser.txt_in.", 0) == 0 ||
            name.rfind("Diffuser.norm_out.", 0) == 0) {
            pending_[name] = bf16_floats(d.data(), n);
            if (pending_.size() == 2) finish_pair(name.substr(9, name.find(".lora_") - 9));
            return;
        }
        if ((name.rfind("VAE.decoder.", 0) == 0 || name.rfind("VAE.post_quant_conv.", 0) == 0) &&
            vae_kept(name))
            vae_write(w_, "vae." + name.substr(4), e, d);
    }

    bool settled() const override { return pending_.empty(); }

private:
    void finish_pair(const std::string& g) {
        const std::vector<float>& A = pending_.at("Diffuser." + g + ".lora_A.default.weight");
        const std::vector<float>& B = pending_.at("Diffuser." + g + ".lora_B.default.weight");
        if (g == "img_in") img_in(A, B);
        else if (g == "norm_out.linear") norm_out(A, B);
        else txt_in(A, B);
        pending_.clear();
    }

    // Runs every image, so the LoRA is merged into it for good.
    void img_in(const std::vector<float>& A, const std::vector<float>& B) {
        const int64_t K = 4 * kLatent;
        std::vector<float> W = read_nf4(base_, "img_in");
        for (int64_t n = 0; n < kDim; ++n)
            for (int64_t k = 0; k < K; ++k) {
                double acc = 0;
                for (int64_t r = 0; r < kRank; ++r)
                    acc += (double)B[(size_t)(n * kRank + r)] * A[(size_t)(r * K + k)];
                W[(size_t)(n * K + k)] += (float)acc;
            }
        write_floats(w_, "img_in.weight", W);
        write_floats(w_, "img_in.bias", read_f32(base_, "img_in.bias"));
    }

    // AdaLayerNormContinuous's (scale, shift) at the one timestep.
    void norm_out(const std::vector<float>& A, const std::vector<float>& B) {
        const std::vector<float> x = silu_bf16(read_f32(base_, "temb"));
        const std::vector<float> bias = read_f32(base_, "norm_out.linear.bias");
        const std::vector<float> y = linear_bf16(x, 1, read_nf4(base_, "norm_out.linear", true),
                                                 2 * kDim, kDim, bias.data(), Rounding::Gemv4bit);
        const std::vector<float> h = linear_bf16(x, 1, A, kRank, kDim, nullptr, Rounding::Gemm);
        const std::vector<float> l = linear_bf16(h, 1, B, 2 * kDim, kRank, nullptr, Rounding::Gemm);
        std::vector<float> out(y.size());
        for (size_t i = 0; i < y.size(); ++i) out[i] = bf16r(y[i] + l[i]);
        write_floats(w_, "norm_out", out);
    }

    // The text stream's input: txt_in(txt_norm(prompt)). The bf16 Linear4bit
    // hands back the fp32 its input came in as, so the LoRA's sum is not rounded.
    void txt_in(const std::vector<float>& A, const std::vector<float>& B) {
        const std::vector<float> g = read_f32(base_, "txt_norm");
        std::vector<float> x((size_t)(tokens_ * kTextDim));
        for (int64_t t = 0; t < tokens_; ++t) {
            const float* p = &prompt_[(size_t)(t * kTextDim)];
            float ss = 0.0f;
            for (int64_t c = 0; c < kTextDim; ++c) ss += p[c] * p[c];
            const float inv = 1.0f / std::sqrt(ss / (float)kTextDim + 1e-6f);
            for (int64_t c = 0; c < kTextDim; ++c)
                x[(size_t)(t * kTextDim + c)] = bf16r(p[c] * inv * g[(size_t)c]);
        }
        const std::vector<float> bias = read_f32(base_, "txt_in.bias");
        const std::vector<float> y = linear_bf16(x, tokens_, read_nf4(base_, "txt_in"), kDim,
                                                 kTextDim, bias.data(), Rounding::Gemm);
        const std::vector<float> h = linear_bf16(x, tokens_, A, kRank, kTextDim, nullptr, Rounding::Gemm);
        const std::vector<float> l = linear_bf16(h, tokens_, B, kDim, kRank, nullptr, Rounding::Gemm);
        std::vector<float> out(y.size());
        for (size_t i = 0; i < y.size(); ++i) out[i] = y[i] + l[i];
        write_floats(w_, "txt", out);
    }

    nn::SafetensorsWriter& w_;
    const nn::SafetensorsFile& base_;
    std::vector<float> prompt_;
    int64_t tokens_;
    std::map<std::string, std::vector<float>> pending_;
};

// A file small enough to hold: fetched whole, checked, written beside the install.
std::string fetch_small(const RemoteFile& f, const std::string& name,
                        const std::atomic<bool>* cancel) {
    std::vector<uint8_t> data((size_t)f.bytes);
    RemoteRange(f, 0, f.bytes, cancel).read(data.data(), data.size());
    spirula::Sha256 sha;
    sha.update(data.data(), data.size());
    NN_CHECK(sha.hex() == f.sha256, "%s arrived damaged (SHA-256 mismatch); run again", f.name);
    const std::string path = (fs::path(install_dir()) / name).string();
    std::ofstream o(path, std::ios::binary | std::ios::trunc);
    o.write(reinterpret_cast<const char*>(data.data()), (std::streamsize)data.size());
    NN_CHECK(o.good(), "cannot write %s", path.c_str());
    return path;
}

// Context 0 of the precomputed prompt: what Marigold v2's own inference uses
// for a single image. Its text is never encoded here -- the 7B encoder is not
// part of the model as published.
std::vector<float> read_prompt(Task t, int64_t* tokens, const std::atomic<bool>* cancel) {
    const std::string tag = task_name(t);
    const std::string emb = fetch_small(prompt_embeds(t), tag + "-prompt.pt", cancel);
    const std::string msk = fetch_small(prompt_mask(t), tag + "-mask.pt", cancel);
    const nn::OnnxTensor e = nn::TorchCheckpoint(emb).read("");
    const nn::OnnxTensor m = nn::TorchCheckpoint(msk).read("");
    NN_CHECK(e.shape.size() == 3 && e.shape[2] == kTextDim && m.shape.size() == 2 &&
                 m.shape[1] == e.shape[1],
             "%s: not the [contexts, tokens, %d] prompt this build expects", emb.c_str(), kTextDim);
    *tokens = e.shape[1];
    for (int64_t i = 0; i < *tokens; ++i)
        NN_CHECK(m.data[(size_t)i] != 0.0f, "%s: a padded prompt, which needs an attention mask",
                 msk.c_str());
    std::error_code ec;
    fs::remove(emb, ec);
    fs::remove(msk, ec);
    return std::vector<float>(e.data.begin(), e.data.begin() + *tokens * kTextDim);
}

void build_task(Task t, const std::function<void(uint64_t)>& moved,
                const std::atomic<bool>* cancel) {
    int64_t tokens = 0;
    std::vector<float> prompt = read_prompt(t, &tokens, cancel);
    std::vector<Source> srcs{probe(trainables(t), cancel)};
    nn::SafetensorsWriter w;
    TaskBuilder::plan(w, t, srcs[0], tokens);
    const nn::SafetensorsFile base(base_path());
    TaskBuilder b(w, base, std::move(prompt), tokens);
    build(task_path(t), w, srcs, b, moved, cancel);
    moved(prompt_embeds(t).bytes + prompt_mask(t).bytes);
}

// ================
// What is there
// ================

bool format_is(const std::string& path, const char* format) {
    static std::mutex mu;
    static std::map<std::string, std::pair<std::string, bool>> seen;
    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) return false;
    const auto stamp = std::to_string(fs::file_size(path, ec)) + ":" +
                       std::to_string((long long)fs::last_write_time(path, ec).time_since_epoch().count());
    std::lock_guard<std::mutex> lk(mu);
    auto it = seen.find(path);
    if (it != seen.end() && it->second.first == stamp) return it->second.second;
    bool ok = false;
    try {
        const nn::SafetensorsFile f(path);
        auto m = f.metadata().find("format");
        ok = m != f.metadata().end() && m->second == format;
    } catch (const std::exception&) {
    }
    seen[path] = {stamp, ok};
    return ok;
}

uint64_t free_bytes() {
    fs::path p = install_dir();
    std::error_code ec;
    while (!p.empty() && !fs::exists(p, ec)) p = p.parent_path();
    const fs::space_info s = fs::space(p.empty() ? fs::path(".") : p, ec);
    return ec ? 0 : s.available;
}

// Bytes of `srcs` a resumable build at `path` has already streamed.
uint64_t streamed(const std::string& path, const std::vector<uint64_t>& srcs) {
    Resume r;
    if (!load_resume(path + ".progress", "", r)) return 0;
    uint64_t done = r.pos;
    for (size_t i = 0; i < r.source && i < srcs.size(); ++i) done += srcs[i];
    return done;
}

void add_needs(InstallNeeds& n, const std::string& path, const std::vector<uint64_t>& srcs,
               uint64_t out_bytes) {
    uint64_t total = 0;
    for (uint64_t b : srcs) total += b;
    const uint64_t done = std::min(streamed(path, srcs), total);
    n.download += total - done;
    n.disk += (uint64_t)((double)out_bytes * (double)(total - done) / (double)total);
}

// One install at a time per cache: the GUI's download and a terminal's would
// write the same files. The OS drops the lock with the process.
class InstallLock {
public:
    explicit InstallLock(const std::string& path) {
#ifdef _WIN32
        h_ = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
        ok_ = h_ != INVALID_HANDLE_VALUE;
#else
        fd_ = ::open(path.c_str(), O_RDWR | O_CREAT, 0644);
        ok_ = fd_ >= 0 && ::flock(fd_, LOCK_EX | LOCK_NB) == 0;
#endif
        NN_CHECK(ok_, "another process is installing Marigold v2 into %s; wait for it, or "
                 "stop it -- what it has done is kept", fs::path(path).parent_path().string().c_str());
    }
    ~InstallLock() {
#ifdef _WIN32
        if (h_ != INVALID_HANDLE_VALUE) CloseHandle(h_);
#else
        if (fd_ >= 0) ::close(fd_);
#endif
    }

private:
    bool ok_ = false;
#ifdef _WIN32
    HANDLE h_ = INVALID_HANDLE_VALUE;
#else
    int fd_ = -1;
#endif
};

}  // namespace

bool base_installed() { return format_is(base_path(), kBaseFormat); }
bool task_installed(Task t) { return format_is(task_path(t), kTaskFormat); }

bool InstallNeeds::fits() const { return free >= disk + kDiskSpare; }

InstallNeeds install_needs(bool depth, bool normals) {
    InstallNeeds n;
    if (!base_installed()) {
        std::vector<uint64_t> b;
        for (const RemoteFile& f : transformer_shards()) b.push_back(f.bytes);
        b.push_back(vae_file().bytes);
        add_needs(n, base_path(), b, base_install_bytes());
    }
    for (Task t : {Task::Depth, Task::Normals}) {
        if ((t == Task::Depth ? !depth : !normals) || task_installed(t)) continue;
        add_needs(n, task_path(t), {trainables(t).bytes}, task_install_bytes());
        n.download += prompt_embeds(t).bytes + prompt_mask(t).bytes;
    }
    n.free = free_bytes();
    return n;
}

void install(bool depth, bool normals, const InstallProgress& progress,
             const std::atomic<bool>* cancel) {
    const InstallNeeds needs = install_needs(depth, normals);
    if (needs.download == 0) return;
    NN_CHECK(needs.fits(),
             "installing Marigold v2 writes %.1f GB more under %s, and that disk has %.1f GB "
             "free (%.1f GB is kept spare)",
             needs.disk / 1e9, install_dir().c_str(), needs.free / 1e9, kDiskSpare / 1e9);
    std::error_code ec;
    fs::create_directories(install_dir(), ec);
    NN_CHECK(!ec, "cannot create %s: %s", install_dir().c_str(), ec.message().c_str());
    const InstallLock lock((fs::path(install_dir()) / "install.lock").string());

    uint64_t done = 0;
    const uint64_t total = needs.download;
    // A build first reports the prefix a resume skips, which needs.download
    // already left out.
    auto counter = [&](uint64_t skip) {
        return [&, skip, seen = (uint64_t)0](uint64_t k) mutable {
            const uint64_t ahead = skip > seen ? skip - seen : 0;
            seen += k;
            done += k - std::min(k, ahead);
            if (progress) progress(std::min(done, total), total);
        };
    };
    if (!base_installed()) {
        std::vector<uint64_t> b;
        for (const RemoteFile& f : transformer_shards()) b.push_back(f.bytes);
        b.push_back(vae_file().bytes);
        build_base(counter(streamed(base_path(), b)), cancel);
    }
    for (Task t : {Task::Depth, Task::Normals}) {
        if ((t == Task::Depth ? !depth : !normals) || task_installed(t)) continue;
        build_task(t, counter(streamed(task_path(t), {trainables(t).bytes})), cancel);
    }
}

}  // namespace marigold
