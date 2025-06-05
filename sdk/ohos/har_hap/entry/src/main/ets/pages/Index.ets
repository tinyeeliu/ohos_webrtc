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

import webrtc, { Logging, LoggingSeverity, VideoRenderController, ScalingMode } from '@ohos/webrtc';
import abilityAccessCtrl, { PermissionRequestResult } from '@ohos.abilityAccessCtrl';
import webSocket from '@ohos.net.webSocket';
import promptAction from '@ohos.promptAction';
// import buffer from '@ohos.buffer';
import fs from '@ohos.file.fs';
import audio from '@ohos.multimedia.audio';
import { BusinessError } from '@kit.BasicServicesKit';
// import { util ,buffer} from '@kit.ArkTS';

const TAG: string = '[chindeo_webrtc]';

// 用于测试的信令服务器
const SIGNAL_SERVER = `ws://192.168.20.19:3100/websocket`;
// 用于测试的Web客户端地址
// https://webrtc.youzhi.life/
// 用于测试的STUN服务器地址
// const STUN_SERVER = "stun:47.112.22.152:3478"

const videoStartBitrateKbps =512;
const videoMinBitrateKbps= 512;
const videoMaxBitrateKbps= 512;
const audioStartBitrate=128;
const audioStereo= 0;
const VIDEO_RESOLUTION_WIDTH = 640;
const VIDEO_RESOLUTION_HEIGHT = 480;
const FPS = 15;
const H264 = "H264";
const VP8 = "VP8";
const VP9 = "VP9";
const OPUS = "opus";


interface Info{
  videoCodecNumbers:string[],
  vp8LineNumber:string,
  vp9LineNumber:string,
  h264LineNumber:string,
  videoCodecNumbersOriginal:string,
}

interface Invite {
  name:"";
  number:"";
  uuid:"";
  isVideo:false;
  isCallout:false;
}
// sdp?: webrtc.RTCSessionDescriptionInit;
// ice?: webrtc.RTCIceCandidateInit;

interface ServerInfo {
  serveNumber:"";
  serveIp:"";
  serveDisplayName:"";
}

interface MessageData {
  eventName:"";
  locId?:"";
  number?:"";
  data?:"";
}

interface WsTrack {
  streamId: string;
  videoTrackId: string;
  audioTrackId: string;
}

@Entry
@Component
struct Index {
  @State buttonTitle: string = '拨打';
  @State answerBtn:number = Visibility.Hidden;
  @State remoteVideo:number = Visibility.Hidden;
  @State registerBtn:number = Visibility.Hidden;
  @State serverText: string = '';
  @State statusText: string = '';
  @State private number: string = '';
  @State private peerId: string = '1034';
  @State private uuid?: string = '';
  @State private locId: string = '12160708';
  @State private clientType: string = 'bed';
  @State private status: string = 'hangup';
  @State private ws_status: boolean = false;
  @State private hardDecode: boolean = false;
  @State private isCallout: boolean = false;
  @State private ices:webrtc.RTCIceCandidateInit[] = [];
  private mXComponentController1: VideoRenderController = new VideoRenderController();
  private mXComponentController2: VideoRenderController = new VideoRenderController();
  private ws: webSocket.WebSocket = webSocket.createWebSocket();
  private localAudioSource?: webrtc.AudioSource;
  private localAudioSource2?: webrtc.AudioSource;
  private localVideoSource?: webrtc.VideoSource;
  private localAudioTrack?: webrtc.MediaStreamTrack;
  private localAudioTrack2?: webrtc.MediaStreamTrack;
  private localVideoTrack?: webrtc.MediaStreamTrack;
  private remoteAudioTrack?: webrtc.MediaStreamTrack;
  private remoteVideoTrack?: webrtc.MediaStreamTrack;
  private adm?: webrtc.AudioDeviceModule;
  private pcf?: webrtc.PeerConnectionFactory;
  private pc?: webrtc.RTCPeerConnection;
  // private recvChannel?: webrtc.RTCDataChannel;
  // private sendChannel?: webrtc.RTCDataChannel;
  private displayMediaStream?: webrtc.MediaStream;
  private mediaDevices = new webrtc.MediaDevices();
  // private onDataChannelMessageReceived = (event: webrtc.MessageEvent<string | ArrayBuffer>) => {
  //   // this is Index
  //   console.info(TAG, "dataChannel.OnMessage: ", event);
  //   console.info(TAG, "dataChannel.OnMessage: (typeof event.data) = ", typeof event.data);
  //   console.info(TAG, "dataChannel.OnMessage: (event.data instanceof ArrayBuffer) = ", event.data instanceof ArrayBuffer);
  //
  //   this.setStatus("Received data channel message");
  //
  //   if (typeof event.data === 'string' || event.data instanceof String) {
  //     console.info(TAG, 'Incoming string message: ' + event.data);
  //   } else {
  //     console.info(TAG, 'Incoming data message');
  //   }
  // }

