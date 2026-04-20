# Opus DRED 本地构建辅助

> 顶层入口：[`DRED_AUDIO_LAB.md`](../../DRED_AUDIO_LAB.md)

这个目录提供在全新 checkout 上重建 Opus 1.6 DRED / Deep PLC / OSCE 构建
环境所需的脚手架。`third_party/` 在本仓库 `.gitignore` 中，所以实际的
Opus 源码、DNN 权重和补丁版 `third_party/opus/BUILD.gn` 不能直接纳入
版本库；`setup_opus_dred.py` 会一次性把这三样本地化。

## 一键使用

```sh
# 1. 确保 third_party/opus 目录存在（通过 build.sh 或 gclient sync 创建）。
bash build.sh   # 或者项目既定的同步步骤

# 2. 拉 Opus 1.6 源码 + 内置 DNN 权重，并安装补丁 BUILD.gn。
python3 tools_webrtc/opus_dred/setup_opus_dred.py

# 3. 生成构建目录，编译 audio_network_lab。
gn gen out/AudioLab
ninja -C out/AudioLab modules/audio_coding:audio_network_lab
```

脚本幂等：本地已是 1.6 且 BUILD.gn 无差异时再次运行为 no-op。

## 脚本做了什么

1. 从官方 `downloads.xiph.org` 下载 `opus-1.6.tar.gz`（SHA-256 固定为
   `b7637334...`），覆盖 `third_party/opus/src/`；旧的 `src/` 会被
   移到 `third_party/opus/src.pre-opus-1.5/` 保底。
2. 校验 Opus 1.6 自带的权重文件是否就位（`bbwenet_data.c`、
   `dred_rdovae_*_data.c`、`fargan_data.c`、`lace_data.c`、
   `nolace_data.c`、`plc_data.c` 等）。与 1.5.x 不同，1.6 已经把这些
   多 MB 的 `*_data.c` 直接打包进源码 tarball，不再需要额外下载权重包。
3. 把 `opus.BUILD.gn.patched` 复制到 `third_party/opus/BUILD.gn`，
   原文件会备份为 `BUILD.gn.pre-opus-1.6`。

## 从 1.5.x 升级的语义差异

- DRED 模型重训、权重体积缩到约 600 kB，但码流**不向后兼容**。
- 配套的 `kOpusDredLocalVersion` 已从 1 升到 2，SDP `opusdredver` 随之
  广播；遇到 1.5.x 旧对端会自动落到 mismatch 分支，保持 DRED 关闭。
  详见 [`DRED_AUDIO_LAB.md`](../../DRED_AUDIO_LAB.md) 第二节。

## 文件

- `setup_opus_dred.py` — 上面描述的 bootstrap 脚本。
- `opus.BUILD.gn.patched` — 带 `ENABLE_DEEP_PLC` / `ENABLE_DRED` /
  `ENABLE_OSCE` 的 GN 目标定义，包含 Opus 1.6 的 `src/dnn/`
  （含新增的 BWE 权重 `bbwenet_data.c`）和 x86 / ARM SIMD 子集
  （`opus_sse2` / `opus_sse41` / `opus_avx2` 包含 1.6 新增的
  `pitch_avx` / `inner_product_FLP_avx2` / `NSQ_del_dec_avx2`，
  ARM 下启用 NEON）。

## 需要访问的外部主机

- `downloads.xiph.org` — Opus 1.6 源码 tarball；1.6 不再需要
  `media.xiph.org`。

如果这台机器不能直达 `downloads.xiph.org`，手动把 tarball 放到临时目录
后，改 `setup_opus_dred.py` 里 `urllib.request.urlopen` 的 URL 指向
本地镜像即可，校验逻辑会继续生效。

## 验证 DRED 已生效

构建完成后跑一组基线/FEC/DRED 三选一，对照 `concealed_samples`
判断 DRED 是否在恢复：

```sh
python3 -c "
import math, struct
with open('/tmp/sine10s.pcm','wb') as f:
    for i in range(48000*10):
        t = i/48000.0
        v = int(10000 * (0.6*math.sin(2*math.pi*440*t) +
                         0.3*math.sin(2*math.pi*660*t)))
        f.write(struct.pack('<h', v))
"

for mode in none opus_fec dred; do
  ./out/AudioLab/audio_network_lab \
    --input_pcm=/tmp/sine10s.pcm \
    --output_audio=/tmp/out_${mode}.wav \
    --summary_json=/tmp/summary_${mode}.json \
    --redundancy_mode=$mode \
    --loss_pct=20 --frame_ms=20 --bitrate_bps=24000 \
    --expected_loss_pct=20 --network_random_seed=42
done
```

正常结果示例（20% 丢包，seed 42，10 s 正弦）：

| 方案 | 码率 | concealed_samples | concealment_events |
|------|-----:|------------------:|-------------------:|
| none | 24.2 kbps | 98 400 | 87 |
| opus_fec | 24.2 kbps | 95 520 | 86 |
| **dred** | 28.9 kbps | 约 4 800 | 7 |

DRED 把 PLC 的 concealment 量压低约 95%，代价约 +4.7 kb/s。

## 参考

- [`DRED_AUDIO_LAB.md`](../../DRED_AUDIO_LAB.md) — 集成/实验/结论统一入口。
- [`../../DRED_OPUS_1_6_COMPARISON.md`](../../DRED_OPUS_1_6_COMPARISON.md)
  — 1.5.2 → 1.6 的 burst 丢包对照报告。
