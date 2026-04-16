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
  echo "Usage: $0 <audio_network_lab_bin> <input_pcm> <output_dir> [audio_network_lab flags...]"
  exit 1
fi

BIN=$1
INPUT_PCM=$2
OUTPUT_DIR=$3
shift 3

if [ ! -x "$BIN" ]; then
  echo "Cannot execute audio_network_lab binary: $BIN"
  exit 2
fi

if [ ! -f "$INPUT_PCM" ]; then
  echo "Cannot find input PCM: $INPUT_PCM"
  exit 2
fi

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPORT_TOOL="$SCRIPT_DIR/audio_network_lab_report.py"
COMPARE_TOOL="$SCRIPT_DIR/audio_network_lab_compare.py"
GEN_REPORT_TOOL="$SCRIPT_DIR/audio_network_lab_gen_report.py"

if [ ! -f "$REPORT_TOOL" ] || [ ! -f "$COMPARE_TOOL" ] || [ ! -f "$GEN_REPORT_TOOL" ]; then
  echo "Cannot find analysis tools next to this script."
  exit 2
fi

mkdir -p "$OUTPUT_DIR"

OUTPUT_AUDIO="$OUTPUT_DIR/output.wav"
SUMMARY_JSON="$OUTPUT_DIR/summary.json"
TEXTLOG="$OUTPUT_DIR/textlog.txt"
SUMMARY_REPORT_JSON="$OUTPUT_DIR/summary_report.json"
COMPARE_REPORT_JSON="$OUTPUT_DIR/audio_compare.json"
REPORT_MD="$OUTPUT_DIR/report.md"

CHANNELS=1
ARGS=()
while [ "$#" -gt 0 ]; do
  case "$1" in
    --channels=*)
      CHANNELS=${1#*=}
      ARGS+=("$1")
      shift
      ;;
    --channels)
      if [ "$#" -lt 2 ]; then
        echo "--channels requires a value."
        exit 2
      fi
      CHANNELS=$2
      ARGS+=("$1" "$2")
      shift 2
      ;;
    --output_audio|--summary_json|--textlog)
      echo "Do not pass $1; this wrapper manages output paths."
      exit 2
      ;;
    --output_audio=*|--summary_json=*|--textlog=*)
      echo "Do not pass explicit output paths to this wrapper."
      exit 2
      ;;
    *)
      ARGS+=("$1")
      shift
      ;;
  esac
done

"$BIN" \
  --input_pcm="$INPUT_PCM" \
  --output_audio="$OUTPUT_AUDIO" \
  --summary_json="$SUMMARY_JSON" \
  --textlog="$TEXTLOG" \
  "${ARGS[@]}"

python3 "$REPORT_TOOL" "$SUMMARY_JSON" --report_json "$SUMMARY_REPORT_JSON"
python3 "$COMPARE_TOOL" \
  --reference "$INPUT_PCM" \
  --reference_format pcm \
  --test "$OUTPUT_AUDIO" \
  --test_format wav \
  --sample_rate_hz 48000 \
  --channels "$CHANNELS" \
  --report_json "$COMPARE_REPORT_JSON"
python3 "$GEN_REPORT_TOOL" \
  --summary_json "$SUMMARY_JSON" \
  --summary_report_json "$SUMMARY_REPORT_JSON" \
  --audio_compare_json "$COMPARE_REPORT_JSON" \
  --output_md "$REPORT_MD"

echo
echo "Generated:"
echo "  $OUTPUT_AUDIO"
echo "  $SUMMARY_JSON"
echo "  $TEXTLOG"
echo "  $SUMMARY_REPORT_JSON"
echo "  $COMPARE_REPORT_JSON"
echo "  $REPORT_MD"
