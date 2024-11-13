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

#include "media_constraints_util.h"

#include <vector>
#include <cmath>
#include <optional>
#include <algorithm>

#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {

static constexpr int kMaxDimension = std::numeric_limits<int>::max();
static constexpr int kMaxFrameRate = 1000;

class CandidateSettings;

template <typename ConstraintType>
bool ConstraintHasMax(const ConstraintType& constraint)
{
    return constraint.HasMax() || constraint.HasExact();
}

template <typename ConstraintType>
bool ConstraintHasMin(const ConstraintType& constraint)
{
    return constraint.HasMin() || constraint.HasExact();
}

template <typename ConstraintType>
auto ConstraintMax(const ConstraintType& constraint) -> decltype(constraint.Max())
{
    RTC_DCHECK(ConstraintHasMax(constraint));
    return constraint.HasExact() ? constraint.Exact() : constraint.Max();
}

template <typename ConstraintType>
auto ConstraintMin(const ConstraintType& constraint) -> decltype(constraint.Min())
{
    RTC_DCHECK(ConstraintHasMin(constraint));
    return constraint.HasExact() ? constraint.Exact() : constraint.Min();
}

template <typename T>
class NumericRangeSet {
public:
    NumericRangeSet() = default;
    NumericRangeSet(std::optional<T> min, std::optional<T> max) : min_(std::move(min)), max_(std::move(max)) {}
    NumericRangeSet(const NumericRangeSet& other) = default;
    NumericRangeSet& operator=(const NumericRangeSet& other) = default;
    ~NumericRangeSet() = default;

    const std::optional<T>& Min() const
    {
        return min_;
    }
    const std::optional<T>& Max() const
    {
        return max_;
    }
    bool IsEmpty() const
    {
        return max_ && min_ && *max_ < *min_;
    }

    NumericRangeSet Intersection(const NumericRangeSet& other) const
    {
        std::optional<T> min = min_;
        if (other.Min())
            min = min ? std::max(*min, *other.Min()) : other.Min();

        std::optional<T> max = max_;
        if (other.Max())
            max = max ? std::min(*max, *other.Max()) : other.Max();

        return NumericRangeSet(min, max);
    }

    bool Contains(T value) const
    {
        return (!Min() || value >= *Min()) && (!Max() || value <= *Max());
    }

    template <typename ConstraintType>
    static NumericRangeSet<T> FromConstraint(ConstraintType constraint, T lower_bound, T upper_bound)
    {
        RTC_DCHECK_LE(lower_bound, upper_bound);
        if ((ConstraintHasMax(constraint) && ConstraintMax(constraint) < lower_bound) ||
            (ConstraintHasMin(constraint) && ConstraintMin(constraint) > upper_bound))
        {
            return NumericRangeSet<decltype(constraint.Min())>(1, 0);
        }

        return NumericRangeSet<T>(
            ConstraintHasMin(constraint) && ConstraintMin(constraint) >= lower_bound ? ConstraintMin(constraint)
                                                                                     : std::optional<T>(),
            ConstraintHasMax(constraint) && ConstraintMax(constraint) <= upper_bound ? ConstraintMax(constraint)
                                                                                     : std::optional<T>());
    }

    template <typename ConstraintType>
    static NumericRangeSet<T> FromConstraint(ConstraintType constraint)
    {
        return NumericRangeSet<T>(
            ConstraintHasMin(constraint) ? ConstraintMin(constraint) : std::optional<T>(),
            ConstraintHasMax(constraint) ? ConstraintMax(constraint) : std::optional<T>());
    }

    static NumericRangeSet<T> FromValue(T value)
    {
        return NumericRangeSet<T>(value, value);
    }

    static NumericRangeSet<T> EmptySet()
    {
        return NumericRangeSet(1, 0);
    }

private:
    std::optional<T> min_;
    std::optional<T> max_;
};

