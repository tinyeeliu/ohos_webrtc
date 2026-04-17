# audio_network_lab

`audio_network_lab` 是本仓库中的 host 侧音频传输实验台，用来在本地构建一条固定、可重复的验证链路：

`PCM 输入 -> Opus/WebRTC 编码 -> 网络劣化 -> NetEq 解码 -> 输出音频 + 统计摘要`

这条链路不是完整通话栈，也不依赖 PeerConnection。它的目标是把你真正关心的机制单独拉出来，稳定复现实验条件，重点观察：

- Opus 编码参数变化
- WebRTC Opus in-band FEC
- RED 冗余
- NetEq 在抖动、乱序、丢包、固定断连下的恢复行为
- PLC / concealment / accelerate / preemptive expand / secondary decode
- 传输后语音的 WER
- 传输后语音的情感归类稳定性

如果你的目标是评估“人还能不能听清”和“模型还能不能识别正确语义/情绪”，推荐的观察顺序是：

1. `summary.json`
2. `wer_report.json`
3. 情感评估输出 `summary.json` / `scenario_summary.json`

## 功能边界

当前版本的约束如下：

- 仅支持 `--codec=opus`
- 输入 PCM 固定为 `16-bit signed little-endian`
- 输入采样率固定为 `48000 Hz`
- `--redundancy_mode={none,opus_fec,red,dred}` 全部可用。`dred` 依赖
Opus 1.5+ Deep REDundancy，需要先执行
`python3 tools_webrtc/opus_dred/setup_opus_dred.py` 完成本地初始化；
详见 `[tools_webrtc/opus_dred/README.md](../../../../tools_webrtc/opus_dred/README.md)`
与统一入口 `[DRED_AUDIO_LAB.md](../../../../DRED_AUDIO_LAB.md)`。
- 选择 `--redundancy_mode=dred` 时，recovery 深度默认按
`max(--expected_loss_pct, --loss_pct)` 做自适应映射（20/30/50/80/100
帧，单帧 10 ms）。若想手动钉死，可加 `--dred_duration_frames=N`
（`N∈[1,100]`，单位 10 ms）。`summary.json` 的
`encoder.dred_duration_frames` / `encoder.dred_duration_source`
会如实记录最终取值和来源（`auto` 或 `flag`）。
- lab 默认会把 `AudioEncoderOpusConfig::adaptive_dred` 关掉，
让 DRED 深度严格等于 `summary.encoder.dred_duration_frames`；
生产路径（非 lab）默认开启自适应，由编码器基于
`OnReceivedUplinkPacketLossFraction` 的平滑丢包率调整深度，参见
`DRED_AUDIO_LAB.md` 的「生产路径 DRED 控制链路」章节。
- 若要在 lab 中切到“库内调控”模式，可加
`--use_encoder_adaptive_dred=true`。该模式下 `summary.json` 会额外记录
`encoder.use_encoder_adaptive_dred=true`，用于区分“lab 固定深度”与
“库内 runtime 自适应”。
- 情感评估依赖本机 `.cache/` 下的 TESS 与 `emotion2vec` 模型（体积大，不在 Git 中；用环境脚本下载，见「本仓库 Python 环境」）

## 目录内容

本目录把“运行链路”、“解析结果”、“WER 评估”、“情感评估”、“环境引导”拆开，避免把所有逻辑都堆到一个入口里。

核心工具：

