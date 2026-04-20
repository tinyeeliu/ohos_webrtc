# DRED & audio_network_lab 统一入口

本文档是仓库里 **Opus DRED** 与 **audio_network_lab** 相关工作的唯一入口，
回答四个问题：集成、环境、跑实验、当前结论。

## 相关文档

- [`tools_webrtc/opus_dred/README.md`](./tools_webrtc/opus_dred/README.md) —
  本地 Opus 1.6 + DRED 构建环境的 bootstrap（`third_party/opus` 被
  git-ignore，必须用这个脚本拉）。
- [`modules/audio_coding/neteq/tools/audio_network_lab.md`](./modules/audio_coding/neteq/tools/audio_network_lab.md) —
  `audio_network_lab` 完整手册：所有 flag、脚本、WER/情感评估链路。
- [`DRED_OPUS_1_6_COMPARISON.md`](./DRED_OPUS_1_6_COMPARISON.md) —
  Opus 1.5.2 → 1.6 升级后的 WER/码率对照报告（2026-04 跑的 manifest_standard）。

## 一、快速路径（三步跑通）

```sh
# 1) 本地拉 Opus 1.6 + DRED 权重 + 补丁 BUILD.gn（首次运行）
python3 tools_webrtc/opus_dred/setup_opus_dred.py

# 2) 生成构建目录并编译实验台
gn gen out/AudioLab --args='rtc_enable_protobuf=false rtc_include_tests=false'
ninja -C out/AudioLab modules/audio_coding:audio_network_lab

# 3) 跑一次基线
./out/AudioLab/audio_network_lab \
  --input_pcm=/tmp/input.pcm \
  --output_audio=/tmp/baseline.wav \
  --summary_json=/tmp/baseline.json \
  --redundancy_mode=none
```

更完整的 flag / WER / 情感评估链路见
`modules/audio_coding/neteq/tools/audio_network_lab.md`。

## 二、生产路径 DRED 控制链路

为了让生产代码（非 lab）享受 `webrtc-opus-ng` 级别的自适应能力，当前分支
补齐了三条控制链：

### 1. 编码器内实时自适应（默认开启）

`AudioEncoderOpusImpl::SetProjectedPacketLossRate` 基于
`PacketLossFractionSmoother` 平滑后的丢包率，把 `OPUS_SET_DRED_DURATION`
按下表钳到 `[200 ms, config.dred]`：

| 平滑丢包率 | DRED 深度 |
| --- | --- |
| ≤ 2% | 200 ms |
| ≤ 5% | 300 ms |
| ≤ 10% | 500 ms |
| ≤ 20% | 800 ms |
| > 20% | 1000 ms（libopus 上限）|

更新带 **100 ms 死区**，避免逐帧抖动。相关代码：
`modules/audio_coding/codecs/opus/audio_encoder_opus.{h,cc}` 中的
`AdaptiveDredDurationFrames` / `MaybeUpdateDredDuration`。

### 2. ANA 覆盖通道

`AudioEncoderRuntimeConfig` 新增 `dred_duration_frames`；DRED-aware
controller 可显式下发 0 或正数深度覆盖内部自动映射，负值释放 override。
只要 `config.dred > 0` 且 SDP 协商成功，任何 ANA controller 都可以直接
参与 DRED 控制。

### 3. SDP 版本门控（严格）

编解码器会在 `AppendSupportedEncoders/Decoders` 里上报：

```
usedred=1; opusdredver=2
```

- `kOpusDredLocalVersion = 2` 绑定 **Opus 1.6 DRED 模型**
  （权重 ~600 kB，四次幂训练损失；与 1.5.x 模型码流不兼容）。
- `SdpToConfig` 只在 `usedred=1` 且 `opusdredver` 与本地匹配时开启 DRED。
- 版本不匹配会记录 `LS_WARNING` 并保持 DRED 关闭。
- 缺省 `opusdredver` 被视为版本 1（1.5.x 旧对端），同样落到 mismatch
  分支，避免向无法解码的对端推送冗余数据。

> lab 场景（`audio_network_lab`）会显式关掉 `adaptive_dred`，保证实验
> 结果严格等于传入参数。需要验证自适应链路时可加
> `--use_encoder_adaptive_dred=true`。

### 相关单测

`modules/audio_coding/neteq/tools/audio_network_lab_unittest.cc`：

