#!/usr/bin/env python3
"""Marigold v2 in PyTorch, one transformer block on the GPU at a time.

What src/marigold/ is checked against. diffusers' own QwenImageTransformerBlock
runs each block; its weights are the original bf16 Qwen-Image-Edit-2509 shards
put through bitsandbytes' NF4 round trip, which is the base Marigold v2's LoRA
was trained on. Needs the original checkpoints (~45 GB), not our converted ones.

    pip install torch diffusers==0.38.0 bitsandbytes==0.49.2 safetensors numpy
    SS_MARIGOLD_DUMP=/tmp/ours ./build_vulkan/marigold_test --image IMG --task depth
    python3 tools/marigold/reference.py --qwen <Qwen-Image-Edit-2509 dir> \
        --marigold <marigold-v2-0 dir> --task depth --rgb /tmp/ours/rgb_in.npy \
        --out /tmp/ref
    python3 tools/marigold/reference.py --compare /tmp/ref --ours /tmp/ours

The default --mode fp32 merges the LoRA and is the tight check; --mode autocast
runs the way Marigold v2's own inference does (bf16 autocast, LoRA unmerged).
The constants -- timestep embedding, modulation, the text stream's input -- are
always computed with bitsandbytes' own Linear4bit under autocast, the numerics
the LoRA was trained with and the converter reproduces.
"""
import argparse
import json
import os
import sys

import numpy as np
import torch
import torch.nn.functional as F

DIM, HEADS, HEAD_DIM, LAYERS = 3072, 24, 128, 60
TASKS = {
    "depth": ("depth/Log-stage2", "qwen_edit_2509_qwen_depth_realimg512"),
    "normals": ("normals", "qwen_edit_2509_qwen_normals_dummy512"),
}
# bitsandbytes' llm_int8_skip_modules in Marigold v2's loader: kept bf16.
NOT_QUANTIZED = {"transformer_blocks.0.img_mod.1"}


def nf4_roundtrip(w):
    from bitsandbytes.functional import dequantize_4bit, quantize_4bit
    q, st = quantize_4bit(w.cuda(), blocksize=64, quant_type="nf4", compress_statistics=False)
    return dequantize_4bit(q, st)


class Checkpoint:
    def __init__(self, qwen, marigold, task):
        from safetensors import safe_open
        tdir = os.path.join(qwen, "transformer")
        index = json.load(open(os.path.join(tdir, "diffusion_pytorch_model.safetensors.index.json")))
        self.where = index["weight_map"]
        self.tdir = tdir
        self.open = {}
        sub, prefix = TASKS[task]
        tr = safe_open(os.path.join(marigold, sub, "trainables.safetensors"), "pt", device="cpu")
        self.lora = {k: tr.get_tensor(k) for k in tr.keys() if k.startswith("Diffuser.")}
        self.vae_trained = {k[4:]: tr.get_tensor(k) for k in tr.keys() if k.startswith("VAE.")}
        emb = os.path.join(marigold, "qwen_text_embeddings", prefix)
        self.prompt = torch.load(emb + "_prompt_embeds.pt", map_location="cpu")[:1]
        mask = torch.load(emb + "_prompt_mask.pt", map_location="cpu")[:1]
        assert bool(mask.all()), "a padded prompt context needs the attention mask"

    def get(self, name):
        from safetensors import safe_open
        f = self.where[name]
        if f not in self.open:
            self.open[f] = safe_open(os.path.join(self.tdir, f), "pt", device="cpu")
        return self.open[f].get_tensor(name)


class RefLinear(torch.nn.Module):
    def __init__(self, ck, name, merged):
        super().__init__()
        w = ck.get(name + ".weight")
        w = w.cuda() if name in NOT_QUANTIZED else nf4_roundtrip(w)
        self.bias = ck.get(name + ".bias").cuda()
        a = ck.lora.get("Diffuser." + name + ".lora_A.default.weight")
        b = ck.lora.get("Diffuser." + name + ".lora_B.default.weight")
        self.a = a.cuda() if a is not None else None
        self.b = b.cuda() if b is not None else None
        self.merged = merged
        if merged:
            self.weight = w.float()
            if self.a is not None:
                self.weight += self.b.float() @ self.a.float()
            self.bias = self.bias.float()
            self.a = self.b = None
        else:
            self.weight = w

    def forward(self, x):
        y = F.linear(x, self.weight, self.bias)
        if self.a is not None:
            y = y + F.linear(F.linear(x, self.a), self.b)
        return y


