# audio_network_lab

`audio_network_lab` 是本仓库里用来本地复现 Opus/WebRTC 音频传输问题的实验
台，把一条固定链路拉起来：

```
PCM 输入 → Opus/WebRTC 编码 → 网络劣化 → NetEq 解码 → 输出音频 + 统计摘要
```

它不是完整通话栈，也不依赖 PeerConnection，目标是把以下机制单独拉出来
稳定复现：Opus 编码参数、Opus in-band FEC、RED 冗余、NetEq 在抖动/
乱序/丢包/固定断连下的恢复、PLC / concealment / accelerate / preemptive
expand / secondary decode，以及传输后语音的 WER 与情感归类稳定性。

> DRED 相关的集成、版本门控、跨版本对照和生产路径策略统一放到
> [`DRED_AUDIO_LAB.md`](../../../../DRED_AUDIO_LAB.md)。本文件专注工具本身。

## 边界条件

- 仅支持 `--codec=opus`。
- 输入 PCM 固定为 16-bit signed little-endian / 48000 Hz。
- `--redundancy_mode = {none, opus_fec, red, dred}` 全部可用；`dred` 依赖
  本地先跑过
  [`tools_webrtc/opus_dred/setup_opus_dred.py`](../../../../tools_webrtc/opus_dred/README.md)。
- Lab 默认关掉 `AudioEncoderOpusConfig::adaptive_dred`，让 DRED 深度
  严格等于 `summary.encoder.dred_duration_frames`；如果要验证
  「库内 runtime 调控」，加 `--use_encoder_adaptive_dred=true`。
- 选 `dred` 且不传 `--dred_duration_frames` 时会按
  `max(--expected_loss_pct, --loss_pct)` 自适应映射到 20/30/50/80/100
  帧（单帧 10 ms）；`summary.json` 里的 `encoder.dred_duration_source`
  会标注 `auto` 或 `flag`。
- 情感评估依赖本机 `.cache/` 下的 TESS 与 `emotion2vec`，体积大，不进
  Git。由下面的 Python 环境脚本按需拉取。

## 目录里的工具

- [`audio_network_lab`](./audio_network_lab_main.cc) — host 侧实验可执行
  文件（构建目标 `modules/audio_coding:audio_network_lab`）。
- [`audio_network_lab_prepare_input.py`](./audio_network_lab_prepare_input.py) —
  把 wav 转成链路需要的 pcm 并复制参考文本。
- [`audio_network_lab_report.py`](./audio_network_lab_report.py) —
  解析 `summary.json` 给编码/网络/FEC/PLC/NetEq 汇总。
- [`audio_network_lab_wer.py`](./audio_network_lab_wer.py) —
  跑 Whisper ASR + jiwer，输出 `wer_report.json`。
- [`audio_network_lab_run_and_eval_speech.sh`](./audio_network_lab_run_and_eval_speech.sh) —
  一条命令串起「准备输入 → 跑实验 → 解析 summary → ASR/WER」。
- 情感评估：
  [`audio_network_lab_tess_emotion_baseline.py`](./audio_network_lab_tess_emotion_baseline.py) /
  [`..._eval.py`](./audio_network_lab_tess_emotion_eval.py) 及其 shell wrapper，
  分别对 TESS 干净语料和弱网退化语料跑 `emotion2vec`。
- 环境与校验：
  [`audio_network_lab_setup_python_envs.sh`](./audio_network_lab_setup_python_envs.sh) /
  [`audio_network_lab_check_python_envs.sh`](./audio_network_lab_check_python_envs.sh) /
  [`audio_network_lab_download_emotion_assets.py`](./audio_network_lab_download_emotion_assets.py) /
  [`audio_network_lab_requirements_asr.txt`](./audio_network_lab_requirements_asr.txt) /
  [`audio_network_lab_requirements_emotion.txt`](./audio_network_lab_requirements_emotion.txt)。

波形对比工具（`audio_network_lab_compare.py` / `audio_network_lab_gen_report.py` /
`audio_network_lab_run_and_analyze.sh`）保留但不推荐作为主路径。

## 样本与本机缓存

- 随仓库附带的代表语音在
  [`audio_network_lab_assets/representative_audio`](./audio_network_lab_assets/representative_audio)
  ，目录下有 `manifest_quick.txt`（4 条，冒烟用）、
  `manifest_standard.txt`（8 条，默认基线）、`manifest_full.txt`
  （全部），每条 wav 旁边都有 `*_reference.txt` 参考文本。
- 不随 Git 的资源：TESS 语料 + `iic/emotion2vec_plus_large` 模型（合
  计 ~2 GB），默认放在仓库根的 `.cache/`（已被 `.gitignore`）。用下面
  的环境脚本拉取：

