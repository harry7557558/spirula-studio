// End-to-end pipeline test against a SYNTHETIC checkpoint.
//
// It writes a real, format-correct SAM 3 model file -- every tensor the modules
// look up, at a shrunken but structurally identical configuration -- then runs
// the whole library over it: concept segmentation, visual segmentation, and
// three frames of video tracking.
//
// What this proves, without needing the 1.7 GB release weights:
//   * every W.get() in every module resolves, so no weight name has drifted;
//   * every shape lines up end to end, including the ones derived from the
//     hyperparameters (mask resolution, FFN widths, head dims, RoPE table
//     lengths);
//   * the arena's scoping is sound -- a tensor that outlives its rewind shows
//     up here as garbage or a crash;
//   * the tracker's memory bank, eviction and association logic run.
//
// What it cannot prove: numerical fidelity to the reference. The weights are
// noise, so the outputs are meaningless; only their finiteness is checked.
// nn/tests/nn_ops_test.cpp is where the arithmetic is verified.

#include "nn/core/Half.h"
#include "nn/core/Log.h"
#include "nn/Device.h"
#include "sam/Masking.h"
#include "sam/Sam.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const char* what) {
    std::printf("  %-4s %s\n", ok ? "ok" : "FAIL", what);
    if (!ok) ++g_failures;
}

// ---------------------------------------------------------------------------
// Synthetic checkpoint writer
// ---------------------------------------------------------------------------

// A miniature of the real configuration. Every ratio that the code depends on
// is preserved: head dims stay in the set the attention kernel supports, the
// SAM decoder's cross-attention still halves the model dim, the mask
// downsampler still divides by 16 down to the token grid.
struct Config {
    int img_size = 112;
    int patch = 14;          // -> 8x8 tokens, 32x32 masks
    int embed = 256;
    int depth = 2;
    int heads = 4;           // head_dim 64
    int mlp = 512;
    int window = 4;          // 4 windows of 16 tokens
    int global_idx = 1;
    int text_width = 256;
    int text_heads = 4;
    int text_layers = 2;
    int ctx_len = 8;
    int vocab = 64;
    int text_out = 256;
    int neck = 256;
    int fenc_layers = 1;
    int fenc_heads = 8;      // head_dim 32
    int ffn = 256;
    int ddec_layers = 1;
    int ddec_heads = 8;
    int queries = 8;
    int geom_layers = 1;
    int sam_dim = 256;
    int sam_depth = 1;
    int multimask = 3;
    int mem_dim = 64;
    int mem_layers = 1;
    int maskmem = 3;
    int max_ptrs = 4;

    // ---- SAM 2 (Hiera) variant ----
    // `patch` doubles as the stride of the backbone's primary feature level, so
    // a SAM 2 config sets it to 16 and the grid arithmetic is unchanged.
    bool sam2 = false;
    int hiera_embed = 24;          // -> head dim 24, which divides neither 16 nor 32
    int hiera_stages[4] = {2, 1, 1, 1};
    int hiera_window[4] = {8, 4, 4, 2};
    int hiera_global = 1;          // block 1 attends globally
    int hiera_bkg = 7;             // pos_embed grid before interpolation

    int grid() const { return img_size / patch; }
    int hiera_depth() const {
        return hiera_stages[0] + hiera_stages[1] + hiera_stages[2] + hiera_stages[3];
    }
};

// A SAM 2 miniature: 128x128 in, patch_embed to 32x32, four stages down to
// 4x4, and a stride-16 grid of 8x8 so the tracker head below sees exactly the
// shapes the SAM 3 config gives it.
Config sam2_config() {
    Config c;
    c.sam2 = true;
    c.img_size = 128;
    c.patch = 16;                  // stride of the stride-16 stage -> grid 8
    c.neck = 256;
    c.sam_dim = 256;
    return c;
}

class ModelWriter {
public:
    ModelWriter(const std::string& path, const Config& c) : cfg(c), out(path, std::ios::binary) {}
    bool ok() const { return (bool)out; }

    void writeHeader(int n_tensors) {
        const uint32_t magic = 0x73616D33u;
        wr32((int32_t)magic);
        wr32(3);                     // version
        wr32(0);                     // ftype: f32
        wr32(n_tensors);
        // Hyperparameters, in the exact order the loader reads them.
        wr32(cfg.img_size); wr32(cfg.patch); wr32(cfg.embed); wr32(cfg.depth);
        wr32(cfg.heads);
        wr32((int32_t)((double)cfg.mlp / cfg.embed * 1000.0));  // mlp_ratio x1000
        wr32(cfg.window);
        wr32(1); wr32(cfg.global_idx);
        wr32(cfg.text_width); wr32(cfg.text_heads); wr32(cfg.text_layers);
        wr32(cfg.ctx_len); wr32(cfg.vocab); wr32(cfg.text_out);
        wr32(cfg.neck);
        wr32(cfg.fenc_layers); wr32(cfg.fenc_heads); wr32(cfg.ffn);
        wr32(cfg.ddec_layers); wr32(cfg.ddec_heads); wr32(cfg.ffn); wr32(cfg.queries);
        wr32(cfg.geom_layers); wr32(1); wr32(4);
        wr32(cfg.sam_dim); wr32(cfg.sam_depth); wr32(cfg.multimask); wr32(3);
        wr32(cfg.mem_dim); wr32(cfg.mem_layers); wr32(cfg.maskmem); wr32(cfg.max_ptrs);
        wr32(2);   // n_amb_experts
        wr32(0);   // visual_only
    }

