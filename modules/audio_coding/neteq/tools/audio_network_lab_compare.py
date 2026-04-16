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
import json
import math
import wave
from array import array
from dataclasses import dataclass
from pathlib import Path


@dataclass
class AudioData:
    sample_rate_hz: int
    channels: int
    samples: array

    @property
    def frames(self):
        return len(self.samples) // self.channels


def load_pcm16le(path, sample_rate_hz, channels):
    data = array("h")
    with open(path, "rb") as f:
        data.frombytes(f.read())
    if len(data) % channels != 0:
        raise ValueError(
            f"{path} length is not divisible by channels={channels}; "
            "expected 16-bit PCM interleaved samples."
        )
    return AudioData(sample_rate_hz=sample_rate_hz, channels=channels, samples=data)


def load_wav(path):
    with wave.open(str(path), "rb") as wav:
        if wav.getsampwidth() != 2:
            raise ValueError(f"{path} is not 16-bit PCM WAV.")
        frames = wav.readframes(wav.getnframes())
        data = array("h")
        data.frombytes(frames)
        return AudioData(
            sample_rate_hz=wav.getframerate(),
            channels=wav.getnchannels(),
            samples=data,
        )


def load_audio(path, fmt, sample_rate_hz, channels):
    if fmt == "wav":
        return load_wav(path)
    if fmt == "pcm":
        return load_pcm16le(path, sample_rate_hz=sample_rate_hz, channels=channels)
    raise ValueError(f"Unsupported format: {fmt}")


def detect_format(path, explicit_format):
    if explicit_format:
        return explicit_format
    suffix = Path(path).suffix.lower()
    if suffix == ".wav":
        return "wav"
    return "pcm"


def mix_to_mono(audio):
    if audio.channels == 1:
        return list(audio.samples)
    mono = []
    for frame in range(audio.frames):
        frame_offset = frame * audio.channels
        total = 0
        for ch in range(audio.channels):
            total += audio.samples[frame_offset + ch]
        mono.append(total / audio.channels)
    return mono


def choose_alignment_lag_frames(reference_mono, test_mono, max_lag_frames, window_frames):
    best_lag = 0
    best_score = -1.0
    max_search = min(max_lag_frames, max(len(reference_mono), len(test_mono)))

    for lag in range(-max_search, max_search + 1):
        ref_start = max(0, -lag)
        test_start = max(0, lag)
        available = min(
            window_frames,
            len(reference_mono) - ref_start,
            len(test_mono) - test_start,
        )
        if available < 128:
            continue

        sum_xy = 0.0
        sum_x2 = 0.0
        sum_y2 = 0.0
        for i in range(available):
            x = reference_mono[ref_start + i]
            y = test_mono[test_start + i]
            sum_xy += x * y
            sum_x2 += x * x
            sum_y2 += y * y

        if sum_x2 == 0.0 or sum_y2 == 0.0:
            continue
        score = sum_xy / math.sqrt(sum_x2 * sum_y2)
        if score > best_score:
            best_score = score
            best_lag = lag

    return best_lag, best_score


def rms_dbfs(samples):
    if not samples:
        return float("-inf")
    energy = 0.0
    for sample in samples:
        energy += float(sample) * float(sample)
    rms = math.sqrt(energy / float(len(samples)))
    if rms == 0.0:
        return float("-inf")
    return 20.0 * math.log10(rms / 32768.0)


def peak_dbfs(samples):
    if not samples:
        return float("-inf")
    peak = max(abs(int(sample)) for sample in samples)
    if peak == 0:
        return float("-inf")
    return 20.0 * math.log10(float(peak) / 32768.0)


def correlation(reference, test):
    count = min(len(reference), len(test))
    if count == 0:
        return 0.0

    sum_x = 0.0
    sum_y = 0.0
    sum_x2 = 0.0
    sum_y2 = 0.0
    sum_xy = 0.0
    for i in range(count):
        x = float(reference[i])
        y = float(test[i])
        sum_x += x
        sum_y += y
        sum_x2 += x * x
        sum_y2 += y * y
        sum_xy += x * y

    numerator = count * sum_xy - sum_x * sum_y
    denom_x = count * sum_x2 - sum_x * sum_x
    denom_y = count * sum_y2 - sum_y * sum_y
    if denom_x <= 0.0 or denom_y <= 0.0:
        return 0.0
    return numerator / math.sqrt(denom_x * denom_y)


