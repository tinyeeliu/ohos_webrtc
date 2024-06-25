#ifndef OH_WEB_RTC_OHOS_DESKTOP_H
#define OH_WEB_RTC_OHOS_DESKTOP_H
#include <queue>

#include "ohos_camera_capture.h"
#include <multimedia/player_framework/native_avscreen_capture.h>
#include <multimedia/player_framework/native_avscreen_capture_base.h>


struct BufferData {
    uint8_t *buffer;
    int32_t length;
    int32_t stride;
    int32_t width;
    int32_t height;
    int64_t timestamp;
};

namespace webrtc {
namespace ohos {

class OhosDesktop {
public:
    int32_t Create(int32_t queueSize = 1);
    int32_t Config(int32_t width = 2160, int32_t height = 1440); // 分辨率太高处理较慢
    int32_t Start();
    int32_t Stop();
    
    void RegisterCaptureDataCallback(rtc::VideoSinkInterface<webrtc::VideoFrame> *dataCallback);
    void UnregisterCaptureDataCallback();
    void DesktopBufferAvailable();
    void DesktopBufferOn(uint8_t *buffer,int32_t bufferLength, int32_t stride, int32_t width, int32_t height, int64_t timestamp);
    OhosDesktop() = default;
    ~OhosDesktop();
    
private:
    int32_t Release();
    static void OnError(OH_AVScreenCapture *capture, int32_t errorCode, void *userData);
    static void OnStateChange(struct OH_AVScreenCapture *capture, OH_AVScreenCaptureStateCode stateCode, void *userData);
    static void OnBufferAvailable(OH_AVScreenCapture *capture, OH_AVBuffer *buffer, OH_AVScreenCaptureBufferType bufferType,
                           int64_t timestamp, void *userData);
    void SetStateCode(OH_AVScreenCaptureStateCode stateCode);
    rtc::VideoSinkInterface<webrtc::VideoFrame> *dataCallback_ {nullptr};
    OH_AVScreenCapture *capture_ {nullptr};
    OH_AVScreenCaptureStateCode stateCode_;
    bool isStarted_ {false};
    bool isConfig_ {false};
    std::atomic_bool isRunning_ {false};
    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<BufferData> desktopAVBufferQueue_;
    size_t maxQueueSize_ {1};
    int64_t timestampStarts_ {0x7FFFFFFFFFFFFFFF};
};

} // namespace ohos
} // namespace webrtc
#endif //OH_WEB_RTC_OHOS_DESKTOP_H