```sh
# 建情感环境并一次性下载 TESS + emotion2vec
bash modules/audio_coding/neteq/tools/audio_network_lab_setup_python_envs.sh \
  --emotion --download-emotion-assets

# 只补资源（已经有 .venv_emotion）
bash modules/audio_coding/neteq/tools/audio_network_lab_setup_python_envs.sh \
  --download-emotion-assets
```

下载后默认路径：`.cache/datasets/tess/tess`（TESS）、
`.cache/modelscope/models/iic/emotion2vec_plus_large`（模型）。
TESS 来自 Hugging Face `myleslinder/tess`（CC BY-NC 4.0），模型来自
ModelScope `iic/emotion2vec_plus_large`。

## Python 环境

仓库根目录约定两个本地 venv：

- `.venv_asr/` — ASR / WER（`faster-whisper`、`jiwer`）。
- `.venv_emotion/` — 情感评估（`emotion2vec`、`modelscope`）。

```sh
# 一次创建两个
bash modules/audio_coding/neteq/tools/audio_network_lab_setup_python_envs.sh --all

# 单独创建
bash modules/audio_coding/neteq/tools/audio_network_lab_setup_python_envs.sh --asr
bash modules/audio_coding/neteq/tools/audio_network_lab_setup_python_envs.sh --emotion

# 校验
bash modules/audio_coding/neteq/tools/audio_network_lab_check_python_envs.sh
```

目录里的 shell wrapper 只按下面两步选 Python：
1. 环境变量 `ASR_PYTHON_BIN` / `EMOTION_PYTHON_BIN`；
2. 本仓库的 `.venv_asr/bin/python` / `.venv_emotion/bin/python`。
找不到会直接报错，不会静默退化到系统 Python。

## 构建

本仓库已经用 `build.sh` 把 `ninja` / `gn` / `depot_tools` /
`third_party` 的初始化都做了；如果你是从零开始，先跑 `bash build.sh`
补齐依赖，再进入下面的步骤。

```sh
# 1) 首次跑 DRED 实验前
python3 tools_webrtc/opus_dred/setup_opus_dred.py

# 2) 生成构建目录（关闭用不上的子系统）
gn gen out/AudioLab --args='
  rtc_enable_protobuf=false
  rtc_include_tests=true
  rtc_build_examples=false
  rtc_build_tools=false
  rtc_build_libvpx=false
  rtc_libvpx_build_vp9=false
  rtc_include_dav1d_in_internal_decoder_factory=false
  enable_libaom=false
  rtc_enable_google_benchmarks=false
'

# 3) 编译实验台 + 单测
ninja -C out/AudioLab modules/audio_coding:audio_network_lab
ninja -C out/AudioLab modules/audio_coding:audio_network_lab_unittests

# 4) 跑单测
./out/AudioLab/audio_network_lab_unittests
```

macOS 本机构建额外建议：

```sh
sysroot="$(xcrun --sdk macosx --show-sdk-path)"
clang_base_path="//buildtools/local_xcode_compat"
use_custom_libcxx=false
use_lld=false
gtest_enable_absl_printers=false
```

预期单测覆盖：baseline / Opus FEC / RED / 固定 outage / 非法 burst
参数 / DRED 自适应（`auto`）/ DRED `--dred_duration_frames` 覆盖 /
SDP 版本门控。

## 命令速查

### 基线

```sh
python3 -c 'import math,struct; p="/tmp/lab_input.pcm"; \
  open(p,"wb").write(b"".join(struct.pack("<h", int(12000*math.sin(2*math.pi*440*i/48000))) for i in range(48000))); \
  print(p)'

./out/AudioLab/audio_network_lab \
  --input_pcm=/tmp/lab_input.pcm \
  --output_audio=/tmp/lab_baseline.wav \
  --summary_json=/tmp/lab_baseline.json \
  --redundancy_mode=none
```

### Opus FEC / RED / 固定断连

```sh
./out/AudioLab/audio_network_lab \
  --input_pcm=/tmp/lab_input.pcm --redundancy_mode=opus_fec \
  --expected_loss_pct=10 --loss_pct=10 \
  --summary_json=/tmp/lab_fec.json --output_audio=/tmp/lab_fec.wav

./out/AudioLab/audio_network_lab \
  --input_pcm=/tmp/lab_input.pcm --redundancy_mode=red \
  --loss_pct=15 \
  --summary_json=/tmp/lab_red.json --output_audio=/tmp/lab_red.wav

./out/AudioLab/audio_network_lab \
  --input_pcm=/tmp/lab_input.pcm \
  --fixed_loss_events_ms=300:200,1200:150 \
  --summary_json=/tmp/lab_outage.json --output_audio=/tmp/lab_outage.wav
```

