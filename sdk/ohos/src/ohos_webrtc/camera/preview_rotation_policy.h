#ifndef WEBRTC_CAMERA_PREVIEW_ROTATION_POLICY_H
#define WEBRTC_CAMERA_PREVIEW_ROTATION_POLICY_H

namespace webrtc {
struct HalPreviewRotation {
    bool applied;
    bool recoveryRequired;
    int degrees;
};

template <typename GetRotation, typename SetRotation>
HalPreviewRotation TryApplyPreviewRotation(bool enabled, bool previouslyApplied,
    int displayDegrees, GetRotation getRotation, SetRotation setRotation)
{
    int degrees = 0;
    // The camera producer targets NativeImage, not the renderer's XComponent.
    // A display-locked preview applies sensor rotation only on some devices.
    // Keep the requested display compensation in the captured texture itself.
    const bool applied = enabled && getRotation(displayDegrees, &degrees) && setRotation(degrees, false);
    // Failure does not prove the previous transform disappeared. Reset before
    // software fallback; if resetting fails the owner must recover the session.
    const bool recovery = !applied && previouslyApplied && !setRotation(0, false);
    return {applied, recovery, degrees};
}

struct PreviewRotationPolicy {
    int textureRotation;
    int frameRotation;
    bool swapTextureDimensions;
};

// The software path deliberately matches the pre-adaptation camera contract.
constexpr PreviewRotationPolicy ResolvePreviewRotation(
    int cameraDegrees, int displayDegrees, bool front, bool halApplied, int halDegrees)
{
    return halApplied
        ? PreviewRotationPolicy{halDegrees, 0, halDegrees == 90 || halDegrees == 270}
        : PreviewRotationPolicy{front ? (360 - displayDegrees) % 360 : displayDegrees,
              front ? (cameraDegrees + displayDegrees) % 360 : (cameraDegrees - displayDegrees + 360) % 360,
              false};
}
} // namespace webrtc
#endif
