# 实焦置信度与训练遮罩（`spirula focus`）

本地版本基于上游 release `v2026.10.16`（`65246c5579c127081bff6f4422bd12a542906f78`）。
在“数据集 / 空三”的深度与法线步骤勾选实焦权重，自动选择 MoGe-2 大模型。
实焦算法保持不变，最终结果直接合并写入数据集的 `masks/`。

- 有原遮罩：`最终遮罩 = 原遮罩 × 实焦置信度`，均按 0–1 权重计算。
- 没有原遮罩：按全白、权重 1 处理，直接生成同名 PNG 实焦遮罩。
- 保留图片子目录及已有遮罩的命名；`a.png` 和 `a.jpg.png` 均沿用训练器的查找规则。
- 输出为原始栅格尺寸的 8 位灰度 PNG；不按 EXIF 旋转。白色 255 表示完整权重，黑色 0 表示不参与，灰色表示部分权重。
- 训练页面无需单独的实焦开关；保留默认的“使用数据集遮罩”即可。训练读取 `masks/`，不读取旧 `focus/`。

## 备份与重复生成

开始逐图计算前，先备份本次所有原 masks。原文件的完整字节保存在
`.spirula-focus/original_masks/<SHA-256>/<原相对路径>`，逐图记录在
`.spirula-focus/records/`。原来无文件的图片也会有记录。
不要删除 `.spirula-focus/`，否则会丢失原始遮罩及防止重复压暗所需的状态。

每次重新生成都从备份原遮罩重新相乘，不会将本工具上次的结果继续相乘。
如果用户在生成后重新用 SAM3 等工具修改了 masks，文件哈希变化会把这份新遮罩作为新原始遮罩备份。
计算期间若 masks 被其他程序改写，当前图片会报错并拒绝覆盖。
PNG 与记录均通过临时文件替换，记录同时保留写入前与待写入哈希，可识别中断的写回。

原遮罩与照片尺寸必须相同，避免悄悄拉伸导致错位；尺寸不同时报错，原始备份保留。
恢复时可按逐图 JSON 中的 `output` / `original` 相对路径拷回原文件；`original` 为空表示原来没有遮罩。

## 训练如何使用灰度

数据读取器保留 masks 的 0–255 灰度，不再把所有非零像素变成 1。
合并图片 alpha 通道时只进一步屏蔽无效区域。训练内部将遮罩拆成非零支持区域及连续权重，
CUDA 与 Vulkan 共用同一训练流程：RGB 光度梯度乘以 `mask / 255`，用于增密的误差图也乘以该权重。
例如同等误差下灰度 51 的像素，其 RGB 梯度只有灰度 255 的约 20%。
深度、法线监督和几何正则不作为实焦算法的调参对象；原有训练选项保持其含义。
这里加权的是实际优化梯度与增密误差图；日志中的原始损失标量仍按原有损失函数汇报。

## 命令行与模型

```text
spirula focus <dataset> --allowed 1.4 --train-side 5760
```

读取 `depths/`，生成并合并 `masks/`。缺少深度时提示先执行 `spirula geometry --depth`。
已有 masks 不会导致跳过生成。模型沿用 Spirula 的模型缓存或界面选定的模型路径，分发包不含深度模型。

## 最终遮罩染色图

计算实焦权重并写入 masks 后，会同时在 `focus_overlay/` 输出 JPEG 染色预览。
染色依据是与原遮罩合并后的最终 masks：白色权重区域保持原色，权重越低蓝色越浓；
权重为零时蓝色混合强度为 65%，仍能看到原图细节。原有 SAM 等遮罩排除的区域同样染色。
预览长边最多 2000 像素，沿用原始栅格方向，不应用 EXIF 旋转；预览不参与训练。
保留子目录，文件名在完整原图名称后追加 `.jpg`，避免不同图片格式同名时覆盖。
完成后打开同目录的 `index.html`，可按文件名搜索、分页浏览、点击单张放大；
每张图的百分比表示最终遮罩的平均权重，页面不依赖网络。