    // The "sam2" header: a different magic and 57 fields, in the order
    // convert_sam2_to_ggml.py writes them. No trailing tokenizer.
    void writeHeaderSam2(int n_tensors) {
        wr32((int32_t)0x73616D32u);
        wr32(1);                     // version
        wr32(0);                     // ftype: f32
        wr32(n_tensors);
        wr32(cfg.img_size);
        wr32(1);                     // backbone_type: hiera
        wr32(cfg.hiera_embed);
        wr32(1);                     // num_heads
        wr32(4);                     // num_stages
        for (int i = 0; i < 4; ++i) wr32(cfg.hiera_stages[i]);
        wr32(1);                     // one global-attention block
        wr32(cfg.hiera_global);
        for (int i = 1; i < 8; ++i) wr32(0);
        wr32(3);                     // q_pool
        for (int i = 0; i < 4; ++i) wr32(cfg.hiera_window[i]);
        wr32(cfg.hiera_bkg); wr32(cfg.hiera_bkg);
        wr32(1);                     // scalp
        wr32(cfg.neck);
        wr32(2); wr32(2); wr32(3); wr32(0); wr32(0);   // fpn_top_down_levels
        wr32(cfg.sam_dim); wr32(cfg.sam_depth); wr32(cfg.multimask); wr32(3);
        wr32(cfg.mem_dim); wr32(cfg.mem_layers); wr32(cfg.maskmem); wr32(cfg.max_ptrs);
        wr32(2000); wr32(-1000);     // sigmoid scale / bias, x100
        const int32_t flags[15] = {1, 1, 1, 1, 1, 1, 0, /*multimask_for_tracking=*/1,
                                   0, 1, 1, 1, 1, /*multimask_in_sam=*/1,
                                   /*is_sam2_1=*/1};
        for (int32_t f : flags) wr32(f);
    }

    // Tensor record. `shape` is PyTorch order; the file stores ggml's ne[],
    // which is the reverse -- exactly what convert_sam3_to_ggml.py writes, and
    // what WeightStore::load un-reverses.
    void tensor(const std::string& name, std::vector<int32_t> shape) {
        wr32((int32_t)shape.size());
        wr32((int32_t)name.size());
        wr32(0);  // f32
        for (size_t i = shape.size(); i-- > 0;) wr32(shape[i]);
        out.write(name.data(), (std::streamsize)name.size());
        // 32-byte alignment before the payload.
        const size_t pos = (size_t)out.tellp();
        const size_t pad = (32 - pos % 32) % 32;
        static const char zeros[32] = {};
        if (pad) out.write(zeros, (std::streamsize)pad);

        int64_t n = 1;
        for (int32_t d : shape) n *= d;
        buf.resize((size_t)n);
        for (float& v : buf) v = dist(rng);
        out.write((const char*)buf.data(), (std::streamsize)(n * 4));
        ++count;
    }

    // A tiny but valid CLIP vocabulary: every single character plus the two
    // sentinels, and no merges. Enough for the tokenizer to produce ids.
    void writeTokenizer() {
        wr32((int32_t)0x746F6B00);
        std::vector<std::string> vocab;
        for (int c = 'a'; c <= 'z'; ++c) vocab.push_back(std::string(1, (char)c) + "</w>");
        for (int c = 'a'; c <= 'z'; ++c) vocab.push_back(std::string(1, (char)c));
        vocab.push_back("<|startoftext|>");
        vocab.push_back("<|endoftext|>");
        wr32((int32_t)vocab.size());
        for (size_t i = 0; i < vocab.size(); ++i) {
            wr32((int32_t)vocab[i].size());
            out.write(vocab[i].data(), (std::streamsize)vocab[i].size());
            // Reserve the last two ids for the sentinels, as CLIP does.
            const int id = (i + 2 >= vocab.size())
                               ? (int)(49406 + (vocab.size() - i - 1 == 1 ? 0 : 1))
                               : (int)i;
            wr32(id);
        }
        wr32(0);  // no merges
    }

    int count = 0;

private:
    void wr32(int32_t v) { out.write((const char*)&v, 4); }

    Config cfg;
    std::ofstream out;
    std::vector<float> buf;
    std::mt19937 rng{1234};
    std::normal_distribution<float> dist{0.0f, 0.02f};
};

