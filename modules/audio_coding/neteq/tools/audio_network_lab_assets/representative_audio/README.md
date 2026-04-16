# representative_audio

当前仓库内置测试集已替换为新的多样化英语语音集合，不再保留 `VOA + ELLLO`。

当前版本使用 `LibriSpeech test-clean` 的多个章节，构造成多个独立评测样本：

- `LibriSpeech test-clean`：干净英文朗读

目录结构：

```text
representative_audio/
├── manifest.txt
├── manifest_quick.txt
├── manifest_standard.txt
├── manifest_full.txt
└── librispeech/
```

说明：

- `manifest.txt` 默认等同 `manifest_standard.txt`，兼容现有实验脚本。
- 每行格式为 `sample_id|relative/path.wav`。
- 每个 wav 旁边都带有 `<sample_id>_reference.txt` 参考文本。
- 样本都被转成 `48kHz / mono / wav`，时长控制在约 `24~38s`。
- 这是第一阶段替换版，优先解决样本数量不足；后续可继续叠加 `VCTK / Common Voice`。

当前标准集样本：

- `read_ls_001` -> `librispeech/read_ls_001.wav`
- `read_ls_002` -> `librispeech/read_ls_002.wav`
- `read_ls_003` -> `librispeech/read_ls_003.wav`
- `read_ls_004` -> `librispeech/read_ls_004.wav`
- `read_ls_005` -> `librispeech/read_ls_005.wav`
- `read_ls_006` -> `librispeech/read_ls_006.wav`
- `read_ls_007` -> `librispeech/read_ls_007.wav`
- `read_ls_008` -> `librispeech/read_ls_008.wav`

使用方式：

```bash
python3 tools/prepare_representative_audio.py --force
```

如果只想查看清单：

```bash
cat representative_audio/manifest.txt
```
