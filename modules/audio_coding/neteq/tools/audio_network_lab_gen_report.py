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
from pathlib import Path


def load_json(path):
    with Path(path).open("r", encoding="utf-8") as f:
        return json.load(f)


def fmt_float(value, digits=2):
    return f"{float(value):.{digits}f}"


def build_markdown(summary, summary_report, compare_report):
    input_cfg = summary["input"]
    encoder = summary["encoder"]
    network = summary["network"]
    output = summary["output"]
    network_stats = summary["network_stats"]
    neteq_stats = summary["neteq_stats"]

    lines = []
    lines.append("# Audio Network Lab Report")
    lines.append("")
    lines.append("## Experiment")
    lines.append("")
    lines.append(
        f"- Input: `{input_cfg['input_pcm']}` "
        f"({input_cfg['sample_rate_hz']} Hz, {input_cfg['channels']} ch, "
        f"{input_cfg['input_duration_ms']} ms)"
    )
    lines.append(
        f"- Output: `{output['output_audio']}` "
        f"({output['sample_rate_hz']} Hz, {output['channels']} ch, "
        f"{output['decoded_duration_ms']} ms)"
    )
    lines.append(
        f"- Encoder: `{encoder['codec']}`, bitrate={encoder['bitrate_bps']} bps, "
        f"frame={encoder['frame_ms']} ms, dtx={str(encoder['enable_dtx']).lower()}, "
        f"redundancy={encoder['redundancy_mode']}, expected_loss={encoder['expected_loss_pct']}%"
    )
    lines.append(
        f"- Network: bandwidth={network['bandwidth_kbps']} kbps, delay={network['delay_ms']} ms, "
        f"jitter={network['jitter_stddev_ms']} ms, loss={network['loss_pct']}%, "
        f"reorder={str(network['allow_reorder']).lower()}, queue={network['queue_len_packets']}, "
        f"avg_burst={network['avg_burst_loss_length']}, seed={network['random_seed']}"
    )
    if network["fixed_loss_events_ms"]:
        lines.append(f"- Fixed loss events: `{network['fixed_loss_events_ms']}`")
    lines.append("")

    lines.append("## Network Summary")
    lines.append("")
    lines.append("| Metric | Value |")
    lines.append("| --- | ---: |")
    lines.append(f"| Enqueued packets | {network_stats['enqueued_packets']} |")
    lines.append(f"| Delivered packets | {network_stats['delivered_packets']} |")
    lines.append(f"| Lost packets | {network_stats['lost_packets']} |")
    lines.append(f"| Queue dropped packets | {network_stats['queue_dropped_packets']} |")
    lines.append(
        f"| Fixed-loss dropped packets | {network_stats['fixed_loss_dropped_packets']} |"
    )
    lines.append(f"| Reordered packets | {network_stats['reordered_packets']} |")
    lines.append(
        f"| Actual encoded bitrate | {fmt_float(summary_report['encoder']['actual_encoded_kbps'])} kbps |"
    )
    lines.append(
        f"| Delivery rate | {fmt_float(summary_report['network']['delivery_rate_pct'])}% |"
    )
    lines.append(
        f"| Loss rate | {fmt_float(summary_report['network']['loss_rate_pct'])}% |"
    )
    lines.append("")

    lines.append("## NetEq Summary")
    lines.append("")
    lines.append("| Metric | Value |")
    lines.append("| --- | ---: |")
    lines.append(f"| Concealed samples | {neteq_stats['concealed_samples']} |")
    lines.append(f"| Silent concealed samples | {neteq_stats['silent_concealed_samples']} |")
    lines.append(f"| Concealment events | {neteq_stats['concealment_events']} |")
    lines.append(f"| FEC packets received | {neteq_stats['fec_packets_received']} |")
    lines.append(f"| FEC packets discarded | {neteq_stats['fec_packets_discarded']} |")
    lines.append(f"| Packets discarded | {neteq_stats['packets_discarded']} |")
    lines.append(
        f"| Concealment ratio | {fmt_float(summary_report['neteq']['concealment_ratio_pct'])}% |"
    )
    lines.append(
        f"| Secondary decoded rate | {fmt_float(summary_report['neteq']['secondary_decoded_rate_ratio'], 4)} |"
    )
    lines.append(
        f"| Expand rate | {fmt_float(summary_report['neteq']['expand_rate_ratio'], 4)} |"
    )
    lines.append(
        f"| Accelerate rate | {fmt_float(summary_report['neteq']['accelerate_rate_ratio'], 4)} |"
    )
    lines.append(
        f"| Preemptive expand rate | {fmt_float(summary_report['neteq']['preemptive_expand_rate_ratio'], 4)} |"
    )
    lines.append(
        f"| Avg jitter buffer delay | {fmt_float(summary_report['neteq']['avg_jitter_buffer_delay_ms'])} ms |"
    )
    lines.append(
        f"| Avg target delay | {fmt_float(summary_report['neteq']['avg_jitter_buffer_target_delay_ms'])} ms |"
    )
    lines.append(
        f"| Interruptions | {summary_report['neteq']['interruption_count']} |"
    )
    lines.append(
        f"| Total interruption duration | {summary_report['neteq']['total_interruption_duration_ms']} ms |"
    )
    lines.append("")

    lines.append("## Audio Compare")
    lines.append("")
    lines.append("| Metric | Value |")
    lines.append("| --- | ---: |")
    lines.append(
        f"| Alignment lag | {compare_report['alignment']['lag_frames']} frames "
        f"({fmt_float(compare_report['alignment']['lag_ms'], 3)} ms) |"
    )
    lines.append(
        f"| Alignment score | {fmt_float(compare_report['alignment']['normalized_correlation'], 5)} |"
    )
    lines.append(
        f"| Overlap duration | {fmt_float(compare_report['overlap']['duration_ms'])} ms |"
    )
    lines.append(f"| SNR | {fmt_float(compare_report['overlap']['snr_db'])} dB |")
    lines.append(
        f"| Pearson correlation | {fmt_float(compare_report['overlap']['pearson_correlation'], 5)} |"
    )
    lines.append(
        f"| Error RMS | {fmt_float(compare_report['overlap']['error_rms_dbfs'])} dBFS |"
    )
    lines.append(
        f"| Error peak | {fmt_float(compare_report['overlap']['error_peak_dbfs'])} dBFS |"
    )
    lines.append(
        f"| Mean abs error | {fmt_float(compare_report['overlap']['mean_abs_error_pcm16'])} |"
    )
    lines.append(
        f"| Max abs error | {compare_report['overlap']['max_abs_error_pcm16']} |"
    )
    lines.append(
        f"| Clipped test samples | {compare_report['overlap']['clipped_test_samples']} |"
    )
    lines.append("")

    lines.append("## Quick Interpretation")
    lines.append("")
    lines.append(
        f"- 当前 case 的实际网络丢包率是 {fmt_float(summary_report['network']['loss_rate_pct'])}%，"
        f"实验目标配置为 {fmt_float(network['loss_pct'])}%。"
    )
    lines.append(
        f"- NetEq concealment 占输出样本的 {fmt_float(summary_report['neteq']['concealment_ratio_pct'])}%，"
        f"FEC 收到 {summary_report['neteq']['fec_packets_received']} 包。"
    )
    lines.append(
        f"- 参考音频与输出音频对齐后 lag 为 "
        f"{fmt_float(compare_report['alignment']['lag_ms'], 3)} ms，"
        f"SNR 为 {fmt_float(compare_report['overlap']['snr_db'])} dB，"
        f"相关性为 {fmt_float(compare_report['overlap']['pearson_correlation'], 5)}。"
    )
    if summary_report["neteq"]["interruption_count"] > 0:
        lines.append(
            f"- 发生了 {summary_report['neteq']['interruption_count']} 次 interruption，"
            f"累计 {summary_report['neteq']['total_interruption_duration_ms']} ms。"
        )
    else:
        lines.append("- 未观测到 NetEq interruption。")
    lines.append("")

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="Generate a Markdown report for one audio_network_lab run."
    )
    parser.add_argument("--summary_json", required=True)
    parser.add_argument("--summary_report_json", required=True)
    parser.add_argument("--audio_compare_json", required=True)
    parser.add_argument("--output_md", required=True)
    args = parser.parse_args()

    summary = load_json(args.summary_json)
    summary_report = load_json(args.summary_report_json)
    compare_report = load_json(args.audio_compare_json)
    markdown = build_markdown(summary, summary_report, compare_report)

    output_path = Path(args.output_md)
    with output_path.open("w", encoding="utf-8") as f:
        f.write(markdown)
        f.write("\n")


if __name__ == "__main__":
    main()