  //
  build() {
    Column() {
      Row() {
        Text("服务信息")
          .fontSize(32)
          .fontWeight(FontWeight.Normal)
          .margin({ right: 24 })
        Text(this.serverText)
          .fontSize(32)
          .fontWeight(FontWeight.Bold)
      }
      .alignSelf(ItemAlign.Start)

      Row() {
        Text("状态")
          .fontSize(32)
          .fontWeight(FontWeight.Normal)
          .margin({ right: 24 })
        Text(this.statusText)
          .fontSize(32)
          .fontWeight(FontWeight.Bold)
      }
      .alignSelf(ItemAlign.Start)

      Row() {
        Text("本机ID")
          .fontSize(32)
          .fontWeight(FontWeight.Normal)
          .margin({ right: 24 })
          .align(Alignment.Start)
        Text(this.number)
          .fontSize(32)
          .fontWeight(FontWeight.Bold)
      }
      .alignSelf(ItemAlign.Start)
      .padding({ top: 10 })

      Row() {
        Text("静音")
          .width(200)
          .height(54)
          .fontSize(28)
          .margin({ top: 20 })
          // .textAlign(TextAlign.Start)
          .backgroundColor(0xFFFFFF)
        Toggle({ type: ToggleType.Switch })
          .margin({ left: 100, right: 10 })
          .onChange((isOn: boolean) => {
            // this.speakerMute = isOn
            // this.adm.setSpeakerMute(this.speakerMute)
            this.adm?.setMicrophoneMute(isOn)
            // this.adm.setNoiseSuppressorEnabled(true)
            // this.pc?.setAudioPlayout(isOn);
            // this.pc?.setAudioRecording(isOn);
          })
      }
      .backgroundColor(0xFFFFFF)
      .alignSelf(ItemAlign.Start)
      .padding({ top: 24 })

      Row() {
        TextInput({ placeholder: '请输入对端ID',text:  this.peerId })
          .width(200)
          .height(54)
          .fontSize(28)
          .fontWeight(FontWeight.Normal)
          .type(InputType.Number)
          .layoutWeight('20%')
          .onChange((value: string) => {
            this.peerId = value;
          })

        Button(this.buttonTitle)
          .width(150)
          .height(54)
          .fontSize(20)
          .fontWeight(FontWeight.Bold)
          .margin({ top: 20 })
          .onClick(() => {
            if (this.buttonTitle == '挂断') {
              this.sendMessage(JSON.stringify(  {
                eventName: '__apply',
                locId: this.locId,
                number: this.number.toString(),
                clientType:this.clientType,
                data: JSON.stringify(
                  {
                    to: this.peerId.toString(),
                    uuid: this.uuid,
                    isAll: true,
                    apply:"hangup",
                  }
                )
              }));
              this.remoteVideo = Visibility.Hidden
              this.answerBtn = Visibility.Hidden
              this.buttonTitle = '拨打';
              this.isCallout=false;
              this.uuid="";
              this.status="hangup";
              this.setStatus("Registering with server");
              return;
            }

            if (this.peerId.length < 2 || this.peerId.length > 4) {
              promptAction.showToast({ message: "请输入2到4位数ID" });
              return;
            }

            if (!this.pc){
              this.createPeerConnection();
            }

            this.sendMessage(JSON.stringify(  {
              eventName: '__apply',
              locId: this.locId,
              number: this.number.toString(),
              clientType:this.clientType,
              data: JSON.stringify(
                {
                  to: this.peerId.toString(),
                  uuid: this.uuid,
                  isVideo: true,
                  apply:"invite"
                }
              )
            }));
            this.isCallout=true;
            this.buttonTitle = '挂断';
          })
          .margin({ left: 10, right: 20 })

        Button("接听")
          .visibility(this.answerBtn)
          .width(150)
          .height(54)
          .fontSize(20)
          .fontWeight(FontWeight.Bold)
          .margin({ top: 20 })
          .onClick(() => {
            setTimeout(()=>{
              this.sendMessage(JSON.stringify(  {
                eventName: '__apply',
                locId: this.locId,
                number: this.number.toString(),
                clientType:this.clientType,
                data: JSON.stringify(
                  {
                    to: this.peerId.toString(),
                    uuid: this.uuid,
                    apply:"answer",
                    isVideo: true,
                  }
                )
              }));
            },1000)
          })
          .margin({ left: 10, right: 20 })

        Button("注册")
          .visibility(this.registerBtn)
          .width(150)
          .height(54)
          .fontSize(20)
          .fontWeight(FontWeight.Bold)
          .margin({ top: 20 })
          .onClick(() => {
            this.ws.connect(SIGNAL_SERVER+`?locId=${this.locId}&number=${this.number}&displayName=${this.number}&clientType=${this.clientType}`, (err, success) => {
              console.info(TAG, "Connecting to server"+err);
            })
            this.ws_status=true;
            // setTimeout(()=>{
            //   this.sendMessage(JSON.stringify(  {
            //     eventName: '__apply',
            //     locId: this.locId,
            //     number: this.number.toString(),
            //     clientType:this.clientType,
            //     data: JSON.stringify(
            //       {
            //         to: this.peerId.toString(),
            //         uuid: this.uuid,
            //         apply:"answer",
            //         isVideo: true,
            //       }
            //     )
            //   }));
            // },1000)
          })
          .margin({ left: 10, right: 20 })

        Row() {
          Checkbox({ name: 'remote_offerer', group: 'checkboxGroup' })
            .select(false)
            .selectedColor(0x39a2db)
            .onChange((value: boolean) => {
              console.info('remote_offerer change is' + value)

            }).margin({ right: 8 })

          Text("Remote offerer")
            .fontSize(20)
            .fontWeight(FontWeight.Normal)
        }.layoutWeight('20%').visibility(Visibility.Hidden)

      }
      .alignSelf(ItemAlign.Start)
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
        XComponent({
          id: "local-video",
          type: XComponentType.SURFACE,
          controller: this.mXComponentController1
        })
          .width('320px')
          .height('280px')
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
          .visibility(this.remoteVideo)
          .width('320px')
          .height('280px')
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
    .justifyContent(FlexAlign.Start)
  }

  aboutToAppear(): void {
    // // 设置音频策略，允许和其它音频流并发
    // let strategy: audio.AudioSessionStrategy = {
    //   concurrencyMode: audio.AudioConcurrencyMode.CONCURRENCY_MIX_WITH_OTHERS
    // };
    //
    // let audioManager = audio.getAudioManager();
    // audioManager.getAudioScene().then((scene) => {
    //   console.info(TAG, 'getAudioScene :'+  scene.toString());
    // }).catch((err: BusinessError) => {
    //   console.error(`audio getAudioScene ERROR: ${err}`);
    // });
    // audioManager.getVolumeManager().on("volumeChange",e =>{
    //   console.error(`audio volumeChange: ${e.volume}`);
    // })
    //  audioManager.getRoutingManager().getDevices(audio.DeviceFlag.OUTPUT_DEVICES_FLAG).then(o =>{
    //   if (o.length === 0) {
    //     console.error("audio 无可用输出设备!");
    //   }
    //   o.forEach(i =>{
    //     console.error(`audio id:${i.id}
    //     audio  name:${i.name}
    //     audio  displayName:${i.displayName}
    //     audio  sampleRates:${i.sampleRates}
    //     audio  channelCounts:${i.channelCounts}
    //     audio  channelMasks:${i.channelMasks}
    //     audio  address:${i.address}
    //     audio  encodingTypes:${i.encodingTypes}
    //     audio  deviceType:${i.deviceType}
    //     audio  deviceRole:${i.deviceRole}`)
    //   })
    // }).catch((err: BusinessError) =>{
    //    console.error(`audio 无可用输出设备 ERROR: ${err}`);
    //  });


    // let audioSessionManager = audioManager.getSessionManager();
    // audioSessionManager.activateAudioSession(strategy).then(() => {
    //   console.info(TAG, 'activate AudioSession SUCCESS');
    // }).catch((err: BusinessError) => {
    //   console.error(`audio ERROR: ${err}`);
    // });

    Logging.enableLogThreads();
    Logging.enableLogTimeStamps();
    Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);

    abilityAccessCtrl.createAtManager()
      .requestPermissionsFromUser(getContext(), [
        'ohos.permission.CAMERA',
        'ohos.permission.MICROPHONE',
        'ohos.permission.MODIFY_AUDIO_SETTINGS',
        'ohos.permission.INTERNET',
    ])
      .then(async (result: PermissionRequestResult) => {
        let granted = true;
        for (let index = 0; index < result.permissions.length; index++) {
          console.info(TAG, "request permission [" + result.permissions[index] + "] with result: " + result.authResults[index]);
          if (result.authResults[index] != abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED) {
            granted = false;
          }
        }

        if (!granted) {
          this.setError("Permission Not Granted");
          return;
        }

        console.info(TAG, "Connecting to server");
        this.setStatus("Connecting to server");

        this.number = "1100";
        this.ws.connect(SIGNAL_SERVER+`?locId=${this.locId}&number=${this.number}&displayName=${this.number}&clientType=${this.clientType}`, (err, success) => {
          console.info(TAG, "Connecting to server"+err);
        })
        this.ws_status=true;
      });

    this.ws.on('open', (err, data) => {
      console.info(TAG, 'websocket open');
      this.ws_status=true;
      // this.sendMessage("HELLO " + this.uuid);
      this.setStatus("Registering with server");
      this.createPeerConnection();
    });

    this.ws.on('close', (err, data: webSocket.CloseResult) => {
      console.info(TAG, 'websocket closed');
      this.setStatus('Disconnected from server');

      this.pc?.close();
      this.pc = undefined;
      this.ws_status=false;
      setTimeout(() => {
        this.ws.connect(SIGNAL_SERVER+`?locId=${this.locId}&number=${this.number}&displayName=${this.number}&clientType=${this.clientType}`);
        this.ws_status=true;
        }, 1000);
    });

    this.ws.on('error', (err) => {
      console.info(TAG, 'websocket err: ' + err.code);
      this.setError("Unable to connect to server: " + err.message);
      this.ws_status=false;
      setTimeout(() => {
        this.ws.connect(SIGNAL_SERVER+`?locId=${this.locId}&number=${this.number}&displayName=${this.number}&clientType=${this.clientType}`);
        this.ws_status=true;
      }, 1000);
    });

    this.ws.on('message', (err, data: string | ArrayBuffer) => {
      console.info(TAG, 'websocket receive message: ' + data);
          try {
            let msg = JSON.parse(data.toString()) as MessageData;
            console.warn(TAG, 'receive eventName: ' + msg.eventName?.toString());
            if (msg && msg.eventName?.toString()=== '__invite') {
              if(msg.data){
                let data = JSON.parse(msg.data) as Invite;
                this.uuid = data.uuid
                this.setStatus("Registered with server, waiting for call");
                this.createPeerConnection();
                this.status = "invite"
                this.peerId = data.number
                this.isCallout = data.isCallout
                console.warn(TAG, '__invite from: ' + data.number);

                // if( this.displayMediaStream){
                //   let t: WsTrack =  {
                //     streamId: this.displayMediaStream.id,
                //     videoTrackId: "",
                //     audioTrackId: "",
                //   }
                //   this.displayMediaStream?.getTracks().forEach(track=>{
                //     if(track.kind == "video"){
                //       t.videoTrackId = track.id
                //     }else{
                //       t.audioTrackId = track.id
                //     }
                //   })
                //   this.sendMessage(JSON.stringify(  {
                //     eventName: '__track',
                //     locId: this.locId,
                //     number: this.uuid.toString(),
                //     clientType:this.clientType,
                //     data: JSON.stringify(t)
                //   }));
                // }
              }
            } else if (msg && msg.eventName?.toString()=== '__sdp') {
              if(msg.data) {
                let data = JSON.parse(msg.data) as webrtc.RTCSessionDescriptionInit;
                console.warn(TAG,'parse sdp: '+ data.type,);
                this.receiveSdp(data);
              }
            } else if (msg && msg.eventName?.toString()=== '__candidate') {
              if(msg.data) {
                let data = JSON.parse(msg.data) as webrtc.RTCIceCandidateInit;
                this.receiveIce(data);
              }
            } else if (msg && msg.eventName?.toString()=== '__answer') {
              this.status = "answer"
              // this.handleAnswer(JSON.parse(message.data))
            } else if (msg && msg.eventName?.toString() === '__hangup') {
              this.status = "hangup"

              if(this.pc){
                this.pc.close()
                this.pc = undefined;
              }
              this.uuid=""
              this.buttonTitle = '拨打';
              this.answerBtn =Visibility.Hidden
              this.remoteVideo =Visibility.Hidden
              this.setStatus("Registering with server");
            } else if (msg && msg.eventName?.toString()=== '__register') {
              if(msg.data){
                let data = JSON.parse(msg.data) as ServerInfo;
                this.serverText = "ip:"+ data.serveIp +" 号码："+ data.serveNumber +" 名称："+data.serveDisplayName
              }
              this.ws_status=true;
            }

          } catch (error) {
            console.error(TAG, 'receive invalid message: ' + data);
          }
      //   }
      // }
    });

    this.createAudioDeviceModule();
    this.createPeerConnectionFactory();
  }

