#include "video_frame_rate_gate.h"
#include <cassert>
#include <thread>

int main() {
  auto control = std::make_shared<webrtc::VideoFrameRateLimit>();
  webrtc::VideoFrameRateGate gate(control);
  assert(gate.Accept(0)); assert(gate.Accept(1));  // Default SDK behavior.
  control->fps = 0; assert(!gate.Accept(2));
  control->fps = 0.5; int accepted = 0;
  for (int64_t time = 0; time < 10000000; time += 33333) accepted += gate.Accept(time);
  assert(accepted == 5);  // 30 FPS input, bounded fractional output.
  control->fps = 0.25; assert(gate.Accept(10000000));
  assert(!gate.Accept(13999999)); assert(gate.Accept(14000000));
  control->fps = 0; assert(!gate.Accept(15000000));
  control->fps = 1; assert(gate.Accept(15000001)); assert(!gate.Accept(15000002));
  gate.Reset(); assert(gate.Accept(15000002));  // Reinitialized encoder.
  webrtc::VideoFrameRateGate another(control); assert(another.Accept(15000002));
  auto independent = std::make_shared<webrtc::VideoFrameRateLimit>();
  webrtc::VideoFrameRateGate unrestricted(independent); assert(unrestricted.Accept(0));
  assert(unrestricted.Accept(1));  // A separate factory is unaffected.
  std::thread writer([&] { for (int i = 0; i < 10000; i++) control->fps = i % 2; });
  for (int i = 0; i < 10000; i++) gate.Accept(i);
  writer.join(); control->fps = 0; assert(!gate.Accept(10000));
}
