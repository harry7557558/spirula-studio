#include "nn/io/Onnx.h"

#include "nn/core/Error.h"
#include "nn/core/Half.h"
#include "nn/core/Log.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <unordered_set>

namespace nn {
namespace {

// ONNX TensorProto.DataType, the handful that can appear as a weight.
enum : int32_t {
    kFloat = 1,
    kUint8 = 2,
    kInt8 = 3,
    kInt32 = 6,
    kInt64 = 7,
    kDouble = 11,
    kFloat16 = 10,
};

// TensorProto.DataLocation
constexpr int64_t kLocationExternal = 1;

const char* dtype_name(int32_t t) {
    switch (t) {
        case kFloat:   return "float";
        case kUint8:   return "uint8";
        case kInt8:    return "int8";
        case kInt32:   return "int32";
        case kInt64:   return "int64";
        case kDouble:  return "double";
        case kFloat16: return "float16";
        default:       return "?";
    }
}

// A bounds-checked cursor over the file. Every read validates against `end`
// before it advances, so a truncated or hostile file throws instead of walking
// off the buffer -- this parses bytes fetched over the network.
struct Reader {
    const uint8_t* p = nullptr;
    const uint8_t* end = nullptr;
    const char*    what = "";

    bool done() const { return p >= end; }

    [[noreturn]] void bad(const char* why) const {
        nn::fail("%s: malformed ONNX (%s)", what, why);
    }

    uint64_t varint() {
        uint64_t r = 0;
        int shift = 0;
        while (true) {
            if (p >= end) bad("truncated varint");
            const uint8_t c = *p++;
            // 10 groups of 7 bits covers a full uint64; more means corruption.
            if (shift > 63) bad("varint too long");
            r |= (uint64_t)(c & 0x7F) << shift;
            if (!(c & 0x80)) return r;
            shift += 7;
        }
    }