  aboutToDisappear(): void {
    this.pcf?.stopAecDump();
    Logging.deleteInjectedLoggable();

    this.ws.close();

    this.ws.off('open');
    this.ws.off('close');
    this.ws.off('error');
    this.ws.off('message');
  }

  onPageShow(): void {
    console.info(TAG, 'onPageShow');
  }

  onPageHide(): void {
    console.info(TAG, 'onPageHide');
  }

  getRandomId() {
    return Math.floor(Math.random() * (9000 - 10) + 10).toString();
  }

  setStatus(text: string) {
    console.info(TAG, text);
    this.statusText = text;
  }

  setError(error: string) {
    console.error(TAG, error);
    this.statusText = error;
  }

  handleIncomingError(error: string) {
    this.setError('ERROR: ' + error);
    this.ws.close();
  }

  async createAudioDeviceModule() {
    this.adm = new webrtc.AudioDeviceModule({
      useStereoInput: true, useStereoOutput: true
    });
    // this.adm.setSpeakerMute(false)
    this.adm.oncapturererror = (event) => {
      console.error(TAG, 'audio oncapturererror: ' + JSON.stringify(event));
    };
    this.adm.oncapturerstatechange = (event) => {
      console.info(TAG, 'audio oncapturerstatechange: ' + JSON.stringify(event));
    };
    this.adm.oncapturersamplesready = (event) => {
      console.debug(TAG, 'audio oncapturersamplesready: ' + JSON.stringify(event));
    };
    this.adm.onrenderererror = (event) => {
      console.error(TAG, 'audio onrenderererror: ' + JSON.stringify(event));
    };
    this.adm.onrendererstatechange = (event) => {
      console.info(TAG, 'audio onrendererstatechange: ' + JSON.stringify(event));
    };
    console.info(TAG, 'audio isBuiltInAcousticEchoCancelerSupported: ' + webrtc.AudioDeviceModule.isBuiltInAcousticEchoCancelerSupported());
    console.info(TAG, 'audio isBuiltInNoiseSuppressorSupported: ' +webrtc.AudioDeviceModule.isBuiltInNoiseSuppressorSupported());
  }

