// Fails when a compute entry point in any embedded SPIR-V blob exceeds the
// portable device limits: 32 KB of workgroup memory or 128 bytes of push
// constants. The blobs come from the listfiles ss_embed_spirv() records
// (cmake/SsSlang.cmake), so every embedded shader set is covered.
//
//   shader_limits_test [<index of listfiles>]

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace {

constexpr uint64_t kMaxShared = 32768;
constexpr uint64_t kMaxPush = 128;

enum : uint32_t {
    OpEntryPoint = 15, OpTypeBool = 20, OpTypeInt = 21, OpTypeFloat = 22,
    OpTypeVector = 23, OpTypeMatrix = 24, OpTypeArray = 28,
    OpTypeRuntimeArray = 29, OpTypeStruct = 30, OpTypePointer = 32,
    OpConstantTrue = 41, OpConstantFalse = 42, OpConstant = 43,
    OpSpecConstantTrue = 48, OpSpecConstantFalse = 49, OpSpecConstant = 50, OpSpecConstantOp = 52,
    OpVariable = 59, OpDecorate = 71, OpMemberDecorate = 72,
    DecArrayStride = 6, DecMatrixStride = 7, DecOffset = 35,
    ScWorkgroup = 4, ScPushConstant = 9, ExecGLCompute = 5,
};

struct Type {
    uint32_t op = 0;
    std::vector<uint32_t> ops;  // operands after the result id
};

struct Layout {
    uint64_t size = 0, align = 1;
    bool known = true;  // false: an array length this parser cannot evaluate
};

// An OpSpecConstantOp over integers, at the constants' default values: the
// host raises a size-driving spec constant only on devices that fit it.
bool eval_spec_op(uint32_t op, const std::vector<int64_t>& v, int64_t* out) {
    auto need = [&](size_t n) { return v.size() == n; };
    switch (op) {
    case 113: case 114: if (!need(1)) return false; *out = v[0]; return true;
    case 126: if (!need(1)) return false; *out = -v[0]; return true;
    case 200: if (!need(1)) return false; *out = ~v[0]; return true;
    case 168: if (!need(1)) return false; *out = !v[0]; return true;
    case 169: if (!need(3)) return false; *out = v[0] ? v[1] : v[2]; return true;
    }
    if (!need(2)) return false;
    int64_t a = v[0], b = v[1];
    switch (op) {
    case 128: *out = a + b; return true;
    case 130: *out = a - b; return true;
    case 132: *out = a * b; return true;
    case 134: case 135: if (!b) return false; *out = a / b; return true;
    case 137: case 138: case 139: if (!b) return false; *out = a % b; return true;
    case 194: *out = (int64_t)((uint64_t)a >> b); return true;
    case 195: *out = a >> b; return true;
    case 196: *out = a << b; return true;
    case 197: *out = a | b; return true;
    case 198: *out = a ^ b; return true;
    case 199: *out = a & b; return true;
    case 164: case 170: *out = a == b; return true;
    case 165: case 171: *out = a != b; return true;
    case 166: *out = a || b; return true;
    case 167: *out = a && b; return true;
    case 172: *out = (uint64_t)a > (uint64_t)b; return true;
    case 173: *out = a > b; return true;
    case 174: *out = (uint64_t)a >= (uint64_t)b; return true;
    case 175: *out = a >= b; return true;
    case 176: *out = (uint64_t)a < (uint64_t)b; return true;
    case 177: *out = a < b; return true;
    case 178: *out = (uint64_t)a <= (uint64_t)b; return true;
    case 179: *out = a <= b; return true;
    }
    return false;
}

struct Module {
    std::map<uint32_t, Type> types;
    std::map<uint32_t, int64_t> constants;
    std::map<uint32_t, uint32_t> array_stride, matrix_stride;
    std::map<uint32_t, std::map<uint32_t, uint32_t>> member_offset;
    std::map<uint32_t, std::pair<uint32_t, uint32_t>> vars;  // id -> (sc, pointee)
    struct Entry { std::string name; std::vector<uint32_t> iface; };
    std::vector<Entry> entries;

    Layout layout(uint32_t id) const {
        auto it = types.find(id);
        if (it == types.end()) return {};
        const Type& t = it->second;
        switch (t.op) {
        case OpTypeBool: return {4, 4, true};
        case OpTypeInt:
        case OpTypeFloat: return {t.ops[0] / 8u, t.ops[0] / 8u, true};
        case OpTypePointer: return {8, 8, true};
        case OpTypeVector: {
            Layout c = layout(t.ops[0]);
            uint64_t n = t.ops[1];
            return {c.size * n, c.size * (n == 3 ? 4 : n), c.known};
        }
        case OpTypeMatrix: {
            Layout col = layout(t.ops[0]);
            auto s = matrix_stride.find(id);
            uint64_t stride = s != matrix_stride.end() ? s->second : col.align;
            return {stride * t.ops[1], col.align, col.known};
        }
        case OpTypeArray: {
            Layout e = layout(t.ops[0]);
            auto n = constants.find(t.ops[1]);
            auto s = array_stride.find(id);
            uint64_t stride = s != array_stride.end()
                ? s->second : (e.size + e.align - 1) / e.align * e.align;
            if (n == constants.end() || n->second < 0) return {0, e.align, false};
            return {stride * (uint64_t)n->second, e.align, e.known};
        }
        case OpTypeRuntimeArray: return {0, layout(t.ops[0]).align, true};
        case OpTypeStruct: {
            auto offs = member_offset.find(id);
            Layout r;
            for (uint32_t m = 0; m < t.ops.size(); m++) {
                Layout ml = layout(t.ops[m]);
                uint64_t off = (r.size + ml.align - 1) / ml.align * ml.align;
                if (offs != member_offset.end() && offs->second.count(m))
                    off = offs->second.at(m);
                r.size = std::max(r.size, off + ml.size);
                r.align = std::max(r.align, ml.align);
                r.known = r.known && ml.known;
            }
            return r;
        }
        }
        return {};
    }
};

bool parse(const std::vector<uint32_t>& w, Module& m) {
    if (w.size() < 5 || w[0] != 0x07230203u) return false;
    for (size_t i = 5; i < w.size();) {
        uint32_t op = w[i] & 0xffff, n = w[i] >> 16;
        if (n == 0 || i + n > w.size()) return false;
        const uint32_t* a = &w[i + 1];
        switch (op) {
        case OpEntryPoint: {
            if (a[0] != ExecGLCompute) break;
            Module::Entry e;
            size_t k = 2;
            for (; k < n - 1; k++) {
                uint32_t word = a[k];
                bool end = false;
                for (int b = 0; b < 4; b++) {
                    char c = (char)((word >> (8 * b)) & 0xff);
                    if (!c) { end = true; break; }
                    e.name += c;
                }
                if (end) { k++; break; }
            }
            e.iface.assign(a + k, a + n - 1);
            m.entries.push_back(e);
            break;
        }
        case OpTypeBool: case OpTypeInt: case OpTypeFloat: case OpTypeVector:
        case OpTypeMatrix: case OpTypeArray: case OpTypeRuntimeArray:
        case OpTypeStruct: case OpTypePointer:
            m.types[a[0]] = {op, std::vector<uint32_t>(a + 1, a + n - 1)};
            break;
        case OpConstantTrue: case OpSpecConstantTrue: m.constants[a[1]] = 1; break;
        case OpConstantFalse: case OpSpecConstantFalse: m.constants[a[1]] = 0; break;
        case OpConstant: case OpSpecConstant: {
            if (n < 4) break;
            auto t = m.types.find(a[0]);
            bool is_signed = t != m.types.end() && t->second.op == OpTypeInt &&
                             t->second.ops[1] == 1;
            m.constants[a[1]] = is_signed ? (int64_t)(int32_t)a[2] : (int64_t)a[2];
            break;
        }
        case OpSpecConstantOp: {
            std::vector<int64_t> v;
            for (uint32_t k = 3; k < n - 1; k++) {
                auto c = m.constants.find(a[k]);
                if (c == m.constants.end()) { v.clear(); break; }
                v.push_back(c->second);
            }
            int64_t r;
            if (!v.empty() && eval_spec_op(a[2], v, &r)) m.constants[a[1]] = r;
            break;
        }
        case OpVariable: {
            auto p = m.types.find(a[0]);
            if (p != m.types.end()) m.vars[a[1]] = {a[2], p->second.ops[1]};
            break;
        }
        case OpDecorate:
            if (a[1] == DecArrayStride) m.array_stride[a[0]] = a[2];
            break;
        case OpMemberDecorate:
            if (a[2] == DecOffset) m.member_offset[a[0]][a[1]] = a[3];
            if (a[2] == DecMatrixStride) m.matrix_stride[a[0]] = a[3];
            break;
        }
        i += n;
    }
    return true;
}

std::vector<std::string> read_lines(const std::string& path) {
    std::vector<std::string> out;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) out.push_back(line);
    }
    return out;
}