def build_report(reference_audio, test_audio, lag_frames, lag_score):
    if reference_audio.sample_rate_hz != test_audio.sample_rate_hz:
        raise ValueError("Reference and test audio sample rates differ.")
    if reference_audio.channels != test_audio.channels:
        raise ValueError("Reference and test audio channel counts differ.")

    ref_start = max(0, -lag_frames)
    test_start = max(0, lag_frames)
    overlap_frames = min(
        reference_audio.frames - ref_start,
        test_audio.frames - test_start,
    )
    if overlap_frames <= 0:
        raise ValueError("Reference and test audio have no overlap after alignment.")

    ref_sample_start = ref_start * reference_audio.channels
    test_sample_start = test_start * test_audio.channels
    sample_count = overlap_frames * reference_audio.channels

    reference_overlap = reference_audio.samples[
        ref_sample_start : ref_sample_start + sample_count
    ]
    test_overlap = test_audio.samples[test_sample_start : test_sample_start + sample_count]

    error_samples = []
    max_abs_error = 0
    mean_abs_error = 0.0
    signal_energy = 0.0
    error_energy = 0.0
    clipped_samples = 0
    for ref_sample, test_sample in zip(reference_overlap, test_overlap):
        diff = int(test_sample) - int(ref_sample)
        error_samples.append(diff)
        abs_diff = abs(diff)
        max_abs_error = max(max_abs_error, abs_diff)
        mean_abs_error += abs_diff
        signal_energy += float(ref_sample) * float(ref_sample)
        error_energy += float(diff) * float(diff)
        if abs(int(test_sample)) >= 32767:
            clipped_samples += 1

    mean_abs_error /= float(sample_count)
    if error_energy == 0.0:
        snr_db = float("inf")
    elif signal_energy == 0.0:
        snr_db = float("-inf")
    else:
        snr_db = 10.0 * math.log10(signal_energy / error_energy)

    reference_mono = mix_to_mono(reference_audio)[ref_start : ref_start + overlap_frames]
    test_mono = mix_to_mono(test_audio)[test_start : test_start + overlap_frames]

    return {
        "sample_rate_hz": reference_audio.sample_rate_hz,
        "channels": reference_audio.channels,
        "alignment": {
            "lag_frames": lag_frames,
            "lag_ms": 1000.0 * float(lag_frames) / float(reference_audio.sample_rate_hz),
            "normalized_correlation": lag_score,
        },
        "reference": {
            "frames": reference_audio.frames,
            "duration_ms": 1000.0
            * float(reference_audio.frames)
            / float(reference_audio.sample_rate_hz),
            "rms_dbfs": rms_dbfs(reference_audio.samples),
            "peak_dbfs": peak_dbfs(reference_audio.samples),
        },
        "test": {
            "frames": test_audio.frames,
            "duration_ms": 1000.0
            * float(test_audio.frames)
            / float(test_audio.sample_rate_hz),
            "rms_dbfs": rms_dbfs(test_audio.samples),
            "peak_dbfs": peak_dbfs(test_audio.samples),
        },
        "overlap": {
            "frames": overlap_frames,
            "duration_ms": 1000.0 * float(overlap_frames) / float(reference_audio.sample_rate_hz),
            "reference_start_frame": ref_start,
            "test_start_frame": test_start,
            "snr_db": snr_db,
            "reference_rms_dbfs": rms_dbfs(reference_overlap),
            "test_rms_dbfs": rms_dbfs(test_overlap),
            "error_rms_dbfs": rms_dbfs(error_samples),
            "error_peak_dbfs": peak_dbfs(error_samples),
            "mean_abs_error_pcm16": mean_abs_error,
            "max_abs_error_pcm16": max_abs_error,
            "pearson_correlation": correlation(reference_mono, test_mono),
            "clipped_test_samples": clipped_samples,
        },
    }