  async createPeerConnectionFactory() {
    this.pcf = new webrtc.PeerConnectionFactory({
      videoEncoderFactory: new webrtc.SoftwareVideoEncoderFactory(),
      videoDecoderFactory: new webrtc.SoftwareVideoDecoderFactory(),
      adm: this.adm
    });
    if (this.hardDecode){
      this.pcf = new webrtc.PeerConnectionFactory({
        videoEncoderFactory: new webrtc.HardwareVideoEncoderFactory(false),
        videoDecoderFactory: new webrtc.HardwareVideoDecoderFactory(),
        adm: this.adm
      });
    }

    Logging.d(TAG, 'this.pcf: ' + JSON.stringify(this.pcf));
    // MediaDevices 依赖默认的 PeerConnnectionFactory，且需要与创建 PeerConnection 的是同一个。
    webrtc.PeerConnectionFactory.setDefault(this.pcf);
    // let filesDir = getContext().filesDir;
    // let file = fs.openSync(filesDir + '/test.pcm', fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE);
    // this.pcf.startAecDump(file.fd, -1);
    // console.info(TAG, 'filesDir: ' + filesDir);
  }

  // async prepareLocalMedia() {
  //   if (!this.localAudioTrack) {
  //     this.localAudioSource = this.pcf?.createAudioSource({ echoCancellation: true, noiseSuppression: true });
  //     console.info(TAG, 'audioSource: ' + JSON.stringify(this.localAudioSource));
  //
  //     this.localAudioTrack = this.pcf?.createAudioTrack("audio", this.localAudioSource);
  //     console.info(TAG, 'audioTrack: ' + JSON.stringify(this.localAudioTrack));
  //   }
  //
  //   if (!this.localVideoTrack) {
  //     this.localVideoSource = this.pcf?.createVideoSource({width: VIDEO_RESOLUTION_WIDTH, height: VIDEO_RESOLUTION_HEIGHT,frameRate:FPS}, false);
  //     console.info(TAG, 'videoSource: ' + JSON.stringify(this.localVideoSource));
  //     this.localVideoSource!.oncapturerstarted = (event: webrtc.VideoCapturerStartedEvent) => {
  //       console.info(TAG, 'oncapturerstarted: event=' + JSON.stringify(event));
  //     };
  //     this.localVideoSource!.oncapturerstopped = (event: webrtc.Event) => {
  //       console.info(TAG, 'oncapturerstopped: event=' + JSON.stringify(event));
  //     };
  //
  //     this.localVideoTrack = this.pcf?.createVideoTrack("video", this.localVideoSource);
  //     console.info(TAG, 'createVideoTrack' + '' + ': ' + JSON.stringify(this.localVideoTrack));
  //
  //     if (this.localVideoTrack) {
  //       this.mXComponentController1.setVideoTrack(this.localVideoTrack);
  //     }
  //
  //     // this.mXComponentController1.setMirror(true);
  //     // this.mXComponentController1.setScalingMode(ScalingMode.AspectFit);
  //   }
  // }