- `AudioEncoderOpusDredTest.AdaptiveMappingMatchesLossBuckets`
- `AudioEncoderOpusDredTest.SdpUsedredWithMatchingVersionEnablesAdaptiveDred`
- `AudioEncoderOpusDredTest.SdpUsedredWithMismatchedVersionKeepsDredOff`
- `AudioEncoderOpusDredTest.SdpUsedredWithoutVersionAssumesLegacyPeerAndKeepsDredOff`
- `AudioEncoderOpusDredTest.SdpWithoutUsedredKeepsDredOff`

## 三、当前结论（2026-04，Opus 1.6）

基准样本：`modules/audio_coding/neteq/tools/audio_network_lab_assets/representative_audio/manifest_standard.txt`
（8 条 LibriSpeech test-clean 干净朗读）。

编码：Opus VBR, `bitrate_bps=32000`, `frame_ms=20`。
网络：Gilbert-Elliot, `network_random_seed=1`，三档 burst。
ASR：`faster-whisper small.en` + `jiwer`，并按
`hypothesis_word_count > reference_word_count * 1.3` 剔除 Whisper 重复幻觉。

**12 组 / 方案 / 版本平均 WER**（清洗后）：

| 方案 | 1.5.2 (n=12) | 1.6 (n=24, 清洗后) | Δ vs 1.5.2 |
|---|---:|---:|---:|
| PLC | 0.1372 | **0.1315** | -4.1% |
| DRED+PLC | 0.0765 | **0.0569** | **-25.6%** |
| LBRR+PLC | 0.1301 | **0.1195** | -8.1% |

**要点**：

1. **DRED 是 1.6 改动的赢家**，三个 burst 场景全赢（`burst_extreme`
   绝对降幅 -0.048），码率仅 +0.4~0.8 kb/s。
2. PLC / LBRR 也有小幅正收益，说明 1.6 升级没有对非 DRED 路径造成回归。
3. 完整矩阵、ASR 幻觉分析、`manifest_quick` 的失效修正过程见
   [`DRED_OPUS_1_6_COMPARISON.md`](./DRED_OPUS_1_6_COMPARISON.md)。

## 四、常用复现命令

本节只列和 DRED 直接相关的命令；更多 flag（带宽、抖动、固定断连、情感
评估）见 `modules/audio_coding/neteq/tools/audio_network_lab.md`。

```sh
# burst + DRED 自适应
./out/AudioLab/audio_network_lab \
  --input_pcm=/tmp/input.pcm \
  --output_audio=/tmp/out_dred.wav \
  --summary_json=/tmp/out_dred.json \
  --redundancy_mode=dred \
  --loss_pct=20 --avg_burst_loss_length=5 --expected_loss_pct=20 \
  --network_random_seed=1

# 手动钉住 DRED 深度（50 帧 = 500 ms）
./out/AudioLab/audio_network_lab \
  --input_pcm=/tmp/input.pcm \
  --output_audio=/tmp/out_dred_500ms.wav \
  --summary_json=/tmp/out_dred_500ms.json \
  --redundancy_mode=dred --dred_duration_frames=50 \
  --loss_pct=10 --expected_loss_pct=10

# 用 Opus DNN 丢包生成器（更贴近真实网络的 burst 长尾）
./out/AudioLab/audio_network_lab \
  --input_pcm=/tmp/input.pcm \
  --output_audio=/tmp/out_dnn.wav \
  --summary_json=/tmp/out_dnn.json \
  --redundancy_mode=dred --loss_model=dnn_lossgen \
  --loss_pct=20 --expected_loss_pct=20
```

`--loss_model=gilbert_elliot`（默认）参数可控、重复性强，适合做控制变量
基准；`dnn_lossgen` 使用 `third_party/opus/src/dnn/lossgen.c` 的 RNN
逐包决策，更接近真实网络的 burst 长尾分布。

## 五、文档维护约定

- 新的大规模评测结果优先更新本文件第三节（结论摘要）和
  `DRED_OPUS_1_6_COMPARISON.md`（详细数据）。
- 调整 DRED 自适应曲线 / ANA 规则 / SDP 版本号时同步更新本文件
  第二节。Opus 版本每次 roll：
  1. 改 `tools_webrtc/opus_dred/setup_opus_dred.py` 的 URL/SHA-256；
  2. 如果 DRED 模型/码流变，把 `kOpusDredLocalVersion` +1，并在本节
     说明映射（当前：1 = 1.5.x 已退役，2 = 1.6）。
- `third_party/opus` roll 后如果 `dnn/lossgen.c` 的权重或 API 变动，
  抽查一次 `--loss_model=dnn_lossgen` 的 burst 分布是否仍符合预期。
