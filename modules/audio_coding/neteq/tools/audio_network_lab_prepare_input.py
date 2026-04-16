#!/usr/bin/env python3
#
# Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import argparse
import shutil
import wave
from pathlib import Path


def infer_reference_text(audio_path):
    candidates = []
    stem = audio_path.stem
    candidates.append(audio_path.with_name(f"{stem}_reference.txt"))
    parent_name = audio_path.parent.name
    candidates.append(audio_path.with_name(f"{parent_name}_reference.txt"))
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    return None


def wav_to_pcm16le(input_wav, output_pcm, expected_sample_rate, expected_channels):
    with wave.open(str(input_wav), "rb") as wav:
        channels = wav.getnchannels()
        sample_rate_hz = wav.getframerate()
        sample_width = wav.getsampwidth()
        if sample_width != 2:
            raise ValueError(f"{input_wav} is not 16-bit PCM WAV.")
        if sample_rate_hz != expected_sample_rate:
            raise ValueError(
                f"{input_wav} sample rate is {sample_rate_hz}, expected {expected_sample_rate}."
            )
        if channels != expected_channels:
            raise ValueError(
                f"{input_wav} channels is {channels}, expected {expected_channels}."
            )
        frames = wav.readframes(wav.getnframes())
    output_pcm.write_bytes(frames)


def main():
    parser = argparse.ArgumentParser(
        description="Prepare audio_network_lab input PCM from a WAV/PCM source."
    )
    parser.add_argument("--input_audio", required=True, help="Input WAV or PCM file.")
    parser.add_argument("--output_pcm", required=True, help="Output PCM path.")
    parser.add_argument(
        "--sample_rate_hz",
        type=int,
        default=48000,
        help="Expected sample rate for WAV input.",
    )
    parser.add_argument(
        "--channels",
        type=int,
        default=1,
        help="Expected channels for WAV input.",
    )
    parser.add_argument(
        "--reference_text_out",
        help="Optional output path for copied/inferred reference text.",
    )
    parser.add_argument(
        "--reference_text",
        help="Optional explicit reference text file to copy.",
    )
    args = parser.parse_args()

    input_audio = Path(args.input_audio)
    output_pcm = Path(args.output_pcm)
    output_pcm.parent.mkdir(parents=True, exist_ok=True)

    if input_audio.suffix.lower() == ".wav":
        wav_to_pcm16le(
            input_audio,
            output_pcm,
            expected_sample_rate=args.sample_rate_hz,
            expected_channels=args.channels,
        )
    else:
        shutil.copyfile(input_audio, output_pcm)

    if args.reference_text_out:
        dst = Path(args.reference_text_out)
        dst.parent.mkdir(parents=True, exist_ok=True)
        src = Path(args.reference_text) if args.reference_text else infer_reference_text(input_audio)
        if src and src.is_file():
            shutil.copyfile(src, dst)


if __name__ == "__main__":
    main()