  async prepareDisplayMedia() {
    if (!this.displayMediaStream) {
      this.displayMediaStream = await this.mediaDevices.getUserMedia({
        video:true,
        audio: true,
        // ohosSystemAudio: true,
      });

      this.localVideoTrack = this.displayMediaStream.getVideoTracks()[0];
      console.info(TAG, 'videoTrack: ' + JSON.stringify(this.localVideoTrack));

      if (this.localVideoTrack) {
        let videoSource = this.localVideoTrack.getSource();
        if (videoSource) {
          this.localVideoSource = videoSource as webrtc.VideoSource;
          console.info(TAG, 'videoSource: ' + JSON.stringify(this.localVideoSource));
          this.localVideoSource!.oncapturerstarted = (event: webrtc.VideoCapturerStartedEvent) => {
            console.info(TAG, 'oncapturerstarted: event=' + JSON.stringify(event));
          };
          this.localVideoSource!.oncapturerstopped = (event: webrtc.Event) => {
            console.info(TAG, 'oncapturerstopped: event=' + JSON.stringify(event));
          };
        }

        this.mXComponentController1.setVideoTrack(this.localVideoTrack);
        // this.mXComponentController1.setScalingMode(ScalingMode.AspectFit);
      }

      let audioTracks = this.displayMediaStream.getAudioTracks();
      if (audioTracks.length > 0) {
        this.localAudioTrack = audioTracks[0];
        console.info(TAG, 'audioTrack: ' + JSON.stringify(this.localAudioTrack));
      }
      if (audioTracks.length > 1) {
        this.localAudioTrack2 = audioTracks[1];
        console.info(TAG, 'audioTrack2: ' + JSON.stringify(this.localAudioTrack2));
      }
    }
  }

  async createPeerConnection() {
    if (this.pc){
      return
    }
    // await this.prepareLocalMedia();
    await this.prepareDisplayMedia();

    this.pc = this.pcf?.createPeerConnection({
      iceServers: [
        { urls: 'stun:47.112.22.152:3478' },
        {
          username: 'Chindeo',
          credential: 'P@ssw0rd',
          urls: 'turn:47.112.22.152:3478?transport=udp'
        },
        {
          username: 'Chindeo',
          credential: 'P@ssw0rd',
          urls: 'turn:47.112.22.152:3478?transport=tcp'
        },
      ]
    });

    if (!this.pc) {
      console.error(TAG, 'failed to create RTCPeerConnection');
      return;
    }
    this.pc.ontrack = (event: webrtc.RTCTrackEvent) => {
      console.info(TAG, 'ontrack: ' + JSON.stringify(event));
      if (event.track.kind == 'video') {
        this.remoteVideoTrack = event.track;
        this.remoteVideo = Visibility.Visible
        this.answerBtn =Visibility.Hidden
        this.mXComponentController2.setMirror(true);
        this.mXComponentController2.setVideoTrack(event.track);
      } else if (event.track.kind == 'audio' ) {
        // this.remoteAudioTrack = event.track;

      }
    };
    this.pc.onsignalingstatechange = (event) => {
      console.info(TAG, 'onsignalingstatechange: ' + this.pc?.signalingState);
      if(this.pc?.signalingState=="stable"){
        let ices = this.ices;
        this.ices =[];
        ices.forEach(ice =>{
          this.pc?.addIceCandidate(ice).catch((e: Error) => {
            console.error(TAG, "add ice candidate error: " + e);
            this.setError(e.message);
          });
        })
      }
    }
    this.pc.onconnectionstatechange = (event) => {
      console.info(TAG, 'onconnectionstatechange: ' + this.pc?.connectionState);
      if (this.pc?.connectionState == 'failed') {
        this.setError("Peer connection failed");
        // this.ws.close();
      } else if(this.pc?.connectionState=='connected' && this.status=="invite" && !this.isCallout){
        this.answerBtn = Visibility.Visible
        this.buttonTitle='挂断'
      }
    }
    this.pc.onnegotiationneeded = (event) => {
      // this is Index
      console.info(TAG, 'onnegotiationneeded: this=' + JSON.stringify(this.pc));
      console.info(TAG, 'onnegotiationneeded: event=' + JSON.stringify(event));
    }

    this.pc.onicecandidate = (event) => {
      // console.info(TAG, 'onicecandidate: this=' + JSON.stringify(this));
      console.info(TAG, 'onicecandidate: event=' + JSON.stringify(event));
      if (event.candidate) {
        this.sendMessage(JSON.stringify(
          // { ice: event.candidate }
          {
            eventName: '__candidate',
            locId: this.locId,
            number: this.number.toString(),
            clientType:this.clientType,
            data: JSON.stringify({
              uuid: this.uuid,
              candidate: event.candidate.candidate,
              sdpMLineIndex: event.candidate.sdpMLineIndex,
              sdpMid: event.candidate.sdpMid
            })
          }
        ));
      }
    };
    this.pc.onicecandidateerror = (event) => {
      console.error(TAG, 'onicecandidateerror: event=' + JSON.stringify(event));
    };
    this.pc.oniceconnectionstatechange = (event) => {
      console.info(TAG, 'oniceconnectionstatechange: ' + this.pc?.iceConnectionState);
      if (this.pc?.iceConnectionState === 'failed') {
        this.setError("Ice connection failed");
        this.ws.close();
      }
    };
    this.pc.onicegatheringstatechange = (event) => {
      console.info(TAG, 'onicegatheringstatechange: ' + this.pc?.iceGatheringState);
      switch (this.pc?.iceGatheringState) {
        case "new":
          console.info(TAG, 'onicegatheringstatechange: gathering is either just starting or has been reset');
          break;
        case "gathering":
          console.info(TAG, 'onicegatheringstatechange: gathering has begun or is ongoing');
          break;
        case "complete":
          console.info(TAG, 'onicegatheringstatechange: gathering has ended');
          break;
      }
    };
    // this.pc.ondatachannel = (event) => {
    //   console.info(TAG, 'ondatachannel: this=' + JSON.stringify(this));
    //   console.info(TAG, 'ondatachannel: event=' + JSON.stringify(event));
    //   this.setStatus("Data channel created");
    //   this.recvChannel = event.channel;
    //   this.recvChannel.onopen = this.onDataChannelOpen;
    //   this.recvChannel.onmessage = this.onDataChannelMessageReceived;
    //   this.recvChannel.onerror = this.onDataChannelError;
    //   this.recvChannel.onclose = this.onDataChannelClose;
    // }

    // if (this.localAudioTrack) {
    //   let rtpSender1 = this.pc.addTrack(this.localAudioTrack);
    //   console.info(TAG, 'rtpSender1: ' + JSON.stringify(rtpSender1));
    // }
    // if (this.localAudioTrack2) {
    //   let rtpSender2 = this.pc.addTrack(this.localAudioTrack2);
    //   console.info(TAG, 'rtpSender2: ' + JSON.stringify(rtpSender2));
    // }
    // if (this.localVideoTrack) {
    //   let rtpSender3 = this.pc.addTrack(this.localVideoTrack);
    //   console.info(TAG, 'rtpSender3: ' + JSON.stringify(rtpSender3));
    // }

    console.info(TAG, 'transceivers: ' + JSON.stringify(this.pc.getTransceivers()));

  //   this.sendChannel = this.pc.createDataChannel("send");
  //   console.info(TAG, 'sendChannel: ' + JSON.stringify(this.sendChannel));
  //   this.sendChannel.onopen = (event) => {
  //     // this is Index
  //     console.info(TAG, '=dataChannel.OnOpen: this=', JSON.stringify(this));
  //     console.info(TAG, "=dataChannel.OnOpen: ", event);
  //
  //     this.sendChannel?.send("Hi! (from ArkTS)");
  //     let data1 = buffer.alloc(20, 0);
  //     this.sendChannel?.send(data1.buffer);
  //
  //     let data2 = new Int32Array(new ArrayBuffer(20));
  //     data2.fill(1);
  //     this.sendChannel?.send(data2.buffer);
  //
  //     let data3 = new Uint8Array(20);
  //     data3.fill(2);
  //     this.sendChannel?.send(data3.buffer);
  //   };
  //   this.sendChannel.onmessage = this.onDataChannelMessageReceived;
  //   this.sendChannel.onerror = this.onDataChannelError;
  //   this.sendChannel.onclose = this.onDataChannelClose;
  }

