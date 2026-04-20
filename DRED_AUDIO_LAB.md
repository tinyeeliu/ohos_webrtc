# DRED & Audio Network Lab Unified Entry

这个文档是当前仓库里 **Opus DRED** 与 **audio_network_lab** 的统一入口，方便新人和评审快速定位：

- 设计与集成：DRED 在 WebRTC 中如何接入
- 本地环境：如何一键准备 Opus DRED 编译环境
- 实验评测：如何跑 PLC / DRED / LBRR 的随机与 burst 丢包对比
- 最新结果：当前分支上的关键 WER 与码率结论
- **版本升级对比（2026-04-20）**：Opus 1.5.2 → 1.6 的 burst WER/码率对照见
  [`DRED_OPUS_1_6_COMPARISON.md`](./DRED_OPUS_1_6_COMPARISON.md)。

## 一、先看这里（按顺序）

1. **总体实验入口（快速上手）**  
   [`AUDIO_NETWORK_LAB.md`](./AUDIO_NETWORK_LAB.md)
2. **完整实验手册（参数、脚本、WER/情感评估）**  
   [`modules/audio_coding/neteq/tools/audio_network_lab.md`](./modules/audio_coding/neteq/tools/audio_network_lab.md)
3. **DRED 本地 bootstrap（第三方 Opus + DNN 数据）**  
   [`tools_webrtc/opus_dred/README.md`](./tools_webrtc/opus_dred/README.md)

## 二、生产路径 DRED 控制链路

为了让生产代码（非 lab）也能享受 `webrtc-opus-ng` 级别的自适应能力，当前分支补齐了三条控制链：

1. **编码器内实时自适应**（新增，默认开启）  
   `AudioEncoderOpusImpl::SetProjectedPacketLossRate` 基于 `PacketLossFractionSmoother` 给出的平滑丢包率，把 `OPUS_SET_DRED_DURATION` 按下表钳到 `[200 ms, config.dred]`：

   | 平滑丢包率 | DRED 深度 |
   | --- | --- |
   | ≤ 2% | 200 ms |
   | ≤ 5% | 300 ms |
   | ≤ 10% | 500 ms |
   | ≤ 20% | 800 ms |
   | > 20% | 1000 ms（libopus 上限）|

   更新带 **100 ms 死区**，避免逐帧抖动。相关代码：`modules/audio_coding/codecs/opus/audio_encoder_opus.{h,cc}` 里的
   `AdaptiveDredDurationFrames` / `MaybeUpdateDredDuration`。

2. **ANA 覆盖通道**  
   `AudioEncoderRuntimeConfig` 新增 `dred_duration_frames`；DRED-aware controller 可显式下发 0/正数深度，覆盖内部自动映射。负值释放 override，自动映射重新接管。只要 `config.dred > 0` 且 SDP 协商成功，任何 ANA controller 都可以直接参与 DRED 控制。

3. **SDP 版本门控（严格）**  
   编解码器会在 `AppendSupportedEncoders/Decoders` 里上报：

   ```
   usedred=1; opusdredver=2
   ```

   当前分支已升级到 **Opus 1.6**，`kOpusDredLocalVersion = 2` 绑定 1.6 DRED 模型（权重 ~600 kB，使用四次幂训练损失，与 1.5.x 模型码流不兼容）。`SdpToConfig` 只有在 `usedred=1` 且 `opusdredver` 与本地匹配时才会开启 DRED；版本不匹配会记录 `LS_WARNING` 并保持 DRED 关闭；缺省 `opusdredver` 会被视为版本 1（1.5.x 旧对端），同样落到 mismatch 分支，避免向无法解码的对端推送冗余数据。

> lab 场景（`audio_network_lab`）沿用“注入丢包率 → 固定深度”语义：`CreateOpusConfig` 会显式关掉 `adaptive_dred`，保证实验结果严格对应传入参数；需要 A/B 验证自适应链路时可把该开关打开。

相关单测（`modules/audio_coding/neteq/tools/audio_network_lab_unittest.cc`）：

- `AudioEncoderOpusDredTest.AdaptiveMappingMatchesLossBuckets`
- `AudioEncoderOpusDredTest.SdpUsedredWithMatchingVersionEnablesAdaptiveDred`
- `AudioEncoderOpusDredTest.SdpUsedredWithMismatchedVersionKeepsDredOff`
- `AudioEncoderOpusDredTest.SdpUsedredWithoutVersionAssumesLegacyPeerAndKeepsDredOff`
- `AudioEncoderOpusDredTest.SdpWithoutUsedredKeepsDredOff`

## 三、当前测试结论（代表语音 + burst 丢包）

数据来源：