// class BoolSet {
// public:
//     // Creates a universal set.
//     BoolSet() : is_universal_(true) {}
//     // Creates a set containing the elements in |elements|.
//     // It is the responsibility of the caller to ensure that |elements| is not
//     // equivalent to the universal set and that |elements| has no repeated
//     // values. Takes ownership of |elements|.
//     explicit BoolSet(std::vector<bool> elements) : is_universal_(false), elements_(std::move(elements)) {}
//     static BoolSet EmptySet()
//     {
//         return BoolSet(std::vector<bool>());
//     }
//     static BoolSet UniversalSet()
//     {
//         return BoolSet();
//     }
//
//     BoolSet(const BoolSet& other) = default;
//     BoolSet& operator=(const BoolSet& other) = default;
//     BoolSet(BoolSet&& other) = default;
//     BoolSet& operator=(BoolSet&& other) = default;
//     ~BoolSet() = default;
//
//     bool Contains(bool value) const
//     {
//         return is_universal_ || std::find(elements_.begin(), elements_.end(), value) != elements_.end();
//     }
//
//     bool IsEmpty() const
//     {
//         return !is_universal_ && elements_.empty();
//     }
//
//     bool HasExplicitElements() const
//     {
//         return !elements_.empty();
//     }
//
//     BoolSet Intersection(const BoolSet& other) const
//     {
//         if (is_universal_)
//             return other;
//         if (other.is_universal_)
//             return *this;
//         if (IsEmpty() || other.IsEmpty())
//             return EmptySet();
//
//         std::vector<bool> intersection;
//         for (const auto& entry : elements_) {
//             if (std::find(other.elements_.begin(), other.elements_.end(), entry) != other.elements_.end())
//                 intersection.push_back(entry);
//         }
//         return BoolSet(std::move(intersection));
//     }
//
//     bool FirstElement() const
//     {
//         RTC_DCHECK(HasExplicitElements());
//         return elements_[0];
//     }
//
//     const std::vector<bool>& elements() const
//     {
//         RTC_DCHECK(!is_universal_);
//         return elements_;
//     }
//
//     bool is_universal() const
//     {
//         return is_universal_;
//     }
//
// private:
//     bool is_universal_;
//     std::vector<bool> elements_;
// };

using DoubleRangeSet = NumericRangeSet<double>;
using IntRangeSet = NumericRangeSet<int32_t>;

double SquareEuclideanDistance(double x1, double y1, double x2, double y2)
{
    double x = x1 - x2;
    double y = y1 - y2;
    return x * 2 + y * y;
}

std::string FacingModeToString(FacingMode facingMode)
{
    switch (facingMode) {
        case FacingMode::kUser:
            return "user";
        case FacingMode::kEnvironment:
            return "environment";
        default:
            return "";
    }
}

void UpdateFailedConstraintName(const BaseConstraint& constraint, std::string* failed_constraint_name)
{
    if (failed_constraint_name) {
        *failed_constraint_name = constraint.GetName();
    }
}

double NumericConstraintFitnessDistance(double value1, double value2)
{
    if (std::fabs(value1 - value2) <= DoubleConstraint::kConstraintEpsilon)
        return 0.0;

    return std::fabs(value1 - value2) / std::max(std::fabs(value1), std::fabs(value2));
}

template <typename NumericConstraint>
double NumericValueFitness(const NumericConstraint& constraint, decltype(constraint.Min()) value)
{
    return constraint.HasIdeal() ? NumericConstraintFitnessDistance(value, constraint.Ideal()) : 0.0;
}

template <typename NumericConstraint>
double
NumericRangeFitness(const NumericConstraint& constraint, decltype(constraint.Min()) min, decltype(constraint.Min()) max)
{
    if (constraint.HasIdeal()) {
        auto ideal = constraint.Ideal();
        if (ideal < min) {
            return NumericConstraintFitnessDistance(min, ideal);
        } else if (ideal > max) {
            return NumericConstraintFitnessDistance(max, ideal);
        }
    }

    return 0.0;
}

double StringConstraintFitnessDistance(const std::string& value, const StringConstraint& constraint)
{
    if (!constraint.HasIdeal())
        return 0.0;

    for (auto& ideal_value : constraint.Ideal()) {
        if (value == ideal_value) {
            return 0.0;
        }
    }

    return 1.0;
}

class CandidateSettings {
public:
    CandidateSettings(std::string deviceId, std::string groupId, FacingMode facingMode, video::VideoProfile profile)
        : deviceId_(deviceId),
          groupId_(groupId),
          facingMode_(facingMode),
          profile_(profile),
          targetWidth_(profile.resolution.width),
          targetHeight_(profile.resolution.height),
          targetAspectRatio_(static_cast<double>(profile.resolution.width) / profile.resolution.height),
          targetFrameRate_({profile.frameRateRange.min, profile.frameRateRange.max})
    {
    }

    double Fitness(const MediaTrackConstraintSet& constraint_set)
    {
        return DeviceFitness(constraint_set) + ProfileFitness(constraint_set);
    }

    double DeviceFitness(const MediaTrackConstraintSet& constraint_set)
    {
        return StringConstraintFitnessDistance(deviceId_, constraint_set.deviceId) +
               StringConstraintFitnessDistance(groupId_, constraint_set.groupId) +
               StringConstraintFitnessDistance(FacingModeToString(facingMode_), constraint_set.facingMode);
    }

