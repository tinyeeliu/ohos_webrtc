#!/usr/bin/env python3
#
# Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Download TESS wav corpus and emotion2vec ModelScope snapshot into repo-local .cache.

These assets are intentionally not committed (large). See audio_network_lab.md.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import sys
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
DEFAULT_TESS_DIR = ROOT / ".cache" / "datasets" / "tess" / "tess"
DEFAULT_MODEL_DIR = (
    ROOT / ".cache" / "modelscope" / "models" / "iic" / "emotion2vec_plus_large"
)
TESS_HF_DATASET = "myleslinder/tess"
TESS_ZIP_IN_REPO = "data/tess.zip"
MODEL_ID = "iic/emotion2vec_plus_large"


def parse_args():
    p = argparse.ArgumentParser(
        description="Download TESS + emotion2vec_plus_large into .cache (not in git)."
    )
    p.add_argument(
        "--tess-dir",
        type=Path,
        default=DEFAULT_TESS_DIR,
        help="Flat directory of *.wav (default: .cache/datasets/tess/tess)",
    )
    p.add_argument(
        "--model-dir",
        type=Path,
        default=DEFAULT_MODEL_DIR,
        help="Local ModelScope layout for emotion2vec (default matches eval scripts)",
    )
    p.add_argument("--skip-tess", action="store_true")
    p.add_argument("--skip-model", action="store_true")
    p.add_argument(
        "--force",
        action="store_true",
        help="Re-download even if destination already looks populated",
    )
    return p.parse_args()


def tess_ready(tess_dir: Path) -> bool:
    return bool(list(tess_dir.glob("*.wav")))


def download_tess(tess_dir: Path, force: bool) -> None:
    if tess_ready(tess_dir) and not force:
        print(f"TESS: skip (wavs already under {tess_dir})")
        return

    try:
        from huggingface_hub import hf_hub_download
    except ImportError as e:
        raise RuntimeError(
            "huggingface_hub is required. Install emotion venv deps first."
        ) from e

    print(f"TESS: downloading {TESS_HF_DATASET} ({TESS_ZIP_IN_REPO}) …")
    zip_path = hf_hub_download(
        repo_id=TESS_HF_DATASET,
        repo_type="dataset",
        filename=TESS_ZIP_IN_REPO,
    )
    zip_path = Path(zip_path)

    tess_dir.parent.mkdir(parents=True, exist_ok=True)
    if tess_dir.exists() and force:
        shutil.rmtree(tess_dir)
    tess_dir.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        with zipfile.ZipFile(zip_path) as zf:
            zf.extractall(tmp)
        inner = tmp / "tess"
        if not inner.is_dir():
            raise RuntimeError(f"Unexpected zip layout: missing {inner.name}/")
        wavs = list(inner.glob("*.wav"))
        if not wavs:
            raise RuntimeError(f"No *.wav under {inner}")
        for wav in wavs:
            shutil.copy2(wav, tess_dir / wav.name)

    print(
        json.dumps(
            {
                "tess_dir": str(tess_dir),
                "num_wav": len(list(tess_dir.glob("*.wav"))),
            },
            ensure_ascii=False,
        )
    )


def model_ready(model_dir: Path) -> bool:
    if not model_dir.is_dir():
        return False
    # Heuristic: snapshot contains config / weights
    for name in (
        "configuration.json",
        "config.json",
        "config.yaml",
        "model.pt",
        "pytorch_model.bin",
    ):
        if (model_dir / name).exists():
            return True
    return any(model_dir.iterdir())


def download_model(model_dir: Path, force: bool) -> None:
    if model_ready(model_dir) and not force:
        print(f"emotion2vec: skip (model already under {model_dir})")
        return

    os.environ.setdefault("MODELSCOPE_CACHE", str(ROOT / ".cache" / "modelscope"))
    os.environ.setdefault("HF_HOME", str(ROOT / ".cache" / "huggingface"))

    try:
        from modelscope import snapshot_download
    except ImportError:
        try:
            from modelscope.hub.snapshot_download import snapshot_download
        except ImportError as e:
            raise RuntimeError(
                "modelscope is required. Install emotion venv deps (funasr pulls it in)."
            ) from e

    if model_dir.exists() and force:
        shutil.rmtree(model_dir)
    model_dir.parent.mkdir(parents=True, exist_ok=True)

    print(f"emotion2vec: downloading {MODEL_ID} …")
    out = snapshot_download(MODEL_ID, local_dir=str(model_dir))
    print(json.dumps({"model_dir": out}, ensure_ascii=False))


def main() -> int:
    args = parse_args()
    if not args.skip_tess:
        download_tess(args.tess_dir, args.force)
    if not args.skip_model:
        download_model(args.model_dir, args.force)
    print("audio_network_lab emotion assets download finished.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
