#!/usr/bin/env python3
"""One-stop setup for the Opus 1.6+ DRED (Deep REDundancy) integration.

`third_party/` is git-ignored in this repo, so the upgraded Opus source tree
and the patched `BUILD.gn` cannot live under version control. This script
reproduces the local state needed to build the DRED code paths on a fresh
checkout.

Upgrade notes (2026-04 / Opus 1.6):
  * 1.6 ships the DNN weight arrays directly inside the release tarball
    (`dnn/*_data.c` files are multi-MB blobs). The separate
    `opus_data-*.tar.gz` bundle used for 1.5.x is therefore no longer
    required. That step has been removed.
  * The 1.6 DRED bitstream is NOT wire-compatible with 1.5.x. We bump
    `kOpusDredLocalVersion` to 2 in `modules/audio_coding/codecs/opus/
    audio_coder_opus_common.h` so SDP negotiation with 1.5.x peers falls
    back to PLC/LBRR instead of silently mis-decoding.

Steps performed:
  1. Verify that `third_party/opus/src` already exists (populated via the
     standard `gclient sync` workflow of this project) and upgrade it in
     place to Opus 1.6 if the bundled tree is older.
  2. Install the patched `third_party/opus/BUILD.gn` that enables
     `ENABLE_DEEP_PLC`, `ENABLE_DRED`, `ENABLE_OSCE`, compiles the new
     `dnn/` sources (including the 1.6 BWE weight blob `bbwenet_data.c`),
     and wires up x86/ARM SIMD fast paths.

Usage:
    python3 tools_webrtc/opus_dred/setup_opus_dred.py [--force]

Exit code is 0 on success. Re-running the script is safe and idempotent.
"""

from __future__ import annotations

import argparse
import hashlib
import pathlib
import shutil
import sys
import tarfile
import tempfile
import urllib.request

REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
OPUS_DIR = REPO_ROOT / "third_party" / "opus"
OPUS_SRC_DIR = OPUS_DIR / "src"
OPUS_DNN_DIR = OPUS_SRC_DIR / "dnn"
BUILD_GN_PATCHED = pathlib.Path(__file__).with_name("opus.BUILD.gn.patched")

# Opus 1.6 upstream release (official Xiph tarball, published 2025-12-15).
OPUS_VERSION = "1.6"
OPUS_RELEASE_URL = (
    f"https://downloads.xiph.org/releases/opus/opus-{OPUS_VERSION}.tar.gz"
)
# Upstream-announced SHA-256 for opus-1.6.tar.gz. Kept pinned to refuse a
# silent substitution that could ship a different DRED model under the same
# negotiated version number.
OPUS_RELEASE_SHA256 = (
    "b7637334527201fdfd6dd6a02e67aceffb0e5e60155bbd89175647a80301c92c"
)

# Name of the directory inside the release tarball.
OPUS_TARBALL_ROOT = f"opus-{OPUS_VERSION}"

# Backup directory for the pre-existing src/ tree. We keep whatever the
# previous tree was (the 1.5.x variant used "src.pre-dred") untouched and
# stash the pre-1.6 state beside it.
OPUS_SRC_BACKUP = "src.pre-opus-1.5"

# A few markers we use to tell 1.5.x (no BWE / no 24-bit DRED API) from 1.6.
# `opus_decoder_dred_decode24` is a 1.6-only symbol.
REQUIRED_IDENTIFIERS_1_6 = (
    b"opus_decoder_dred_decode24",
)

# Files that must be present in src/dnn/ after extraction for the patched
# BUILD.gn to compile. 1.6 adds bbwenet_data.c for the BWE model; the
# multi-MB *_data.c files now live inside the source tarball itself rather
# than requiring a separate download.
REQUIRED_DATA_FILES = (
    "bbwenet_data.c",
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


def opus_is_1_6(src_dir: pathlib.Path) -> bool:
    opus_h = src_dir / "include" / "opus.h"
    if not opus_h.exists():
        return False
    text = opus_h.read_bytes()
    return all(marker in text for marker in REQUIRED_IDENTIFIERS_1_6)


def ensure_opus_1_6(force: bool) -> None:
    if OPUS_SRC_DIR.exists() and opus_is_1_6(OPUS_SRC_DIR) and not force:
        log("Opus source tree already at 1.6 (has 24-bit DRED API); skipping.")
        return

    log(f"Upgrading Opus source tree to {OPUS_VERSION}.")
    OPUS_DIR.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        tarball = pathlib.Path(tmp) / f"opus-{OPUS_VERSION}.tar.gz"
        download(OPUS_RELEASE_URL, tarball)

        digest = sha256_of(tarball)
        log(f"opus-{OPUS_VERSION}.tar.gz sha256={digest}")
        if OPUS_RELEASE_SHA256 and digest != OPUS_RELEASE_SHA256:
            raise SystemExit(
                "Opus release SHA-256 mismatch. Refusing to clobber "
                "third_party/opus/src. Update OPUS_RELEASE_SHA256 after you "
                "audit the new archive."
            )

        with tarfile.open(tarball, "r:gz") as tf:
            tf.extractall(tmp)

        extracted = pathlib.Path(tmp) / OPUS_TARBALL_ROOT
        if not extracted.exists():
            raise SystemExit(
                f"Extracted tarball root {OPUS_TARBALL_ROOT} not found."
            )
        if OPUS_SRC_DIR.exists():
            backup = OPUS_DIR / OPUS_SRC_BACKUP
            if backup.exists():
                shutil.rmtree(backup)
            log(f"backing up existing src/ -> {backup}")
            OPUS_SRC_DIR.rename(backup)
        shutil.move(str(extracted), str(OPUS_SRC_DIR))

    if not opus_is_1_6(OPUS_SRC_DIR):
        raise SystemExit(
            "Upgrade did not produce an Opus 1.6 tree; aborting."
        )
    log(f"Opus upgraded to {OPUS_VERSION}.")


def verify_dnn_data() -> None:
    """1.6 ships all DNN weight arrays inline; just sanity-check presence."""
    if not OPUS_DNN_DIR.exists():
        raise SystemExit(
            f"{OPUS_DNN_DIR} missing. Upgrade Opus to {OPUS_VERSION} before "
            "running this step."
        )
    missing = [n for n in REQUIRED_DATA_FILES if not (OPUS_DNN_DIR / n).exists()]
    if missing:
        raise SystemExit(
            "Expected inline DNN weight files missing from "
            f"{OPUS_DNN_DIR}: " + ", ".join(missing)
        )
    log(
        "DNN weight arrays already bundled in the 1.6 source tree "
        f"({len(REQUIRED_DATA_FILES)} *_data.c files verified)."
    )


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
        backup = target.with_suffix(".gn.pre-opus-1.6")
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
        ensure_opus_1_6(args.force)
    verify_dnn_data()
    install_build_gn(args.force)
    log("Done. You can now run: gn gen out/AudioLab && ninja -C out/AudioLab")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