    void skip(uint64_t n) {
        if ((uint64_t)(end - p) < n) bad("truncated field");
        p += n;
    }
};

// One protobuf field: its number, and a sub-reader over its payload (wire type
// 2) or its integer value (wire types 0/1/5).
struct Field {
    uint32_t number = 0;
    uint32_t wire = 0;
    uint64_t value = 0;   // wire 0/1/5
    Reader   bytes;       // wire 2
};

bool next_field(Reader& r, Field& f) {
    if (r.done()) return false;
    const uint64_t key = r.varint();
    f.number = (uint32_t)(key >> 3);
    f.wire = (uint32_t)(key & 7);
    switch (f.wire) {
        case 0:
            f.value = r.varint();
            return true;
        case 1:
            if (r.end - r.p < 8) r.bad("truncated fixed64");
            std::memcpy(&f.value, r.p, 8);
            r.p += 8;
            return true;
        case 5: {
            if (r.end - r.p < 4) r.bad("truncated fixed32");
            uint32_t v32 = 0;
            std::memcpy(&v32, r.p, 4);
            r.p += 4;
            f.value = v32;
            return true;
        }
        case 2: {
            const uint64_t n = r.varint();
            if ((uint64_t)(r.end - r.p) < n) r.bad("truncated length-delimited field");
            f.bytes = Reader{r.p, r.p + n, r.what};
            r.p += n;
            return true;
        }
        default:
            // 3/4 are the deprecated group encoding; nothing emits them.
            r.bad("unsupported wire type");
    }
}

std::string to_string(const Reader& r) {
    return std::string(reinterpret_cast<const char*>(r.p), (size_t)(r.end - r.p));
}

float bits_to_float(uint64_t bits32) {
    const uint32_t u = (uint32_t)bits32;
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}

// Convert a raw_data payload of `dt` into f32. Element counts are checked
// against the declared shape by the caller.
void decode_raw(const Reader& raw, int32_t dt, int64_t count, std::vector<float>& out,
                const char* name) {
    const size_t bytes = (size_t)(raw.end - raw.p);
    auto need = [&](size_t elem_size) {
        if (bytes != (size_t)count * elem_size)
            nn::fail("initializer '%s': %zu raw bytes for %lld %s elements", name, bytes,
                     (long long)count, dtype_name(dt));
    };
    out.resize((size_t)count);
    // raw_data is little-endian by specification. Every platform this builds
    // for is little-endian, so these are memcpys; a big-endian port would swap
    // here and nowhere else.
    switch (dt) {
        case kFloat: {
            need(4);
            std::memcpy(out.data(), raw.p, bytes);
            break;
        }
        case kFloat16: {
            need(2);
            for (int64_t i = 0; i < count; ++i) {
                uint16_t h;
                std::memcpy(&h, raw.p + (size_t)i * 2, 2);
                out[(size_t)i] = nn::half_to_float(h);
            }
            break;
        }
        case kDouble: {
            need(8);
            for (int64_t i = 0; i < count; ++i) {
                double d;
                std::memcpy(&d, raw.p + (size_t)i * 8, 8);
                out[(size_t)i] = (float)d;
            }
            break;
        }
        case kInt64: {
            need(8);
            for (int64_t i = 0; i < count; ++i) {
                int64_t v;
                std::memcpy(&v, raw.p + (size_t)i * 8, 8);
                out[(size_t)i] = (float)v;
            }
            break;
        }
        case kInt32: {
            need(4);
            for (int64_t i = 0; i < count; ++i) {
                int32_t v;
                std::memcpy(&v, raw.p + (size_t)i * 4, 4);
                out[(size_t)i] = (float)v;
            }
            break;
        }
        default:
            nn::fail("initializer '%s' has dtype %s, which this reader does not convert",
                     name, dtype_name(dt));
    }
}

// Where a TensorProto says its bytes really are. An export over 2 GB has to
// put them beside the model (protobuf's own message limit), and Metric3D's
// giant2 does: `location` names a sibling file, `offset` and `length` a slice
// of it.
struct ExternalRef {
    std::string location;
    uint64_t    offset = 0;
    uint64_t    length = 0;
};

// The model file's bytes, plus lazily-opened handles on whatever sibling files
// its initializers point at. One handle per location, because a checkpoint
// with external data has one such file and thousands of tensors in it.
struct Loader {
    std::string          path;
    std::string          dir;
    std::vector<uint8_t> buf;
    std::unordered_map<std::string, std::shared_ptr<std::ifstream>> external;
    std::vector<uint8_t> scratch;

    void open(const std::string& p) {
        path = p;
        const size_t slash = p.find_last_of("/\\");
        dir = (slash == std::string::npos) ? std::string() : p.substr(0, slash + 1);

        std::ifstream fin(p, std::ios::binary | std::ios::ate);
        NN_CHECK((bool)fin, "cannot open '%s'", p.c_str());
        const std::streamoff size = fin.tellg();
        NN_CHECK(size > 16, "'%s' is %lld bytes; not an ONNX model", p.c_str(),
                 (long long)size);
        fin.seekg(0);
        buf.resize((size_t)size);
        NN_CHECK((bool)fin.read(reinterpret_cast<char*>(buf.data()), size),
                 "cannot read '%s'", p.c_str());
    }

