#ifdef PROBE_FEATURE_ENABLED

#include <cstdint>
#include <string>

namespace webrtc {
namespace ohos {

class GateRuleProbeHelper {
public:
    GateRuleProbeHelper(int32_t seed);
    ~GateRuleProbeHelper();
    int32_t Compute(int32_t input) const;
    std::string Describe() const;
    int32_t seed_;
};

}  // namespace ohos
}  // namespace webrtc
