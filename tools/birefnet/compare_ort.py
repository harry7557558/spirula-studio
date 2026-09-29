#!/usr/bin/env python3
"""Compare src/birefnet/'s forward pass against onnxruntime.

The onnx-community exports carry the same weights as ZhengPeng7's safetensors
(checked tensor by tensor), so a disagreement is ours -- with one exception
the README describes: the exports and the checkpoints use the pre-einops patch
order, which is what we run too.

    pip install onnx onnxruntime numpy pillow
    python3 tools/birefnet/compare_ort.py --onnx BiRefNet_lite-ONNX/onnx/model.onnx \
        --image shoe.jpg --write-input /tmp/in.npy
    SS_NN_COOPMAT=0 SS_BIREFNET_F32_WEIGHTS=1 SS_BIREFNET_DUMP=/tmp/ours \
        ./build_vulkan/birefnet_test --model birefnet-lite --npy /tmp/in.npy \
        --logits /tmp/ours/logits.npy
    python3 tools/birefnet/compare_ort.py --onnx ... --input /tmp/in.npy --ours /tmp/ours

Measured on BiRefNet_lite: 3e-6 relative L2 on the logits with f32 weights and
no cooperative matrix, 7e-4 as shipped (f16 weights), masks identical.
"""
import argparse
import os

import numpy as np

# Our stage dumps -> the export's tensors. Ours are channel-last.
STAGES = {
    "squeezed": "/squeeze_module/squeeze_module.0/conv_out/Conv_output_0",
    "dec4": "/decoder/decoder_block4/conv_out/Conv_output_0",
    "dec3": "/decoder/decoder_block3/conv_out/Conv_output_0",
    "dec2": "/decoder/decoder_block2/conv_out/Conv_output_0",
    "d1": "/decoder/decoder_block1/conv_out/Conv_output_0",
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--onnx", required=True)
    ap.add_argument("--image")
    ap.add_argument("--write-input")
    ap.add_argument("--input")
    ap.add_argument("--ours")
    ap.add_argument("--size", type=int, default=1024)
    a = ap.parse_args()

    if a.image:
        from PIL import Image
        mean = np.array([0.485, 0.456, 0.406], np.float32)
        std = np.array([0.229, 0.224, 0.225], np.float32)
        im = Image.open(a.image).convert("RGB").resize((a.size, a.size), Image.BILINEAR)
        x = ((np.asarray(im, np.float32) / 255.0 - mean) / std).transpose(2, 0, 1)[None]
        np.save(a.write_input or "/tmp/birefnet_in.npy", x.astype(np.float32))
        print("wrote", a.write_input or "/tmp/birefnet_in.npy")
        return

    import onnx
    import onnxruntime as ort
    m = onnx.load(a.onnx)
    names = ["output_image"] + [v for k, v in STAGES.items()
                                if os.path.exists(os.path.join(a.ours, k + ".npy"))]
    del m.graph.output[:]
    for n in names:
        m.graph.output.append(onnx.helper.make_tensor_value_info(n, onnx.TensorProto.FLOAT, None))
    s = ort.InferenceSession(m.SerializeToString(), providers=["CPUExecutionProvider"])
    outs = dict(zip(names, s.run(None, {"input_image": np.load(a.input)})))

    def report(name, ours, ref):
        rel = np.linalg.norm(ours - ref) / max(np.linalg.norm(ref), 1e-12)
        print(f"{name:10s} relL2 {rel:.3g}  max |d| {np.abs(ours - ref).max():.3g}")

    for k, v in STAGES.items():
        f = os.path.join(a.ours, k + ".npy")
        if os.path.exists(f):
            report(k, np.load(f).transpose(2, 0, 1)[None], outs[v])
    lg = np.load(os.path.join(a.ours, "logits.npy"))
    report("logits", lg, outs["output_image"])
    print("mask agreement", ((lg > 0) == (outs["output_image"] > 0)).mean())


if __name__ == "__main__":
    main()
