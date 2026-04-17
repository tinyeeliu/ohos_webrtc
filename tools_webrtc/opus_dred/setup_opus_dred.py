#!/usr/bin/env python3
"""One-stop setup for the Opus 1.5+ DRED (Deep REDundancy) integration.

`third_party/` is git-ignored in this repo, so the upgraded Opus source tree,
its pre-generated DNN weight arrays, and the patched `BUILD.gn` cannot live
under version control. This script reproduces the local state needed to build
the DRED code paths on a fresh checkout.

Steps performed:
  1. Verify that `third_party/opus/src` already exists (populated via the
     standard `gclient sync` workflow of this project) and upgrade it in place
     to Opus 1.5.2 if the bundled tree is older.
  2. Download the pre-generated DNN weight tarball
     (`opus_data-735117b.tar.gz`, ~156 MB) from media.xiph.org, verify its
     SHA-256, extract only the `*_data.c` / `*_data.h` files into
     `third_party/opus/src/dnn/`, and discard the ~2 GB of PyTorch training
     weights that the compiled C library does not need.
  3. Install the patched `third_party/opus/BUILD.gn` that enables
     `ENABLE_DEEP_PLC`, `ENABLE_DRED`, `ENABLE_OSCE`, compiles the new `dnn/`
     sources, and wires up x86/ARM SIMD fast paths.

Usage:
    python3 tools_webrtc/opus_dred/setup_opus_dred.py [--force]

Exit code is 0 on success. Re-running the script is safe and idempotent.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import pathlib
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request

REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
OPUS_DIR = REPO_ROOT / "third_party" / "opus"
OPUS_SRC_DIR = OPUS_DIR / "src"
OPUS_DNN_DIR = OPUS_SRC_DIR / "dnn"
BUILD_GN_PATCHED = pathlib.Path(__file__).with_name("opus.BUILD.gn.patched")

# Opus 1.5.2 upstream release (official Xiph tarball).
OPUS_RELEASE_URL = "https://downloads.xiph.org/releases/opus/opus-1.5.2.tar.gz"
# Set to the expected sha256 once you've pinned a known-good archive. Leaving
# it empty disables the hard check and only logs the observed digest.
OPUS_RELEASE_SHA256 = ""

# DNN weight data bundle matching the model hash referenced in
# `src/dnn/download_model.sh` shipped with Opus 1.5.2. The file name already
# embeds the git hash of the upstream weight snapshot; the sha256 is printed
# on download so you can pin it per-site.
DNN_DATA_URL = (
    "https://media.xiph.org/opus/models/opus_data-735117b.tar.gz"
)
DNN_DATA_SHA256_KNOWN_GOOD = ""
REQUIRED_DATA_FILES = (
    "dred_rdovae_dec_data.c",
    "dred_rdovae_enc_data.c",
    "dred_rdovae_stats_data.c",
    "fargan_data.c",
    "lace_data.c",
    "lossgen_data.c",
    "nolace_data.c",
    "pitchdnn_data.c",
    "plc_data.c",
)


def log(msg: str) -> None:
    print(f"[opus-dred] {msg}")


def sha256_of(path: pathlib.Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def download(url: str, dest: pathlib.Path) -> None:
    log(f"downloading {url}")
    with urllib.request.urlopen(url) as resp, dest.open("wb") as out:
        shutil.copyfileobj(resp, out, length=1 << 20)
    log(f"  -> {dest} ({dest.stat().st_size / (1 << 20):.1f} MiB)")


def opus_has_dred(src_dir: pathlib.Path) -> bool:
    opus_h = src_dir / "include" / "opus.h"
    if not opus_h.exists():
        return False
    text = opus_h.read_text(errors="ignore")
    return "OPUS_SET_DRED_DURATION" in text or "opus_dred_parse" in text


def ensure_opus_1_5(force: bool) -> None:
    if OPUS_SRC_DIR.exists() and opus_has_dred(OPUS_SRC_DIR) and not force:
        log("Opus source tree already exposes DRED APIs; skipping upgrade.")
        return

    log("Opus source tree missing DRED APIs; upgrading to 1.5.2.")
    OPUS_DIR.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        tarball = pathlib.Path(tmp) / "opus-1.5.2.tar.gz"
        download(OPUS_RELEASE_URL, tarball)

        digest = sha256_of(tarball)
        log(f"opus-1.5.2.tar.gz sha256={digest}")
        if OPUS_RELEASE_SHA256 and digest != OPUS_RELEASE_SHA256:
            raise SystemExit(
                "Opus release SHA-256 mismatch. Refusing to clobber "
                "third_party/opus/src. Update OPUS_RELEASE_SHA256 after you "
                "audit the new archive."
            )

        with tarfile.open(tarball, "r:gz") as tf:
            tf.extractall(tmp)

        extracted = pathlib.Path(tmp) / "opus-1.5.2"
        if OPUS_SRC_DIR.exists():
            backup = OPUS_DIR / "src.pre-dred"
            if backup.exists():
                shutil.rmtree(backup)
            log(f"backing up existing src/ -> {backup}")
            OPUS_SRC_DIR.rename(backup)
        shutil.move(str(extracted), str(OPUS_SRC_DIR))

    if not opus_has_dred(OPUS_SRC_DIR):
        raise SystemExit(
            "Upgrade did not produce a DRED-capable Opus tree; aborting."
        )
    log("Opus upgraded to 1.5.2.")


def dnn_data_present() -> bool:
    return all((OPUS_DNN_DIR / name).exists() for name in REQUIRED_DATA_FILES)


def install_dnn_data(force: bool) -> None:
    if dnn_data_present() and not force:
        log("DNN weight arrays already present; skipping download.")
        return

    if not OPUS_DNN_DIR.exists():
        raise SystemExit(
            f"{OPUS_DNN_DIR} missing. Upgrade Opus to 1.5.2 before running "
            "this step."
        )

    log(
        "DNN weight arrays missing. Fetching "
        f"{DNN_DATA_URL.rsplit('/', 1)[-1]} (~156 MiB)."
    )
    with tempfile.TemporaryDirectory() as tmp:
        tarball = pathlib.Path(tmp) / "opus_data.tar.gz"
        download(DNN_DATA_URL, tarball)

        digest = sha256_of(tarball)
        log(f"opus_data-735117b.tar.gz sha256={digest}")
        if (
            DNN_DATA_SHA256_KNOWN_GOOD
            and digest != DNN_DATA_SHA256_KNOWN_GOOD
        ):
            raise SystemExit(
                "DNN tarball SHA-256 mismatch against "
                "DNN_DATA_SHA256_KNOWN_GOOD. Refusing to install unknown "
                "weights."
            )

        with tarfile.open(tarball, "r:gz") as tf:
            members = []
            for m in tf.getmembers():
                # Only pull the data .c/.h files - skip the 2 GB models/ dir
                # of PyTorch checkpoints which are training artefacts.
                if m.name.startswith("dnn/models/"):
                    continue
                if not m.name.startswith("dnn/"):
                    continue
                if not (m.name.endswith("_data.c") or m.name.endswith("_data.h")):
                    continue
                # Strip leading "dnn/" to write directly into OPUS_DNN_DIR.
                m.name = m.name[len("dnn/"):]
                members.append(m)
            if not members:
                raise SystemExit("Tarball contained no *_data.* files.")
            tf.extractall(OPUS_DNN_DIR, members=members)

    missing = [n for n in REQUIRED_DATA_FILES if not (OPUS_DNN_DIR / n).exists()]
    if missing:
        raise SystemExit(
            "DNN extract incomplete, missing: " + ", ".join(missing)
        )
    log(f"DNN weight arrays installed under {OPUS_DNN_DIR}.")


def install_build_gn(force: bool) -> None:
    target = OPUS_DIR / "BUILD.gn"
    if not BUILD_GN_PATCHED.exists():
        raise SystemExit(
            f"Patched BUILD.gn template missing: {BUILD_GN_PATCHED}"
        )
    if target.exists() and not force:
        if target.read_bytes() == BUILD_GN_PATCHED.read_bytes():
            log("third_party/opus/BUILD.gn already matches patched template.")
            return
        backup = target.with_suffix(".gn.pre-dred")
        if not backup.exists():
            log(f"backing up existing BUILD.gn -> {backup.name}")
            shutil.copy2(target, backup)
    shutil.copy2(BUILD_GN_PATCHED, target)
    log(f"installed DRED-aware BUILD.gn at {target}")


def check_environment() -> None:
    if not OPUS_DIR.exists():
        raise SystemExit(
            "third_party/opus is missing. Run `gclient sync` (or the "
            "project-specific equivalent) before invoking this script."
        )


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--force",
        action="store_true",
        help="Reinstall everything even if existing artefacts look current.",
    )
    ap.add_argument(
        "--skip-upgrade",
        action="store_true",
        help="Skip the Opus source upgrade step (assume tree already has DRED).",
    )
    args = ap.parse_args(argv)

    check_environment()
    if not args.skip_upgrade:
        ensure_opus_1_5(args.force)
    install_dnn_data(args.force)
    install_build_gn(args.force)
    log("Done. You can now run: gn gen out/AudioLab && ninja -C out/AudioLab")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