def official_linear(ck, name):
    """bitsandbytes Linear4bit plus a peft-style LoRA, to run under autocast."""
    import bitsandbytes as bnb
    w, b = ck.get(name + ".weight"), ck.get(name + ".bias")
    if name in NOT_QUANTIZED:
        lin = torch.nn.Linear(w.shape[1], w.shape[0])
        lin.weight.data, lin.bias.data = w.float(), b.float()
    else:
        lin = bnb.nn.Linear4bit(w.shape[1], w.shape[0], bias=True, compute_dtype=torch.bfloat16,
                                compress_statistics=False, quant_type="nf4")
        lin.weight = bnb.nn.Params4bit(w, requires_grad=False, quant_type="nf4", blocksize=64,
                                       compress_statistics=False)
        lin.bias = torch.nn.Parameter(b.float(), requires_grad=False)
    lin = lin.cuda()
    a = ck.lora.get("Diffuser." + name + ".lora_A.default.weight")
    up = ck.lora.get("Diffuser." + name + ".lora_B.default.weight")

    def forward(x):
        y = lin(x)
        if a is not None:
            y = y + F.linear(F.linear(x.to(torch.bfloat16), a.cuda()), up.cuda())
        return y
    return forward


class Constant(torch.nn.Module):
    def __init__(self, value):
        super().__init__()
        self.value = value

    def forward(self, *args):
        return self.value


def autocast():
    return torch.autocast("cuda", dtype=torch.bfloat16)


def timestep_embedding(ck):
    from diffusers.models.embeddings import Timesteps
    l1 = official_linear(ck, "time_text_embed.timestep_embedder.linear_1")
    l2 = official_linear(ck, "time_text_embed.timestep_embedder.linear_2")
    with autocast():
        t = torch.full((1,), 499.0, device="cuda", dtype=torch.bfloat16) / 1000.0
        proj = Timesteps(256, flip_sin_to_cos=True, downscale_freq_shift=0, scale=1000)(t)
        return l2(F.silu(l1(proj.to(torch.bfloat16))))


def make_block(ck, i, temb, merged, dumps):
    from diffusers.models.transformers.transformer_qwenimage import QwenImageTransformerBlock
    p = f"transformer_blocks.{i}."
    with torch.device("meta"):
        blk = QwenImageTransformerBlock(DIM, HEADS, HEAD_DIM)
    blk.to_empty(device="cuda")
    for n in ("norm_q", "norm_k", "norm_added_q", "norm_added_k"):
        getattr(blk.attn, n).weight.data = ck.get(p + "attn." + n + ".weight").float().cuda()
    with autocast():
        for s in ("img", "txt"):
            mod = official_linear(ck, p + s + "_mod.1")(F.silu(temb))
            dumps.setdefault("mod_" + s, []).append(mod.float().reshape(6, DIM))
            setattr(blk, s + "_mod", Constant(mod if not merged else mod.float()))
    a = blk.attn
    for n in ("to_q", "to_k", "to_v", "add_q_proj", "add_k_proj", "add_v_proj", "to_add_out"):
        setattr(a, n, RefLinear(ck, p + "attn." + n, merged))
    a.to_out[0] = RefLinear(ck, p + "attn.to_out.0", merged)
    for s in ("img", "txt"):
        mlp = getattr(blk, s + "_mlp")
        mlp.net[0].proj = RefLinear(ck, p + s + "_mlp.net.0.proj", merged)
        mlp.net[2] = RefLinear(ck, p + s + "_mlp.net.2", merged)
    return blk


def load_vae(qwen, ck, dtype):
    from diffusers import AutoencoderKLQwenImage
    vae = AutoencoderKLQwenImage.from_pretrained(qwen, subfolder="vae", torch_dtype=dtype)
    missing, unexpected = vae.load_state_dict(ck.vae_trained, strict=False)
    assert not unexpected, unexpected
    assert not [m for m in missing if m.startswith("decoder.") or m.startswith("post_quant")]
    return vae.cuda().eval()