### DRED

```sh
# 自适应（默认）
./out/AudioLab/audio_network_lab \
  --input_pcm=/tmp/lab_input.pcm --redundancy_mode=dred \
  --expected_loss_pct=20 --loss_pct=20 --avg_burst_loss_length=5 \
  --summary_json=/tmp/lab_dred.json --output_audio=/tmp/lab_dred.wav

# 钉住 DRED 深度（50 帧 = 500 ms）
./out/AudioLab/audio_network_lab \
  --input_pcm=/tmp/lab_input.pcm --redundancy_mode=dred \
  --dred_duration_frames=50 --expected_loss_pct=10 --loss_pct=10 \
  --summary_json=/tmp/lab_dred_500ms.json --output_audio=/tmp/lab_dred_500ms.wav

# 用「库内 runtime 调控」而不是 lab 的固定深度
./out/AudioLab/audio_network_lab \
  --input_pcm=/tmp/lab_input.pcm --redundancy_mode=dred \
  --expected_loss_pct=20 --loss_pct=20 --avg_burst_loss_length=5 \
  --use_encoder_adaptive_dred=true \
  --summary_json=/tmp/lab_dred_runtime.json --output_audio=/tmp/lab_dred_runtime.wav
```

### DNN 丢包生成器

```sh
./out/AudioLab/audio_network_lab \
  --input_pcm=/tmp/lab_input.pcm --redundancy_mode=dred \
  --loss_model=dnn_lossgen --loss_pct=10 --expected_loss_pct=10 \
  --summary_json=/tmp/lab_dnn.json --output_audio=/tmp/lab_dnn.wav
```

输出里会额外带 `network.loss_model="dnn_lossgen"` 与
`network_stats.dnn_lossgen_dropped_packets`。此模式下
`SimulatedNetwork` 自身的随机丢包率被置 0，丢包决策完全交给
`third_party/opus/src/dnn/lossgen.c` 的 RNN；带宽 / 时延 / 抖动仍然
正常叠加。模型权重内嵌在 libopus 中，不需要额外资源。

### 端到端跑 WER

```sh
modules/audio_coding/neteq/tools/audio_network_lab_run_and_eval_speech.sh \
  ./out/AudioLab/audio_network_lab \
  modules/audio_coding/neteq/tools/audio_network_lab_assets/representative_audio/librispeech/read_ls_001.wav \
  /tmp/audio_network_lab_ls001 \
  --channels=1 --redundancy_mode=opus_fec \
  --expected_loss_pct=10 --loss_pct=10
```

产物：`input.pcm` / `output.wav` / `summary.json` / `summary_report.json` /
`wer_report.json` / `reference.txt` / `hypothesis.txt` / `textlog.txt`。

只想对已有 wav 跑一次 WER：

```sh
.venv_asr/bin/python \
  modules/audio_coding/neteq/tools/audio_network_lab_wer.py \
  --output_audio /tmp/audio_network_lab_ls001/output.wav \
  --reference_audio modules/audio_coding/neteq/tools/audio_network_lab_assets/representative_audio/librispeech/read_ls_001.wav \
  --report_json /tmp/audio_network_lab_ls001/wer_report.json
```

## 网络模型

`audio_network_lab` 内置 `call/simulated_network.h` 的 `SimulatedNetwork`，
在容量排队后叠加基础时延 + 高斯抖动，支持以下控制：

| flag | 含义 |
|---|---|
| `--bandwidth_kbps` | 链路容量（kbps）；`0` 表示不限速 |
| `--delay_ms` | 单向基础时延（ms） |
| `--jitter_stddev_ms` | 时延抖动标准差（高斯，ms） |
| `--loss_pct` | 目标丢包率（%） |
| `--loss_model` | `gilbert_elliot`（默认）或 `dnn_lossgen` |
| `--avg_burst_loss_length` | GE 平均 burst 长度，需 `> 1` 才生效；`dnn_lossgen` 忽略 |
| `--allow_reorder` | 允许乱序 |
| `--queue_len_packets` | 最大排队包数；`0` 表示不限制 |
| `--fixed_loss_events_ms` | 固定断连 `start:duration,start:duration`（ms） |
| `--network_random_seed` | 随机种子，用于复现 |

注意事项：

- GE 模型需要 `avg_burst_loss_length > ceil(p/(1-p))`；不满足会直接
  报错，这是预期行为（避免 SimulatedNetwork 内部断言）。
