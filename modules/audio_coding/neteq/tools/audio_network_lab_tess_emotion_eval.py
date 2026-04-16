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
import subprocess
import sys
from datetime import datetime
from pathlib import Path

import soundfile as sf
import soxr
from sklearn.metrics import accuracy_score, classification_report

from audio_network_lab_tess_emotion_baseline import (
    DEFAULT_MODEL,
    LOCAL_MODELSCOPE_MODEL,
    AutoModel,
    build_summary,
    collect_samples,
    run_inference,
)


ROOT = Path(__file__).resolve().parents[4]
DEFAULT_TESS_DIR = ROOT / ".cache" / "datasets" / "tess" / "tess"
DEFAULT_RESULTS_DIR = ROOT / "results" / "audio_network_lab_emotion_eval"
LAB_BIN = ROOT / "out" / "AudioLab" / "audio_network_lab"
EMO_LABELS = ["neutral", "happy", "sad", "angry", "fear", "disgust"]

SCENARIOS = [
    ("clean", []),
    ("uniform_10", ["--loss_pct=10", "--redundancy_mode=none"]),
    ("uniform_20", ["--loss_pct=20", "--redundancy_mode=none"]),
    ("burst_moderate", ["--loss_pct=10", "--avg_burst_loss_length=5", "--redundancy_mode=none"]),
    ("burst_heavy", ["--loss_pct=20", "--avg_burst_loss_length=8", "--redundancy_mode=none"]),
    ("delay_jitter_10", ["--loss_pct=10", "--delay_ms=50", "--jitter_stddev_ms=10", "--queue_len_packets=50", "--redundancy_mode=none"]),
]


def parse_args():
    parser = argparse.ArgumentParser(
        description="Run audio_network_lab + emotion2vec evaluation on TESS."
    )
    parser.add_argument("--lab-bin", type=Path, default=LAB_BIN)
    parser.add_argument("--tess-dir", type=Path, default=DEFAULT_TESS_DIR)
    parser.add_argument("--results-dir", type=Path, default=DEFAULT_RESULTS_DIR)
    parser.add_argument("--model", default=DEFAULT_MODEL)
    parser.add_argument("--batch-size", type=int, default=64)
    parser.add_argument("--limit", type=int, default=0)
    parser.add_argument("--run-name", default="")
    parser.add_argument("--bitrate-bps", type=int, default=32000)
    parser.add_argument("--frame-ms", type=int, default=20)
    parser.add_argument("--channels", type=int, default=1)
    parser.add_argument("--network-random-seed", type=int, default=1)
    return parser.parse_args()


def resolve_model_name(model_name):
    if model_name == DEFAULT_MODEL and LOCAL_MODELSCOPE_MODEL.exists():
        return str(LOCAL_MODELSCOPE_MODEL)
    return model_name


def filter_samples(samples):
    return [sample for sample in samples if sample["label"] != "ps"]


def prepare_inputs(samples, prepared_dir):
    prepared_dir.mkdir(parents=True, exist_ok=True)
    prepared = []
    for sample in samples:
        output_path = prepared_dir / f"{sample['utt_id']}.wav"
        if not output_path.exists():
            audio, sample_rate_hz = sf.read(str(sample["path"]), always_2d=False)
            if getattr(audio, "ndim", 1) > 1:
                audio = audio.mean(axis=1)
            if sample_rate_hz != 48000:
                audio = soxr.resample(audio, sample_rate_hz, 48000)
            sf.write(output_path, audio, 48000, subtype="PCM_16")
        prepared.append({**sample, "path": str(output_path)})
    return prepared


def wav_to_pcm(wav_path, pcm_path):
    data, sample_rate_hz = sf.read(str(wav_path), dtype="int16")
    if sample_rate_hz != 48000:
        raise ValueError(f"{wav_path} sample rate is {sample_rate_hz}, expected 48000.")
    pcm_path.write_bytes(data.tobytes())


def run_lab_one(lab_bin, sample, scenario_name, scenario_args, output_wav, summary_json, bitrate_bps, frame_ms, channels, network_random_seed):
    input_pcm = output_wav.with_suffix(".pcm")
    wav_to_pcm(Path(sample["path"]), input_pcm)
    cmd = [
        str(lab_bin),
        f"--input_pcm={input_pcm}",
        f"--output_audio={output_wav}",
        f"--summary_json={summary_json}",
        f"--bitrate_bps={bitrate_bps}",
        f"--frame_ms={frame_ms}",
        f"--channels={channels}",
        f"--network_random_seed={network_random_seed}",
    ]
    cmd.extend(scenario_args)
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def write_eval_csv(rows, path):
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(
            f,
            fieldnames=[
                "scenario",
                "utt_id",
                "speaker",
                "word",
                "label",
                "predicted_label",
                "score",
                "audio_path",
                "summary_json",
            ],
        )
        writer.writeheader()
        writer.writerows(rows)


def aggregate_rows(scenario, samples, prediction_rows):
    aggregated = []
    by_utt = {row["utt_id"]: row for row in prediction_rows}
    for sample in samples:
        row = by_utt[sample["utt_id"]]
        aggregated.append(
            {
                "scenario": scenario,
                "utt_id": sample["utt_id"],
                "speaker": sample["speaker"],
                "word": sample["word"],
                "label": sample["label"],
                "predicted_label": row["predicted_label"],
                "score": row["score"],
                "audio_path": row["path"],
                "summary_json": row.get("summary_json", ""),
            }
        )
    return aggregated


