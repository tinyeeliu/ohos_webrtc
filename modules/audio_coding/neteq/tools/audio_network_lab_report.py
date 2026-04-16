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
from pathlib import Path


def pct(numerator, denominator):
    if denominator == 0:
        return 0.0
    return 100.0 * float(numerator) / float(denominator)


def rate_percents_to_ratio(value):
    return float(value) / 100.0


def avg_or_zero(total, count):
    if count == 0:
        return 0.0
    return float(total) / float(count)


def build_report(summary):
    input_cfg = summary["input"]
    encoder = summary["encoder"]
    network = summary["network"]
    encoded = summary["encoded"]
    network_stats = summary["network_stats"]
    output = summary["output"]
    neteq_stats = summary["neteq_stats"]

    input_duration_ms = max(int(input_cfg["input_duration_ms"]), 1)
    encoded_kbps = (float(encoded["bytes"]) * 8.0) / float(input_duration_ms)
    delivered = int(network_stats["delivered_packets"])
    enqueued = int(network_stats["enqueued_packets"])
    lost = int(network_stats["lost_packets"])
    queue_dropped = int(network_stats["queue_dropped_packets"])
    fixed_dropped = int(network_stats["fixed_loss_dropped_packets"])
    reordered = int(network_stats["reordered_packets"])

    decoded_samples = max(int(output["decoded_samples"]), 1)
    concealed_samples = int(neteq_stats["concealed_samples"])
    silent_concealed_samples = int(neteq_stats["silent_concealed_samples"])
    fec_packets_received = int(neteq_stats["fec_packets_received"])
    fec_packets_discarded = int(neteq_stats["fec_packets_discarded"])
    packets_discarded = int(neteq_stats["packets_discarded"])
    interruptions = int(neteq_stats["interruption_count"])
    interruption_ms = int(neteq_stats["total_interruption_duration_ms"])
    emitted_count = int(neteq_stats["jitter_buffer_emitted_count"])

    return {
        "input": {
            "path": input_cfg["input_pcm"],
            "sample_rate_hz": int(input_cfg["sample_rate_hz"]),
            "channels": int(input_cfg["channels"]),
            "duration_ms": int(input_cfg["input_duration_ms"]),
        },
        "encoder": {
            "codec": encoder["codec"],
            "bitrate_bps": int(encoder["bitrate_bps"]),
            "frame_ms": int(encoder["frame_ms"]),
            "enable_dtx": bool(encoder["enable_dtx"]),
            "redundancy_mode": encoder["redundancy_mode"],
            "expected_loss_pct": int(encoder["expected_loss_pct"]),
            "encoded_packets": int(encoded["packets"]),
            "encoded_bytes": int(encoded["bytes"]),
            "actual_encoded_kbps": encoded_kbps,
        },
        "network": {
            "configured_loss_pct": float(network["loss_pct"]),
            "configured_delay_ms": int(network["delay_ms"]),
            "configured_jitter_stddev_ms": int(network["jitter_stddev_ms"]),
            "configured_queue_len_packets": int(network["queue_len_packets"]),
            "enqueued_packets": enqueued,
            "delivered_packets": delivered,
            "lost_packets": lost,
            "queue_dropped_packets": queue_dropped,
            "fixed_loss_dropped_packets": fixed_dropped,
            "reordered_packets": reordered,
            "delivery_rate_pct": pct(delivered, enqueued),
            "loss_rate_pct": pct(lost, enqueued),
            "queue_drop_rate_pct": pct(queue_dropped, enqueued),
            "fixed_loss_drop_rate_pct": pct(fixed_dropped, enqueued),
            "reorder_rate_pct": pct(reordered, delivered),
        },
        "output": {
            "path": output["output_audio"],
            "sample_rate_hz": int(output["sample_rate_hz"]),
            "channels": int(output["channels"]),
            "duration_ms": int(output["decoded_duration_ms"]),
            "decoded_samples": int(output["decoded_samples"]),
        },
        "neteq": {
            "concealed_samples": concealed_samples,
            "silent_concealed_samples": silent_concealed_samples,
            "concealment_events": int(neteq_stats["concealment_events"]),
            "concealment_ratio_pct": pct(concealed_samples, decoded_samples),
            "silent_concealment_ratio_pct": pct(
                silent_concealed_samples, decoded_samples
            ),
            "fec_packets_received": fec_packets_received,
            "fec_packets_discarded": fec_packets_discarded,
            "packets_discarded": packets_discarded,
            "fec_receive_rate_pct": pct(fec_packets_received, encoded["packets"]),
            "jitter_buffer_delay_ms": int(neteq_stats["jitter_buffer_delay_ms"]),
            "avg_jitter_buffer_delay_ms": avg_or_zero(
                neteq_stats["jitter_buffer_delay_ms"], emitted_count
            ),
            "jitter_buffer_target_delay_ms": int(
                neteq_stats["jitter_buffer_target_delay_ms"]
            ),
            "avg_jitter_buffer_target_delay_ms": avg_or_zero(
                neteq_stats["jitter_buffer_target_delay_ms"], emitted_count
            ),
            "jitter_buffer_minimum_delay_ms": int(
                neteq_stats["jitter_buffer_minimum_delay_ms"]
            ),
            "avg_jitter_buffer_minimum_delay_ms": avg_or_zero(
                neteq_stats["jitter_buffer_minimum_delay_ms"], emitted_count
            ),
            "expand_rate_ratio": rate_percents_to_ratio(neteq_stats["expand_rate"]),
            "speech_expand_rate_ratio": rate_percents_to_ratio(
                neteq_stats["speech_expand_rate"]
            ),
            "accelerate_rate_ratio": rate_percents_to_ratio(
                neteq_stats["accelerate_rate"]
            ),
            "preemptive_expand_rate_ratio": rate_percents_to_ratio(
                neteq_stats["preemptive_expand_rate"]
            ),
            "secondary_decoded_rate_ratio": rate_percents_to_ratio(
                neteq_stats["secondary_decoded_rate"]
            ),
            "secondary_discarded_rate_ratio": rate_percents_to_ratio(
                neteq_stats["secondary_discarded_rate"]
            ),
            "max_expand_rate_ratio": rate_percents_to_ratio(
                neteq_stats["max_expand_rate"]
            ),
            "max_secondary_decoded_rate_ratio": rate_percents_to_ratio(
                neteq_stats["max_secondary_decoded_rate"]
            ),
            "max_secondary_discarded_rate_ratio": rate_percents_to_ratio(
                neteq_stats["max_secondary_discarded_rate"]
            ),
            "interruption_count": interruptions,
            "total_interruption_duration_ms": interruption_ms,
            "mean_interruption_duration_ms": (
                float(interruption_ms) / float(interruptions)
                if interruptions > 0
                else 0.0
            ),
        },
    }