// Enumerates every tensor the model needs. Called twice: once to count, once
// to write, so the header's tensor count is exact.
//
// Split at the neck: everything from the SAM prompt encoder down is shared
// between the two families, which is exactly what the two converters produce.
void emit_sam3_tensors(ModelWriter& w, const Config& c) {
    const int E = c.embed, D = c.neck, TW = c.text_width, MD = c.mem_dim;
    const int G = c.grid();
    const int pg = c.img_size / c.patch / 3;   // pretrained pos-embed grid
    const int hd = E / c.heads;

    // ---- ViT ----
    w.tensor("vit.patch_embed.proj.weight", {E, 3, c.patch, c.patch});
    w.tensor("vit.pos_embed", {1, pg, pg, E});
    w.tensor("vit.ln_pre.weight", {E});
    w.tensor("vit.ln_pre.bias", {E});
    for (int i = 0; i < c.depth; ++i) {
        const std::string p = "vit.blocks." + std::to_string(i);
        w.tensor(p + ".norm1.weight", {E});
        w.tensor(p + ".norm1.bias", {E});
        w.tensor(p + ".attn.qkv.weight", {3 * E, E});
        w.tensor(p + ".attn.qkv.bias", {3 * E});
        w.tensor(p + ".attn.proj.weight", {E, E});
        w.tensor(p + ".attn.proj.bias", {E});
        w.tensor(p + ".norm2.weight", {E});
        w.tensor(p + ".norm2.bias", {E});
        w.tensor(p + ".mlp.lin1.weight", {c.mlp, E});
        w.tensor(p + ".mlp.lin1.bias", {c.mlp});
        w.tensor(p + ".mlp.lin2.weight", {E, c.mlp});
        w.tensor(p + ".mlp.lin2.bias", {E});
        const int rope_n = (i == c.global_idx) ? G * G : c.window * c.window;
        w.tensor(p + ".attn.freqs_cis", {rope_n, hd / 2, 2});
    }

    // ---- necks ----
    for (const char* pre : {"neck.det.", "neck.trk."}) {
        const std::string s = pre;
        w.tensor(s + "0.dconv_2x2_0.weight", {E, 512, 2, 2});
        w.tensor(s + "0.dconv_2x2_0.bias", {512});
        w.tensor(s + "0.dconv_2x2_1.weight", {512, D, 2, 2});
        w.tensor(s + "0.dconv_2x2_1.bias", {D});
        w.tensor(s + "0.conv_1x1.weight", {D, D, 1, 1});
        w.tensor(s + "0.conv_1x1.bias", {D});
        w.tensor(s + "0.conv_3x3.weight", {D, D, 3, 3});
        w.tensor(s + "0.conv_3x3.bias", {D});
        w.tensor(s + "1.dconv_2x2.weight", {E, 512, 2, 2});
        w.tensor(s + "1.dconv_2x2.bias", {512});
        w.tensor(s + "1.conv_1x1.weight", {D, 512, 1, 1});
        w.tensor(s + "1.conv_1x1.bias", {D});
        w.tensor(s + "1.conv_3x3.weight", {D, D, 3, 3});
        w.tensor(s + "1.conv_3x3.bias", {D});
        for (int level = 2; level <= 3; ++level) {
            const std::string l = s + std::to_string(level);
            w.tensor(l + ".conv_1x1.weight", {D, E, 1, 1});
            w.tensor(l + ".conv_1x1.bias", {D});
            w.tensor(l + ".conv_3x3.weight", {D, D, 3, 3});
            w.tensor(l + ".conv_3x3.bias", {D});
        }
    }

    // ---- text encoder ----
    w.tensor("text.token_embed.weight", {c.vocab, TW});
    w.tensor("text.pos_embed", {c.ctx_len, TW});
    w.tensor("text.ln_final.weight", {TW});
    w.tensor("text.ln_final.bias", {TW});
    w.tensor("text.resizer.weight", {c.text_out, TW});
    w.tensor("text.resizer.bias", {c.text_out});
    for (int i = 0; i < c.text_layers; ++i) {
        const std::string p = "text.blocks." + std::to_string(i);
        w.tensor(p + ".attn.in_proj.weight", {3 * TW, TW});
        w.tensor(p + ".attn.in_proj.bias", {3 * TW});
        w.tensor(p + ".attn.out_proj.weight", {TW, TW});
        w.tensor(p + ".attn.out_proj.bias", {TW});
        w.tensor(p + ".ln_1.weight", {TW});
        w.tensor(p + ".ln_1.bias", {TW});
        w.tensor(p + ".ln_2.weight", {TW});
        w.tensor(p + ".ln_2.bias", {TW});
        w.tensor(p + ".mlp.fc1.weight", {4 * TW, TW});
        w.tensor(p + ".mlp.fc1.bias", {4 * TW});
        w.tensor(p + ".mlp.fc2.weight", {TW, 4 * TW});
        w.tensor(p + ".mlp.fc2.bias", {TW});
    }

    // ---- fusion encoder ----
    for (int i = 0; i < c.fenc_layers; ++i) {
        const std::string p = "fenc.layers." + std::to_string(i);
        for (const char* a : {"sa", "ca"}) {
            w.tensor(p + "." + a + ".in_proj_weight", {3 * D, D});
            w.tensor(p + "." + a + ".in_proj_bias", {3 * D});
            w.tensor(p + "." + a + ".out_proj.weight", {D, D});
            w.tensor(p + "." + a + ".out_proj.bias", {D});
        }
        for (const char* n : {"norm1", "norm2", "norm3"}) {
            w.tensor(p + "." + n + ".weight", {D});
            w.tensor(p + "." + n + ".bias", {D});
        }
        w.tensor(p + ".linear1.weight", {c.ffn, D});
        w.tensor(p + ".linear1.bias", {c.ffn});
        w.tensor(p + ".linear2.weight", {D, c.ffn});
        w.tensor(p + ".linear2.bias", {D});
    }

    // ---- DETR decoder ----
    w.tensor("ddec.query_embed.weight", {c.queries, D});
    w.tensor("ddec.presence_token.weight", {1, D});
    w.tensor("ddec.reference_points.weight", {c.queries, 4});
    w.tensor("ddec.norm.weight", {D});
    w.tensor("ddec.norm.bias", {D});
    for (int j = 0; j < 3; ++j) {
        const int o = (j == 2) ? 4 : D;
        w.tensor("ddec.bbox_embed.layers." + std::to_string(j) + ".weight", {o, D});
        w.tensor("ddec.bbox_embed.layers." + std::to_string(j) + ".bias", {o});
    }
    w.tensor("ddec.ref_point_head.layers.0.weight", {D, 512});
    w.tensor("ddec.ref_point_head.layers.0.bias", {D});
    w.tensor("ddec.ref_point_head.layers.1.weight", {D, D});
    w.tensor("ddec.ref_point_head.layers.1.bias", {D});
    for (const char* ax : {"x", "y"}) {
        const std::string p = std::string("ddec.boxRPB_embed_") + ax;
        w.tensor(p + ".layers.0.weight", {D, 2});
        w.tensor(p + ".layers.0.bias", {D});
        w.tensor(p + ".layers.1.weight", {c.ddec_heads, D});
        w.tensor(p + ".layers.1.bias", {c.ddec_heads});
    }
    for (int j = 0; j < 3; ++j) {
        const int o = (j == 2) ? 1 : D;
        w.tensor("ddec.presence_token_head.layers." + std::to_string(j) + ".weight", {o, D});
        w.tensor("ddec.presence_token_head.layers." + std::to_string(j) + ".bias", {o});
    }
    w.tensor("ddec.presence_token_out_norm.weight", {D});
    w.tensor("ddec.presence_token_out_norm.bias", {D});
    for (int i = 0; i < c.ddec_layers; ++i) {
        const std::string p = "ddec.layers." + std::to_string(i);
        for (const char* a : {"sa", "ca", "ca_text"}) {
            w.tensor(p + "." + a + ".in_proj_weight", {3 * D, D});
            w.tensor(p + "." + a + ".in_proj_bias", {3 * D});
            w.tensor(p + "." + a + ".out_proj.weight", {D, D});
            w.tensor(p + "." + a + ".out_proj.bias", {D});
        }
        for (const char* n : {"norm1", "norm2", "norm3", "norm_ca_text"}) {
            w.tensor(p + "." + n + ".weight", {D});
            w.tensor(p + "." + n + ".bias", {D});
        }
        w.tensor(p + ".linear1.weight", {c.ffn, D});
        w.tensor(p + ".linear1.bias", {c.ffn});
        w.tensor(p + ".linear2.weight", {D, c.ffn});
        w.tensor(p + ".linear2.bias", {D});
    }

    // ---- scoring ----
    w.tensor("scoring.prompt_proj.weight", {D, D});
    w.tensor("scoring.prompt_proj.bias", {D});
    w.tensor("scoring.hs_proj.weight", {D, D});
    w.tensor("scoring.hs_proj.bias", {D});
    w.tensor("scoring.prompt_mlp.layers.0.weight", {c.ffn, D});
    w.tensor("scoring.prompt_mlp.layers.0.bias", {c.ffn});
    w.tensor("scoring.prompt_mlp.layers.1.weight", {D, c.ffn});
    w.tensor("scoring.prompt_mlp.layers.1.bias", {D});
    w.tensor("scoring.prompt_mlp.out_norm.weight", {D});
    w.tensor("scoring.prompt_mlp.out_norm.bias", {D});

    // ---- geometry encoder ----
    w.tensor("geom.points_direct_project.weight", {D, 2});
    w.tensor("geom.points_direct_project.bias", {D});
    w.tensor("geom.boxes_direct_project.weight", {D, 4});
    w.tensor("geom.boxes_direct_project.bias", {D});
    w.tensor("geom.points_pool_project.weight", {D, D});
    w.tensor("geom.points_pool_project.bias", {D});
    w.tensor("geom.boxes_pool_project.weight", {D, D, 7, 7});
    w.tensor("geom.boxes_pool_project.bias", {D});
    w.tensor("geom.points_pos_enc_project.weight", {D, D});
    w.tensor("geom.points_pos_enc_project.bias", {D});
    w.tensor("geom.boxes_pos_enc_project.weight", {D, D + 2});
    w.tensor("geom.boxes_pos_enc_project.bias", {D});
    w.tensor("geom.label_embed.weight", {2, D});
    w.tensor("geom.cls_embed.weight", {1, D});
    w.tensor("geom.final_proj.weight", {D, D});
    w.tensor("geom.final_proj.bias", {D});
    for (const char* n : {"geom.norm", "geom.encode_norm", "geom.img_pre_norm"}) {
        w.tensor(std::string(n) + ".weight", {D});
        w.tensor(std::string(n) + ".bias", {D});
    }
    for (int i = 0; i < c.geom_layers; ++i) {
        const std::string p = "geom.layers." + std::to_string(i);
        for (const char* a : {"sa", "ca"}) {
            w.tensor(p + "." + a + ".in_proj_weight", {3 * D, D});
            w.tensor(p + "." + a + ".in_proj_bias", {3 * D});
            w.tensor(p + "." + a + ".out_proj.weight", {D, D});
            w.tensor(p + "." + a + ".out_proj.bias", {D});
        }
        for (const char* n : {"norm1", "norm2", "norm3"}) {
            w.tensor(p + "." + n + ".weight", {D});
            w.tensor(p + "." + n + ".bias", {D});
        }
        w.tensor(p + ".linear1.weight", {c.ffn, D});
        w.tensor(p + ".linear1.bias", {c.ffn});
        w.tensor(p + ".linear2.weight", {D, c.ffn});
        w.tensor(p + ".linear2.bias", {D});
    }

    // ---- segmentation head ----
    for (int i = 0; i < 3; ++i) {
        w.tensor("seg.pixel_decoder.conv_layers." + std::to_string(i) + ".weight",
                 {D, D, 3, 3});
        w.tensor("seg.pixel_decoder.conv_layers." + std::to_string(i) + ".bias", {D});
        w.tensor("seg.pixel_decoder.norms." + std::to_string(i) + ".weight", {D});
        w.tensor("seg.pixel_decoder.norms." + std::to_string(i) + ".bias", {D});
    }
    for (int j = 0; j < 3; ++j) {
        w.tensor("seg.mask_predictor.mask_embed.layers." + std::to_string(j) + ".weight",
                 {D, D});
        w.tensor("seg.mask_predictor.mask_embed.layers." + std::to_string(j) + ".bias", {D});
    }
    w.tensor("seg.cross_attend_prompt.in_proj_weight", {3 * D, D});
    w.tensor("seg.cross_attend_prompt.in_proj_bias", {3 * D});
    w.tensor("seg.cross_attend_prompt.out_proj.weight", {D, D});
    w.tensor("seg.cross_attend_prompt.out_proj.bias", {D});
    w.tensor("seg.cross_attn_norm.weight", {D});
    w.tensor("seg.cross_attn_norm.bias", {D});
    w.tensor("seg.instance_seg_head.weight", {D, D, 1, 1});
    w.tensor("seg.instance_seg_head.bias", {D});
    w.tensor("seg.semantic_seg_head.weight", {1, D, 1, 1});
    w.tensor("seg.semantic_seg_head.bias", {1});

}