- [audio_network_lab](/Users/leyang/Projects/ohos_webrtc/out/AudioLab/audio_network_lab)
Host 侧实验可执行文件。
- [audio_network_lab_report.py](/Users/leyang/Projects/ohos_webrtc/modules/audio_coding/neteq/tools/audio_network_lab_report.py)
读取 `summary.json`，输出编码、网络、FEC、PLC、NetEq 关键结论。
- [audio_network_lab_prepare_input.py](/Users/leyang/Projects/ohos_webrtc/modules/audio_coding/neteq/tools/audio_network_lab_prepare_input.py)
把 `wav` 准备成实验链路所需的 `pcm`，并复制参考文本。
- [audio_network_lab_wer.py](/Users/leyang/Projects/ohos_webrtc/modules/audio_coding/neteq/tools/audio_network_lab_wer.py)
对单次传输结果做参考文本对齐、Whisper 转写和 WER 统计。
- [audio_network_lab_run_and_eval_speech.sh](/Users/leyang/Projects/ohos_webrtc/modules/audio_coding/neteq/tools/audio_network_lab_run_and_eval_speech.sh)
一次性完成“准备输入 -> 运行实验 -> 解析 summary -> 跑 ASR/WER”。
- [audio_network_lab_tess_emotion_baseline.py](/Users/leyang/Projects/ohos_webrtc/modules/audio_coding/neteq/tools/audio_network_lab_tess_emotion_baseline.py)
用 `emotion2vec` 在干净 TESS 上跑情感分类基线。
- [audio_network_lab_tess_emotion_eval.py](/Users/leyang/Projects/ohos_webrtc/modules/audio_coding/neteq/tools/audio_network_lab_tess_emotion_eval.py)
用 `audio_network_lab` 生成弱网音频，再在 TESS 上跑情感分类退化评估。
- [audio_network_lab_run_tess_emotion_baseline.sh](/Users/leyang/Projects/ohos_webrtc/modules/audio_coding/neteq/tools/audio_network_lab_run_tess_emotion_baseline.sh)
选择本仓库情感评估 Python 环境并运行基线脚本。
- [audio_network_lab_run_tess_emotion_eval.sh](/Users/leyang/Projects/ohos_webrtc/modules/audio_coding/neteq/tools/audio_network_lab_run_tess_emotion_eval.sh)
选择本仓库情感评估 Python 环境并运行弱网评估脚本。
- [audio_network_lab_setup_python_envs.sh](./audio_network_lab_setup_python_envs.sh)
在本仓库根目录创建 `.venv_asr` / `.venv_emotion` 并安装依赖；可选 `--download-emotion-assets` 拉取 TESS 与 emotion2vec。
- [audio_network_lab_download_emotion_assets.py](./audio_network_lab_download_emotion_assets.py)
将 TESS 与 `emotion2vec_plus_large` 下载到 `.cache/`（不提交 Git）。
- [audio_network_lab_check_python_envs.sh](/Users/leyang/Projects/ohos_webrtc/modules/audio_coding/neteq/tools/audio_network_lab_check_python_envs.sh)
检查本仓库 Python 环境是否完整。
- [audio_network_lab_requirements_asr.txt](/Users/leyang/Projects/ohos_webrtc/modules/audio_coding/neteq/tools/audio_network_lab_requirements_asr.txt)
ASR / WER 依赖。
- [audio_network_lab_requirements_emotion.txt](/Users/leyang/Projects/ohos_webrtc/modules/audio_coding/neteq/tools/audio_network_lab_requirements_emotion.txt)
情感评估依赖。

可选的波形对比工具仍然保留，但不再是主线：

- [audio_network_lab_compare.py](/Users/leyang/Projects/ohos_webrtc/modules/audio_coding/neteq/tools/audio_network_lab_compare.py)
- [audio_network_lab_gen_report.py](/Users/leyang/Projects/ohos_webrtc/modules/audio_coding/neteq/tools/audio_network_lab_gen_report.py)
- [audio_network_lab_run_and_analyze.sh](/Users/leyang/Projects/ohos_webrtc/modules/audio_coding/neteq/tools/audio_network_lab_run_and_analyze.sh)

## 仓库内样本与本机缓存（TESS / 模型）

- **随仓库提供**：代表语音样本在
`[audio_network_lab_assets/representative_audio](./audio_network_lab_assets/representative_audio)`（体积小，可提交）。
- **不在 Git 中（需本机下载）**：TESS 情感语料与 `iic/emotion2vec_plus_large` 体积大（合计约 **2G** 量级），默认放在仓库根目录下 `**.cache/`**（已在 `.gitignore`），避免污染远端仓库。

下载方式（需联网，且先具备 `.venv_emotion`）：

```sh
# 推荐：创建情感环境并拉取 TESS + emotion2vec
bash modules/audio_coding/neteq/tools/audio_network_lab_setup_python_envs.sh \
  --emotion --download-emotion-assets

# 若环境已建好，仅补充数据与模型：
bash modules/audio_coding/neteq/tools/audio_network_lab_setup_python_envs.sh \
  --download-emotion-assets
```