std::string stem(const std::string& path) {
    size_t s = path.find_last_of("/\\");
    std::string name = s == std::string::npos ? path : path.substr(s + 1);
    if (name.size() > 4 && name.compare(name.size() - 4, 4, ".spv") == 0)
        name.resize(name.size() - 4);
    return name;
}

}  // namespace

int main(int argc, char** argv) {
    std::string index = argc > 1 ? argv[1] : SS_SPIRV_LISTFILES;
    std::vector<std::string> lists = read_lines(index);
    if (lists.empty()) {
        std::printf("FAIL: no listfiles in %s\n", index.c_str());
        return 1;
    }

    int failures = 0;
    size_t blobs = 0, entries = 0;
    uint64_t max_shared = 0, max_push = 0;
    for (const std::string& list : lists) {
        for (const std::string& path : read_lines(list)) {
            std::ifstream f(path, std::ios::binary);
            std::vector<char> bytes((std::istreambuf_iterator<char>(f)),
                                    std::istreambuf_iterator<char>());
            std::vector<uint32_t> words(bytes.size() / 4);
            for (size_t i = 0; i < words.size(); i++)
                words[i] = (uint32_t)(uint8_t)bytes[4 * i] |
                           ((uint32_t)(uint8_t)bytes[4 * i + 1] << 8) |
                           ((uint32_t)(uint8_t)bytes[4 * i + 2] << 16) |
                           ((uint32_t)(uint8_t)bytes[4 * i + 3] << 24);
            Module m;
            if (!parse(words, m) || m.entries.empty()) {
                std::printf("FAIL %s: unreadable or no compute entry point\n",
                            path.c_str());
                failures++;
                continue;
            }
            blobs++;
            for (const Module::Entry& e : m.entries) {
                entries++;
                uint64_t shared = 0, push = 0;
                bool known = true;
                for (uint32_t id : e.iface) {
                    auto v = m.vars.find(id);
                    if (v == m.vars.end()) continue;
                    Layout l = m.layout(v->second.second);
                    if (v->second.first == ScWorkgroup || v->second.first == ScPushConstant)
                        known = known && l.known;
                    if (v->second.first == ScWorkgroup)
                        shared = (shared + l.align - 1) / l.align * l.align + l.size;
                    else if (v->second.first == ScPushConstant)
                        push += l.size;
                }
                max_shared = std::max(max_shared, shared);
                max_push = std::max(max_push, push);
                if (!known) {
                    std::printf("FAIL %s.%s: an array length is not a constant "
                                "this test can evaluate\n",
                                stem(path).c_str(), e.name.c_str());
                    failures++;
                } else if (shared > kMaxShared || push > kMaxPush) {
                    std::printf("FAIL %s.%s: shared %llu B, push %llu B\n",
                                stem(path).c_str(), e.name.c_str(),
                                (unsigned long long)shared, (unsigned long long)push);
                    failures++;
                }
            }
        }
    }

    // A parser that silently measured nothing would pass everything.
    if (max_shared == 0 || max_push == 0) {
        std::printf("FAIL: measured no shared memory or push constants at all\n");
        failures++;
    }
    std::printf("%zu blobs, %zu entry points; largest shared %llu B, push %llu B\n",
                blobs, entries, (unsigned long long)max_shared,
                (unsigned long long)max_push);
    std::printf("%s (%d failures)\n", failures ? "FAILED" : "PASSED", failures);
    return failures ? 1 : 0;
}