    double ProfileFitness(const MediaTrackConstraintSet& constraint_set)
    {
        return NumericValueFitness(constraint_set.width, targetWidth_) +
               NumericValueFitness(constraint_set.height, targetHeight_) +
               NumericValueFitness(constraint_set.aspectRatio, targetAspectRatio_) +
               NumericRangeFitness(constraint_set.frameRate, targetFrameRate_.min, targetFrameRate_.max);
    }

    bool
    ApplyConstraintSet(const MediaTrackConstraintSet& constraint_set, std::string* failed_constraint_name = nullptr)
    {
        // resizeMode is not supported
        auto constrainedWidth = IntRangeSet::FromConstraint(constraint_set.width);
        if (!constrainedWidth.Contains(targetWidth_)) {
            UpdateFailedConstraintName(constraint_set.width, failed_constraint_name);
            return false;
        }

        auto constrainedHeight = IntRangeSet::FromConstraint(constraint_set.height);
        if (!constrainedHeight.Contains(targetHeight_)) {
            UpdateFailedConstraintName(constraint_set.height, failed_constraint_name);
            return false;
        }

        auto constrainedAspectRatio = DoubleRangeSet::FromConstraint(constraint_set.aspectRatio);
        if (!constrainedAspectRatio.Contains(targetAspectRatio_)) {
            UpdateFailedConstraintName(constraint_set.aspectRatio, failed_constraint_name);
            return false;
        }

        auto constrainedFrameRate =
            DoubleRangeSet::FromConstraint(constraint_set.frameRate, MinFrameRate(), MaxFrameRate());
        if (constrainedFrameRate.IsEmpty()) {
            UpdateFailedConstraintName(constraint_set.frameRate, failed_constraint_name);
            return false;
        }
        if (constrainedFrameRate.Min()) {
            profile_.frameRateRange.min = constrainedFrameRate.Min().value();
        }
        if (constrainedFrameRate.Max()) {
            profile_.frameRateRange.max = constrainedFrameRate.Max().value();
        }

        constrained_width_ =
            constrained_width_.Intersection(IntRangeSet::FromConstraint(constraint_set.width, 1L, kMaxDimension));
        constrained_height_ =
            constrained_height_.Intersection(IntRangeSet::FromConstraint(constraint_set.height, 1L, kMaxDimension));
        constrained_aspect_ratio_ = constrained_aspect_ratio_.Intersection(
            DoubleRangeSet::FromConstraint(constraint_set.aspectRatio, 0.0, HUGE_VAL));
        constrained_frame_rate_ = constrained_frame_rate_.Intersection(
            DoubleRangeSet::FromConstraint(constraint_set.frameRate, 0.0, kMaxFrameRate));

        return true;
    }

    bool SatisfiesFrameRateConstraint(const DoubleConstraint& constraint) const
    {
        double constraint_min = ConstraintHasMin(constraint) ? ConstraintMin(constraint) : -1.0;
        double constraint_max =
            ConstraintHasMax(constraint) ? ConstraintMax(constraint) : static_cast<double>(kMaxFrameRate);
        bool constraint_min_out_of_range = constraint_min > MaxFrameRate();
        bool constraint_max_out_of_range = constraint_max < MinFrameRate();
        bool constraint_self_contradicts = constraint_min > constraint_max;

        return !constraint_min_out_of_range && !constraint_max_out_of_range && !constraint_self_contradicts;
    }

    CameraCaptureSettings GetSetting()
    {
        CameraCaptureSettings setting;

        setting.deviceId = deviceId_;
        setting.profile.resolution.width = targetWidth_;
        setting.profile.resolution.height = targetHeight_;
        setting.profile.frameRateRange.min = targetFrameRate_.min;
        setting.profile.frameRateRange.max = targetFrameRate_.max;
        setting.profile.format = profile_.format;

        return setting;
    }

    int NativeWidth() const
    {
        return profile_.resolution.width;
    }
    int NativeHeight() const
    {
        return profile_.resolution.height;
    }
    double NativeAspectRatio() const
    {
        RTC_DCHECK(NativeWidth() > 0 || NativeHeight() > 0);
        return static_cast<double>(NativeWidth()) / NativeHeight();
    }
    video::FrameRateRange NativeFrameRateRange() const
    {
        return profile_.frameRateRange;
    }

    const std::optional<double>& MinFrameRateConstraint() const
    {
        return constrained_frame_rate_.Min();
    }
    const std::optional<double>& MaxFrameRateConstraint() const
    {
        return constrained_frame_rate_.Max();
    }

