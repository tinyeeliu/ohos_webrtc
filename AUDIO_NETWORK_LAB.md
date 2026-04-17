# Audio Network Lab Entry

`audio_network_lab` is a host-side, reproducible experiment loop for packet loss, jitter, reorder, and NetEq behavior analysis.

Unified DRED + audio lab entry (recommended first stop):

- [DRED_AUDIO_LAB.md](./DRED_AUDIO_LAB.md)

Primary document:

- [modules/audio_coding/neteq/tools/audio_network_lab.md](./modules/audio_coding/neteq/tools/audio_network_lab.md)

TESS 与 `emotion2vec` 模型体积大，不在 Git 中；跑情感评估前在本机执行 `bash modules/audio_coding/neteq/tools/audio_network_lab_setup_python_envs.sh --emotion --download-emotion-assets`（详见主文档「本仓库 Python 环境」）。

## Fast path (3 steps)

1. Build:

```sh
gn gen out/AudioLab --args='rtc_enable_protobuf=false rtc_include_tests=false'
ninja -C out/AudioLab modules/audio_coding:audio_network_lab
```

1. Run baseline:

```sh
./out/AudioLab/audio_network_lab \
  --input_pcm=/tmp/input.pcm \
  --output_audio=/tmp/baseline.wav \
  --summary_json=/tmp/baseline.json \
  --redundancy_mode=none
```

1. Inspect result:

```sh
python3 modules/audio_coding/neteq/tools/audio_network_lab_report.py \
  /tmp/baseline.json \
  --report_json=/tmp/baseline.summary_report.json
```

## Typical experiment progression

- `baseline` -> verify encode/decode loop is healthy
- `uniform loss + opus_fec` -> verify FEC gain
- `red + medium/high loss` -> verify secondary decoding behavior
- `burst/outage` -> verify concealment and interruption behavior
- `delay + jitter (+reorder)` -> verify jitter buffer adaptation

## Important reproducibility knobs

- Keep input audio, codec params, and network params fixed
- Set `--network_random_seed` for deterministic random impairments
- Keep outputs in `results/audio_network_lab_runs/<RUN_ID>/`

## Common output files

- `summary.json`: raw stats from one run
- `summary_report.json`: readable interpretation of summary
- `wer_report.json`: speech intelligibility impact
- `scenario_summary.json`: multi-scenario aggregate (emotion eval)

For complete options, environment setup, WER, and emotion regression flow, use the full guide above.