def run(args):
    torch.backends.cuda.matmul.allow_tf32 = False
    torch.backends.cudnn.allow_tf32 = False
    # cuBLAS's bf16 split reductions differ by GPU; without them the constants
    # are exact arithmetic, which is what the converter reproduces.
    torch.backends.cuda.matmul.allow_bf16_reduced_precision_reduction = False
    merged = args.mode == "fp32"
    act = torch.float32 if merged else torch.bfloat16
    ck = Checkpoint(args.qwen, args.marigold, args.task)
    os.makedirs(args.out, exist_ok=True)

    def dump(name, t):
        np.save(os.path.join(args.out, name + ".npy"), t.detach().float().cpu().numpy())

    rgb = torch.from_numpy(np.load(args.rgb)).float()           # [H, W, 3] in [0, 1]
    H, W = rgb.shape[:2]
    assert H % 16 == 0 and W % 16 == 0
    h, w = H // 8, W // 8
    vae = load_vae(args.qwen, ck, act)
    mean = torch.tensor(vae.config.latents_mean, device="cuda").view(1, 16, 1, 1, 1)
    std = torch.tensor(vae.config.latents_std, device="cuda").view(1, 16, 1, 1, 1)
    with torch.no_grad(), autocast() if not merged else torch.autocast("cuda", enabled=False):
        x = (rgb.permute(2, 0, 1)[None, :, None].cuda() * 2 - 1).to(act)
        post = vae.encode(x).latent_dist
        mu = post.mean.float()
        dump("mu", mu[0, :, 0].permute(1, 2, 0))
        print(f"posterior std: median {post.std.float().median().item():.3g}, "
              f"max {post.std.float().max().item():.3g}")
        z = (mu - mean) / std
        tokens = z[:, :, 0].view(1, 16, h // 2, 2, w // 2, 2).permute(0, 2, 4, 1, 3, 5)
        tokens = tokens.reshape(1, (h // 2) * (w // 2), 64)
        dump("tokens_in", tokens[0])

    from diffusers.models.normalization import RMSNorm
    from diffusers.models.transformers.transformer_qwenimage import QwenEmbedRope
    dumps = {}
    with torch.no_grad():
        temb = timestep_embedding(ck)
        dump("temb", temb[0])
        txt_norm = RMSNorm(3584, eps=1e-6).cuda()
        txt_norm.weight.data = ck.get("txt_norm.weight").float().cuda()
        with autocast():
            txt = official_linear(ck, "txt_in")(txt_norm(ck.prompt.cuda()))
            emb = official_linear(ck, "norm_out.linear")(F.silu(temb).to(torch.bfloat16))
        dump("txt0", txt[0])
        scale, shift = emb.float().chunk(2, dim=1)
        dump("norm_out", torch.cat([scale, shift]))
        txt = txt.to(act)
        img = RefLinear(ck, "img_in", merged)(tokens.to(act))
        dump("img_in", img[0])
        rope = QwenEmbedRope(theta=10000, axes_dim=[16, 56, 56], scale_rope=True)
        freqs = rope([(1, h // 2, w // 2)], max_txt_seq_len=txt.shape[1], device="cuda")

        ctx = autocast() if not merged else torch.autocast("cuda", enabled=False)
        for i in range(LAYERS):
            blk = make_block(ck, i, temb, merged, dumps)
            with ctx:
                txt, img = blk(img, txt, None, temb, freqs)
            if i in args.dump_blocks:
                dump(f"block{i:02d}", img[0])
                dump(f"block{i:02d}_txt", txt[0])
            del blk
            print(f"\rblock {i + 1}/{LAYERS}", end="", flush=True)
        print()
        for s in ("img", "txt"):
            dump("mod_" + s, torch.stack(dumps["mod_" + s]))

        with ctx:
            out = F.layer_norm(img.float(), (DIM,), eps=1e-6)
            out = out * (1 + scale[:, None]) + shift[:, None]
            pred = RefLinear(ck, "proj_out", merged)(out.to(act))
        dump("pred", pred[0])
        pred = pred.float().view(1, h // 2, w // 2, 16, 2, 2).permute(0, 3, 1, 4, 2, 5)
        pred = pred.reshape(1, 16, 1, h, w)
        lat = (z - pred) * std + mean
        dump("latent_out", lat[0, :, 0].permute(1, 2, 0))
        with ctx:
            dec = vae.decode(lat.to(act)).sample.float()[0, :, 0]
        dump("decoded", dec.permute(1, 2, 0))
        if args.task == "depth":
            dump("depth_rel", dec.mean(0))
        else:
            n = dec / dec.norm(dim=0, keepdim=True).clamp_min(1e-6)
            dump("normal_raw", n.permute(1, 2, 0))


def compare(args):
    names = sorted(f[:-4] for f in os.listdir(args.ours) if f.endswith(".npy"))
    worst = 0.0
    for n in names:
        rp = os.path.join(args.compare, n + ".npy")
        if not os.path.exists(rp):
            continue
        a = np.load(os.path.join(args.ours, n + ".npy")).astype(np.float64)
        b = np.load(rp).astype(np.float64)
        if a.shape != b.shape:
            if a.size != b.size:
                print(f"  {n:14s} shape {a.shape} vs {b.shape}")
                continue
            a = a.reshape(b.shape)
        rel = np.linalg.norm(a - b) / max(np.linalg.norm(b), 1e-30)
        worst = max(worst, rel)
        print(f"  {n:14s} max |d| {np.abs(a - b).max():10.3e}   rel L2 {rel:9.3e}   "
              f"ref rms {np.sqrt((b * b).mean()):9.3e}")
    print(f"worst rel L2 {worst:.3e}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--qwen")
    ap.add_argument("--marigold")
    ap.add_argument("--task", choices=sorted(TASKS), default="depth")
    ap.add_argument("--rgb", help="[H, W, 3] float32 .npy in [0, 1], sides multiples of 16")
    ap.add_argument("--out")
    ap.add_argument("--mode", choices=("fp32", "autocast"), default="fp32")
    ap.add_argument("--dump-blocks", type=lambda s: [int(v) for v in s.split(",")],
                    default=[0, 1, 29, 59])
    ap.add_argument("--compare")
    ap.add_argument("--ours")
    args = ap.parse_args()
    if args.compare:
        return compare(args)
    if not (args.qwen and args.marigold and args.rgb and args.out):
        ap.error("--qwen, --marigold, --rgb and --out are required")
    run(args)


if __name__ == "__main__":
    sys.exit(main())
