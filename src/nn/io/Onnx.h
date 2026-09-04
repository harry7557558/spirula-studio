#pragma once
// Just enough ONNX to read a checkpoint: the initializers, and the epsilon of
// every BatchNormalization node.
//
// We do not host or convert weights. Each artifact fetched is byte-for-byte
// the one its publisher serves, so a parity check against the reference
// implementation compares two implementations of the same numbers rather than
// two checkpoints. The price is reading protobuf, and the price is small: the
// graph structure is hard-coded in each model's forward pass, so nothing here
// has to understand a node, an operator, a type or a shape inference rule. It
// walks three nested messages by field number and skips everything else by
// wire type.
//
//   ModelProto   field 7  -> GraphProto
//   GraphProto   field 5  -> repeated TensorProto   (initializers)
//                field 1  -> repeated NodeProto     (only for BN epsilon)
//   TensorProto  field 1 dims, 2 data_type, 8 name, 9 raw_data, 4 float_data,
//                      13 external_data, 14 data_location
//
// This is deliberately NOT a general ONNX reader. It will not run a model, and
// it rejects rather than guesses: an initializer in a dtype we do not convert,
// or a truncated payload, throws with the tensor's name in the message.

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace nn {

// One initializer, converted to f32 on the host. ONNX shapes are PyTorch order
// already, which is also nn::Tensor's, so the shape is carried verbatim.
struct OnnxTensor {
    std::string          name;
    std::vector<int64_t> shape;
    std::vector<float>   data;
    // The file stored this one as float16, so re-encoding it to F16 for the
    // device is lossless. The fp16 exports of a large model are the ones worth
    // fetching, and this is how a loader knows it is not throwing bits away.
    bool                 was_f16 = false;

    int64_t numel() const {
        int64_t n = 1;
        for (int64_t d : shape) n *= d;
        return n;
    }
    // "[16, 3, 3, 3]", for error messages.
    std::string shapeString() const;
};

// Just enough of a node to resolve an anonymous initializer to the module it
// belongs to. Torch lowers nn.Linear to MatMul + Add, and only the Add's bias
// keeps its qualified name -- the weight arrives as "onnx::MatMul_2537".
// Walking Add -> MatMul recovers the pairing.
struct OnnxNode {
    std::string              op_type;
    std::string              name;
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
};

// A graph input or output. `shape` carries -1 where the dimension is a
// symbolic name rather than a number, which is how a fixed input size the
// export baked in ("image" at [1, 3, 784, 784]) is told from a dynamic one.
struct OnnxValueInfo {
    std::string          name;
    std::vector<int64_t> shape;
};

struct OnnxFile {
    std::vector<OnnxTensor> initializers;
    std::vector<OnnxNode>   nodes;
    std::vector<OnnxValueInfo> inputs;
    std::vector<OnnxValueInfo> outputs;
    // Keyed by the *scale* initializer's name -- BatchNormalization's input 1,
    // which is the only name a caller folding BN into a conv already knows.
    // Absent means the node did not spell epsilon out, i.e. the ONNX default.
    std::unordered_map<std::string, float> bn_epsilon;

    const OnnxTensor* find(const std::string& name) const;

    // A graph input by name, or null. Initializers appear as graph inputs in
    // some exporters, so this is only trustworthy for the real ones.
    const OnnxValueInfo* input(const std::string& name) const;
    const OnnxValueInfo* output(const std::string& name) const;

    // The node that produces `tensor`, or null.
    const OnnxNode* producer(const std::string& tensor) const;

    // For every `<module>.bias` initializer consumed by an Add whose other
    // input comes from a MatMul, the name of that MatMul's weight. Keyed by
    // the module prefix, so "transformers.0.self_attn.Wqkv" maps to the
    // initializer holding its [in, out] matrix.
    std::unordered_map<std::string, std::string> linearWeights() const;
};

// Throws nn::Error naming `path` on anything malformed.
OnnxFile read_onnx(const std::string& path);

// The same file in two passes, for a checkpoint too big to hold decoded:
// Metric3D's giant2 is 2.75 GB of f16 initializers, which is 5.5 GB once
// converted, and `read_onnx` would hold all of it at once. `structure` skips
// every payload; `initializers` decodes one tensor at a time and hands each to
// `sink`, which is expected to upload it and let it go.
OnnxFile read_onnx_structure(const std::string& path);
void     read_onnx_initializers(const std::string& path,
                                const std::function<void(OnnxTensor&&)>& sink);

}  // namespace nn