    double MaxFrameRate() const
    {
        if (MaxFrameRateConstraint()) {
            return std::min(*MaxFrameRateConstraint(), static_cast<double>(profile_.frameRateRange.max));
        }
        return static_cast<double>(profile_.frameRateRange.max);
    }

    double MinFrameRate() const
    {
        if (MinFrameRateConstraint()) {
            return std::max(*MinFrameRateConstraint(), static_cast<double>(profile_.frameRateRange.min));
        }
        return static_cast<double>(profile_.frameRateRange.min);
    }

private:
    std::string deviceId_;
    std::string groupId_;
    FacingMode facingMode_;
    video::VideoProfile profile_;

    uint32_t targetWidth_;
    uint32_t targetHeight_;
    double targetAspectRatio_;
    video::FrameRateRange targetFrameRate_;

    NumericRangeSet<int> constrained_width_;
    NumericRangeSet<int> constrained_height_;
    NumericRangeSet<double> constrained_aspect_ratio_;
    NumericRangeSet<double> constrained_frame_rate_;
};

bool FacingModeSatisfiesConstraint(webrtc::FacingMode value, const webrtc::StringConstraint& constraint)
{
    std::string string_value = FacingModeToString(value);
    if (string_value.empty()) {
        return constraint.Exact().empty();
    }

    return constraint.Matches(string_value);
}

bool DeviceSatisfiesConstraintSet(
    const CameraDeviceInfo& device, const MediaTrackConstraintSet& constraint_set, std::string* failed_constraint_name)
{
    if (!constraint_set.deviceId.Matches(device.deviceId)) {
        UpdateFailedConstraintName(constraint_set.deviceId, failed_constraint_name);
        return false;
    }

    if (!constraint_set.groupId.Matches(device.groupId)) {
        UpdateFailedConstraintName(constraint_set.groupId, failed_constraint_name);
        return false;
    }

    if (!FacingModeSatisfiesConstraint(device.facingMode, constraint_set.facingMode)) {
        UpdateFailedConstraintName(constraint_set.facingMode, failed_constraint_name);
        return false;
    }

    return true;
}