等价地，也可直接用情感环境里的 Python 调用：

```sh
.venv_emotion/bin/python \
  modules/audio_coding/neteq/tools/audio_network_lab_download_emotion_assets.py
```

下载完成后，评估脚本使用的默认路径为：

- TESS（扁平 `*.wav`）：`.cache/datasets/tess/tess`
- emotion2vec（ModelScope 快照）：`.cache/modelscope/models/iic/emotion2vec_plus_large`

TESS 来源：Hugging Face 数据集 `[myleslinder/tess](https://huggingface.co/datasets/myleslinder/tess)` 中的 `data/tess.zip`（与官方 TESS 内容一致，CC BY-NC 4.0）。模型来源：ModelScope `[iic/emotion2vec_plus_large](https://www.modelscope.cn/models/iic/emotion2vec_plus_large)`。

若改用自有语料或模型，保持与 `[audio_network_lab_tess_emotion_baseline.py](./audio_network_lab_tess_emotion_baseline.py)` 中 `--tess-dir` / `--model` 约定一致即可。

## 推荐结果目录

建议固定两类结果根目录。

单次语音传输评估：

```text
results/audio_network_lab_runs/<RUN_ID>/
├── input.pcm
├── output.wav
├── summary.json
├── summary_report.json
├── wer_report.json
├── reference.txt
├── hypothesis.txt
└── textlog.txt
```

情感评估：

```text
results/audio_network_lab_emotion_eval/<RUN_ID>/
├── predictions.csv
├── summary.json
├── scenario_summary.json
├── emotion_predictions.csv
├── report.md
├── prepared_inputs/
├── generated/
└── summaries/
```

建议把 `<RUN_ID>` 设计成：

```text
<date>_<audio_or_dataset>_<scenario>_<strategy>
```

例如：

```text
20260415_librispeech_uniform10_opus_fec
20260415_tess_delay_jitter10_baseline
```

## 新环境中一步步搭建

以下步骤假定你在一个新的 host 环境中，从头把这条实验链路拉起来。

### 1. 准备系统依赖

至少需要：

- `python3`
- `git`
- `ninja`
- `gn`
- `depot_tools/gclient`

macOS 建议先装好 Xcode Command Line Tools：

```sh
xcode-select --install
```

### 2. 安装 `ninja`

从官方仓库源码安装：

```sh
git clone https://github.com/ninja-build/ninja.git
cd ninja
python3 configure.py --bootstrap
mkdir -p "$HOME/.local/bin"
cp ninja "$HOME/.local/bin/"
export PATH="$HOME/.local/bin:$PATH"
ninja --version
```

### 3. 安装 `gn`

从 GN 官方源构建：

```sh
git clone https://gn.googlesource.com/gn
cd gn
python3 build/gen.py
ninja -C out
mkdir -p "$HOME/.local/bin"
cp out/gn "$HOME/.local/bin/"
export PATH="$HOME/.local/bin:$PATH"
gn --version
```

### 4. 安装 `gclient`

如果本机还没有 `depot_tools`：

```sh
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH="$PWD/depot_tools:$PATH"
gclient --version
```

### 5. 拉齐 WebRTC 依赖

如果仓库只是裸 `git clone`，通常缺少完整 `third_party/` 树。必须在源码根目录同步依赖：

```sh
gclient sync
```

### 6. 生成构建目录

在源码根目录执行：

```sh
gn gen out/AudioLab --args='
  rtc_enable_protobuf=false
  rtc_include_tests=false
  rtc_build_examples=false
  rtc_build_tools=false
  rtc_build_libvpx=false
  rtc_libvpx_build_vp9=false
  rtc_include_dav1d_in_internal_decoder_factory=false
  enable_libaom=false
  rtc_enable_google_benchmarks=false
'
```

macOS 如果使用系统 Xcode clang，建议额外加：

```sh
sysroot="'"$(xcrun --sdk macosx --show-sdk-path)"'"
clang_base_path="//buildtools/local_xcode_compat"
use_custom_libcxx=false
use_lld=false
gtest_enable_absl_printers=false
```

完整示例：