  // onDataChannelOpen(event: webrtc.Event) {
  //   // this is webrtc.RTCDataChannel
  //   console.info(TAG, "dataChannel.OnOpen: ", event);
  //   if (this instanceof webrtc.RTCDataChannel) {
  //     this.send("Hi! (from ArkTS)");
  //   }
  // }
  //
  // async onDataChannelError(event: webrtc.Event) {
  //   // this is webrtc.RTCDataChannel
  //   console.info(TAG, "dataChannel.OnError: ", event);
  // }
  //
  // async onDataChannelClose(event: webrtc.Event) {
  //   // this is webrtc.RTCDataChannel
  //   console.info(TAG, 'dataChannel.OnClose: this=', JSON.stringify(this));
  //   console.info(TAG, "dataChannel.OnClose", event);
  // }

  // async sendOffer() {
  //   try {
  //     let offer = await this.pc?.createOffer();
  //     console.info(TAG, 'createOffer: ' + JSON.stringify(offer));
  //
  //     this.pc?.setLocalDescription(offer);
  //
  //     this.sendMessage(JSON.stringify({ sdp: offer }));
  //   } catch (e) {
  //     console.error(TAG, 'sendOffer: ' + JSON.stringify(e));
  //   }
  // }

  async sendAnswer() {
    this.setStatus("Got SDP offer, creating answer");
    if(!this.pc){
        return
    }
    this.pc.createAnswer().then((desc) => {
      // console.info(TAG, 'Success to create answer: ' + JSON.stringify(desc));
      if(!this.pc){
        return
      }

      if(desc.sdp){
        // desc.sdp = this.preferCodec(desc.sdp, H264)
        // console.info(TAG, 'preferCodec: ' + desc.sdp);
        // // desc.sdp = this.setBitrate(
        // //   H264,
        // //   true,
        // //   desc.sdp,
        // //   videoStartBitrateKbps,
        // //   videoMinBitrateKbps,
        // //   videoMaxBitrateKbps
        // // )
        // console.info(TAG, 'setBitrate H264: ' + desc.sdp);
        // desc.sdp = this.setBitrate(
        //   OPUS,
        //   false,
        //   desc.sdp,
        //   audioStartBitrate,
        //   0,
        //   0,
        //   audioStereo
        // )
        // // a=rtpmap:111 opus/48000/2
        // // a=rtpmap:9 G722/8000
        // // a=rtpmap:0 PCMU/8000
        // // a=rtpmap:8 PCMA/8000
        // console.info(TAG, 'setBitrate OPUS: ' + desc.sdp);
        // desc.sdp= this.addNack(desc.sdp)
        // console.info(TAG, 'addNack: ' + desc.sdp);
      }
      this.pc.setLocalDescription(desc).then(() => {
        this.setStatus("Sending SDP answer");
        if(!this.pc){
          return
        }
        if(this.pc.localDescription){
          this.sendMessage(JSON.stringify({
            eventName: '__sdp',
            locId: this.locId,
            number: this.number.toString(),
            clientType:this.clientType,
            data: JSON.stringify({
              sdp: this.pc.localDescription.sdp,
              sdpType:this.pc?.localDescription?.type,
              uuid: this.uuid,
            })
          }));
        }
      });
    }).catch((e: Error) => {
      console.error(TAG, 'Failed to create answer: ' + e.message);
      this.setError(e.message);
    });
  }