- 对比总表：`results/dred_burst_wer_codec_compare_20260416.md`
- 库内调控（VBR burst 复核）：`results/dred_burst_wer_vbr_libadaptive_20260417/report.md`
- 原始结果目录：
  - `results/dred_burst_wer_20260416/` (VBR)
  - `results/dred_burst_wer_cbr_20260416/` (CBR)
  - `results/dred_burst_wer_cvbr_20260416/` (CVBR)

关键结论（`manifest_quick`，4 条样本均值）：

1. **WebRTC Opus 默认是 VBR，不是 CBR**。CBR 需显式开启（`cbr=1` / `--cbr=true`）。
2. **CBR 下三种方案码率基本锁定在 31.99 kb/s**（目标 32 kb/s）。
3. 在本批样本里，**DRED+PLC 在 burst 场景的 WER 最优**：
   - moderate: PLC `0.050279` / DRED+PLC `0.047653` / LBRR+PLC `0.055314` (CBR)
   - heavy: PLC `0.118702` / DRED+PLC `0.050576` / LBRR+PLC `0.106295` (CBR)
   - extreme: PLC `0.238967` / DRED+PLC `0.099135` / LBRR+PLC `0.211638` (CBR)
4. **CVBR 在本批语料下与 VBR 结果一致**（WER/码率几乎重合），后续若要拉开差异，建议使用更长或更复杂语料再测。
5. 新增复核：在 `manifest_quick` 的 VBR burst 矩阵中，开启
   `--use_encoder_adaptive_dred=true`（库内 runtime 调控）得到的 WER 与
   既有 VBR 报告一致；原因是这组 loss 桶下，库内自适应最终落到同一组
   DRED 深度（50/80/100 帧）。

## 四、最小复现实验命令

```sh
# 1) 准备 DRED 依赖（只需首次）
python3 tools_webrtc/opus_dred/setup_opus_dred.py

# 2) 编译实验工具
gn gen out/AudioLab
ninja -C out/AudioLab audio_network_lab audio_network_lab_unittests

# 3) 跑一个 burst + CBR + DRED 的示例（Gilbert-Elliot 可控基准）
./out/AudioLab/audio_network_lab \
  --input_pcm=/tmp/input.pcm \
  --output_audio=/tmp/out_dred.wav \
  --summary_json=/tmp/out_dred.json \
  --redundancy_mode=dred \
  --cbr=true \
  --loss_pct=20 \
  --avg_burst_loss_length=5 \
  --expected_loss_pct=20

# 4) 跑同样配置，但换成 Opus DNN 丢包生成器（真实网络拟真）
./out/AudioLab/audio_network_lab \
  --input_pcm=/tmp/input.pcm \
  --output_audio=/tmp/out_dred_dnn.wav \
  --summary_json=/tmp/out_dred_dnn.json \
  --redundancy_mode=dred \
  --cbr=true \
  --loss_pct=20 \
  --loss_model=dnn_lossgen \
  --expected_loss_pct=20
```

`--loss_model` 选项：

- `gilbert_elliot`（默认）：两状态 Markov 模型，参数完全可控，可复现性强，适合做"控制变量"基准
- `dnn_lossgen`：直接调用 `third_party/opus/src/dnn/lossgen.c`，基于 Xiph 团队从真实网络 trace 训练的小型 RNN 逐包决策是否丢包；产生的 burst 更接近真实互联网的长尾分布（绝大多数为短 burst，偶发 5~10 包长 burst），适合验证 DRED/FEC/PLC 在"更真实网络"下的效果。摘要 JSON 会输出 `network.loss_model` 与 `network_stats.dnn_lossgen_dropped_packets`。

## 五、文档维护建议

- 有新的大规模评测结果时，优先更新：
  - 本文档（统一入口中的“当前测试结论”）
  - `results/dred_burst_wer_codec_compare_*.md`（详细数据）
- 如果 DRED 策略（如自适应深度曲线、ANA 覆盖规则、SDP 版本号）有变更，同步更新：
  - 本文件“生产路径 DRED 控制链路”章节
  - `modules/audio_coding/neteq/tools/audio_network_lab.md`
  - 如果改动涉及 DRED 模型/ABI（Opus 1.5 → 1.6 等），记得同步 `kOpusDredLocalVersion` 并在此处说明。历史迁移：1 = 1.5.x 模型（已退役），2 = 1.6 模型（当前）。下一次 roll 时直接 +1 并更新 `tools_webrtc/opus_dred/setup_opus_dred.py` 的 URL/sha256。
- 如果 Opus 上游对 `dnn/lossgen.c`/`lossgen_data.c` 的权重或 API 做了更新，`third_party/opus` roll 之后需要在这里记录一次，并抽查 `--loss_model=dnn_lossgen` 下的 burst 分布是否仍符合预期
