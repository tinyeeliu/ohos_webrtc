/**
 * Copyright (c) 2024 Archermind Technology (Nanjing) Co. Ltd.
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

const TAG: string = '[Index]';

@Entry
@Component
struct Index {
  build() {
    Column() {
      Row() {
        Button("switch to working page")
          .width(300)
          .height(60)
          .fontSize(20)
          .fontWeight(FontWeight.Bold)
          .margin({ top: 20 })
          .onClick(() => {
            console.log(TAG, "switch to working page!");
            router.pushUrl({ url: "pages/workPage"})
          })
          .margin({ left: 10, right: 20 })

      }
      .alignSelf(ItemAlign.Center)
      .padding({ top: 24 })

      Row() {
        Button("applyConstraints test page")
          .width(300)
          .height(60)
          .fontSize(20)
          .fontWeight(FontWeight.Bold)
          .margin({ top: 20 })
          .onClick(() => {
            console.log(TAG, "switch to applyConstraints test page!");
            router.pushUrl({ url: "pages/ApplyConstraintsTestPage"})
          })
          .margin({ left: 10, right: 20 })

      }
      .alignSelf(ItemAlign.Center)
      .padding({ top: 24 })

      Row() {
        Button('相机功能测试')
          .width(300)
          .height(60)
          .fontSize(20)
          .fontWeight(FontWeight.Bold)
          .margin({ top: 20 })
          .onClick(() => {
            console.log(TAG, 'switch to media camera test page!');
            router.pushUrl({ url: 'pages/MediaCameraTestPage'})
          })
          .margin({ left: 10, right: 20 })

      }
      .alignSelf(ItemAlign.Center)
      .padding({ top: 24 })

      Row() {
        Button('连接网络音频测试')
          .width(300)
          .height(60)
          .fontSize(20)
          .fontWeight(FontWeight.Bold)
          .margin({ top: 20 })
          .onClick(() => {
            console.log(TAG, 'switch to connectivity test page!');
            router.pushUrl({ url: 'pages/ConnectivityTestPage'})
          })
          .margin({ left: 10, right: 20 })

      }
      .alignSelf(ItemAlign.Center)
      .padding({ top: 24 })

    }
    .width('100%')
    .height('100%')
    .margin({ left: 24, right: 24, top: 24, bottom: 24 })
    .justifyContent(FlexAlign.Center)
  }

  aboutToAppear(): void {
    console.info(TAG, 'aboutToAppear');
  }

  aboutToDisappear(): void {
    console.info(TAG, 'aboutToDisappear');
  }

  onPageShow(): void {
    console.info(TAG, 'onPageShow');
  }

  onPageHide(): void {
    console.info(TAG, 'onPageHide');
  }

}
