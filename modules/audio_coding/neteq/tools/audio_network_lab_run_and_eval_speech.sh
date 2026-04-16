#!/bin/bash
#
# Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
#

set -euo pipefail

if [ "$#" -lt 3 ]; then
  echo "Usage: $0 <audio_network_lab_bin> <input_audio> <output_dir> [audio_network_lab flags...]"
  exit 1
fi

BIN=$1
INPUT_AUDIO=$2
OUTPUT_DIR=$3
shift 3

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/../../../.." && pwd)
PREPARE_TOOL="$SCRIPT_DIR/audio_network_lab_prepare_input.py"
REPORT_TOOL="$SCRIPT_DIR/audio_network_lab_report.py"
WER_TOOL="$SCRIPT_DIR/audio_network_lab_wer.py"

PYTHON_BIN=${ASR_PYTHON_BIN:-}
if [ -z "$PYTHON_BIN" ] && [ -x "$REPO_ROOT/.venv_asr/bin/python" ]; then
  PYTHON_BIN="$REPO_ROOT/.venv_asr/bin/python"
fi
if [ -z "$PYTHON_BIN" ]; then
  echo "Missing ASR Python environment."
  echo "Create $REPO_ROOT/.venv_asr first, or set ASR_PYTHON_BIN explicitly."
  echo "Suggested command:"
  echo "  bash $SCRIPT_DIR/audio_network_lab_setup_python_envs.sh --asr"
  exit 3
fi

if [ ! -x "$BIN" ]; then
  echo "Cannot execute audio_network_lab binary: $BIN"
  exit 2
fi

mkdir -p "$OUTPUT_DIR"

INPUT_PCM="$OUTPUT_DIR/input.pcm"
REFERENCE_TXT="$OUTPUT_DIR/reference.txt"
OUTPUT_AUDIO="$OUTPUT_DIR/output.wav"
SUMMARY_JSON="$OUTPUT_DIR/summary.json"
TEXTLOG="$OUTPUT_DIR/textlog.txt"
SUMMARY_REPORT_JSON="$OUTPUT_DIR/summary_report.json"
WER_REPORT_JSON="$OUTPUT_DIR/wer_report.json"
HYP_TXT="$OUTPUT_DIR/hypothesis.txt"

CHANNELS=1
PREPARE_ARGS=()
LAB_ARGS=()
while [ "$#" -gt 0 ]; do
  case "$1" in
    --channels=*)
      CHANNELS=${1#*=}
      LAB_ARGS+=("$1")
      shift
      ;;
    --channels)
      CHANNELS=$2
      LAB_ARGS+=("$1" "$2")
      shift 2
      ;;
    *)
      LAB_ARGS+=("$1")
      shift
      ;;
  esac
done

"$PYTHON_BIN" "$PREPARE_TOOL" \
  --input_audio "$INPUT_AUDIO" \
  --output_pcm "$INPUT_PCM" \
  --sample_rate_hz 48000 \
  --channels "$CHANNELS" \
  --reference_text_out "$REFERENCE_TXT"

"$BIN" \
  --input_pcm="$INPUT_PCM" \
  --output_audio="$OUTPUT_AUDIO" \
  --summary_json="$SUMMARY_JSON" \
  --textlog="$TEXTLOG" \
  "${LAB_ARGS[@]}"

"$PYTHON_BIN" "$REPORT_TOOL" "$SUMMARY_JSON" --report_json "$SUMMARY_REPORT_JSON"
"$PYTHON_BIN" "$WER_TOOL" \
  --output_audio "$OUTPUT_AUDIO" \
  --reference_text "$REFERENCE_TXT" \
  --report_json "$WER_REPORT_JSON" \
  --reference_text_out "$REFERENCE_TXT" \
  --hypothesis_text_out "$HYP_TXT"

echo
echo "Generated:"
echo "  $INPUT_PCM"
echo "  $OUTPUT_AUDIO"
echo "  $SUMMARY_JSON"
echo "  $SUMMARY_REPORT_JSON"
echo "  $WER_REPORT_JSON"
echo "  $REFERENCE_TXT"
echo "  $HYP_TXT"
echo "  $TEXTLOG"