```sh
gn gen out/AudioLab --args='
  sysroot="'"$(xcrun --sdk macosx --show-sdk-path)"'"
  clang_base_path="//buildtools/local_xcode_compat"
  use_custom_libcxx=false
  use_lld=false
  gtest_enable_absl_printers=false
  rtc_enable_protobuf=false
  rtc_include_tests=false
  rtc_build_examples=false
  rtc_build_tools=false
  rtc_build_libvpx=false
  rtc_libvpx_build_vp9=false
  rtc_include_dav1d_in_internal_decoder_factory=false
  enable_libaom=false
  rtc_enable_google_benchmarks=false
'
```

### 7. 编译实验台与单测

```sh
ninja -C out/AudioLab modules/audio_coding:audio_network_lab
ninja -C out/AudioLab modules/audio_coding:audio_network_lab_unittests
```

### 8. 跑一遍单测

```sh
./out/AudioLab/audio_network_lab_unittests
```

预期覆盖：

- baseline
- Opus FEC
- RED
- 固定 outage
- 非法 burst loss 参数
- DRED 自适应（`auto`）
- DRED `--dred_duration_frames` 覆盖

## 本仓库 Python 环境

这个项目默认使用仓库根目录下的两个本地虚拟环境：

- `.venv_asr/`：ASR / WER
- `.venv_emotion/`：情感识别评估

创建这两个环境需要联网安装 Python 包；如果你处在离线环境，应该先准备本地 wheel 镜像或内部包源，再执行下面的安装脚本。

### 一次性创建两个环境

```sh
bash modules/audio_coding/neteq/tools/audio_network_lab_setup_python_envs.sh --all
```

只创建 ASR 环境：

```sh
bash modules/audio_coding/neteq/tools/audio_network_lab_setup_python_envs.sh --asr
```

只创建情感环境：

```sh
bash modules/audio_coding/neteq/tools/audio_network_lab_setup_python_envs.sh --emotion
```

创建情感环境并下载 TESS + emotion2vec（首次跑情感评估推荐）：

```sh
bash modules/audio_coding/neteq/tools/audio_network_lab_setup_python_envs.sh \
  --emotion --download-emotion-assets
```

仅下载情感资产（已有 `.venv_emotion` 时）：

```sh
bash modules/audio_coding/neteq/tools/audio_network_lab_setup_python_envs.sh \
  --download-emotion-assets
```

如果你想显式指定 Python：

```sh
bash modules/audio_coding/neteq/tools/audio_network_lab_setup_python_envs.sh \
  --all \
  --python "$(which python3)"
```

### 检查环境是否完整

```sh
bash modules/audio_coding/neteq/tools/audio_network_lab_check_python_envs.sh
```

### Wrapper 的解释器选择规则

本目录下的 shell wrapper 只按以下顺序选择解释器：

1. 环境变量 `ASR_PYTHON_BIN` / `EMOTION_PYTHON_BIN`
2. 本仓库 `.venv_asr/bin/python` / `.venv_emotion/bin/python`

如果两者都没有，wrapper 会直接报错并提示先创建环境，而不是静默回退到别的路径。

## 网络模型与关键参数

网络阶段复用 `call/simulated_network.h` 的 `SimulatedNetwork`，在容量排队之后叠加基础时延和高斯抖动，并支持：

- 均匀随机丢包
- Gilbert-Elliot 突发随机丢包
- 固定断连事件
- 可选乱序
- 可选有限队列


| 标志                        | 含义                                            |
| ------------------------- | --------------------------------------------- |
| `--bandwidth_kbps`        | 链路容量（kbps）；`0` 表示不限速                          |
| `--delay_ms`              | 单向基础时延（ms）                                    |
| `--jitter_stddev_ms`      | 时延抖动标准差（高斯，ms）                                |
| `--loss_pct`              | 随机丢包率（%）                                      |
| `--avg_burst_loss_length` | Gilbert-Elliot 平均 burst 长度；仅当 `> 1` 时启用突发模型   |
| `--allow_reorder`         | 允许乱序                                          |
| `--queue_len_packets`     | 最大链路排队包数；`0` 表示不限制                            |
| `--fixed_loss_events_ms`  | 固定断连，格式 `start:duration,start:duration`，单位 ms |
| `--network_random_seed`   | 随机种子，保证同参结果可复现                                |


