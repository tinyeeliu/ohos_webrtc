//
// Created on 2024/3/6.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".


#ifndef OH_WEB_RTC_OHOS_CAMERA_H
#define OH_WEB_RTC_OHOS_CAMERA_H

#include "hilog/log.h"
#include <arm-linux-ohos/bits/alltypes.h>
#include <multimedia/image_framework/image_receiver_mdk.h>
#include <ohcamera/camera.h>
#include <ohcamera/camera_manager.h>
#include <multimedia/image_framework/image/image_receiver_native.h>
#include <multimedia/image_framework/image/image_common.h>
#include <multimedia/image_framework/image/image_native.h>
#include <native_buffer/native_buffer.h>
#include <native_image/native_image.h>
#include <map>

#include "rtc_base/ref_counter.h"
#include "api/video/video_source_interface.h"
#include "api/video/video_frame.h"
#include "rtc_base/synchronization/mutex.h"
#include "modules/video_capture/video_capture.h"


namespace webrtc{
namespace ohos{


class OhosImageReveiveOn {
public:
    virtual int32_t ImageReceiverCallback(OH_ImageReceiverNative *receiver) = 0;
    int32_t InitImageReceiver();
    int32_t ImageReceiverRelease();
    static uint64_t GetImageReceiverSurfaceID(OH_ImageReceiverNative *receiver);
    uint64_t GetImageReceiverID();
    OH_ImageReceiverNative* GetImageReceiver();

private:
    uint64_t imageReceiverID_ {0};
    OH_ImageReceiverNative *imageReceiverNative_ {nullptr};
};


class ImageReceiverOnManager {
public:
    static ImageReceiverOnManager& GetInstance();
    int32_t AddImageReceiverOn(uint64_t id, std::shared_ptr<OhosImageReveiveOn> imgRecOn);
    int32_t DelImageReceiverOn(uint64_t id);
    OhosImageReveiveOn* GetReceiverOnCb(const uint64_t imageReceiverID);
    static void ImageReceiverCallback(OH_ImageReceiverNative* receiver);

private:
    ImageReceiverOnManager() {};
    ~ImageReceiverOnManager() {};
    ImageReceiverOnManager(const ImageReceiverOnManager&) {};
    ImageReceiverOnManager& operator=(const ImageReceiverOnManager&) {};
    Mutex mapMutex_;
    std::map<uint64_t, std::weak_ptr<OhosImageReveiveOn> > receiverOnMap_ RTC_GUARDED_BY(mapMutex_);
};


class OhosCamera: public OhosImageReveiveOn {
public:
  OhosCamera();
  int32_t InitCamera();
  int32_t StartCamera();
  int32_t StopCamera();
  int32_t CameraRelease();
  
  uint32_t GetCameraIndex();
  int32_t SetCameraIndex(uint32_t camera_index);
  bool ImageReceiverOn(uint8_t *buffer, int32_t width, int32_t height, int32_t stride, size_t bufferSize);
  int32_t ImageReceiverCallback(OH_ImageReceiverNative *receiver) override;
  void RegisterCaptureDataCallback(rtc::VideoSinkInterface<webrtc::VideoFrame> *dataCallback);
  void UnregisterCaptureDataCallback();
  ~OhosCamera();

private:
  int32_t CameraInputCreateAndOpen();
  int32_t CameraInputRelease();
  
  int32_t PreviewOutputCreate();
  int32_t PreviewOutputRelease();
  
  int32_t CaptureSessionSetting();
  int32_t CaptureSessionUnsetting();

  int32_t DeleteCameraOutputCapability();
  int32_t DeleteCameras();
  int32_t DeleteCameraManage();
  
  rtc::VideoSinkInterface<webrtc::VideoFrame> *data_callback_ {nullptr};
  webrtc::VideoCaptureCapability configured_capability_;
  bool is_camera_started_ {false};
  Camera_Manager* camera_manager_ {nullptr};
  Camera_Device* cameras_ {nullptr};
  uint32_t cameras_size_ {0};
  Camera_CaptureSession* capture_session_ {nullptr};
  Camera_OutputCapability* camera_output_capability_ {nullptr};
  const Camera_Profile* preview_profile_ {nullptr};
  Camera_PreviewOutput* preview_output_ {nullptr};
  Camera_Input* camera_input_ {nullptr};
  uint32_t camera_dev_index_ {0};
  uint32_t profile_index_ {0};
};
} // namespace ohos
} // namespace webrtc
#endif //OH_WEB_RTC_OHOS_CAMERA_H