已有最终 masks 时，可单独生成染色图，无需深度或模型，也不会修改 masks 或备份状态：

```text
spirula focus <dataset> --overlay-only
```

`--out-dir` 指定读取的 masks 目录，`--overlay-dir` 指定染色图目录。
缺少遮罩时按完整权重显示；损坏或尺寸不一致的遮罩报错。
染色呈现 masks 的逐像素权重；训练仍须启用数据集遮罩，其他训练屏蔽选项可能进一步限制参与范围。

## Method

1. **Depth.** `spirula geometry --depth` (MoGe-2; `--model moge2-vitl` is the
   one the numbers below come from). Only inverse depth `u = 1/z` up to scale
   is used; the scale cancels.
2. **Edge blur, in pixels.** On the photo downsampled by an integer factor to
   ~3000-4000 px (`focus_measure_factor`), each non-maximum-suppressed edge is
   re-smoothed at s = 1, 1.4, 2, 2.8, 4 px. For a step edge blurred by sigma the
   squared gradient peak is `C^2 / (2 pi (sigma^2 + s^2))`, so `1/m^2` is linear
   in `s^2` and intercept/slope is `sigma^2`. High-ISO noise inflates the fine
   scales and makes blurry regions read sharp; the expected noise energy of
   every scale is measured on the flattest 32 px blocks of the same image per
   brightness level (correlated JPEG noise included), subtracted, and a scale
   enters the fit only where the edge clears it by 2x in amplitude.
3. **Defocus curve.** Thin-lens defocus grows linearly with `|u - u_f|`. Per
   64 inverse-depth slices, the weighted 25th percentile of edge blur (the
   sharpest content at that depth bounds the optics; shading only adds blur)
   is fitted with
   `b(u) = sqrt(b0^2 + (k_near (u - u_f)+)^2 + (k_far (u_f - u)+)^2)`
   by a grid over `u_f` and non-negative least squares with Huber reweighting.
   Slices above ~2 px are down-weighted: the estimator saturates there and the
   plateau they form dragged `u_f` behind the true minimum. Edges that fit
   below 0.2 px are dropped: fine texture (cast bronze, engraving) gives a
   negative variance there, which is a failed fit, not an edge, and it pulled
   `b0` to zero so that ordinary sharpness read as defocus.
4. **Weight.** Defocus `sqrt(b(u)^2 - max(b0, 0.35)^2)` (0.35 px: about the
   sharpest an edge comes out after lens, demosaicing and downsampling),
   converted to pixels at the
   training resolution, against one threshold `--allowed` (default 1.4 px at a
   5760 px long side) with a tanh transition: weight 0.5 at the threshold.

## Numbers

Validated outside this tree on three museum photographs (high ISO, 35-70 mm,
f/5.6-7.1, 6000-8200 px) against 28 hand-labelled regions (sharp / slightly
blurry / blurry): 28/28 with MoGe-2 vit-large depth, 26/28 with vit-base. With
depth from the fp16 Metric3D export the same method was unusable: its output
was quantized to ~0.1 m, 10-25 distinct values per image. On a 1648-image
capture with three lenses the default threshold needed no per-image or
per-lens tuning, and training with the weights left no holes in the subject.

## Known limits

- Glass (crystal balls, display cases) has refracted or reflected content at a
  virtual depth; depth decides there, local sharpness would mislead.
- Two objects at the same depth with different sharpness cannot be separated.
  Local blur evidence can, but it also cannot tell a soft shadow from a
  defocused edge and falsely suppressed in-focus, textureless table tops.
- An image with too few sharp edges to fit keeps weight 1 everywhere.
- Pixels the geometry model left without depth (its border, sky) take the
  weight of their surroundings rather than 0, so the frame edge is not cut.
