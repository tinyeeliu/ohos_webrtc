#!/usr/bin/env python3
#
# Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

from __future__ import annotations

import argparse
import csv
import json
import os
import re
import sys
from collections import Counter
from datetime import datetime
from pathlib import Path
from typing import Any

from funasr import AutoModel
from sklearn.metrics import accuracy_score, classification_report, confusion_matrix


ROOT = Path(__file__).resolve().parents[4]
DEFAULT_TESS_DIR = ROOT / ".cache" / "datasets" / "tess" / "tess"
DEFAULT_RESULTS_DIR = ROOT / "results" / "audio_network_lab_emotion_eval"
DEFAULT_MODEL = "iic/emotion2vec_plus_large"
LOCAL_MODELSCOPE_MODEL = (
    ROOT / ".cache" / "modelscope" / "models" / "iic" / "emotion2vec_plus_large"
)
LABEL_ORDER = ["neutral", "happy", "sad", "angry", "fear", "disgust", "ps"]
CANONICAL_LABELS = {
    "neutral": "neutral",
    "happy": "happy",
    "sad": "sad",
    "angry": "angry",
    "anger": "angry",
    "fear": "fear",
    "fearful": "fear",
    "disgust": "disgust",
    "disgusted": "disgust",
    "ps": "ps",
    "surprised": "ps",
    "surprise": "ps",
}


def parse_args():
    parser = argparse.ArgumentParser(description="Evaluate emotion2vec on TESS.")
    parser.add_argument("--tess-dir", type=Path, default=DEFAULT_TESS_DIR)
    parser.add_argument("--results-dir", type=Path, default=DEFAULT_RESULTS_DIR)
    parser.add_argument("--model", default=DEFAULT_MODEL)
    parser.add_argument("--batch-size", type=int, default=64)
    parser.add_argument("--limit", type=int, default=0)
    parser.add_argument("--run-name", default="")
    return parser.parse_args()


def collect_samples(tess_dir):
    wavs = sorted(tess_dir.glob("*.wav"))
    if not wavs:
        raise FileNotFoundError(f"No wav files found under {tess_dir}")
    samples = []
    for wav_path in wavs:
        speaker, word, raw_label = wav_path.stem.split("_", 2)
        samples.append(
            {
                "utt_id": wav_path.stem,
                "path": str(wav_path),
                "speaker": speaker,
                "word": word,
                "label": raw_label,
            }
        )
    return samples


def normalize_label(label):
    cleaned = label.strip().lower()
    cleaned = re.sub(r"^[^a-z]+|[^a-z]+$", "", cleaned)
    if "/" in label:
        cleaned = label.split("/")[-1].strip().lower()
    return CANONICAL_LABELS.get(cleaned, cleaned)


def parse_prediction(item: Any):
    if isinstance(item, list) and item:
        item = item[0]
    if isinstance(item, dict):
        labels = item.get("labels")
        scores = item.get("scores")
        if isinstance(labels, list) and labels:
            if isinstance(scores, list) and scores:
                max_index = max(range(len(scores)), key=lambda idx: float(scores[idx]))
                label = str(labels[max_index])
                score = float(scores[max_index])
            else:
                label = str(labels[0])
                score = 0.0
            return normalize_label(label), score, json.dumps(item, ensure_ascii=False)
        text = item.get("text")
        if isinstance(text, str) and text:
            score = float(scores[0]) if isinstance(scores, list) and scores else 0.0
            return normalize_label(text), score, json.dumps(item, ensure_ascii=False)
    if isinstance(item, str):
        return normalize_label(item), 0.0, item
    raise ValueError(f"Unexpected prediction payload: {item!r}")


def write_wav_scp(samples, path):
    with path.open("w", encoding="utf-8") as f:
        for sample in samples:
            f.write(f"{sample['utt_id']} {sample['path']}\n")


def run_inference(model, samples, batch_size, work_dir):
    wav_scp = work_dir / "wav.scp"
    write_wav_scp(samples, wav_scp)
    raw = model.generate(
        str(wav_scp),
        granularity="utterance",
        extract_embedding=False,
        batch_size_s=batch_size,
    )
    if not isinstance(raw, list):
        raw = [raw]

    by_key = {}
    for item in raw:
        key = str(item.get("key") or item.get("utt") or item.get("name") or "")
        pred, score, raw_payload = parse_prediction(item)
        by_key[key] = {
            "predicted_label": pred,
            "score": score,
            "raw_prediction": raw_payload,
        }

    rows = []
    for sample in samples:
        result = by_key.get(sample["utt_id"])
        if result is None:
            raise RuntimeError(f"Missing prediction for {sample['utt_id']}")
        rows.append({**sample, **result})
    return rows


def build_summary(rows):
    y_true = [normalize_label(row["label"]) for row in rows]
    y_pred = [row["predicted_label"] for row in rows]
    accuracy = float(accuracy_score(y_true, y_pred))
    cm = confusion_matrix(y_true, y_pred, labels=LABEL_ORDER)
    report = classification_report(
        y_true,
        y_pred,
        labels=LABEL_ORDER,
        output_dict=True,
        zero_division=0,
    )
    return {
        "num_samples": len(rows),
        "accuracy": accuracy,
        "label_order": LABEL_ORDER,
        "ground_truth_distribution": dict(Counter(y_true)),
        "prediction_distribution": dict(Counter(y_pred)),
        "confusion_matrix": cm.tolist(),
        "classification_report": report,
    }


def write_results(run_dir, rows, summary, model_name):
    with (run_dir / "predictions.csv").open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(
            f,
            fieldnames=[
                "utt_id",
                "path",
                "speaker",
                "word",
                "label",
                "predicted_label",
                "score",
                "raw_prediction",
            ],
        )
        writer.writeheader()
        writer.writerows(rows)

    with (run_dir / "summary.json").open("w", encoding="utf-8") as f:
        json.dump({"model": model_name, **summary}, f, ensure_ascii=False, indent=2)
        f.write("\n")

    with (run_dir / "report.md").open("w", encoding="utf-8") as f:
        f.write("# TESS Emotion Baseline\n\n")
        f.write(f"- Model: `{model_name}`\n")
        f.write(f"- Samples: `{summary['num_samples']}`\n")
        f.write(f"- Accuracy: `{summary['accuracy'] * 100:.2f}%`\n\n")


def main():
    args = parse_args()
    os.environ.setdefault("MODELSCOPE_CACHE", str(ROOT / ".cache" / "modelscope"))
    os.environ.setdefault("HF_HOME", str(ROOT / ".cache" / "huggingface"))
    model_name = str(LOCAL_MODELSCOPE_MODEL) if args.model == DEFAULT_MODEL and LOCAL_MODELSCOPE_MODEL.exists() else args.model
    samples = collect_samples(args.tess_dir)
    if args.limit > 0:
        samples = samples[: args.limit]
    run_name = args.run_name or f"audio_network_lab_tess_baseline_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
    run_dir = args.results_dir / run_name
    run_dir.mkdir(parents=True, exist_ok=True)
    model = AutoModel(model=model_name, disable_update=True)
    rows = run_inference(model, samples, args.batch_size, run_dir)
    summary = build_summary(rows)
    write_results(run_dir, rows, summary, model_name)
    print(json.dumps({"run_dir": str(run_dir), "accuracy": summary["accuracy"], "num_samples": summary["num_samples"], "model": model_name}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