bool SelectSettingsForVideo(
    const std::vector<CameraDeviceInfo>& devices, const MediaTrackConstraints& constraints, int default_width,
    int default_height, double default_frame_rate, CameraCaptureSettings& setting, std::string& failed_constraint_name)
{
    RTC_DLOG(LS_INFO) << __FUNCTION__;
    RTC_DLOG(LS_VERBOSE) << "Constraints: " << constraints.ToString();

    // This function works only if infinity is defined for the double type.
    static_assert(std::numeric_limits<double>::has_infinity, "Requires infinity");

    uint32_t success = 0;
    std::vector<double> best_distance(constraints.Advanced().size() + 4);
    std::fill(best_distance.begin(), best_distance.end(), HUGE_VAL);

    //     std::set<webrtc::MediaTrackSettings> candidates;
    for (auto& device : devices) {
        RTC_DLOG(LS_INFO) << "device: " << device.deviceId << ", " << device.groupId << ", "
                          << FacingModeToString(device.facingMode);
        if (!DeviceSatisfiesConstraintSet(device, constraints.Basic(), &failed_constraint_name)) {
            RTC_DLOG(LS_ERROR) << "Failed to satisfies basic constraints: " << failed_constraint_name;
            continue;
        }

        for (auto& profile : device.profiles) {
            RTC_DLOG(LS_INFO) << "-- profile: " << profile.resolution.width << "x" << profile.resolution.height << ","
                              << profile.frameRateRange.min << "-" << profile.frameRateRange.max << ", "
                              << profile.format;
            CandidateSettings candidate(device.deviceId, device.groupId, device.facingMode, profile);
            if (!candidate.ApplyConstraintSet(constraints.Basic(), &failed_constraint_name)) {
                RTC_DLOG(LS_ERROR) << "Failed to apply basic constraints: " << failed_constraint_name;
                continue;
            }

            // At this point we have a candidate that satisfies all basic constraints.
            std::vector<double> candidate_distance_vector;

            // 1. satisfaction of advanced constraint sets.
            for (const auto& advanced_set : constraints.Advanced()) {
                bool satisfies_advanced_set = false;

                if (DeviceSatisfiesConstraintSet(device, advanced_set, nullptr)) {
                    if (candidate.ApplyConstraintSet(advanced_set, nullptr)) {
                        satisfies_advanced_set = true;
                    } else {
                        RTC_DLOG(LS_ERROR) << "Failed to apply advanced constraints";
                    }
                } else {
                    RTC_DLOG(LS_ERROR) << "Failed to satisfies advanced constraints";
                }

                candidate_distance_vector.push_back(satisfies_advanced_set ? 0 : HUGE_VAL);
            }

            // 2. fitness distance.
            candidate_distance_vector.push_back(candidate.Fitness(constraints.Basic()));

            // 3. default resolution
            candidate_distance_vector.push_back(SquareEuclideanDistance(
                candidate.NativeWidth(), candidate.NativeHeight(), default_width, default_height));

            // 4. default frame rate
            double frame_rate_distance = 0.0;
            if (default_frame_rate < candidate.NativeFrameRateRange().min) {
                frame_rate_distance =
                    NumericConstraintFitnessDistance(candidate.NativeFrameRateRange().min, default_frame_rate);
            } else if (default_frame_rate > candidate.NativeFrameRateRange().max) {
                frame_rate_distance =
                    NumericConstraintFitnessDistance(candidate.NativeFrameRateRange().max, default_frame_rate);
            }
            candidate_distance_vector.push_back(frame_rate_distance);

            // 5. order in devices
            for (std::size_t i = 0; i < devices.size(); ++i) {
                if (device.deviceId == devices[i].deviceId) {
                    candidate_distance_vector.push_back(i);
                    break;
                }
            }

#ifndef NDEBUG
            {
                rtc::StringBuilder builder;
                builder << "[";
                for (auto& distance : candidate_distance_vector) {
                    if (builder.size() > 1) {
                        builder << ", ";
                    }
                    builder << distance;
                }
                builder << "]";
                RTC_DLOG(LS_INFO) << "candidate_distance_vector: " << builder.str();
            }
#endif
            RTC_DCHECK_EQ(best_distance.size(), candidate_distance_vector.size());
            if (std::lexicographical_compare(
                    candidate_distance_vector.begin(), candidate_distance_vector.end(), best_distance.begin(),
                    best_distance.end()))
            {
                best_distance = candidate_distance_vector;
#ifndef NDEBUG
                {
                    rtc::StringBuilder builder;
                    builder << "[";
                    for (auto& distance : best_distance) {
                        if (builder.size() > 1) {
                            builder << ", ";
                        }
                        builder << distance;
                    }
                    builder << "]";
                    RTC_DLOG(LS_INFO) << "best_distance: " << builder.str();
                }
#endif
                setting = candidate.GetSetting();
                success = 1;
            }
        }
    }
    RTC_DLOG(LS_INFO) << "success: " << success;

    return success > 0;
}

template <typename T, typename ConstraintType>
bool GetConstraint(const ConstraintType& constraint, T* value, size_t* mandatory_constraints)
{
    if (constraint.HasExact()) {
        *value = constraint.Exact();
        return true;
    }
    if (constraint.HasIdeal()) {
        *value = constraint.Ideal();
        return true;
    }
    return false;
}

template <typename T, typename ConstraintType>
void ConstraintToOptional(const ConstraintType& constraint, absl::optional<T>* value_out)
{
    T value;
    bool present = GetConstraint(constraint, &value, nullptr);
    if (present) {
        *value_out = value;
    }
}

void CopyConstraintsIntoAudioOptions(const MediaTrackConstraints& constraints, cricket::AudioOptions& options)
{
    if (constraints.IsNull()) {
        return;
    }

    // ignore advanced constraints
    auto& basic_set = constraints.Basic();
    ConstraintToOptional<bool>(basic_set.echoCancellation, &options.echo_cancellation);
    ConstraintToOptional<bool>(basic_set.autoGainControl, &options.auto_gain_control);
    ConstraintToOptional<bool>(basic_set.noiseSuppression, &options.noise_suppression);
    
//     ConstraintToOptional<bool>(basic_set.googEchoCancellation, &options.echo_cancellation);
//     ConstraintToOptional<bool>(basic_set.googAutoGainControl, &options.auto_gain_control);
//     ConstraintToOptional<bool>(basic_set.googNoiseSuppression, &options.noise_suppression);
//     ConstraintToOptional<bool>(basic_set.googHighpassFilter, &options.highpass_filter);
//     ConstraintToOptional<bool>(basic_set.googAudioMirroring, &options.stereo_swapping);
}

void GetScreenCaptureConstraints(const MediaTrackConstraints& constraints, int& width, int& height)
{
    if (constraints.IsNull()) {
        return;
    }

    // ignore advanced constraints
    auto& basic_set = constraints.Basic();
    
    GetConstraint(basic_set.width, &width, nullptr);
    GetConstraint(basic_set.height, &height, nullptr);
}

} // namespace webrtc