  async sendMessage(message: string) {
    // console.info(TAG, 'websocket send: ' + message);
    this.ws.send(message, (error, success) => {
      if (success) console.info(TAG, 'websocket send success: ' + message);
      else console.error(TAG, 'websocket send error: ' + error.code + error.message);
    });
  }

  async receiveSdp(sdp: webrtc.RTCSessionDescriptionInit) {
    if(this.pc && this.isCallout && this.status==="invite"){
      console.info(TAG, 'display media Stream track status: ' + this.status )
      if (this.localAudioTrack) {
        console.info(TAG, 'display media Stream track localAudioTrack')
        let rtpSender1 = this.pc.addTrack(this.localAudioTrack);
        console.info(TAG, 'rtpSender1: ' + JSON.stringify(rtpSender1));
      }
      if (this.localAudioTrack2) {
        console.info(TAG, 'display media Stream track localAudioTrack2')
        let rtpSender2 = this.pc.addTrack(this.localAudioTrack2);
        console.info(TAG, 'rtpSender2: ' + JSON.stringify(rtpSender2));
      }
      if (this.localVideoTrack) {
        console.info(TAG, 'display media Stream track localVideoTrack')
        let rtpSender3 = this.pc.addTrack(this.localVideoTrack);
        console.info(TAG, 'rtpSender3: ' + JSON.stringify(rtpSender3));
      }
    }

    if (sdp.type === 'offer') {
      console.info(TAG, 'receive offer');
      this.pc?.setRemoteDescription(sdp).then(() => {
        this.setStatus("Remote SDP set");
        this.sendAnswer();
      }).catch((e: Error) => {
        console.error(TAG, "Set remote description error: " + e);
        this.setError(e.message);
      });
    } else if (sdp.type === 'answer') {
      console.info(TAG, 'receive answer');
      this.pc?.setRemoteDescription(sdp).then(() => {
        this.setStatus("Remote SDP set");
      }).catch((e: Error) => {
        console.error(TAG, "Set remote description error: " + e);
        this.setError(e.message);
      });
    }else{
      console.error(TAG,"sdp type get unkown :"+ sdp.type)
    }
  }

  async receiveIce(ice: webrtc.RTCIceCandidateInit) {
    console.info(TAG, 'receive icecandidate signalingState：'+this.pc?.signalingState);
    if(this.pc?.signalingState==="stable"){
      this.pc?.addIceCandidate(ice).catch((e: Error) => {
        console.error(TAG, "add ice candidate error: " + e);
        this.setError(e.message);
      });
    }else{
      this.ices.push(ice);
    }
  }

  // 增加音频nack重传
   addNack(sdp:string) :string {
     let sdpLines:string[]  = sdp.split('\r\n');
    for (let i = 0; i < sdpLines.length; ++i) {
      if (sdpLines[i].indexOf('opus') != -1) {
        if (sdpLines[i+1].indexOf('rtcp-fb') != -1) {
          let str = sdpLines[i+1];
          let arr = str.split(' ');
          if (arr.length == 2) {
            if (sdpLines[i+2].indexOf('rtcp-fb') != -1) {
              sdpLines.splice(i+3, 0, arr[0] + " nack")
            } else {
             sdpLines.splice(i+2, 0, arr[0] + " nack")
            }
          }
        }
      }
    }
     return sdpLines.join('\r\n')
  }

  // 缓解丢包花屏问题
  addSps(sdp:string) {
    let sdpLines:string[]  = sdp.split('\r\n');
      sdpLines = sdpLines.map(line => {
        if (line.includes('a=fmtp:') && line.includes('level-asymmetry-allowed')) {
          line += ';sps-pps-idr-in-keyframe=1';
          return line
        }
        return line;
      })
      return sdpLines.join('\r\n')
  }

  splitLines(sdp:string) :Info {
    let info:Info = {
      videoCodecNumbers:[],
      vp8LineNumber:"",
      vp9LineNumber:"",
      h264LineNumber:"",
      videoCodecNumbersOriginal:"",
    }
    sdp?.split('\n').forEach((line:string) => {
      if (line.indexOf('m=video') === 0) {
        info.videoCodecNumbers = [];
        line.split('SAVPF')[1].split(' ').forEach((codecNumber: string) => {
          codecNumber = codecNumber.trim();
          if (!codecNumber || !codecNumber.length) {
            return;
          }
          info.videoCodecNumbers.push(codecNumber);
          info.videoCodecNumbersOriginal = line;
        });
      }
      if (line.indexOf('VP8/90000') !== -1 && !info.vp8LineNumber) {
        info.vp8LineNumber = line.replace('a=rtpmap:', '').split(' ')[0];
      }
      if (line.indexOf('VP9/90000') !== -1 && !info.vp9LineNumber) {
        info.vp9LineNumber = line.replace('a=rtpmap:', '').split(' ')[0];
      }
      if (line.indexOf('H264/90000') !== -1 && !info.h264LineNumber) {
        info.h264LineNumber = line.replace('a=rtpmap:', '').split(' ')[0];
      }
    })
    return info
  }