- `dnn_lossgen` 内部调用 libc `rand()`，lab 启动时会用
  `network_random_seed` 对其 `srand()`，同一 seed 两次运行得到
  完全相同的 burst 序列（WAV bit-for-bit 一致）。
- 做可复现实验时把这五样都固定：输入 PCM、编码参数、网络参数、
  `loss_model` 与 `network_random_seed`。
- GE 参数可控、分布均匀，适合做控制变量基线；`dnn_lossgen` 的 burst
  更接近真实互联网的长尾，适合做「真实网络拟真」对比。

## 推荐实验流程

固定一组代表语料后，按顺序跑：

1. **baseline** — 确认编码/解码链路无异常。
2. **10 % uniform loss** — 对比 `redundancy_mode=none` 与 `opus_fec`。
3. **15 %–20 % loss + red** — 看 `secondary_decoded_rate` 与 WER 变化。
4. **burst / outage** — 看 `concealed_samples`、`interruption_count`、
   `wer` 上升幅度。
5. **delay + jitter (+reorder)** — 看 `jitter_buffer_*`、
   `accelerate_rate`、`preemptive_expand_rate` 与 ASR 稳定性。
6. **TESS clean vs degraded** — 情感分类 `accuracy_drop_pp` 与
   `macro_f1`。

每次都保留：运行命令 / `summary.json` / `summary_report.json` /
`wer_report.json` /（情感）`scenario_summary.json` / 输出 wav。

建议结果目录：

```
results/audio_network_lab_runs/<RUN_ID>/
results/audio_network_lab_emotion_eval/<RUN_ID>/
```

`<RUN_ID>` 约定 `<date>_<audio_or_dataset>_<scenario>_<strategy>`，
例如 `20260420_librispeech_uniform10_opus_fec`。

## 结果解读速查

- **链路正常** — `decoded_duration_ms == input_duration_ms`
- **恢复压力** — `concealment_events` / `concealed_samples`
- **FEC/RED 生效** — `fec_packets_received` / `max_secondary_decoded_rate`
- **缓冲频繁调整** — `accelerate_rate` / `preemptive_expand_rate` /
  `jitter_buffer_target_delay_ms`
- **用户体感风险** — `interruption_count` / `total_interruption_duration_ms` /
  `wer`
- **ASR 异常** — `asr_unstable=true`，若伴随重复 n-gram，输出音频
  通常已明显影响转写稳定性

## 常见问题

- **`gn gen` 报 benchmark / 第三方目录缺失**：仓库同步不完整。先跑
  `bash build.sh`（或本项目约定的同步步骤）补齐 `third_party/`。
- **DRED 不工作 / 链接时找不到符号**：先跑
  `python3 tools_webrtc/opus_dred/setup_opus_dred.py`，确保
  `third_party/opus/BUILD.gn` 是补丁版。
- **WER 升高但 `concealed_samples` 不高**：多半是时域拉伸 / 编码失真，
  先看 `accelerate_rate`、`preemptive_expand_rate` 和 `jitter_buffer_*`。
- **情感评估结果抖动大**：先确认 `clean` 基线是否稳定、
  `--network_random_seed` 是否固定、样本量是否太小（冒烟样本不用于结论）。
- **结果目录膨胀**：冒烟阶段只保留 `*.json`；正式对比保留 wav + textlog；
  `.gitignore` 已经把 `results/` 挡在版本库之外。

## TESS 情感评估

目标：（1）测 `emotion2vec` 在当前环境和数据集上的基线；（2）比较同一
批样本经过弱网后，情感标签退化多少。

```sh
# 冒烟 baseline（干净音频）
modules/audio_coding/neteq/tools/audio_network_lab_run_tess_emotion_baseline.sh \
  --limit 32 --run-name audio_network_lab_tess_baseline_smoke

# 冒烟弱网回归
modules/audio_coding/neteq/tools/audio_network_lab_run_tess_emotion_eval.sh \
  --lab-bin ./out/AudioLab/audio_network_lab \
  --limit 24 --run-name audio_network_lab_tess_eval_smoke
```

正式跑去掉 `--limit`，可按需覆盖 `--bitrate-bps` / `--frame-ms` /
`--channels` / `--network-random-seed`。默认场景：`clean` /
`uniform_10` / `uniform_20` / `burst_moderate` / `burst_heavy` /
`delay_jitter_10`。产物包含 `prepared_inputs/` / `generated/` /
`summaries/` / `emotion_predictions.csv` / `scenario_summary.json` /
`report.md`，看 `accuracy` / `accuracy_drop_pp` / `macro_f1` 即可。
