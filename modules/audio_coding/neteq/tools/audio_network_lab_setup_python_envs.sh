#!/bin/bash
#
# Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source tree.
# An additional intellectual property rights grant can be found in the file
# PATENTS. All contributing project authors may be found in the AUTHORS file
# in the root of the source tree.
#

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/../../../.." && pwd)
PYTHON_BIN=${PYTHON_BIN:-python3}
SETUP_ASR=false
SETUP_EMOTION=false
DOWNLOAD_EMOTION_ASSETS=false

usage() {
  cat <<EOF
Usage: $0 [--asr] [--emotion] [--all] [--download-emotion-assets] [--python /path/to/python3]

Creates local virtual environments under:
  $REPO_ROOT/.venv_asr
  $REPO_ROOT/.venv_emotion

Large TESS + emotion2vec files are not stored in git. After the emotion venv exists,
use --download-emotion-assets to fetch them into $REPO_ROOT/.cache/ (requires network).

You can also run ONLY the download step (no venv recreate) with:
  $0 --download-emotion-assets
(requires an existing $REPO_ROOT/.venv_emotion from a prior --emotion run)
EOF
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --asr)
      SETUP_ASR=true
      shift
      ;;
    --emotion)
      SETUP_EMOTION=true
      shift
      ;;
    --all)
      SETUP_ASR=true
      SETUP_EMOTION=true
      shift
      ;;
    --download-emotion-assets)
      DOWNLOAD_EMOTION_ASSETS=true
      shift
      ;;
    --python)
      PYTHON_BIN="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown arg: $1"
      usage
      exit 1
      ;;
  esac
done

if [ "$SETUP_ASR" = false ] && [ "$SETUP_EMOTION" = false ]; then
  if [ "$DOWNLOAD_EMOTION_ASSETS" = false ]; then
    SETUP_ASR=true
    SETUP_EMOTION=true
  fi
fi

create_env() {
  local env_dir=$1
  local requirements_file=$2
  echo "Creating $env_dir using $PYTHON_BIN"
  "$PYTHON_BIN" -m venv "$env_dir"
  "$env_dir/bin/python" -m pip install --upgrade pip setuptools wheel
  "$env_dir/bin/python" -m pip install -r "$requirements_file"
}

if [ "$SETUP_ASR" = true ]; then
  create_env \
    "$REPO_ROOT/.venv_asr" \
    "$SCRIPT_DIR/audio_network_lab_requirements_asr.txt"
  "$REPO_ROOT/.venv_asr/bin/python" -c \
    "import faster_whisper, jiwer; print('audio_network_lab ASR env ok')"
fi

if [ "$SETUP_EMOTION" = true ]; then
  create_env \
    "$REPO_ROOT/.venv_emotion" \
    "$SCRIPT_DIR/audio_network_lab_requirements_emotion.txt"
  "$REPO_ROOT/.venv_emotion/bin/python" -c \
    "import funasr, librosa, soundfile, soxr, sklearn; print('audio_network_lab emotion env ok')"
fi

if [ "$DOWNLOAD_EMOTION_ASSETS" = true ]; then
  if [ ! -x "$REPO_ROOT/.venv_emotion/bin/python" ]; then
    echo "Missing $REPO_ROOT/.venv_emotion (run: $0 --emotion)"
    exit 1
  fi
  "$REPO_ROOT/.venv_emotion/bin/python" "$SCRIPT_DIR/audio_network_lab_download_emotion_assets.py"
fi
