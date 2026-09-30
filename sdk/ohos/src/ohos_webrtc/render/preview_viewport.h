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

struct PreviewSize {
    int width;
    int height;
};

// Compute the crop in display pixels, then map it into the current EGL buffer.
// The compositor can resize an older buffer while EGL catches up with a resize.
inline PreviewViewport ComputePreviewViewport(PreviewSize buffer, PreviewSize display, PreviewSize frame,
    bool aspectFill)
{
    if (buffer.width <= 0 || buffer.height <= 0 || display.width <= 0 || display.height <= 0 ||
        frame.width <= 0 || frame.height <= 0) {
        return {0, 0, 0, 0};
    }
    const double scaleX = static_cast<double>(display.width) / frame.width;
    const double scaleY = static_cast<double>(display.height) / frame.height;
    const double scale = aspectFill ? std::max(scaleX, scaleY) : std::min(scaleX, scaleY);
    const int width = static_cast<int>(std::lround(frame.width * scale * buffer.width / display.width));
    const int height = static_cast<int>(std::lround(frame.height * scale * buffer.height / display.height));
    return {(buffer.width - width) / 2, (buffer.height - height) / 2, width, height};
}

} // namespace adapter
} // namespace webrtc
#endif