// Hiera trunk + FpnNeck. The per-block dims and window sizes follow the same
// rules Hparams.h derives them by, so a mismatch between the two shows up here
// as a missing weight rather than in a 400 MB checkpoint.
void emit_sam2_tensors(ModelWriter& w, const Config& c) {
    const int D = c.neck;
    w.tensor("hiera.patch_embed.weight", {c.hiera_embed, 3, 7, 7});
    w.tensor("hiera.patch_embed.bias", {c.hiera_embed});
    w.tensor("hiera.pos_embed", {1, c.hiera_embed, c.hiera_bkg, c.hiera_bkg});
    w.tensor("hiera.pos_embed_window",
             {1, c.hiera_embed, c.hiera_window[0], c.hiera_window[0]});

    int stage_end[4] = {0, 0, 0, 0};
    for (int s = 0, acc = 0; s < 4; ++s) {
        acc += c.hiera_stages[s];
        stage_end[s] = acc - 1;
    }
    int dim = c.hiera_embed;
    for (int i = 0; i < c.hiera_depth(); ++i) {
        int dim_out = dim;
        bool is_end = false;
        for (int s = 0; s < 4; ++s) is_end |= (stage_end[s] == i - 1);
        if (i > 0 && is_end) dim_out = dim * 2;
        const std::string p = "hiera.blocks." + std::to_string(i);
        w.tensor(p + ".norm1.weight", {dim});
        w.tensor(p + ".norm1.bias", {dim});
        if (dim != dim_out) {
            w.tensor(p + ".proj.weight", {dim_out, dim});
            w.tensor(p + ".proj.bias", {dim_out});
        }
        w.tensor(p + ".attn.qkv.weight", {3 * dim_out, dim});
        w.tensor(p + ".attn.qkv.bias", {3 * dim_out});
        w.tensor(p + ".attn.proj.weight", {dim_out, dim_out});
        w.tensor(p + ".attn.proj.bias", {dim_out});
        w.tensor(p + ".norm2.weight", {dim_out});
        w.tensor(p + ".norm2.bias", {dim_out});
        w.tensor(p + ".mlp.fc1.weight", {4 * dim_out, dim_out});
        w.tensor(p + ".mlp.fc1.bias", {4 * dim_out});
        w.tensor(p + ".mlp.fc2.weight", {dim_out, 4 * dim_out});
        w.tensor(p + ".mlp.fc2.bias", {dim_out});
        dim = dim_out;
    }

    // convs[] runs coarsest-first, so conv i takes stage (3 - i).
    for (int i = 0; i < 4; ++i) {
        const int cin = c.hiera_embed << (3 - i);
        w.tensor("fpn.convs." + std::to_string(i) + ".weight", {D, cin, 1, 1});
        w.tensor("fpn.convs." + std::to_string(i) + ".bias", {D});
    }
}

