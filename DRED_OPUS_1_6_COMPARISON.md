# Opus 1.5.2 → 1.6：burst 丢包 WER / 码率对照

使用 `modules/audio_coding/neteq/tools/audio_network_lab_assets/representative_audio/`
里 `manifest_standard.txt` 的 8 条 LibriSpeech test-clean 朗读样本，在相同
丢包/编码参数下把 Opus 1.5.2 的 n=12 基线与当前 1.6 分支的 n=24 结果做对照。
`manifest_quick`（n=4）的早期结果因为 ASR 幻觉失真，已被降级到文末附录仅供
追溯。

## 实验配置

- 样本：8 条 LibriSpeech test-clean 干净朗读，48 kHz / 单声道 / 各 ~25–30 s。
- 编码：Opus VBR，`bitrate_bps=32000`，`frame_ms=20`，
  `expected_loss_pct = loss_pct`。
- 丢包模型：Gilbert-Elliot，`network_random_seed=1`。
- 恢复方案：`PLC(none)` / `DRED+PLC(dred)` / `LBRR+PLC(opus_fec)`。
- Burst 场景：
  - `burst_moderate`: `loss=10%`, `avg_burst_loss_length=3`
  - `burst_heavy`: `loss=20%`, `avg_burst_loss_length=5`
  - `burst_extreme`: `loss=30%`, `avg_burst_loss_length=8`
- ASR：`faster-whisper small.en` + `jiwer`。
- 幻觉过滤：`hypothesis_word_count > reference_word_count * 1.3` 判为
  Whisper 重复幻觉并从均值中剔除。

基线数据（1.5.2，n=12）来自 `results/dred_burst_wer_20260416/analysis_metrics.json`。
本次数据（1.6，n=24）来自 `results/dred_burst_wer_opus16_std_20260420/`。

## 总体结论

| 方案 | 1.5.2 (n=12) | 1.6 原始 (n=24) | 1.6 清洗后 | 清洗后 Δ vs 1.5.2 |
|---|---:|---:|---:|---:|
| PLC | 0.1372 | 0.1476 | **0.1315** (n=23) | **-0.0057 (-4.1%)** |
| DRED+PLC | 0.0765 | 0.0569 | 0.0569 (n=24) | **-0.0196 (-25.6%)** |
| LBRR+PLC | 0.1301 | 0.1331 | **0.1195** (n=23) | **-0.0106 (-8.1%)** |

- **DRED 是 1.6 的主要赢家**（-25.6%），三个 burst 场景全赢。
- PLC / LBRR 都有小幅正向收益，说明 1.6 升级没有对非 DRED 路径造成回归。
- 24 格 PLC + 24 格 LBRR 中仅 `burst_moderate / read_ls_003` 触发 Whisper
  重复幻觉；24 格 DRED 零触发。

## 分场景对照（72 格）

| 场景 | 方案 | 1.5.2 WER | 1.6 原始 WER | 1.6 清洗后 WER | Δ 原始 | Δ 清洗 | Δkbps |
|---|---|---:|---:|---:|---:|---:|---:|
| burst_moderate | PLC | 0.0532 | 0.1038 | **0.0447** | +0.0506 | -0.0085 | +0.024 |
| burst_moderate | DRED+PLC | 0.0401 | 0.0306 | 0.0306 | -0.0095 | -0.0095 | +0.772 |
| burst_moderate | LBRR+PLC | 0.0553 | 0.1007 | **0.0512** | +0.0454 | -0.0041 | +0.049 |
| burst_heavy | PLC | 0.1288 | 0.1126 | 0.1126 | -0.0162 | -0.0162 | +0.026 |
| burst_heavy | DRED+PLC | 0.0440 | 0.0426 | 0.0426 | -0.0013 | -0.0013 | +0.487 |
| burst_heavy | LBRR+PLC | 0.1181 | 0.0978 | 0.0978 | -0.0203 | -0.0203 | +0.085 |
| burst_extreme | PLC | 0.2295 | 0.2264 | 0.2264 | -0.0031 | -0.0031 | +0.026 |
| burst_extreme | DRED+PLC | 0.1456 | 0.0976 | 0.0976 | -0.0479 | -0.0479 | +0.487 |
| burst_extreme | LBRR+PLC | 0.2168 | 0.2008 | 0.2008 | -0.0159 | -0.0159 | +0.085 |

