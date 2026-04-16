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

MODE=copy
TRYOPUS_ROOT="/Users/leyang/Projects/tryOpus"
REPO_ROOT=$(cd "$(dirname "$0")/../../../.." && pwd)
TOOLS_ROOT="$REPO_ROOT/modules/audio_coding/neteq/tools"
ASSET_ROOT="$TOOLS_ROOT/tryopus_assets"

while [ "$#" -gt 0 ]; do
  case "$1" in
    --copy)
      MODE=copy
      shift
      ;;
    --tryopus-root)
      TRYOPUS_ROOT="$2"
      shift 2
      ;;
    *)
      echo "Unknown arg: $1"
      echo "Usage: $0 [--copy] [--tryopus-root /path/to/tryOpus]"
      exit 1
      ;;
  esac
done

mkdir -p "$ASSET_ROOT"
mkdir -p "$REPO_ROOT/.cache/datasets/tess"
mkdir -p "$REPO_ROOT/.cache/modelscope/models/iic"

copy_tree() {
  local src=$1
  local dst=$2
  if [ ! -e "$src" ]; then
    echo "Skip missing source: $src"
    return 0
  fi
  rm -rf "$dst"
  cp -R "$src" "$dst"
  echo "copy: $src -> $dst"
}

copy_tree "$TRYOPUS_ROOT/representative_audio" \
  "$ASSET_ROOT/representative_audio"
copy_tree "$TRYOPUS_ROOT/.cache/datasets/tess/tess" \
  "$REPO_ROOT/.cache/datasets/tess/tess"
copy_tree "$TRYOPUS_ROOT/.cache/modelscope/models/iic/emotion2vec_plus_large" \
  "$REPO_ROOT/.cache/modelscope/models/iic/emotion2vec_plus_large"

echo
echo "Ready:"
echo "  representative_audio -> $ASSET_ROOT/representative_audio"
echo "  TESS dataset         -> $REPO_ROOT/.cache/datasets/tess/tess"
echo "  emotion2vec model    -> $REPO_ROOT/.cache/modelscope/models/iic/emotion2vec_plus_large"
