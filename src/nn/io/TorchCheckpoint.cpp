#include "nn/io/TorchCheckpoint.h"

#include "external/miniz.h"
#include "nn/core/Error.h"
#include "nn/core/Half.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <utility>

namespace nn {
namespace {

constexpr uint64_t kMetadataLimit = 64ull << 20;
constexpr size_t kObjectLimit = 2000000;

enum class Kind { None, Mark, Integer, Real, String, Global, Sequence, Dict, Storage, Tensor };

struct Value;
using V = std::shared_ptr<Value>;
struct Value {
    Kind kind = Kind::None;
    int64_t integer = 0;
    double real = 0;
    std::string text;
    std::vector<V> items;
    std::vector<std::pair<V, V>> pairs;
    TorchCheckpoint::Entry tensor;
};

uint32_t storage_width(const std::string& dtype) {
    if (dtype == "FloatStorage" || dtype == "IntStorage") return 4;
    if (dtype == "HalfStorage" || dtype == "BFloat16Storage" || dtype == "ShortStorage") return 2;
    if (dtype == "DoubleStorage" || dtype == "LongStorage") return 8;
    if (dtype == "ByteStorage" || dtype == "CharStorage" || dtype == "BoolStorage") return 1;
    fail("unsupported checkpoint storage dtype '%s'", dtype.c_str());
}

bool allowed_global(const std::string& s) {
    if (s == "collections\nOrderedDict" || s == "torch._utils\n_rebuild_tensor" ||
        s == "torch._utils\n_rebuild_tensor_v2" || s == "torch._utils\n_rebuild_parameter")
        return true;
    if (s.compare(0, 6, "torch\n") != 0) return false;
    const std::string t = s.substr(6);
    return t == "FloatStorage" || t == "HalfStorage" || t == "BFloat16Storage" ||
           t == "DoubleStorage" || t == "LongStorage" || t == "IntStorage" ||
           t == "ShortStorage" || t == "ByteStorage" || t == "CharStorage" || t == "BoolStorage";
}

uint64_t elements(const std::vector<int64_t>& shape) {
    uint64_t n = 1;
    for (int64_t d : shape) {
        NN_CHECK(d >= 0, "negative checkpoint tensor dimension");
        NN_CHECK(!d || n <= (uint64_t)std::numeric_limits<int64_t>::max() / (uint64_t)d,
                 "checkpoint tensor dimensions overflow");
        n *= (uint64_t)d;
    }
    return n;
}

class Pickle {
public:
    Pickle(const std::vector<uint8_t>& bytes, const std::string& path)
        : bytes_(bytes), path_(path) {}