// The tracker head: prompt encoder, mask decoder, memory encoder and memory
// attention. SAM 3 inherited these from SAM 2 verbatim, names included.
void emit_tracker_tensors(ModelWriter& w, const Config& c) {
    const int D = c.neck, MD = c.mem_dim;
    const int G = c.grid();
    (void)G;
    // ---- SAM prompt encoder ----
    w.tensor("sam_pe.pe_gaussian", {D / 2, 2});
    for (int i = 0; i < 4; ++i)
        w.tensor("sam_pe.point_embeddings." + std::to_string(i) + ".weight", {1, D});
    w.tensor("sam_pe.not_a_point_embed.weight", {1, D});
    w.tensor("sam_pe.no_mask_embed.weight", {1, D});
    w.tensor("sam_pe.mask_ds.0.weight", {4, 1, 2, 2});
    w.tensor("sam_pe.mask_ds.0.bias", {4});
    w.tensor("sam_pe.mask_ds.1.weight", {4});
    w.tensor("sam_pe.mask_ds.1.bias", {4});
    w.tensor("sam_pe.mask_ds.3.weight", {16, 4, 2, 2});
    w.tensor("sam_pe.mask_ds.3.bias", {16});
    w.tensor("sam_pe.mask_ds.4.weight", {16});
    w.tensor("sam_pe.mask_ds.4.bias", {16});
    w.tensor("sam_pe.mask_ds.6.weight", {D, 16, 1, 1});
    w.tensor("sam_pe.mask_ds.6.bias", {D});

    // ---- SAM mask decoder ----
    const int NM = c.multimask + 1;
    w.tensor("sam_dec.iou_token.weight", {1, D});
    w.tensor("sam_dec.mask_tokens.weight", {NM, D});
    w.tensor("sam_dec.obj_score_token.weight", {1, D});
    auto attn_block = [&](const std::string& p, int internal) {
        for (const char* n : {"q_proj", "k_proj", "v_proj"}) {
            w.tensor(p + "." + n + ".weight", {internal, D});
            w.tensor(p + "." + n + ".bias", {internal});
        }
        w.tensor(p + ".out_proj.weight", {D, internal});
        w.tensor(p + ".out_proj.bias", {D});
    };
    for (int i = 0; i < c.sam_depth; ++i) {
        const std::string p = "sam_dec.twoway." + std::to_string(i);
        attn_block(p + ".sa", D);
        attn_block(p + ".cross_attn_token_to_image", D / 2);
        attn_block(p + ".cross_attn_image_to_token", D / 2);
        for (const char* n : {"norm1", "norm2", "norm3", "norm4"}) {
            w.tensor(p + "." + n + ".weight", {D});
            w.tensor(p + "." + n + ".bias", {D});
        }
        w.tensor(p + ".mlp.lin1.weight", {c.ffn, D});
        w.tensor(p + ".mlp.lin1.bias", {c.ffn});
        w.tensor(p + ".mlp.lin2.weight", {D, c.ffn});
        w.tensor(p + ".mlp.lin2.bias", {D});
    }
    attn_block("sam_dec.final_attn", D / 2);
    w.tensor("sam_dec.final_norm.weight", {D});
    w.tensor("sam_dec.final_norm.bias", {D});
    w.tensor("sam_dec.upscale.0.weight", {D, 64, 2, 2});
    w.tensor("sam_dec.upscale.0.bias", {64});
    w.tensor("sam_dec.upscale.1.weight", {64});
    w.tensor("sam_dec.upscale.1.bias", {64});
    w.tensor("sam_dec.upscale.3.weight", {64, 32, 2, 2});
    w.tensor("sam_dec.upscale.3.bias", {32});
    w.tensor("sam_dec.conv_s0.weight", {32, D, 1, 1});
    w.tensor("sam_dec.conv_s0.bias", {32});
    w.tensor("sam_dec.conv_s1.weight", {64, D, 1, 1});
    w.tensor("sam_dec.conv_s1.bias", {64});
    for (int mtok = 0; mtok < NM; ++mtok)
        for (int j = 0; j < 3; ++j) {
            const int o = (j == 2) ? 32 : D;
            const std::string p =
                "sam_dec.hyper." + std::to_string(mtok) + ".layers." + std::to_string(j);
            w.tensor(p + ".weight", {o, D});
            w.tensor(p + ".bias", {o});
        }
    for (int j = 0; j < 3; ++j) {
        const int o = (j == 2) ? NM : D;
        w.tensor("sam_dec.iou_prediction_head.layers." + std::to_string(j) + ".weight",
                 {o, D});
        w.tensor("sam_dec.iou_prediction_head.layers." + std::to_string(j) + ".bias", {o});
    }
    for (int j = 0; j < 3; ++j) {
        const int o = (j == 2) ? 1 : D;
        w.tensor("sam_dec.pred_obj_score_head.layers." + std::to_string(j) + ".weight",
                 {o, D});
        w.tensor("sam_dec.pred_obj_score_head.layers." + std::to_string(j) + ".bias", {o});
    }

    // ---- memory encoder ----
    const int ds_conv[4] = {0, 3, 6, 9};
    const int ds_norm[4] = {1, 4, 7, 10};
    const int ds_ch[5] = {1, 4, 16, 64, 0};
    for (int s = 0; s < 4; ++s) {
        const int in_ch = ds_ch[s];
        const int out_ch = (s == 3) ? D : ds_ch[s + 1];
        w.tensor("mem_enc.ds." + std::to_string(ds_conv[s]) + ".weight",
                 {out_ch, in_ch, 3, 3});
        w.tensor("mem_enc.ds." + std::to_string(ds_conv[s]) + ".bias", {out_ch});
        w.tensor("mem_enc.ds." + std::to_string(ds_norm[s]) + ".weight", {out_ch});
        w.tensor("mem_enc.ds." + std::to_string(ds_norm[s]) + ".bias", {out_ch});
    }
    w.tensor("mem_enc.ds.12.weight", {D, D, 1, 1});
    w.tensor("mem_enc.ds.12.bias", {D});
    w.tensor("mem_enc.pix_feat_proj.weight", {D, D, 1, 1});
    w.tensor("mem_enc.pix_feat_proj.bias", {D});
    for (int i = 0; i < 2; ++i) {
        const std::string p = "mem_enc.fuser." + std::to_string(i);
        w.tensor(p + ".dwconv.weight", {D, 1, 7, 7});
        w.tensor(p + ".dwconv.bias", {D});
        w.tensor(p + ".norm.weight", {D});
        w.tensor(p + ".norm.bias", {D});
        w.tensor(p + ".pwconv1.weight", {4 * D, D});
        w.tensor(p + ".pwconv1.bias", {4 * D});
        w.tensor(p + ".pwconv2.weight", {D, 4 * D});
        w.tensor(p + ".pwconv2.bias", {D});
        w.tensor(p + ".gamma", {D});
    }
    w.tensor("mem_enc.out_proj.weight", {MD, D, 1, 1});
    w.tensor("mem_enc.out_proj.bias", {MD});
    w.tensor("mem_enc.tpos_enc", {c.maskmem, 1, 1, MD});

    // ---- memory attention ----
    w.tensor("mem_attn.norm.weight", {D});
    w.tensor("mem_attn.norm.bias", {D});
    for (int i = 0; i < c.mem_layers; ++i) {
        const std::string p = "mem_attn.layers." + std::to_string(i);
        for (const char* n : {"q_proj", "k_proj", "v_proj", "out_proj"}) {
            w.tensor(p + ".sa." + n + ".weight", {D, D});
            w.tensor(p + ".sa." + n + ".bias", {D});
        }
        w.tensor(p + ".ca.q_proj.weight", {D, D});
        w.tensor(p + ".ca.q_proj.bias", {D});
        w.tensor(p + ".ca.k_proj.weight", {D, MD});
        w.tensor(p + ".ca.k_proj.bias", {D});
        w.tensor(p + ".ca.v_proj.weight", {D, MD});
        w.tensor(p + ".ca.v_proj.bias", {D});
        w.tensor(p + ".ca.out_proj.weight", {D, D});
        w.tensor(p + ".ca.out_proj.bias", {D});
        for (const char* n : {"norm1", "norm2", "norm3"}) {
            w.tensor(p + "." + n + ".weight", {D});
            w.tensor(p + "." + n + ".bias", {D});
        }
        w.tensor(p + ".linear1.weight", {c.ffn, D});
        w.tensor(p + ".linear1.bias", {c.ffn});
        w.tensor(p + ".linear2.weight", {D, c.ffn});
        w.tensor(p + ".linear2.bias", {D});
    }

    // ---- object pointers and top-level tensors ----
    for (int j = 0; j < 3; ++j) {
        w.tensor("obj_ptr_proj.layers." + std::to_string(j) + ".weight", {D, D});
        w.tensor("obj_ptr_proj.layers." + std::to_string(j) + ".bias", {D});
    }
    w.tensor("no_obj_ptr", {1, D});
    w.tensor("obj_ptr_tpos_proj.weight", {MD, D});
    w.tensor("obj_ptr_tpos_proj.bias", {MD});
    w.tensor("no_mem_embed", {1, 1, D});
    w.tensor("no_mem_pos_enc", {1, 1, D});
    w.tensor("no_obj_embed_spatial", {1, MD});
    w.tensor("trk_mask_ds.weight", {1, 1, 4, 4});
    w.tensor("trk_mask_ds.bias", {1});
}

