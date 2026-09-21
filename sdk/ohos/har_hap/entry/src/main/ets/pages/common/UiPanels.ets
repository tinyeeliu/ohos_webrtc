/**
 * Copyright (c) 2026 Archermind Technology (Nanjing) Co. Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import router from '@ohos.router';
import { LoopbackMetrics, emptyMetrics } from './LoopbackSession';

/** 左上角返回 + 标题的页头。 */
@Component
export struct BackTitleBar {
  @Prop title: string = '';

  build() {
    Row({ space: 8 }) {
      Text('返回')
        .fontSize(16)
        .fontColor(Color.White)
        .backgroundColor(0x007DFF)
        .borderRadius(6)
        .padding({ left: 12, right: 12, top: 6, bottom: 6 })
        .onClick(() => {
          router.back();
        })
      Text(this.title)
        .fontSize(18)
        .fontWeight(FontWeight.Bold)
        .layoutWeight(1)
    }
    .width('100%')
    .alignItems(VerticalAlign.Center)
  }
}

/** 指标小格:上方灰色标签,下方加粗数值;'-' 表示该字段本次采样未上报。 */
@Component
struct MetricTile {
  @Prop tileLabel: string = '';
  @Prop tileValue: string = '';

  build() {
    Column({ space: 2 }) {
      Text(this.tileLabel)
        .fontSize(10)
        .fontColor(Color.Gray)
        .maxLines(1)
        .textOverflow({ overflow: TextOverflow.Ellipsis })
      Text(this.tileValue)
        .fontSize(12)
        .fontWeight(FontWeight.Bold)
        .fontColor(0x333333)
        .maxLines(1)
        .textOverflow({ overflow: TextOverflow.Ellipsis })
    }
    .layoutWeight(1)
    .alignItems(HorizontalAlign.Start)
    .padding({ left: 4, right: 4, top: 4, bottom: 4 })
    .backgroundColor(Color.White)
    .borderRadius(6)
  }
}

/** 回环链路指标面板:状态/链路/出站/入站分组展示,数据均来自 getStats 实测采样。 */
@Component
export struct VideoMetricsPanel {
  @Prop m: LoopbackMetrics = emptyMetrics();
  @Prop panelTitle: string = '回环链路指标';

  build() {
    Column({ space: 6 }) {
      Row({ space: 8 }) {
        Text(this.panelTitle)
          .fontSize(14)
          .fontWeight(FontWeight.Bold)
          .layoutWeight(1)
        Text(this.m.sampleCount > 0 ? `实测 · 第${this.m.sampleCount}次采样` : '等待首次采样')
          .fontSize(10)
          .fontColor(this.m.sampleCount > 0 ? Color.Green : Color.Gray)
      }
      .width('100%')

      this.GroupLabel('链路状态(发送端 pc1 / 接收端 pc2 实时状态)')
      Row({ space: 6 }) {
        MetricTile({ tileLabel: '连接状态', tileValue: this.m.connState })
        MetricTile({ tileLabel: '信令状态', tileValue: this.m.sigState })
        MetricTile({ tileLabel: '候选收集', tileValue: this.m.gatherState })
      }
      .width('100%')
      Row({ space: 6 }) {
        MetricTile({ tileLabel: 'ICE·发送端', tileValue: this.m.iceSender })
        MetricTile({ tileLabel: 'ICE·接收端', tileValue: this.m.iceReceiver })
        MetricTile({ tileLabel: '候选对状态', tileValue: this.m.pairState })
      }
      .width('100%')

      this.GroupLabel('出站(发送端,来自 outbound-rtp)')
      Row({ space: 6 }) {
        MetricTile({ tileLabel: '出站分辨率', tileValue: this.resText(this.m.outWidth, this.m.outHeight) })
        MetricTile({ tileLabel: '出站帧率', tileValue: this.numText(this.m.outFps, 'fps') })
      }
      .width('100%')
      Row({ space: 6 }) {
        MetricTile({ tileLabel: '发送码率(字节差分)', tileValue: this.numText(this.m.outBitrateKbps, ' kbps') })
        MetricTile({ tileLabel: '累计已发送', tileValue: this.bytesText(this.m.outBytesSent) })
      }
      .width('100%')

      this.GroupLabel('入站(接收端,来自 inbound-rtp)')
      Row({ space: 6 }) {
        MetricTile({ tileLabel: '入站分辨率', tileValue: this.resText(this.m.inWidth, this.m.inHeight) })
        MetricTile({ tileLabel: '入站帧率', tileValue: this.numText(this.m.inFps, 'fps') })
      }
      .width('100%')
      Row({ space: 6 }) {
        MetricTile({ tileLabel: '接收码率(字节差分)', tileValue: this.numText(this.m.inBitrateKbps, ' kbps') })
        MetricTile({ tileLabel: '累计丢包', tileValue: this.numText(this.m.inPacketsLost, '') })
      }
      .width('100%')
      Row({ space: 6 }) {
        MetricTile({ tileLabel: '抖动', tileValue: this.numText(this.m.inJitterMs, ' ms') })
        MetricTile({ tileLabel: '累计已接收', tileValue: this.bytesText(this.m.inBytesReceived) })
      }
      .width('100%')

      this.GroupLabel('链路质量(来自 candidate-pair)')
      Row({ space: 6 }) {
        MetricTile({ tileLabel: 'RTT', tileValue: this.numText(this.m.rttMs, ' ms') })
        MetricTile({ tileLabel: '可用发送带宽', tileValue: this.numText(this.m.availOutKbps, ' kbps') })
      }
      .width('100%')
      Text(`选中候选对(提名:${this.m.pairNominated}):${this.m.pairLocal} → ${this.m.pairRemote}`)
        .fontSize(10)
        .fontColor(0x666666)
        .width('100%')

      this.GroupLabel('原始 stats 实报内容(未上报时显示提示)')
      Text(this.m.outboundRaw === '' ? 'outbound-rtp:未上报(链路未建立或媒体未流出)' : `outbound-rtp: ${this.m.outboundRaw}`)
        .fontSize(10)
        .fontColor(0x007DFF)
        .width('100%')
      Text(this.m.inboundRaw === '' ? 'inbound-rtp:未上报(链路未建立或媒体未到达)' : `inbound-rtp: ${this.m.inboundRaw}`)
        .fontSize(10)
        .fontColor(0x007DFF)
        .width('100%')
    }
    .width('100%')
    .padding(8)
    .borderRadius(8)
    .backgroundColor(0xF1F3F5)
  }

  @Builder
  GroupLabel(title: string) {
    Text(title)
      .fontSize(11)
      .fontColor(0x555555)
      .width('100%')
      .margin({ top: 4 })
  }

  private resText(w: number | null, h: number | null): string {
    return w !== null && h !== null ? `${w}x${h}` : '-';
  }

  private numText(v: number | null, unit: string): string {
    return v === null ? '-' : `${v}${unit}`;
  }

  private bytesText(v: number | null): string {
    if (v === null) {
      return '-';
    }
    if (v >= 1024 * 1024) {
      return `${(v / 1024 / 1024).toFixed(2)} MB`;
    }
    return `${(v / 1024).toFixed(1)} KB`;
  }
}