    V read() {
        while (pos_ < bytes_.size()) {
            const uint8_t op = byte();
            switch (op) {
            case 0x80: { const uint8_t p = byte(); require(p >= 2 && p <= 5, "unsupported pickle protocol"); break; }
            case 0x95: { const uint64_t n = uint_le(8); require(n <= bytes_.size() - pos_, "truncated pickle frame"); break; }
            case '.': require(stack_.size() == 1 && pos_ == bytes_.size(), "invalid pickle STOP"); return stack_[0];
            case 'N': push(make(Kind::None)); break;
            case 0x88: integer(1); break;
            case 0x89: integer(0); break;
            case 'K': integer(byte()); break;
            case 'M': integer((int64_t)uint_le(2)); break;
            case 'J': integer((int32_t)uint_le(4)); break;
            case 0x8a: long_integer(byte()); break;
            case 0x8b: long_integer(uint_le(4)); break;
            case 'G': { uint64_t bits = 0; for (int i = 0; i < 8; ++i) bits = (bits << 8) | byte();
                        V v = make(Kind::Real); std::memcpy(&v->real, &bits, 8); push(v); break; }
            case 'X': string(uint_le(4)); break;
            case 0x8c: string(byte()); break;
            case 0x8d: string(uint_le(8)); break;
            case 'c': { const std::string module = line(), name = line(); global(module + "\n" + name); break; }
            case 0x93: { V name = pop(), module = pop();
                         require(name->kind == Kind::String && module->kind == Kind::String, "invalid STACK_GLOBAL");
                         global(module->text + "\n" + name->text); break; }
            case '(': push(make(Kind::Mark)); break;
            case ')': case ']': push(make(Kind::Sequence)); break;
            case '}': push(make(Kind::Dict)); break;
            case 't': case 'l': { V v = make(Kind::Sequence); v->items = marked(); push(v); break; }
            case 'd': { V v = make(Kind::Dict); put_pairs(v, marked()); push(v); break; }
            case 0x85: tuple(1); break;
            case 0x86: tuple(2); break;
            case 0x87: tuple(3); break;
            case 'a': { V item = pop(); V v = top(); require(v->kind == Kind::Sequence, "invalid APPEND"); v->items.push_back(item); break; }
            case 'e': { std::vector<V> items = marked(); V v = top(); require(v->kind == Kind::Sequence, "invalid APPENDS");
                        v->items.insert(v->items.end(), items.begin(), items.end()); break; }
            case 's': { V value = pop(), key = pop(); put_pairs(top(), {key, value}); break; }
            case 'u': { std::vector<V> pairs = marked(); put_pairs(top(), pairs); break; }
            case 'q': memo_put(byte()); break;
            case 'r': memo_put(uint_le(4)); break;
            case 0x94: memo_put(memo_.size()); break;
            case 'h': memo_get(byte()); break;
            case 'j': memo_get(uint_le(4)); break;
            case 'Q': persistent(); break;
            case 'R': reduce(); break;
            case 'b': { V state = pop(); V target = top();
                        require(target->kind == Kind::Dict && state->kind == Kind::Dict, "unsupported pickle BUILD");
                        for (const auto& p : state->pairs)
                            require(p.first->kind == Kind::String && p.first->text == "_metadata" && p.second->kind == Kind::Dict,
                                    "unsupported checkpoint object attributes"); break; }
            default: fail("%s: unsupported pickle opcode 0x%02x at %llu", path_.c_str(), op,
                          (unsigned long long)(pos_ - 1));
            }
        }
        fail("%s: truncated checkpoint pickle", path_.c_str());
    }

private:
    const std::vector<uint8_t>& bytes_;
    const std::string& path_;
    size_t pos_ = 0, objects_ = 0;
    std::vector<V> stack_, memo_;

