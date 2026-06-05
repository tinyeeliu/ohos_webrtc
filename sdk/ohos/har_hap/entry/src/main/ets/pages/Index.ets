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
import webrtc, { VideoRenderController } from '@ohos/webrtc';

const TAG: string = '[Index]';

@Entry
@Component
struct Index {
  focusXComponentController: VideoRenderController|undefined = new VideoRenderController()
  stream: webrtc.MediaStream|undefined = undefined
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

      if(this.stream || true){
        XComponent({
          id: 'Test' + new Date().getTime(),
          type: XComponentType.SURFACE,
          controller: this.focusXComponentController
        })
          .onDestroy(() => {
            this.focusXComponentController?.setVideoTrack(null)
            this.focusXComponentController = new VideoRenderController()
          })
          .onLoad(() => {
            if(this.stream){
              this.focusXComponentController?.setVideoTrack(this.stream!.getVideoTracks()[0])
            }
          })
          .enableAnalyzer(false)
          .width('100%')
          .height(600)
          .backgroundColor(Color.Green)
      }else {
        Text('nooo')
          .width('100%')
          .height(600)
      }
      Text('测试123')
        .fontSize(20)
        .fontColor(Color.Red)
        .onClick(async () => {
          if (!this.stream) {

            let mediaDevices = new webrtc.MediaDevices();
            this.stream = await mediaDevices.getUserMedia(this.defaultVideoConstraints(false));
            this.focusXComponentController?.setVideoTrack(this.stream?.getVideoTracks()[0])
          } else {
            this.stream?.getTracks().forEach(track => {
              track.stop()
            })
            this.stream = undefined
            this.focusXComponentController?.setVideoTrack(null)
          }
        })
    }
    .width('100%')
    .height('100%')
    .margin({ left: 24, right: 24, top: 24, bottom: 24 })
    .justifyContent(FlexAlign.Center)
  }

  defaultVideoConstraints(arg0: boolean): webrtc.MediaStreamConstraints | undefined {
    // throw new Error('Method not implemented.');
    return {
      video:true
    }
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
