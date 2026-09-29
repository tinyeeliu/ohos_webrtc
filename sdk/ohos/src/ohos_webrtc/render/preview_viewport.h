#ifndef WEBRTC_RENDER_PREVIEW_VIEWPORT_H
#define WEBRTC_RENDER_PREVIEW_VIEWPORT_H

#include <algorithm>
#include <cmath>

namespace webrtc {
namespace adapter {

struct PreviewViewport {
    int x;
    int y;
    int width;
    int height;
};

// Compute the crop in display pixels, then map it into the current EGL buffer.
// The compositor can resize an older buffer while EGL catches up with a resize.
inline PreviewViewport ComputePreviewViewport(int bufferWidth, int bufferHeight,
    int displayWidth, int displayHeight, int frameWidth, int frameHeight, bool aspectFill)
{
    const double scaleX = static_cast<double>(displayWidth) / frameWidth;
    const double scaleY = static_cast<double>(displayHeight) / frameHeight;
    const double scale = aspectFill ? std::max(scaleX, scaleY) : std::min(scaleX, scaleY);
    const int width = static_cast<int>(std::lround(frameWidth * scale * bufferWidth / displayWidth));
    const int height = static_cast<int>(std::lround(frameHeight * scale * bufferHeight / displayHeight));
    return {(bufferWidth - width) / 2, (bufferHeight - height) / 2, width, height};
}

} // namespace adapter
} // namespace webrtc
#endif
