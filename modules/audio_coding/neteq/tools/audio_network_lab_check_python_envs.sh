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

if [ -x "$REPO_ROOT/.venv_asr/bin/python" ]; then
  "$REPO_ROOT/.venv_asr/bin/python" -c \
    "import faster_whisper, jiwer; print('audio_network_lab ASR env ok')"
else
  echo "Missing $REPO_ROOT/.venv_asr/bin/python"
fi

if [ -x "$REPO_ROOT/.venv_emotion/bin/python" ]; then
  "$REPO_ROOT/.venv_emotion/bin/python" -c \
    "import funasr, librosa, soundfile, soxr, sklearn; print('audio_network_lab emotion env ok')"
else
  echo "Missing $REPO_ROOT/.venv_emotion/bin/python"
fi