注意：

- 对给定 `loss_pct`，`avg_burst_loss_length` 必须大于 `ceil(p/(1-p))`
- 可复现实验时要固定：输入 PCM、编码参数、网络参数、`network_random_seed`

## 最小跑通示例

### baseline

```sh
python3 -c 'import math,struct; p="/tmp/lab_input.pcm"; open(p,"wb").write(b"".join(struct.pack("<h", int(12000*math.sin(2*math.pi*440*i/48000))) for i in range(48000))); print(p)'

./out/AudioLab/audio_network_lab \
  --input_pcm=/tmp/lab_input.pcm \
  --output_audio=/tmp/lab_baseline.wav \
  --summary_json=/tmp/lab_baseline.json \
  --redundancy_mode=none
```

### 丢包 + Opus FEC

```sh
./out/AudioLab/audio_network_lab \
  --input_pcm=/tmp/lab_input.pcm \
  --output_audio=/tmp/lab_fec.wav \
  --summary_json=/tmp/lab_fec.json \
  --redundancy_mode=opus_fec \
  --expected_loss_pct=10 \
  --loss_pct=10
```

### RED

```sh
./out/AudioLab/audio_network_lab \
  --input_pcm=/tmp/lab_input.pcm \
  --output_audio=/tmp/lab_red.wav \
  --summary_json=/tmp/lab_red.json \
  --redundancy_mode=red \
  --loss_pct=15
```

### 固定断连

```sh
./out/AudioLab/audio_network_lab \
  --input_pcm=/tmp/lab_input.pcm \
  --output_audio=/tmp/lab_outage.wav \
  --summary_json=/tmp/lab_outage.json \
  --fixed_loss_events_ms=300:200,1200:150
```

### Opus DRED（自适应）

```sh
./out/AudioLab/audio_network_lab \
  --input_pcm=/tmp/lab_input.pcm \
  --output_audio=/tmp/lab_dred.wav \
  --summary_json=/tmp/lab_dred.json \
  --redundancy_mode=dred \
  --expected_loss_pct=20 \
  --loss_pct=20 \
  --avg_burst_loss_length=5
```

结果里的 `encoder.dred_duration_frames` 应显示自适应挑选到的 recovery 深度（上例 20% loss 会得到 80 帧 = 800 ms），`encoder.dred_duration_source=auto`。

如果想钉住具体值：

```sh
./out/AudioLab/audio_network_lab \
  --input_pcm=/tmp/lab_input.pcm \
  --output_audio=/tmp/lab_dred_500ms.wav \
  --summary_json=/tmp/lab_dred_500ms.json \
  --redundancy_mode=dred \
  --dred_duration_frames=50 \
  --expected_loss_pct=10 \
  --loss_pct=10
```

如果你要验证“**库内 runtime 调控**”而不是 lab 的固定深度策略，可开启：

```sh
./out/AudioLab/audio_network_lab \
  --input_pcm=/tmp/lab_input.pcm \
  --output_audio=/tmp/lab_dred_encoder_adapt.wav \
  --summary_json=/tmp/lab_dred_encoder_adapt.json \
  --redundancy_mode=dred \
  --expected_loss_pct=20 \
  --loss_pct=20 \
  --avg_burst_loss_length=5 \
  --use_encoder_adaptive_dred=true
```

此时：

- `encoder.use_encoder_adaptive_dred=true`
- `encoder.dred_duration_source` 仍记录为 `auto/flag`（lab 选出来的上限）
- 实际编码期间 DRED 深度由库内 `AudioEncoderOpusImpl` 按 runtime 丢包反馈动态更新

## 单次语音 WER 评估

建议分两步看：

1. 先看 `summary.json`，确认网络和 NetEq 行为是否符合预期
2. 再看 `WER` 和 `ASR stable/unstable`

### 1. 结构化解读 `summary.json`

```sh
python3 modules/audio_coding/neteq/tools/audio_network_lab_report.py \
  /tmp/lab_fec.json \
  --report_json=/tmp/lab_fec.summary_report.json
```

重点字段包括：

