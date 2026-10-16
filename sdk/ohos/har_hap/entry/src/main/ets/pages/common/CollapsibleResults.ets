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

@Component
export struct CollapsibleResults {
  @Link items: string[];
  @State expanded: boolean = false;
  @Prop maxVisible: number = 3;

  build() {
    Column() {
      if (this.items.length > 0) {
        ForEach(this.visibleItems(), (item: string, index: number) => {
          Text(item)
            .fontSize(13)
            .fontColor(this.itemColor(item))
            .margin({ bottom: 4 })
            .alignSelf(ItemAlign.Start)
        }, (item: string, index: number) => 'r' + index.toString())
        if (this.items.length > this.maxVisible) {
          Text(this.expanded ? '收起 ▲' : '展开全部 ' + this.items.length + ' 条 ▼')
            .fontSize(12)
            .fontColor(0x007DFF)
            .margin({ top: 2, bottom: 4 })
            .onClick(() => { this.expanded = !this.expanded; })
        }
      }
    }
    .width('100%')
    .alignItems(HorizontalAlign.Start)
  }

  private visibleItems(): string[] {
    if (this.expanded || this.items.length <= this.maxVisible) {
      return this.items;
    }
    return this.items.slice(this.items.length - this.maxVisible);
  }

  private itemColor(item: string): number {
    if (item.startsWith('✓')) {
      return 0x00AA00;
    }
    if (item.startsWith('✗')) {
      return 0xFF0000;
    }
    return 0x666666;
  }
}