def write_report(run_dir, scenario_summaries, model_name, sample_count):
    report_path = run_dir / "report.md"
    with report_path.open("w", encoding="utf-8") as f:
        f.write("# TESS audio_network_lab Emotion Evaluation\n\n")
        f.write(f"- Model: `{model_name}`\n")
        f.write(f"- Samples per scenario: `{sample_count}`\n\n")
        f.write("## Scenario Summary\n\n")
        f.write("| Scenario | Accuracy | Accuracy Drop vs Clean | Macro F1 |\n")
        f.write("| --- | ---: | ---: | ---: |\n")
        for item in scenario_summaries:
            f.write(
                f"| {item['scenario']} | {item['accuracy'] * 100:.2f}% | {item['accuracy_drop_pp']:.2f} pp | {item['macro_f1']:.4f} |\n"
            )


def main():
    args = parse_args()
    if not args.lab_bin.exists():
        raise FileNotFoundError(f"Missing audio_network_lab: {args.lab_bin}")

    os.environ.setdefault("MODELSCOPE_CACHE", str(ROOT / ".cache" / "modelscope"))
    os.environ.setdefault("HF_HOME", str(ROOT / ".cache" / "huggingface"))

    model_name = resolve_model_name(args.model)
    samples = filter_samples(collect_samples(args.tess_dir))
    if args.limit > 0:
        samples = samples[: args.limit]

    run_name = args.run_name or f"audio_network_lab_tess_emotion_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
    run_dir = args.results_dir / run_name
    run_dir.mkdir(parents=True, exist_ok=True)
    prepared_inputs_root = run_dir / "prepared_inputs"
    generated_root = run_dir / "generated"
    summaries_root = run_dir / "summaries"
    prepared_samples = prepare_inputs(samples, prepared_inputs_root)

    model = AutoModel(model=model_name, disable_update=True)
    rows_by_scenario = {}
    all_eval_rows = []

    for scenario_name, scenario_args in SCENARIOS:
        scenario_audio_dir = generated_root / scenario_name
        scenario_summary_dir = summaries_root / scenario_name
        scenario_audio_dir.mkdir(parents=True, exist_ok=True)
        scenario_summary_dir.mkdir(parents=True, exist_ok=True)
        scenario_samples = []

        if scenario_name == "clean":
            for sample in prepared_samples:
                scenario_samples.append({**sample, "path": sample["path"], "summary_json": ""})
        else:
            for idx, sample in enumerate(prepared_samples, 1):
                output_wav = scenario_audio_dir / f"{sample['utt_id']}.wav"
                summary_json = scenario_summary_dir / f"{sample['utt_id']}.json"
                run_lab_one(
                    args.lab_bin,
                    sample,
                    scenario_name,
                    scenario_args,
                    output_wav,
                    summary_json,
                    args.bitrate_bps,
                    args.frame_ms,
                    args.channels,
                    args.network_random_seed,
                )
                scenario_samples.append({**sample, "path": str(output_wav), "summary_json": str(summary_json)})
                if idx % 200 == 0:
                    print(f"[lab] {scenario_name}: {idx}/{len(prepared_samples)}")

        prediction_rows = run_inference(model, scenario_samples, args.batch_size, scenario_audio_dir)
        for row in prediction_rows:
            for sample in scenario_samples:
                if sample["utt_id"] == row["utt_id"]:
                    row["summary_json"] = sample.get("summary_json", "")
                    break
        rows_by_scenario[scenario_name] = prediction_rows
        all_eval_rows.extend(aggregate_rows(scenario_name, scenario_samples, prediction_rows))

    clean_rows = rows_by_scenario["clean"]
    clean_true = [row["label"] for row in clean_rows]
    clean_pred = [str(row["predicted_label"]) for row in clean_rows]
    clean_accuracy = accuracy_score(clean_true, clean_pred)

    scenario_summaries = []
    for scenario_name, rows in rows_by_scenario.items():
        y_true = [row["label"] for row in rows]
        y_pred = [str(row["predicted_label"]) for row in rows]
        cls_report = classification_report(
            y_true, y_pred, labels=EMO_LABELS, output_dict=True, zero_division=0
        )
        accuracy = accuracy_score(y_true, y_pred)
        scenario_summaries.append(
            {
                "scenario": scenario_name,
                "accuracy": accuracy,
                "accuracy_drop_pp": (clean_accuracy - accuracy) * 100.0,
                "macro_f1": cls_report["macro avg"]["f1-score"],
            }
        )

    with (run_dir / "scenario_summary.json").open("w", encoding="utf-8") as f:
        json.dump(
            {
                "model": model_name,
                "samples_per_scenario": len(prepared_samples),
                "scenario_summaries": scenario_summaries,
            },
            f,
            ensure_ascii=False,
            indent=2,
        )
        f.write("\n")

    write_eval_csv(all_eval_rows, run_dir / "emotion_predictions.csv")
    write_report(run_dir, scenario_summaries, model_name, len(prepared_samples))
    print(
        json.dumps(
            {
                "run_dir": str(run_dir),
                "samples_per_scenario": len(prepared_samples),
                "scenarios": [name for name, _ in SCENARIOS],
            }
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
