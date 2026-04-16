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

PYTHON_BIN=${EMOTION_PYTHON_BIN:-}
if [ -z "$PYTHON_BIN" ] && [ -x "$REPO_ROOT/.venv_emotion/bin/python" ]; then
  PYTHON_BIN="$REPO_ROOT/.venv_emotion/bin/python"
fi
if [ -z "$PYTHON_BIN" ]; then
  echo "Missing emotion Python environment."
  echo "Create $REPO_ROOT/.venv_emotion first, or set EMOTION_PYTHON_BIN explicitly."
  echo "Suggested command:"
  echo "  bash $SCRIPT_DIR/audio_network_lab_setup_python_envs.sh --emotion"
  exit 3
fi

exec "$PYTHON_BIN" "$SCRIPT_DIR/audio_network_lab_tess_emotion_baseline.py" "$@"
