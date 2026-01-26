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
            // TODO jump to workpage
            router.pushUrl({ url: "pages/workPage"})
          })
      .margin({ left: 10, right: 20 })

    }
    .alignSelf(ItemAlign.Center)
    .padding({ top: 24 })

      Row() {
        Button(this.muteBtnTitle)
          .width(150)
          .height(54)
          .fontSize(20)
          .fontWeight(FontWeight.Bold)
          .margin({ top: 20 })
          .onClick(() => {
            console.log(TAG, "audio muted: " + this.muted);
            this.muted = !this.muted;
            this.muteBtnTitle = this.muted ? 'Unmute' : 'Mute';
            this.pc?.setAudioPlayout(!this.muted);
            this.pc?.setAudioRecording(!this.muted);
          })
          .margin({ left: 10, right: 20 })

      }
      .alignSelf(ItemAlign.Start)
      .padding({ top: 24 })

      Row() {
        Button("switchCamera")
          .width(150)
          .height(54)
          .fontSize(20)
          .fontWeight(FontWeight.Bold)
          .margin({ top: 20 })
          .onClick(() => {
            if(this.localVideoSource){
              this.localVideoSource.switchCamera();
            }
          })
          .margin({ left: 10, right: 20 })
      }
      .alignSelf(ItemAlign.Start)
      .padding({ top: 24 })

      Row() {
        XComponent({
          id: "local-video",
          type: XComponentType.SURFACE,
          controller: this.mXComponentController1
        })
          .width('480px')
          .height('480px')
          .onLoad((context?) => {
            console.log(TAG, "context: " + context);
            console.log(TAG, "surfaceId: " + this.mXComponentController1.getXComponentSurfaceId());
          })
          .onDestroy(() => {
            console.log(TAG, "onDestroy");
          })

        XComponent({
          id: "remote-video",
          type: XComponentType.SURFACE,
          controller: this.mXComponentController2
        })
          .width('480px')
          .height('480px')
          .onLoad((context?) => {
            console.log(TAG, "context: " + JSON.stringify(context));
            console.log(TAG, "surfaceId: " + this.mXComponentController2.getXComponentSurfaceId());
          })
          .onDestroy(() => {
            console.log(TAG, "onDestroy");
          })
      }
      .alignSelf(ItemAlign.Start)
      .padding({ top: 10 })
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