def print_report(report):
    encoder = report["encoder"]
    network = report["network"]
    neteq = report["neteq"]
    output = report["output"]

    print("Audio Network Lab Report")
    print("========================")
    print(
        f"Input: {report['input']['path']} "
        f"({report['input']['sample_rate_hz']} Hz, "
        f"{report['input']['channels']} ch, "
        f"{report['input']['duration_ms']} ms)"
    )
    print(
        f"Output: {output['path']} "
        f"({output['sample_rate_hz']} Hz, {output['channels']} ch, "
        f"{output['duration_ms']} ms)"
    )
    print(
        f"Encoder: {encoder['codec']} @ {encoder['bitrate_bps']} bps, "
        f"frame={encoder['frame_ms']} ms, "
        f"redundancy={encoder['redundancy_mode']}, "
        f"actual={encoder['actual_encoded_kbps']:.2f} kbps"
    )
    print(
        f"Network: enqueued={network['enqueued_packets']}, "
        f"delivered={network['delivered_packets']}, "
        f"lost={network['lost_packets']} ({network['loss_rate_pct']:.2f}%), "
        f"queue_drop={network['queue_dropped_packets']}, "
        f"fixed_drop={network['fixed_loss_dropped_packets']}, "
        f"reordered={network['reordered_packets']}"
    )
    print(
        f"NetEq: concealed={neteq['concealed_samples']} "
        f"({neteq['concealment_ratio_pct']:.2f}%), "
        f"fec_rx={neteq['fec_packets_received']}, "
        f"fec_discarded={neteq['fec_packets_discarded']}, "
        f"secondary_decode={neteq['secondary_decoded_rate_ratio']:.4f}, "
        f"expand={neteq['expand_rate_ratio']:.4f}"
    )
    print(
        f"Buffering: avg_delay={neteq['avg_jitter_buffer_delay_ms']:.2f} ms, "
        f"avg_target={neteq['avg_jitter_buffer_target_delay_ms']:.2f} ms, "
        f"avg_min={neteq['avg_jitter_buffer_minimum_delay_ms']:.2f} ms"
    )
    print(
        f"Interruptions: count={neteq['interruption_count']}, "
        f"total={neteq['total_interruption_duration_ms']} ms, "
        f"mean={neteq['mean_interruption_duration_ms']:.2f} ms"
    )


def main():
    parser = argparse.ArgumentParser(
        description="Summarize audio_network_lab summary.json into a compact report."
    )
    parser.add_argument("summary_json", help="Path to audio_network_lab summary.json.")
    parser.add_argument(
        "--report_json",
        help="Optional path to write the derived report as JSON.",
    )
    args = parser.parse_args()

    summary_path = Path(args.summary_json)
    with summary_path.open("r", encoding="utf-8") as f:
        summary = json.load(f)

    report = build_report(summary)
    print_report(report)

    if args.report_json:
        report_path = Path(args.report_json)
        with report_path.open("w", encoding="utf-8") as f:
            json.dump(report, f, indent=2, sort_keys=True)
            f.write("\n")


if __name__ == "__main__":
    main()