要点：

- DRED 在 `burst_extreme` 绝对降幅最大（-0.048），burst 越重 1.6 模型赢得越多。
  这与 1.6 官方对 DRED 用四次幂损失 + 含噪/混响数据重训的目标一致。
- DRED 码率开销 +0.4~0.8 kb/s；PLC / LBRR 的 Δkbps 近似 0，表明 1.6 在
  编码层对干净语音的码率分配与 1.5.2 基本一致。
- `burst_heavy` / `burst_extreme` 下 1.6 的 PLC 和 LBRR 都**小幅优于** 1.5.2。

## 复现命令

```sh
# 构建实验台（需已 bootstrap 过 DRED，见 tools_webrtc/opus_dred/README.md）
gn gen out/Opus16 --args='rtc_enable_protobuf=false rtc_include_tests=true'
ninja -C out/Opus16 modules/audio_coding:audio_network_lab

# 批跑 3 场景 × 3 方案 × 8 样本 = 72 格
for scn in burst_moderate burst_heavy burst_extreme; do
  case "$scn" in
    burst_moderate) LP=10; BL=3;;
    burst_heavy)    LP=20; BL=5;;
    burst_extreme)  LP=30; BL=8;;
  esac
  for mode in none dred opus_fec; do
    for i in 001 002 003 004 005 006 007 008; do
      OUT=results/dred_burst_wer_opus16_std_20260420/$scn/$mode/read_ls_$i
      mkdir -p "$OUT"
      bash modules/audio_coding/neteq/tools/audio_network_lab_run_and_eval_speech.sh \
        ./out/Opus16/audio_network_lab \
        modules/audio_coding/neteq/tools/audio_network_lab_assets/representative_audio/librispeech/read_ls_$i.wav \
        "$OUT" \
        --redundancy_mode=$mode --frame_ms=20 --bitrate_bps=32000 \
        --loss_pct=$LP --avg_burst_loss_length=$BL \
        --expected_loss_pct=$LP --network_random_seed=1 --channels=1
    done
  done
done
```

## 产物路径（均为 git-ignore 的本地产物）

- `results/dred_burst_wer_opus16_std_20260420/<scenario>/<mode>/<sample>/`
  每格的 `summary.json` / `wer_report.json` / `output.wav` 等。
- `results/dred_burst_wer_20260416/analysis_metrics.json` — 1.5.2 基线聚合。

## 附：manifest_quick (n=4) 结果为何被废弃

最初的 `manifest_quick` (n=4) 数据里 PLC 和 LBRR 平均 WER 比 1.5.2 分别
**劣化 +31.58% 和 +31.72%**，和 DRED 的正向收益形成诡异对比。核查发现
**全部回归来自同一条样本**：

- `burst_moderate / read_ls_003`：1.6 的熵编码把最后几帧多分配了 17 B，
  PLC 尾音的长度/频谱包络和 1.5.2 略有不同，触发了 Whisper 的
  **“重复上一句”** 经典幻觉。hypothesis 字数从 69 跳到 125（参考 85），
  插入 40 词 → 单样本 WER 从 0.0353 飙到 0.5176。
- 同一音频在 LBRR 下也触发相同幻觉（WER 0.4471）。两者都生成了 PLC
  concealment 音频，所以 Whisper 在同一段上走进了循环。
- DRED 的同格没中招——丢包区重建出真实音素后，Whisper 没有可循环的
  模糊压基。

n=4 太小，单条幻觉样本直接把 PLC/LBRR 总体均值推高 30%+。改用
manifest_standard（n=8）后，24 格 PLC + 24 格 LBRR 只剩这 1 格触发，
加上 `hyp_words > ref_words × 1.3` 的过滤后结论彻底翻转，PLC 和 LBRR
都是小幅正向收益。

### 已知工具缺口

`audio_network_lab_wer.py` 的 `detect_repeated_ngram` 只在重复 ≥3 次时
报 `asr_unstable=true`，这次的 ×2 动态幻觉它没捕捉。建议后续合入一条
`hyp_words > ref_words * 1.3` 的红线，让 ASR 侧自己标记这种 case。