void emit_tensors(ModelWriter& w, const Config& c) {
    if (c.sam2) emit_sam2_tensors(w, c);
    else        emit_sam3_tensors(w, c);
    emit_tracker_tensors(w, c);
}

// Counts without writing, by running the emitter against a writer whose output
// stream is /dev/null-equivalent.
int count_tensors(const Config& c) {
    ModelWriter counter("", c);   // failed open: writes go nowhere
    emit_tensors(counter, c);
    return counter.count;
}

bool write_model(const std::string& path, const Config& c) {
    ModelWriter w(path, c);
    if (!w.ok()) return false;
    if (c.sam2) w.writeHeaderSam2(count_tensors(c));
    else        w.writeHeader(count_tensors(c));
    emit_tensors(w, c);
    if (!c.sam2) w.writeTokenizer();   // SAM 2 has no text tower
    return true;
}

// ---------------------------------------------------------------------------

nn::Image make_image(int w, int h) {
    nn::Image img;
    img.width = w;
    img.height = h;
    img.channels = 3;
    img.data.resize((size_t)w * h * 3);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const size_t o = ((size_t)y * w + x) * 3;
            img.data[o + 0] = (uint8_t)((x * 5 + y * 3) & 0xFF);
            img.data[o + 1] = (uint8_t)((x * 2 + y * 7) & 0xFF);
            img.data[o + 2] = (uint8_t)((x + y) & 0xFF);
        }
    return img;
}