- 实际编码码率
- enqueued / delivered / lost / reordered
- concealment 比例
- FEC 收到/丢弃数量
- secondary decode / expand / accelerate 比例
- jitter buffer delay / target / minimum
- interruption 次数和总时长

### 2. 一条命令跑实验并输出 WER

要求：

- 已创建 `.venv_asr`
- 输入音频旁边存在 `*_reference.txt`，或者你显式提供参考文本

示例：

```sh
modules/audio_coding/neteq/tools/audio_network_lab_run_and_eval_speech.sh \
  ./out/AudioLab/audio_network_lab \
  modules/audio_coding/neteq/tools/audio_network_lab_assets/representative_audio/librispeech/read_ls_001.wav \
  /tmp/audio_network_lab_ls001 \
  --channels=1 \
  --redundancy_mode=opus_fec \
  --expected_loss_pct=10 \
  --loss_pct=10
```

输出目录包含：

- `input.pcm`
- `output.wav`
- `summary.json`
- `summary_report.json`
- `wer_report.json`
- `reference.txt`
- `hypothesis.txt`
- `textlog.txt`

如果你只想单独跑 WER：

```sh
.venv_asr/bin/python \
  modules/audio_coding/neteq/tools/audio_network_lab_wer.py \
  --output_audio /tmp/audio_network_lab_ls001/output.wav \
  --reference_audio modules/audio_coding/neteq/tools/audio_network_lab_assets/representative_audio/librispeech/read_ls_001.wav \
  --report_json /tmp/audio_network_lab_ls001/wer_report.json
```

解读建议：

- `wer` 上升但 `concealed_samples` 不高，常见于编码失真或轻微时延拉伸
- `wer` 和 `concealed_samples` 同时上升，常见于 burst loss / outage
- `fec_packets_received` 高且 `secondary_decoded_rate` 上升，说明 FEC/RED 真进入了恢复路径
- `accelerate_rate` / `preemptive_expand_rate` 异常升高，通常意味着抖动和缓冲目标在频繁调节
- `asr_unstable=true` 且伴随重复 n-gram，通常说明输出音频已明显影响转写稳定性

## TESS 情感归类评估

这条链路回答两个问题：

1. 干净音频上，`emotion2vec` 在当前数据集和环境里的基线效果如何
2. 同一批样本经过弱网传输后，情感标签会退化多少

### 数据集

默认目录：

- `.cache/datasets/tess/tess`

目录下应直接包含 `.wav` 文件，例如：

- `OAF_back_angry.wav`
- `OAF_back_happy.wav`

### 模型与缓存

默认模型：

- `iic/emotion2vec_plus_large`

缓存目录：

- `.cache/modelscope`
- `.cache/huggingface`

模型与 TESS 需按上文放入 `.cache/`；`audio_network_lab_download_emotion_assets.py` 在目标目录已有内容时会跳过下载，可用 `--force` 强制重新拉取。

### 1. 跑干净语音基线

要求：

- 已创建 `.venv_emotion`

先跑一个 smoke：

```sh
modules/audio_coding/neteq/tools/audio_network_lab_run_tess_emotion_baseline.sh \
  --limit 32 \
  --run-name audio_network_lab_tess_baseline_smoke
```

正式跑则去掉 `--limit`。

输出目录默认在：

- `results/audio_network_lab_emotion_eval/<RUN_NAME>/`

主要产物：

- `predictions.csv`
- `summary.json`
- `report.md`

### 2. 跑弱网情感分类回归

这一步会：

1. 先把 TESS 输入统一准备成 `48 kHz mono`
2. 对每个场景调用 `audio_network_lab`
3. 对生成音频做情感分类
4. 汇总每个场景的准确率和退化幅度

先跑一个小样本 smoke：

```sh
modules/audio_coding/neteq/tools/audio_network_lab_run_tess_emotion_eval.sh \
  --lab-bin ./out/AudioLab/audio_network_lab \
  --limit 24 \
  --run-name audio_network_lab_tess_eval_smoke
```

正式跑则去掉 `--limit`，必要时再调：

- `--bitrate-bps`
- `--frame-ms`
- `--channels`
- `--network-random-seed`

当前内置场景：