    void require(bool ok, const char* why) const { NN_CHECK(ok, "%s: %s", path_.c_str(), why); }
    uint8_t byte() { require(pos_ < bytes_.size(), "truncated pickle operand"); return bytes_[pos_++]; }
    uint64_t uint_le(int count) { uint64_t v = 0; for (int i = 0; i < count; ++i) v |= (uint64_t)byte() << (8 * i); return v; }
    V make(Kind kind) { require(++objects_ <= kObjectLimit, "too many checkpoint objects"); V v = std::make_shared<Value>(); v->kind = kind; return v; }
    void push(V v) { require(stack_.size() < kObjectLimit, "checkpoint stack limit exceeded"); stack_.push_back(std::move(v)); }
    V pop() { require(!stack_.empty(), "checkpoint stack underflow"); V v = stack_.back(); stack_.pop_back(); return v; }
    V top() const { require(!stack_.empty(), "checkpoint stack underflow"); return stack_.back(); }
    void integer(int64_t x) { V v = make(Kind::Integer); v->integer = x; push(v); }
    void long_integer(uint64_t count) {
        require(count <= 8, "checkpoint integer exceeds int64");
        uint64_t n = 0;
        for (uint64_t i = 0; i < count; ++i) n |= (uint64_t)byte() << (8 * i);
        if (count && count < 8 && (n & (1ull << (8 * count - 1)))) n |= ~0ull << (8 * count);
        int64_t signed_n; std::memcpy(&signed_n, &n, 8); integer(signed_n);
    }
    void string(uint64_t n) {
        require(n <= bytes_.size() - pos_, "truncated checkpoint string");
        V v = make(Kind::String); v->text.assign((const char*)bytes_.data() + pos_, (size_t)n);
        pos_ += (size_t)n; push(v);
    }
    std::string line() {
        const size_t start = pos_;
        while (pos_ < bytes_.size() && bytes_[pos_] != '\n') ++pos_;
        require(pos_ < bytes_.size(), "truncated pickle GLOBAL");
        return std::string((const char*)bytes_.data() + start, pos_++ - start);
    }
    void global(const std::string& name) {
        require(allowed_global(name), "checkpoint contains a non-tensor pickle global");
        V v = make(Kind::Global); v->text = name; push(v);
    }
    std::vector<V> marked() {
        size_t mark = stack_.size();
        while (mark && stack_[mark - 1]->kind != Kind::Mark) --mark;
        require(mark > 0, "missing pickle MARK");
        std::vector<V> out(stack_.begin() + mark, stack_.end()); stack_.resize(mark - 1); return out;
    }
    void tuple(size_t n) {
        require(stack_.size() >= n, "truncated pickle tuple");
        V v = make(Kind::Sequence); v->items.assign(stack_.end() - n, stack_.end());
        stack_.resize(stack_.size() - n); push(v);
    }
    void put_pairs(const V& dict, const std::vector<V>& pairs) {
        require(dict->kind == Kind::Dict && pairs.size() % 2 == 0, "invalid pickle dictionary");
        for (size_t i = 0; i < pairs.size(); i += 2) {
            require(pairs[i]->kind == Kind::String, "checkpoint dictionary key is not a string");
            auto found = std::find_if(dict->pairs.begin(), dict->pairs.end(), [&](const auto& p) { return p.first->text == pairs[i]->text; });
            require(found == dict->pairs.end(), "duplicate checkpoint dictionary key");
            dict->pairs.emplace_back(pairs[i], pairs[i + 1]);
        }
    }
    void memo_put(uint64_t i) {
        require(i < kObjectLimit, "checkpoint memo limit exceeded");
        if (memo_.size() <= i) memo_.resize((size_t)i + 1);
        require(!memo_[(size_t)i], "duplicate checkpoint memo index"); memo_[(size_t)i] = top();
    }
    void memo_get(uint64_t i) { require(i < memo_.size() && memo_[(size_t)i], "invalid checkpoint memo reference"); push(memo_[(size_t)i]); }
    std::vector<int64_t> dimensions(const V& v) {
        require(v->kind == Kind::Sequence && v->items.size() <= 16, "invalid checkpoint dimensions");
        std::vector<int64_t> out;
        for (const V& d : v->items) { require(d->kind == Kind::Integer && d->integer >= 0, "negative or non-integer tensor dimension/stride"); out.push_back(d->integer); }
        return out;
    }
    void persistent() {
        V id = pop();
        require(id->kind == Kind::Sequence && id->items.size() == 5, "invalid tensor storage id");
        const auto& a = id->items;
        require(a[0]->kind == Kind::String && a[0]->text == "storage" && a[1]->kind == Kind::Global &&
                a[1]->text.compare(0, 6, "torch\n") == 0 && a[2]->kind == Kind::String &&
                a[3]->kind == Kind::String && a[4]->kind == Kind::Integer && a[4]->integer >= 0,
                "unsupported tensor storage id");
        require(!a[2]->text.empty() && a[2]->text.find_first_not_of("0123456789") == std::string::npos,
                "invalid tensor storage key");
        V v = make(Kind::Storage); v->tensor.storage = a[2]->text;
        v->tensor.dtype = a[1]->text.substr(6); storage_width(v->tensor.dtype);
        v->tensor.storage_elements = (uint64_t)a[4]->integer; push(v);
    }
    void reduce() {
        V args = pop(), call = pop();
        require(call->kind == Kind::Global && args->kind == Kind::Sequence, "invalid pickle REDUCE");
        const auto& a = args->items;
        if (call->text == "collections\nOrderedDict") {
            require(a.empty(), "unsupported OrderedDict constructor"); push(make(Kind::Dict)); return;
        }
        if (call->text == "torch._utils\n_rebuild_parameter") {
            require(a.size() == 3 && a[0]->kind == Kind::Tensor && a[1]->kind == Kind::Integer && a[2]->kind == Kind::Dict,
                    "invalid checkpoint parameter"); push(a[0]); return;
        }
        require(call->text == "torch._utils\n_rebuild_tensor" || call->text == "torch._utils\n_rebuild_tensor_v2",
                "unsupported checkpoint reduction");
        const bool v2 = call->text == "torch._utils\n_rebuild_tensor_v2";
        require((!v2 && a.size() == 4) || (v2 && (a.size() == 6 || a.size() == 7)), "invalid tensor constructor");
        require(a[0]->kind == Kind::Storage && a[1]->kind == Kind::Integer && a[1]->integer >= 0,
                "invalid checkpoint tensor storage offset");
        if (v2) require(a[4]->kind == Kind::Integer && a[5]->kind == Kind::Dict && a[5]->pairs.empty(),
                        "unsupported checkpoint tensor hooks");
        if (a.size() == 7) require(a[6]->kind == Kind::Dict || a[6]->kind == Kind::None, "invalid tensor metadata");
        V v = make(Kind::Tensor); v->tensor = a[0]->tensor;
        v->tensor.offset = (uint64_t)a[1]->integer;
        v->tensor.shape = dimensions(a[2]); v->tensor.stride = dimensions(a[3]);
        require(v->tensor.shape.size() == v->tensor.stride.size(), "tensor shape/stride rank differs");
        const uint64_t n = elements(v->tensor.shape);
        uint64_t last = v->tensor.offset;
        for (size_t i = 0; n && i < v->tensor.shape.size(); ++i) {
            const uint64_t d = (uint64_t)v->tensor.shape[i] - 1, s = (uint64_t)v->tensor.stride[i];
            require(!d || s <= (std::numeric_limits<uint64_t>::max() - last) / d, "tensor span overflows");
            last += d * s;
        }
        require(n ? last < v->tensor.storage_elements : v->tensor.offset <= v->tensor.storage_elements,
                "tensor view exceeds its storage"); push(v);
    }
};

struct Archive {
    mz_zip_archive zip{};
    explicit Archive(const std::string& path) {
        NN_CHECK(mz_zip_reader_init_file(&zip, path.c_str(), 0), "%s: cannot open checkpoint ZIP", path.c_str());
    }
    ~Archive() { mz_zip_reader_end(&zip); }
    Archive(const Archive&) = delete;
    Archive& operator=(const Archive&) = delete;
    std::vector<uint8_t> read(mz_uint index, uint64_t limit, const std::string& path) {
        mz_zip_archive_file_stat st{};
        NN_CHECK(mz_zip_reader_file_stat(&zip, index, &st) && st.m_uncomp_size <= limit &&
                 st.m_uncomp_size <= std::numeric_limits<size_t>::max(), "%s: oversized checkpoint ZIP member", path.c_str());
        std::vector<uint8_t> bytes((size_t)st.m_uncomp_size);
        NN_CHECK(mz_zip_reader_extract_to_mem(&zip, index, bytes.data(), bytes.size(), 0),
                 "%s: corrupt checkpoint ZIP member '%s'", path.c_str(), st.m_filename);
        return bytes;
    }
};

template<class T> T raw_value(const uint8_t* p) { T v; std::memcpy(&v, p, sizeof v); return v; }

float decode(const uint8_t* p, const std::string& type) {
    if (type == "FloatStorage") return raw_value<float>(p);
    if (type == "HalfStorage") return half_to_float(raw_value<uint16_t>(p));
    if (type == "BFloat16Storage") { const uint32_t bits = (uint32_t)raw_value<uint16_t>(p) << 16; return raw_value<float>((const uint8_t*)&bits); }
    if (type == "DoubleStorage") return (float)raw_value<double>(p);
    if (type == "LongStorage") return (float)raw_value<int64_t>(p);
    if (type == "IntStorage") return (float)raw_value<int32_t>(p);
    if (type == "ShortStorage") return (float)raw_value<int16_t>(p);
    if (type == "CharStorage") return (float)raw_value<int8_t>(p);
    return (float)*p;
}

// The numbers of a config dict beside the weights, by dotted path. A list keeps
// its None entries as NaN, so positions still line up.
void flatten_config(const V& v, const std::string& key,
                    std::map<std::string, std::vector<double>>& out) {
    auto number = [](const V& x, double* d) {
        if (x->kind == Kind::Integer) { *d = (double)x->integer; return true; }
        if (x->kind == Kind::Real) { *d = x->real; return true; }
        if (x->kind == Kind::None) { *d = std::nan(""); return true; }
        return false;
    };
    double d = 0;
    if (v->kind == Kind::Dict) {
        for (const auto& p : v->pairs) flatten_config(p.second, key + "." + p.first->text, out);
    } else if (v->kind == Kind::Sequence) {
        std::vector<double> list;
        for (const V& x : v->items) {
            if (!number(x, &d)) return;
            list.push_back(d);
        }
        out[key] = std::move(list);
    } else if (v->kind != Kind::None && number(v, &d)) {
        out[key] = {d};
    }
}

}  // namespace

struct TorchCheckpoint::Impl {
    std::string path;
    std::map<std::string, Entry> entries;
    std::map<std::string, mz_uint> storages;
    std::map<std::string, std::vector<double>> config;
};

TorchCheckpoint::TorchCheckpoint(const std::string& path) : impl_(new Impl) {
    impl_->path = path;
    Archive archive(path);
    std::string prefix;
    mz_uint pickle_index = 0;
    bool found = false;
    std::map<std::string, mz_uint> members;
    const mz_uint files = mz_zip_reader_get_num_files(&archive.zip);
    NN_CHECK(files <= kObjectLimit, "%s: too many checkpoint ZIP members", path.c_str());
    for (mz_uint i = 0; i < files; ++i) {
        mz_zip_archive_file_stat st{};
        NN_CHECK(mz_zip_reader_file_stat(&archive.zip, i, &st), "%s: invalid checkpoint ZIP directory", path.c_str());
        const std::string name = st.m_filename;
        NN_CHECK(members.emplace(name, i).second, "%s: duplicate checkpoint ZIP member", path.c_str());
        if (name == "data.pkl" || (name.size() > 9 && name.compare(name.size() - 9, 9, "/data.pkl") == 0)) {
            NN_CHECK(!found, "%s: more than one checkpoint pickle", path.c_str());
            found = true; pickle_index = i; prefix = name.substr(0, name.size() - 8);
        }
    }
    NN_CHECK(found, "%s: checkpoint has no data.pkl", path.c_str());
    auto byteorder = members.find(prefix + "byteorder");
    if (byteorder != members.end()) {
        const auto bytes = archive.read(byteorder->second, 16, path);
        NN_CHECK(std::string(bytes.begin(), bytes.end()) == "little", "%s: only little-endian checkpoints are supported", path.c_str());
    }
    const auto bytes = archive.read(pickle_index, kMetadataLimit, path);
    V root = Pickle(bytes, path).read();
    NN_CHECK(root->kind == Kind::Dict, "%s: checkpoint root is not a state dictionary", path.c_str());
    // {"state_dict": ...} and MoGe's {"model": ..., "model_config": ...} wrap the
    // weights; whatever sits beside them is a config, kept for config().
    V state = root;
    for (const auto& p : root->pairs)
        if ((p.first->text == "state_dict" || p.first->text == "model") &&
            p.second->kind == Kind::Dict) { state = p.second; break; }
    if (state != root)
        for (const auto& p : root->pairs)
            if (p.second != state) flatten_config(p.second, p.first->text, impl_->config);
    root = state;
    for (const auto& p : root->pairs) {
        NN_CHECK(p.second->kind == Kind::Tensor, "%s: state dictionary entry '%s' is not a tensor", path.c_str(), p.first->text.c_str());
        Entry e = p.second->tensor;
        auto storage = members.find(prefix + "data/" + e.storage);
        NN_CHECK(storage != members.end(), "%s: missing tensor storage '%s'", path.c_str(), e.storage.c_str());
        mz_zip_archive_file_stat st{};
        mz_zip_reader_file_stat(&archive.zip, storage->second, &st);
        const uint32_t width = storage_width(e.dtype);
        NN_CHECK(e.storage_elements <= std::numeric_limits<uint64_t>::max() / width &&
                 st.m_uncomp_size == e.storage_elements * width,
                 "%s: tensor '%s' storage size differs from metadata", path.c_str(), p.first->text.c_str());
        impl_->storages[e.storage] = storage->second;
        impl_->entries.emplace(p.first->text, std::move(e));
    }
    NN_CHECK(!impl_->entries.empty(), "%s: empty checkpoint state dictionary", path.c_str());
}

TorchCheckpoint::~TorchCheckpoint() = default;
TorchCheckpoint::TorchCheckpoint(TorchCheckpoint&&) noexcept = default;
TorchCheckpoint& TorchCheckpoint::operator=(TorchCheckpoint&&) noexcept = default;
const std::string& TorchCheckpoint::path() const { return impl_->path; }
bool TorchCheckpoint::has(const std::string& name) const { return impl_->entries.count(name) != 0; }
const TorchCheckpoint::Entry& TorchCheckpoint::entry(const std::string& name) const {
    const auto it = impl_->entries.find(name);
    NN_CHECK(it != impl_->entries.end(), "%s: no tensor '%s'", path().c_str(), name.c_str());
    return it->second;
}
std::vector<double> TorchCheckpoint::config(const std::string& key) const {
    const auto it = impl_->config.find(key);
    return it == impl_->config.end() ? std::vector<double>{} : it->second;
}
std::vector<std::string> TorchCheckpoint::names() const {
    std::vector<std::string> out;
    out.reserve(impl_->entries.size());
    for (const auto& e : impl_->entries) out.push_back(e.first);
    return out;
}
OnnxTensor TorchCheckpoint::read(const std::string& name) const {
    const Entry& e = entry(name);
    const uint64_t n = elements(e.shape);
    NN_CHECK(n <= std::numeric_limits<size_t>::max() / sizeof(float), "%s: tensor too large for host", path().c_str());
    const uint32_t width = storage_width(e.dtype);
    Archive archive(path());
    const auto bytes = archive.read(impl_->storages.at(e.storage), e.storage_elements * width, path());
    OnnxTensor out;
    out.name = name; out.shape = e.shape; out.was_f16 = e.dtype == "HalfStorage";
    out.data.resize((size_t)n);
    for (uint64_t i = 0; i < n; ++i) {
        uint64_t index = e.offset, remaining = i;
        for (size_t axis = e.shape.size(); axis-- > 0;) {
            index += (remaining % (uint64_t)e.shape[axis]) * (uint64_t)e.stride[axis];
            remaining /= (uint64_t)e.shape[axis];
        }
        out.data[(size_t)i] = decode(bytes.data() + (size_t)(index * width), e.dtype);
    }
    return out;
}

}  // namespace nn
