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
import os
import re
from pathlib import Path

try:
    from faster_whisper import WhisperModel
    from jiwer import wer as jiwer_wer
except ImportError:
    WhisperModel = None
    jiwer_wer = None


_MODEL_CACHE = {}


def normalize_transcript(text):
    text = text.lower()
    text = re.sub(r"[^\w\s']", " ", text)
    return re.sub(r"\s+", " ", text).strip()


def count_words(text):
    norm = normalize_transcript(text)
    if not norm:
        return 0
    return len(norm.split())


def detect_repeated_ngram(text):
    tokens = normalize_transcript(text).split()
    if len(tokens) < 12:
        return None
    for n in (8, 6, 4):
        if len(tokens) < n * 3:
            continue
        for i in range(0, len(tokens) - n * 3 + 1):
            gram = tokens[i : i + n]
            repeats = 1
            j = i + n
            while j + n <= len(tokens) and tokens[j : j + n] == gram:
                repeats += 1
                j += n
            if repeats >= 3:
                return f"repeated {n}-gram x{repeats}"
    return None


def detect_asr_instability(reference_text, hypothesis_text):
    ref_words = count_words(reference_text)
    hyp_words = count_words(hypothesis_text)
    if ref_words > 0 and hyp_words > max(int(ref_words * 1.6), ref_words + 20):
        return f"word_count {hyp_words}>{ref_words}"
    return detect_repeated_ngram(hypothesis_text)


def infer_reference_text(audio_path):
    candidates = []
    audio_path = Path(audio_path)
    stem = audio_path.stem
    candidates.append(audio_path.with_name(f"{stem}_reference.txt"))
    parent_name = audio_path.parent.name
    candidates.append(audio_path.with_name(f"{parent_name}_reference.txt"))
    for candidate in candidates:
        if candidate.is_file():
            return candidate.read_text(encoding="utf-8").strip()
    return None


def get_model(model_name):
    if WhisperModel is None:
        raise RuntimeError(
            "faster_whisper/jiwer are not installed; use the ASR environment or install them."
        )
    model = _MODEL_CACHE.get(model_name)
    if model is None:
        local_only = os.environ.get("RTC_STT_LOCAL_ONLY", "1").lower() not in {"0", "false", "no"}
        model = WhisperModel(model_name, device="auto", compute_type="auto", local_files_only=local_only)
        _MODEL_CACHE[model_name] = model
    return model


def transcribe_audio(audio_path, model_name, language):
    model = get_model(model_name)
    segments, _ = model.transcribe(str(audio_path), language=language, vad_filter=True)
    texts = []
    for segment in segments:
        text = getattr(segment, "text", "")
        if text:
            texts.append(text.strip())
    return " ".join(texts).strip()


def build_report(reference_text, hypothesis_text):
    if jiwer_wer is None:
        raise RuntimeError(
            "faster_whisper/jiwer are not installed; use the ASR environment or install them."
        )
    ref_norm = normalize_transcript(reference_text)
    hyp_norm = normalize_transcript(hypothesis_text)
    wer_value = jiwer_wer(ref_norm, hyp_norm) if ref_norm else None
    instability_reason = detect_asr_instability(reference_text, hypothesis_text) or ""
    return {
        "reference_text": reference_text,
        "hypothesis_text": hypothesis_text,
        "reference_text_normalized": ref_norm,
        "hypothesis_text_normalized": hyp_norm,
        "wer": wer_value,
        "asr_unstable": bool(instability_reason),
        "instability_reason": instability_reason,
        "reference_word_count": count_words(reference_text),
        "hypothesis_word_count": count_words(hypothesis_text),
    }


def print_report(report):
    if report["wer"] is None:
        print("WER: -")
    else:
        print(f"WER: {report['wer']:.2%}")
    print(f"ASR unstable: {str(report['asr_unstable']).lower()}")
    if report["instability_reason"]:
        print(f"Reason: {report['instability_reason']}")
    print(f"Reference words: {report['reference_word_count']}")
    print(f"Hypothesis words: {report['hypothesis_word_count']}")


def main():
    parser = argparse.ArgumentParser(
        description="Run transcript-based WER evaluation for one audio_network_lab output."
    )
    parser.add_argument("--output_audio", required=True)
    parser.add_argument("--reference_audio", help="Optional input audio path used to infer *_reference.txt.")
    parser.add_argument("--reference_text", help="Optional explicit reference text file.")
    parser.add_argument("--stt_model", default=os.environ.get("RTC_STT_MODEL", "small.en"))
    parser.add_argument("--stt_language", default=os.environ.get("RTC_STT_LANGUAGE"))
    parser.add_argument("--report_json", help="Optional JSON output path.")
    parser.add_argument("--reference_text_out", help="Optional path to dump resolved reference text.")
    parser.add_argument("--hypothesis_text_out", help="Optional path to dump ASR hypothesis.")
    args = parser.parse_args()

    reference_text = None
    if args.reference_text:
        reference_text = Path(args.reference_text).read_text(encoding="utf-8").strip()
    elif args.reference_audio:
        reference_text = infer_reference_text(args.reference_audio)
    if not reference_text:
        raise FileNotFoundError(
            "Cannot resolve reference text. Use --reference_text or provide --reference_audio with a *_reference.txt sidecar."
        )

    hypothesis_text = transcribe_audio(args.output_audio, args.stt_model, args.stt_language)
    report = build_report(reference_text, hypothesis_text)
    print_report(report)

    if args.reference_text_out:
        Path(args.reference_text_out).write_text(reference_text + "\n", encoding="utf-8")
    if args.hypothesis_text_out:
        Path(args.hypothesis_text_out).write_text(hypothesis_text + "\n", encoding="utf-8")
    if args.report_json:
        with Path(args.report_json).open("w", encoding="utf-8") as f:
            json.dump(report, f, ensure_ascii=False, indent=2)
            f.write("\n")


if __name__ == "__main__":
    main()