- `clean`
- `uniform_10`
- `uniform_20`
- `burst_moderate`
- `burst_heavy`
- `delay_jitter_10`

输出目录下会有：

- `prepared_inputs/`
- `generated/<scenario>/`
- `summaries/<scenario>/`
- `emotion_predictions.csv`
- `scenario_summary.json`
- `report.md`

情感回归更适合看：

- `accuracy`
- `accuracy_drop_pp`
- `macro_f1`

如果 `clean` 基线本来就偏低，先检查模型和环境，而不要直接把问题归因到网络。

## 可选的波形对比工具

如果你仍然想看参考波形和输出波形的对齐、SNR、相关性，本目录保留了：

- `audio_network_lab_compare.py`
- `audio_network_lab_gen_report.py`
- `audio_network_lab_run_and_analyze.sh`

但对当前项目，建议把它们放在次要位置。

## 推荐实验流程

建议固定一组代表语料，再按下面顺序做：

1. `baseline`
  目标：确认编码/解码链路本身无异常。
2. `10% uniform loss`
  对比 `redundancy_mode=none` 与 `opus_fec`。
3. `15%~20% loss + red`
  看 `secondary_decoded_rate` 与 WER 变化。
4. `burst loss / outage`
  看 `concealed_samples`、`interruption_count`、`wer` 上升幅度。
5. `delay + jitter + reorder`
  看 `jitter_buffer_*`、`accelerate_rate`、`preemptive_expand_rate` 与 ASR 稳定性。
6. `TESS clean vs degraded`
  看情感分类 `accuracy_drop_pp` 和 `macro_f1`。

建议每组实验都保留：

- 运行命令
- `summary.json`
- `summary_report.json`
- `wer_report.json`
- 情感评估的 `summary.json` 或 `scenario_summary.json`
- 输出 `wav`

这样后续做参数 sweep 或写实验结论时，不需要先听全量音频，也能先从语义可懂度和情绪保真两个维度筛选 case。

## 常见问题与排障清单

### 1) `gn gen` 报 `google_benchmark` 或第三方目录缺失

常见原因是仓库仅做了 `git clone`，未同步完整依赖。先在仓库根目录执行：

```sh
gclient sync
```

如果你只想最小化构建 `audio_network_lab`，请沿用本文给出的 `gn args`，确保关闭不必要组件（例如 benchmark、examples、tools）。

### 2) `audio_network_lab` 运行时报 burst loss 参数非法

当 `--avg_burst_loss_length > 1` 时，启用 Gilbert-Elliot 模型，必须满足：

- `0 < --loss_pct < 100`
- `avg_burst_loss_length > ceil(p/(1-p))`, `p=loss_pct/100`

不满足时工具会直接返回错误（这是预期行为，用来避免内部网络模型断言崩溃）。

### 3) 输出 WER 明显升高，但 `concealed_samples` 不高

先检查：

- 编码侧参数是否改动（`bitrate_bps`、`frame_ms`、`channels`）
- 是否有较高 `accelerate_rate` / `preemptive_expand_rate`
- 是否存在长时延和高抖动引发转写稳定性下降

这类情况不一定是“硬丢包”问题，也可能是时域拉伸和编码失真叠加。

### 4) 情感评估结果波动较大

优先确认：

- `clean` 基线是否稳定
- `--network_random_seed` 是否固定
- 样本量是否过小（smoke 样本通常只用于冒烟，不用于结论）

### 5) 结果目录越来越大

建议按阶段清理：

- 临时冒烟：只保留 `summary.json` / `summary_report.json` / `wer_report.json`
- 正式对比：保留完整 `wav` 和 `textlog`
- 使用 `.gitignore` 保持 `results/` 不进入版本库

## 结果解读速查

- **链路是否正常**：`decoded_duration_ms == input_duration_ms`
- **是否发生明显恢复压力**：`concealment_events`, `concealed_samples`
- **FEC/RED 是否生效**：`fec_packets_received`, `max_secondary_decoded_rate`
- **缓冲是否频繁调整**：`accelerate_rate`, `preemptive_expand_rate`, `jitter_buffer_target_delay_ms`
- **用户感知风险**：`interruption_count`, `total_interruption_duration_ms`, `wer`