bool finite_result(const sam::Result& r) {
    for (const auto& d : r.detections) {
        if (!std::isfinite(d.score) || !std::isfinite(d.box.x0) ||
            !std::isfinite(d.box.y1))
            return false;
        if (d.mask.data.size() != (size_t)d.mask.width * d.mask.height) return false;
    }
    return true;
}

}  // namespace

int main() {
    nn::set_log_level(2);
    const Config cfg;
    const std::string path = "synthetic_sam3.ggml";

    std::printf("Synthetic checkpoint\n");
    check(write_model(path, cfg), "write synthetic model file");

    sam::Session session;
    sam::ModelParams mp;
    mp.model_path = path;
    if (!session.loadModel(mp)) {
        std::printf("  FAIL load: %s\n", session.lastError().c_str());
        return 1;
    }
    check(true, "load model");
    check(session.supportsTextPrompts(), "text prompts reported as supported");

    std::printf("\nSingle image\n");
    nn::Image img = make_image(160, 120);
    check(session.encodeImage(img), "encode image (ViT + both necks)");
    if (!session.imageEncoded()) {
        std::printf("  encode failed: %s\n", session.lastError().c_str());
        return 1;
    }

    {
        sam::ConceptPrompt cp;
        cp.text = "a cat";
        cp.score_threshold = 0.0f;   // random weights: take whatever comes out
        cp.max_detections = 4;
        sam::Result r = session.segmentConcept(cp);
        check(session.lastError().empty(), "segmentConcept ran without error");
        check(finite_result(r), "segmentConcept results are finite and well-formed");
        std::printf("       %zu detections\n", r.detections.size());
    }
    {
        // Exercise the exemplar path too: it is the only user of ROI-Align and
        // of the box pooling projection.
        sam::ConceptPrompt cp;
        cp.text = "a cat";
        cp.pos_exemplars.push_back({20.0f, 20.0f, 90.0f, 80.0f});
        cp.neg_exemplars.push_back({100.0f, 10.0f, 150.0f, 60.0f});
        cp.score_threshold = 0.0f;
        cp.max_detections = 2;
        sam::Result r = session.segmentConcept(cp);
        check(session.lastError().empty(), "segmentConcept with exemplar boxes");
        check(finite_result(r), "exemplar results are finite");
    }
    {
        sam::VisualPrompt vp;
        vp.pos_points.push_back({80.0f, 60.0f});
        vp.neg_points.push_back({10.0f, 10.0f});
        sam::Result r = session.segmentVisual(vp);
        check(session.lastError().empty() && r.detections.size() == 1,
              "segmentVisual (points) returns one mask");
        check(finite_result(r), "segmentVisual results are finite");
    }
    {
        sam::VisualPrompt vp;
        vp.box = {30.0f, 20.0f, 130.0f, 100.0f};
        vp.use_box = true;
        vp.multimask = true;
        sam::Result r = session.segmentVisual(vp);
        check(r.detections.size() == 3, "segmentVisual (box, multimask) returns 3 masks");
    }

    std::printf("\nVideo tracking\n");
    {
        sam::VideoParams vp;
        vp.text_prompt = "a cat";
        vp.score_threshold = 0.0f;
        vp.hotstart_delay = 2;
        sam::Tracker tracker(session, vp);
        bool ok = true;
        for (int f = 0; f < 3; ++f) {
            nn::Image frame = make_image(160, 120);
            sam::Result r = tracker.trackFrame(frame);
            if (!session.lastError().empty()) {
                std::printf("       frame %d: %s\n", f, session.lastError().c_str());
                ok = false;
                break;
            }
            if (!finite_result(r)) ok = false;
            std::printf("       frame %d: %zu instances\n", f, r.detections.size());
        }
        check(ok, "text-prompted tracking over 3 frames");
        check(tracker.frameIndex() == 3, "frame index advanced");
    }
    {
        sam::VideoParams vp;  // no text: the visual-only path
        sam::Tracker tracker(session, vp);
        nn::Image frame = make_image(160, 120);
        tracker.trackFrame(frame);
        sam::VisualPrompt prompt;
        prompt.pos_points.push_back({80.0f, 60.0f});
        const int id = tracker.addInstance(prompt);
        check(id > 0, "addInstance seeds a manual track");
        bool ok = id > 0;
        for (int f = 1; f < 3 && ok; ++f) {
            sam::Result r = tracker.propagateFrame(make_image(160, 120));
            if (!session.lastError().empty()) {
                std::printf("       frame %d: %s\n", f, session.lastError().c_str());
                ok = false;
            }
            std::printf("       frame %d: %zu instances\n", f, r.detections.size());
        }
        check(ok, "visual-only propagation over 3 frames");
        check(tracker.refineInstance(id, {{70.0f, 55.0f}}, {}), "refineInstance");
    }

    // Random weights make whether "sky" matches anything noise, so what is
    // checked is the shape of the answer, not its content.
    std::printf("\nFeature-only phrases (sam::Masker)\n");
    {
        sam::MaskOptions mo;
        mo.model = path;
        mo.video = false;
        mo.max_size = 0;
        mo.threshold = 0.0f;
        mo.keep_prompted = true;   // with nothing named, must still keep all
        mo.feature_text = "sky; cloud";
        sam::Masker masker;
        std::string err;
        const bool ok = masker.init(mo, err);
        check(ok, "a feature prompt alone is something to segment");
        if (ok) {
            check(!masker.hasTarget() && masker.hasFeatureMask(),
                  "... with nothing named for training");
            sam::Mask m, f;
            check(masker.run(make_image(160, 120), m, nullptr, -1, &f),
                  "run with a feature mask");
            const bool all_kept =
                std::all_of(m.data.begin(), m.data.end(), [](uint8_t v) { return v == 255; });
            check(m.width == 160 && m.height == 120 && all_kept,
                  "nothing named for training keeps every pixel");
            check(f.width == 160 && f.height == 120 && f.data.size() == 160u * 120u,
                  "the feature mask comes back at the source resolution");
            check(masker.run(make_image(160, 120), m, nullptr, -1, nullptr),
                  "and without one asked for");
        }
    }

    std::printf("\nVRAM\n%s", session.vramReport().c_str());

    // ---- the same drill against a SAM 2 checkpoint --------------------------
    // Different backbone, same tracker head. The interesting parts are that
    // every hiera.* / fpn.* name resolves, that the block table's dims, window
    // sizes and pooling match what the emitter wrote, and that the whole thing
    // reaches a mask through a text-free path.
    {
        std::printf("\nSynthetic SAM 2 checkpoint\n");
        const Config c2 = sam2_config();
        const std::string p2 = "synthetic_sam2.ggml";
        check(write_model(p2, c2), "write synthetic SAM 2 model file");

        sam::Session s2;
        sam::ModelParams mp2;
        mp2.model_path = p2;
        if (!s2.loadModel(mp2)) {
            std::printf("  FAIL load: %s\n", s2.lastError().c_str());
            ++g_failures;
        } else {
            check(true, "load SAM 2 model");
            check(!s2.supportsTextPrompts(), "SAM 2 reports no text prompts");
            nn::Image i2 = make_image(160, 120);
            check(s2.encodeImage(i2), "encode image (hiera + fpn)");

            sam::VisualPrompt vp;
            vp.pos_points.push_back({80.0f, 60.0f});
            sam::Result r = s2.segmentVisual(vp);
            check(s2.lastError().empty() && r.detections.size() == 1,
                  "segmentVisual on SAM 2 returns one mask");
            check(finite_result(r), "SAM 2 results are finite");

            sam::VideoParams tvp;   // no text: propagation only
            sam::Tracker tracker(s2, tvp);
            tracker.trackFrame(make_image(160, 120));
            const int id = tracker.addInstance(vp);
            check(id > 0, "SAM 2 addInstance seeds a track");
            bool ok = id > 0;
            for (int f = 1; f < 3 && ok; ++f) {
                sam::Result pr = tracker.propagateFrame(make_image(160, 120));
                if (!s2.lastError().empty()) {
                    std::printf("       frame %d: %s\n", f, s2.lastError().c_str());
                    ok = false;
                }
                if (!finite_result(pr)) ok = false;
            }
            check(ok, "SAM 2 propagation over 3 frames");
        }

        // ---- the masking policy's seeds ------------------------------------
        // Objects arrive on the frame they were clicked on and not before, a
        // second seed for an object refines it rather than making a third
        // track, and the seeding frame's own mask already contains the new
        // object -- the propagation pass for it ran before it existed.
        {
            std::printf("\nClicked objects (sam::Masker)\n");
            sam::MaskOptions mo;
            mo.model = p2;
            mo.video = true;
            mo.max_size = 0;
            sam::SeedPrompt a;                      // object 0, frame 0
            a.prompt.pos_points.push_back({80.0f, 60.0f});
            sam::SeedPrompt b;                      // object 1, frame 2
            b.object = 1;
            b.frame = 2;
            b.prompt.pos_points.push_back({40.0f, 30.0f});
            sam::SeedPrompt refine;                 // object 0 again, frame 3
            refine.frame = 3;
            refine.prompt.pos_points.push_back({85.0f, 65.0f});
            mo.seeds = {b, a, refine};              // deliberately out of order

            sam::Masker masker;
            std::string err;
            if (!masker.init(mo, err)) {
                std::printf("  FAIL masker init: %s\n", err.c_str());
                ++g_failures;
            } else {
                int ids[4] = {0, 0, 0, 0};
                bool ran = true;
                for (int f = 0; f < 4 && ran; ++f) {
                    sam::Mask m;
                    sam::Result r;
                    ran = masker.run(make_image(160, 120), m, &r, f);
                    if (!ran) {
                        std::printf("  FAIL frame %d: %s\n", f,
                                    masker.lastError().c_str());
                        ++g_failures;
                        break;
                    }
                    std::vector<int> seen;
                    for (const auto& d : r.detections)
                        if (std::find(seen.begin(), seen.end(), d.instance_id) ==
                            seen.end())
                            seen.push_back(d.instance_id);
                    ids[f] = (int)seen.size();
                    check(m.width == 160 && m.height == 120,
                          "mask comes back at the source resolution");
                }
                if (ran) {
                    check(ids[0] == 1, "the first object is in its own frame");
                    check(ids[1] == 1, "and only it, before the second is clicked");
                    check(ids[2] == 2, "the second object appears on frame 2");
                    check(ids[3] == 2, "refining object 0 does not make a third");
                }
            }
        }
        {
            sam::MaskOptions mo;
            mo.model = p2;
            mo.feature_text = "sky";
            sam::Masker masker;
            std::string err;
            check(!masker.init(mo, err), "a feature prompt needs a text model");
        }
        std::remove(p2.c_str());
    }

    std::remove(path.c_str());
    nn::shutdown();

    std::printf("\n%d failures\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