    // Reads the slice into `scratch` and returns a Reader over it. The buffer
    // is reused across tensors, so the returned Reader is valid only until the
    // next call -- every caller decodes before asking for another.
    Reader readExternal(const ExternalRef& ref, const char* name) {
        NN_CHECK(!ref.location.empty(), "initializer '%s' is external with no location",
                 name);
        auto it = external.find(ref.location);
        if (it == external.end()) {
            const std::string full = dir + ref.location;
            auto fin = std::make_shared<std::ifstream>(full, std::ios::binary);
            NN_CHECK((bool)*fin,
                     "'%s' keeps its weights in '%s', which cannot be opened.\n"
                     "  An external-data export is two files; fetch both into the "
                     "same directory.",
                     path.c_str(), full.c_str());
            it = external.emplace(ref.location, std::move(fin)).first;
        }
        std::ifstream& fin = *it->second;
        scratch.resize((size_t)ref.length);
        fin.clear();
        fin.seekg((std::streamoff)ref.offset);
        NN_CHECK((bool)fin.read(reinterpret_cast<char*>(scratch.data()),
                                (std::streamsize)ref.length),
                 "initializer '%s': cannot read %llu bytes at offset %llu of '%s'", name,
                 (unsigned long long)ref.length, (unsigned long long)ref.offset,
                 ref.location.c_str());
        return Reader{scratch.data(), scratch.data() + scratch.size(), path.c_str()};
    }
};

OnnxTensor read_tensor_proto(Loader& L, Reader r) {
    OnnxTensor t;
    int32_t dt = 0;
    Reader raw{}, float_data{};
    bool has_raw = false, has_float = false;
    int64_t location = 0;
    ExternalRef ext;

    Field f;
    while (next_field(r, f)) {
        switch (f.number) {
            case 1:  // dims: repeated int64, packed or not
                if (f.wire == 0) {
                    t.shape.push_back((int64_t)f.value);
                } else if (f.wire == 2) {
                    Reader d = f.bytes;
                    while (!d.done()) t.shape.push_back((int64_t)d.varint());
                }
                break;
            case 2:  // data_type
                dt = (int32_t)f.value;
                break;
            case 4:  // float_data: repeated float, packed
                if (f.wire == 2) { float_data = f.bytes; has_float = true; }
                break;
            case 8:  // name
                if (f.wire == 2) t.name = to_string(f.bytes);
                break;
            case 9:  // raw_data
                if (f.wire == 2) { raw = f.bytes; has_raw = true; }
                break;
            case 13: {  // external_data: repeated StringStringEntryProto
                if (f.wire != 2) break;
                Reader e = f.bytes;
                std::string key, value;
                Field ef;
                while (next_field(e, ef)) {
                    if (ef.number == 1 && ef.wire == 2) key = to_string(ef.bytes);
                    else if (ef.number == 2 && ef.wire == 2) value = to_string(ef.bytes);
                }
                if (key == "location") ext.location = value;
                else if (key == "offset") ext.offset = std::strtoull(value.c_str(), nullptr, 10);
                else if (key == "length") ext.length = std::strtoull(value.c_str(), nullptr, 10);
                break;
            }
            case 14:  // data_location
                location = (int64_t)f.value;
                break;
            default:
                break;  // segment, string_data, doc_string, ...
        }
    }

    const char* name = t.name.empty() ? "<unnamed>" : t.name.c_str();
    const int64_t count = t.numel();
    if (count < 0 || count > (int64_t)1 << 32)
        nn::fail("initializer '%s' declares %lld elements", name, (long long)count);
    t.was_f16 = (dt == kFloat16);

    if (location == kLocationExternal) {
        decode_raw(L.readExternal(ext, name), dt, count, t.data, name);
    } else if (has_raw) {
        decode_raw(raw, dt, count, t.data, name);
    } else if (has_float && dt == kFloat) {
        // The non-raw encoding: 4-byte little-endian floats, packed.
        const size_t bytes = (size_t)(float_data.end - float_data.p);
        if (bytes != (size_t)count * 4)
            nn::fail("initializer '%s': %zu float_data bytes for %lld elements", name,
                     bytes, (long long)count);
        t.data.resize((size_t)count);
        std::memcpy(t.data.data(), float_data.p, bytes);
    } else {
        nn::fail("initializer '%s' carries no data this reader can read (dtype %s)", name,
                 dtype_name(dt));
    }
    return t;
}

// ValueInfo -> Type -> tensor_type -> shape -> repeated Dimension, each a
// dim_value (field 1) or a symbolic dim_param (field 2); the second is what
// -1 records, and is how a baked-in input size is told from a dynamic axis.
void scan_value_info(Reader r, OnnxValueInfo& out) {
    Field f;
    while (next_field(r, f)) {
        if (f.number == 1 && f.wire == 2) {
            out.name = to_string(f.bytes);
        } else if (f.number == 2 && f.wire == 2) {
            Reader type = f.bytes;
            Field tf;
            while (next_field(type, tf)) {
                if (tf.number != 1 || tf.wire != 2) continue;   // tensor_type
                Reader tt = tf.bytes;
                Field ttf;
                while (next_field(tt, ttf)) {
                    if (ttf.number != 2 || ttf.wire != 2) continue;   // shape
                    Reader shape = ttf.bytes;
                    Field sf;
                    while (next_field(shape, sf)) {
                        if (sf.number != 1 || sf.wire != 2) continue;   // dim
                        Reader dim = sf.bytes;
                        Field df;
                        int64_t v = -1;
                        while (next_field(dim, df))
                            if (df.number == 1 && df.wire == 0) v = (int64_t)df.value;
                        out.shape.push_back(v);
                    }
                }
            }
        }
    }
}

// NodeProto: input is repeated field 1, output repeated field 2, name field 3,
// op_type field 4, attribute field 5.
//
// Two things are wanted from it. The epsilon of each BatchNormalization, keyed
// by the scale input's name, so folding BN into a conv uses the number in the
// file rather than assuming PyTorch's default. And the node's shape, so an
// anonymous initializer can be traced back to the module that consumes it
// (see OnnxFile::linearWeights).
void scan_node(Reader r, OnnxNode& out, std::unordered_map<std::string, float>& bn_eps) {
    std::vector<std::string> inputs;
    std::string op;
    float eps = 0.0f;
    bool has_eps = false;

    Field f;
    while (next_field(r, f)) {
        if (f.number == 1 && f.wire == 2) {
            inputs.push_back(to_string(f.bytes));
        } else if (f.number == 2 && f.wire == 2) {
            out.outputs.push_back(to_string(f.bytes));
        } else if (f.number == 3 && f.wire == 2) {
            out.name = to_string(f.bytes);
        } else if (f.number == 4 && f.wire == 2) {
            op = to_string(f.bytes);
        } else if (f.number == 5 && f.wire == 2) {
            // AttributeProto: name field 1, f (float) field 2.
            Reader a = f.bytes;
            std::string aname;
            float av = 0.0f;
            bool has_av = false;
            Field af;
            while (next_field(a, af)) {
                if (af.number == 1 && af.wire == 2) aname = to_string(af.bytes);
                else if (af.number == 2 && af.wire == 5) { av = bits_to_float(af.value); has_av = true; }
            }
            if (aname == "epsilon" && has_av) { eps = av; has_eps = true; }
        }
    }

    if (op == "BatchNormalization" && has_eps && inputs.size() > 1)
        bn_eps[inputs[1]] = eps;
    out.op_type = std::move(op);
    out.inputs = std::move(inputs);
}

// The one walk all three entry points share. `sink` is null to skip initializer
// payloads entirely, which is what makes a structure-only pass cheap.
OnnxFile walk(const std::string& path, bool want_nodes,
              const std::function<void(OnnxTensor&&)>* sink) {
    Loader L;
    L.open(path);

    Reader model{L.buf.data(), L.buf.data() + L.buf.size(), path.c_str()};
    Reader graph{};
    bool has_graph = false;

    Field f;
    while (next_field(model, f)) {
        if (f.number == 7 && f.wire == 2) { graph = f.bytes; has_graph = true; }
    }
    NN_CHECK(has_graph, "'%s' has no graph; not an ONNX model", path.c_str());

    OnnxFile out;
    size_t n_init = 0;
    while (next_field(graph, f)) {
        if (f.number == 5 && f.wire == 2) {
            ++n_init;
            if (sink) (*sink)(read_tensor_proto(L, f.bytes));
        } else if (f.number == 1 && f.wire == 2 && want_nodes) {
            out.nodes.emplace_back();
            scan_node(f.bytes, out.nodes.back(), out.bn_epsilon);
        } else if ((f.number == 11 || f.number == 12) && f.wire == 2) {
            std::vector<OnnxValueInfo>& into = (f.number == 11) ? out.inputs : out.outputs;
            into.emplace_back();
            scan_value_info(f.bytes, into.back());
        }
    }
    NN_CHECK(n_init != 0, "'%s' has no initializers", path.c_str());
    return out;
}

}  // namespace

std::string OnnxTensor::shapeString() const {
    std::string s = "[";
    for (size_t i = 0; i < shape.size(); ++i) {
        if (i) s += ", ";
        s += std::to_string(shape[i]);
    }
    return s + "]";
}

const OnnxTensor* OnnxFile::find(const std::string& name) const {
    for (const OnnxTensor& t : initializers)
        if (t.name == name) return &t;
    return nullptr;
}

const OnnxValueInfo* OnnxFile::input(const std::string& name) const {
    for (const OnnxValueInfo& v : inputs)
        if (v.name == name) return &v;
    return nullptr;
}

const OnnxValueInfo* OnnxFile::output(const std::string& name) const {
    for (const OnnxValueInfo& v : outputs)
        if (v.name == name) return &v;
    return nullptr;
}

const OnnxNode* OnnxFile::producer(const std::string& tensor) const {
    for (const OnnxNode& n : nodes)
        for (const std::string& o : n.outputs)
            if (o == tensor) return &n;
    return nullptr;
}

std::unordered_map<std::string, std::string> OnnxFile::linearWeights() const {
    std::unordered_set<std::string> inits;
    for (const OnnxTensor& t : initializers) inits.insert(t.name);
    // A structure-only pass has no initializer list to test against, so the
    // producer walk is the only evidence: a MatMul operand nothing produces is
    // an initializer.
    const bool know_inits = !initializers.empty();

    std::unordered_map<std::string, std::string> out;
    for (const OnnxNode& n : nodes) {
        if (n.op_type != "Add" || n.inputs.size() != 2) continue;
        for (int b = 0; b < 2; ++b) {
            const std::string& bias = n.inputs[b];
            if (know_inits ? !inits.count(bias) : producer(bias) != nullptr) continue;
            const size_t dot = bias.rfind(".bias");
            if (dot == std::string::npos || dot + 5 != bias.size()) continue;
            const OnnxNode* mm = producer(n.inputs[1 - b]);
            if (!mm || mm->op_type != "MatMul" || mm->inputs.size() != 2) continue;
            // MatMul's second operand is the weight when it is an initializer.
            const std::string& w = mm->inputs[1];
            if (know_inits ? inits.count(w) != 0 : producer(w) == nullptr)
                out[bias.substr(0, dot)] = w;
        }
    }
    return out;
}

OnnxFile read_onnx(const std::string& path) {
    OnnxFile out;
    std::function<void(OnnxTensor&&)> sink = [&](OnnxTensor&& t) {
        out.initializers.push_back(std::move(t));
    };
    OnnxFile structure = walk(path, /*want_nodes=*/true, &sink);
    out.nodes = std::move(structure.nodes);
    out.bn_epsilon = std::move(structure.bn_epsilon);
    out.inputs = std::move(structure.inputs);
    out.outputs = std::move(structure.outputs);

    size_t elems = 0;
    for (const OnnxTensor& t : out.initializers) elems += t.data.size();
    NN_LOG_DEBUG("[onnx] %s: %zu initializers, %.2f MB of weights\n", path.c_str(),
                 out.initializers.size(), (double)elems * 4.0 / 1e6);
    return out;
}

OnnxFile read_onnx_structure(const std::string& path) {
    return walk(path, /*want_nodes=*/true, nullptr);
}

void read_onnx_initializers(const std::string& path,
                            const std::function<void(OnnxTensor&&)>& sink) {
    walk(path, /*want_nodes=*/false, &sink);
}

}  // namespace nn
