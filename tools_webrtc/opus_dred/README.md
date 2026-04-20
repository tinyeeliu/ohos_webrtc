# Opus DRED integration helper

> Unified doc/results entry for DRED + audio lab:
> [`DRED_AUDIO_LAB.md`](../../DRED_AUDIO_LAB.md)

This directory contains the artefacts required to reproduce the Opus 1.6
**Deep REDundancy (DRED)** / **Deep PLC** / **OSCE (LACE/NoLACE/BWE)** build
on a fresh checkout.

`/third_party` is git-ignored in this repo, so the upgraded Opus source tree
and the patched `third_party/opus/BUILD.gn` cannot be versioned directly.
The `setup_opus_dred.py` script reconstructs that local state in a single
command.

> **1.5 → 1.6 note** — 1.6 now ships the DNN weight arrays directly
> inside the release tarball (each `dnn/*_data.c` is a multi-MB blob).
> The separate `opus_data-*.tar.gz` bundle that 1.5.x required is no
> longer downloaded. Because the new DRED model is NOT wire-compatible
> with 1.5.x, `kOpusDredLocalVersion` has been bumped to `2`; SDP
> negotiation with legacy peers automatically falls back to PLC/LBRR.

## Quick start

```bash
# 1. Make sure third_party/opus exists (via the project's usual sync step).
gclient sync   # or your project's equivalent

# 2. Install Opus 1.5.2 sources, DNN weights and the patched BUILD.gn.
python3 tools_webrtc/opus_dred/setup_opus_dred.py

# 3. Regenerate the build files and compile the audio_network_lab harness.
gn gen out/AudioLab
ninja -C out/AudioLab audio_network_lab audio_network_lab_unittests

# 4. Run the unit tests (should include DredModeProducesOutput).
./out/AudioLab/audio_network_lab_unittests
```

## What the script does

1. Downloads `opus-1.6.tar.gz` from the official Xiph mirror (SHA-256 pinned
   against `b7637334...`) and replaces `third_party/opus/src` with it. The
   previous `src/` is moved aside to `third_party/opus/src.pre-opus-1.5/`
   for safety.
2. Sanity-checks that the expected inline weight files
   (`bbwenet_data.c`, `dred_rdovae_*_data.c`, `fargan_data.c`,
   `lace_data.c`, `nolace_data.c`, `plc_data.c`, …) are present. 1.6
   embeds these directly in the source tarball, so no secondary download
   is performed.
3. Copies `opus.BUILD.gn.patched` into `third_party/opus/BUILD.gn`,
   preserving any pre-existing file as `BUILD.gn.pre-opus-1.6`.

The script is idempotent: re-running it is a no-op when the tree already
matches the expected state.

## Files

- `setup_opus_dred.py` – the bootstrap script described above.
- `opus.BUILD.gn.patched` – snapshot of the GN target definitions
that enable `ENABLE_DEEP_PLC`, `ENABLE_DRED`, `ENABLE_OSCE`, include the
Opus 1.6 `src/dnn/` sources (including the new `bbwenet_data.c` BWE
weights), and split x86/ARM SIMD into dedicated source sets
(`opus_sse2`, `opus_sse41`, `opus_avx2` with the new `pitch_avx`,
`inner_product_FLP_avx2` and `NSQ_del_dec_avx2` sources, plus NEON under
ARM).

## Required network hosts

- `downloads.xiph.org` – Opus 1.6 source tarball. That is the only
  external download required; 1.6 no longer needs `media.xiph.org`.

If the host is blocked by your network, download the tarball manually
with the URL baked into the script, drop it into a temporary directory,
and point `urllib.request.urlopen` at a local mirror (or extract the
archive by hand).

## Verifying DRED is actually active

After building, run the three redundancy modes on the same loss trace to
confirm that DRED produces the expected recovery gain:

```bash
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

On a reference run (20 % loss, seed 42, 10 s sine):


| mode     | bitrate   | concealed_samples | concealment_events |
| -------- | --------- | ----------------- | ------------------ |
| none     | 24.2 kbps | 98 400            | 87                 |
| opus_fec | 24.2 kbps | 95 520            | 86                 |
| **dred** | 28.9 kbps | 4 800             | 7                  |


i.e. DRED cuts PLC concealment volume by ~95 % at a +4.7 kbps bitrate cost.

## See also

- `DRED_AUDIO_LAB.md` – consolidated entry for setup, experiments, and latest
  WER/bitrate outcomes.