def print_report(report):
    print("Audio Compare Report")
    print("====================")
    print(
        f"Format: {report['sample_rate_hz']} Hz, {report['channels']} ch, "
        f"lag={report['alignment']['lag_frames']} frames "
        f"({report['alignment']['lag_ms']:.3f} ms), "
        f"align_score={report['alignment']['normalized_correlation']:.5f}"
    )
    print(
        f"Reference: {report['reference']['duration_ms']:.2f} ms, "
        f"rms={report['reference']['rms_dbfs']:.2f} dBFS, "
        f"peak={report['reference']['peak_dbfs']:.2f} dBFS"
    )
    print(
        f"Test: {report['test']['duration_ms']:.2f} ms, "
        f"rms={report['test']['rms_dbfs']:.2f} dBFS, "
        f"peak={report['test']['peak_dbfs']:.2f} dBFS"
    )
    print(
        f"Overlap: {report['overlap']['duration_ms']:.2f} ms, "
        f"SNR={report['overlap']['snr_db']:.2f} dB, "
        f"corr={report['overlap']['pearson_correlation']:.5f}, "
        f"err_rms={report['overlap']['error_rms_dbfs']:.2f} dBFS"
    )
    print(
        f"Error: mean_abs={report['overlap']['mean_abs_error_pcm16']:.2f}, "
        f"max_abs={report['overlap']['max_abs_error_pcm16']}, "
        f"clipped_test_samples={report['overlap']['clipped_test_samples']}"
    )


def main():
    parser = argparse.ArgumentParser(
        description="Compare reference audio and audio_network_lab output."
    )
    parser.add_argument("--reference", required=True, help="Reference PCM/WAV path.")
    parser.add_argument("--test", required=True, help="Test PCM/WAV path.")
    parser.add_argument(
        "--reference_format",
        choices=["auto", "pcm", "wav"],
        default="auto",
        help="Reference file format. Defaults to auto.",
    )
    parser.add_argument(
        "--test_format",
        choices=["auto", "pcm", "wav"],
        default="auto",
        help="Test file format. Defaults to auto.",
    )
    parser.add_argument(
        "--sample_rate_hz",
        type=int,
        default=48000,
        help="Sample rate for raw PCM input.",
    )
    parser.add_argument(
        "--channels",
        type=int,
        default=1,
        help="Channel count for raw PCM input.",
    )
    parser.add_argument(
        "--max_lag_ms",
        type=float,
        default=250.0,
        help="Maximum alignment search range in ms.",
    )
    parser.add_argument(
        "--window_ms",
        type=float,
        default=120.0,
        help="Alignment scoring window length in ms.",
    )
    parser.add_argument(
        "--report_json",
        help="Optional path to write the compare report as JSON.",
    )
    args = parser.parse_args()

    reference_path = Path(args.reference)
    test_path = Path(args.test)
    reference_format = detect_format(
        reference_path, None if args.reference_format == "auto" else args.reference_format
    )
    test_format = detect_format(
        test_path, None if args.test_format == "auto" else args.test_format
    )

    reference_audio = load_audio(
        reference_path, reference_format, args.sample_rate_hz, args.channels
    )
    test_audio = load_audio(test_path, test_format, args.sample_rate_hz, args.channels)

    max_lag_frames = int(
        round(float(args.max_lag_ms) * float(reference_audio.sample_rate_hz) / 1000.0)
    )
    window_frames = int(
        round(float(args.window_ms) * float(reference_audio.sample_rate_hz) / 1000.0)
    )
    lag_frames, lag_score = choose_alignment_lag_frames(
        mix_to_mono(reference_audio),
        mix_to_mono(test_audio),
        max_lag_frames=max_lag_frames,
        window_frames=window_frames,
    )
    report = build_report(reference_audio, test_audio, lag_frames, lag_score)
    print_report(report)

    if args.report_json:
        with Path(args.report_json).open("w", encoding="utf-8") as f:
            json.dump(report, f, indent=2, sort_keys=True)
            f.write("\n")


if __name__ == "__main__":
    main()