  preferCodecHelper(info:Info,sdp:string, codec:string, ignore:string) {
    let preferCodecNumber = ''

    if (codec === VP8) {
      if (!info.vp8LineNumber) {
        return sdp
      }
      preferCodecNumber = info.vp8LineNumber
    }

    if (codec === VP9) {
      if (!info.vp9LineNumber) {
        return sdp
      }
      preferCodecNumber = info.vp9LineNumber
    }

    if (codec === H264) {
      if (!info.h264LineNumber) {
        return sdp
      }

      preferCodecNumber = info.h264LineNumber
    }

    let newLine = info.videoCodecNumbersOriginal.split('SAVPF')[0] + 'SAVPF '

    let newOrder = [preferCodecNumber]

    if (ignore) {
      newOrder = []
    }

    info.videoCodecNumbers.forEach((codecNumber) => {
      if (codecNumber === preferCodecNumber) {
        return;
      }
      newOrder.push(codecNumber);
    })

    newLine += newOrder.join(' ')

    sdp = sdp?.replace(info.videoCodecNumbersOriginal, newLine)
    return sdp
  }

  preferCodec(sdp:string, codecName:string) {
    let info = this.splitLines(sdp)
    console.warn(TAG, 'preferCodec: splitLines=' + JSON.stringify(info));
    if (!info.videoCodecNumbers) {
      return sdp
    }
    if (codecName === VP8 && info.vp8LineNumber === info.videoCodecNumbers[0]) {
      return sdp
    }
    if (codecName === VP9 && info.vp9LineNumber === info.videoCodecNumbers[0]) {
      return sdp
    }
    if (codecName === H264 && info.h264LineNumber === info.videoCodecNumbers[0]) {
      return sdp
    }
    // sdp = this.preferCodecHelper(info,sdp, codecName, "")
    return sdp
  }

  setBitrate (codec:string, isVideoCodec:boolean, sdpDescription:string, bitrateKbps:number, videoMinBitrateKbps:number, videoMaxBitrateKbps:number, audioStereo?:number) {
    let lines = sdpDescription.split('\r\n')
    let rtpmapLineIndex = -1
    let sdpFormatUpdated = false
    let codecRtpMap = ""
    // Search for codec rtpmap in format
    // a=rtpmap:<payload type> <encoding name>/<clock rate> [/<encoding parameters>]
    let regex = '^a=rtpmap:(\\d+) ' + codec + '(/\\d+)+[\r]?$'
    let codecPattern = new RegExp(regex)
    for (let i = 0; i < lines.length - 1; i++) {
      let codecMatcher = lines[i].match(codecPattern)
      if (codecMatcher != null) {
        codecRtpMap = codecMatcher[1]
        rtpmapLineIndex = i
        break
      }
    }
    if (codecRtpMap == null) {
      //console.log("No rtpmap for " + codec + " codec");
      return sdpDescription
    }
    /*console.log("Found " + codec + " rtpmap " + codecRtpMap
      + " at " + lines[rtpmapLineIndex]);*/
    // Check if a=fmtp string already exist in remote SDP for this codec and
    // update it with new bitrate parameter.
    regex = '^a=fmtp:' + codecRtpMap + ' .*?\=.*?$'
    codecPattern = new RegExp(regex)
    for (let i = 0; i < lines.length - 1; i++) {
      let codecMatcher = lines[i].match(codecPattern)
      if (codecMatcher != null) {
        //console.log("Found " + codec + " " + lines[i]);
        if (isVideoCodec) {
          const sdpMatch = lines[i].match(/profile-level-id=[^;\n]*/)
          if (sdpMatch) {
            lines[i] = lines[i].replace(sdpMatch[0].split('=')[1], '42e01f')
          }
          lines[i] += ';x-google-start-bitrate=' + bitrateKbps
          lines[i] += ';x-google-min-bitrate=' + videoMinBitrateKbps
          lines[i] += ';x-google-max-bitrate=' + videoMaxBitrateKbps
        } else {
          lines[i] += ';maxaveragebitrate=' + (bitrateKbps * 1000)
          lines[i] += ';stereo=' + audioStereo
          lines[i] += ';sprop-stereo=' + audioStereo
          lines[i] += ';maxplaybackrate=' + 48000
          lines[i] += ';sprop-maxcapturerate=' + 48000
          lines[i] += ';maxptime=' + 30
        }
        //console.log("Update remote SDP line: " + lines[i]);
        sdpFormatUpdated = true
        break
      }
    }

    let newSdpDescription = ''
    for (let i = 0; i < lines.length - 1; i++) {
      newSdpDescription += lines[i] + '\r\n'
      // Append new a=fmtp line if no such line exist for a codec.
      if (!sdpFormatUpdated && i == rtpmapLineIndex) {
        let bitrateSet:string
        if (isVideoCodec) {
          bitrateSet = 'a=fmtp:' + codecRtpMap + ' ' + 'x-google-start-bitrate=' + bitrateKbps
          bitrateSet += ';x-google-min-bitrate=' + videoMinBitrateKbps
          bitrateSet += ';x-google-max-bitrate=' + videoMaxBitrateKbps
        } else {
          bitrateSet = 'a=fmtp:' + codecRtpMap + ' ' + 'maxaveragebitrate=' + (bitrateKbps * 1000)
          bitrateSet += ';stereo=' + audioStereo
          bitrateSet += ';sprop-stereo=' + audioStereo
          bitrateSet += ';maxplaybackrate=' + 48000
          bitrateSet += ';sprop-maxcapturerate=' + 48000
          bitrateSet += ';maxptime=' + 30
        }
        //console.log("Add remote SDP line: " + bitrateSet);
        newSdpDescription += bitrateSet + '\r\n'
      }
    }

    return newSdpDescription
  }
}
