#include "BasicNpcPolicy.h"
#include "PhysicsDecision.h"
#include "PoseTargets.h"

#include <algorithm>
#include <cmath>

namespace SPS::Controllers {

std::optional<std::string> BasicSosEvent(float arousal)
{
    const auto reading = Core::ValidArousalReading(arousal);
    if (!reading) return std::nullopt;
    const int bend = static_cast<int>(*reading) / 4 - 14;
    return bend < -9 ? "SOSFlaccid" : "SOSBend" + std::to_string(std::min(bend, 9));
}

std::optional<std::string> ManagedSosEvent(float arousal, const ManagedPoseProfile& profile,
    bool tngBackend)
{
    const auto reading = Core::ValidArousalReading(arousal);
    if (!reading) return std::nullopt;
    // Below the threshold the NPC is soft. At or above it the configured erect
    // bend applies; zero keeps the ordinary soft pose rather than inventing one.
    // With arousalBased set the bend follows ManagedPoseBend's gradient.
    const int bend = ManagedPoseBend(*reading, profile);
    // Same curve as the player. SOSBend-9 is the lowest angle, including the
    // soft pose; SOSFlaccid is not a step on this curve.
    return std::string(SPS::Core::PoseTargets::EventName(bend, tngBackend));
}

int ManagedPoseBend(float arousal, const ManagedPoseProfile& profile)
{
    const auto reading = Core::ValidArousalReading(arousal);
    if (!reading) return -1;
    // Same threshold rule as the event path so the two channels stay in step,
    // but the result is the bend itself, which the pose channel can express
    // continuously instead of rounding to one of ten stages.
    if (profile.arousalBased) {
        // Same arithmetic as the player's DesiredBend with a soft bend of 0.
        const float start = std::clamp(profile.startArousal, 0.0F, 99.0F);
        const float fullyErect = std::max(start + 1.0F, profile.threshold);
        const float progress = std::clamp((*reading - start) / (fullyErect - start), 0.0F, 1.0F);
        if (profile.erectBend <= 0) return 0;
        return std::clamp(static_cast<int>(std::lround(
            std::clamp(profile.erectBend, 0, 20) * progress)), 0, 20);
    }
    const bool erect = *reading >= profile.threshold && profile.erectBend > 0;
    return erect ? std::clamp(profile.erectBend, 0, 20) : 0;
}

void BasicNpcPolicy::ObserveScene(ActorSceneStatus status, std::int64_t sampledAtMs)
{
    if (sampledAtMs < 0 || sampledAtMs < sceneAtMs_) return;
    if (status != ActorSceneStatus::clear) {
        clearSinceMs_ = -1;
        applied_.reset();
    } else if (scene_ != ActorSceneStatus::clear || sceneAtMs_ < 0 ||
        sampledAtMs - sceneAtMs_ > kFreshMs) {
        clearSinceMs_ = sampledAtMs;
        applied_.reset();
    }
    scene_ = status;
    sceneAtMs_ = sampledAtMs;
}

std::optional<std::string> BasicNpcPolicy::Decide(float arousal, std::int64_t arousalSampledAtMs,
    std::int64_t nowMs, bool enabled, bool fullyManaged, bool providerSosDisabled) const
{
    if (!enabled || fullyManaged || !providerSosDisabled || scene_ != ActorSceneStatus::clear ||
        sceneAtMs_ < 0 || clearSinceMs_ < 0 || arousalSampledAtMs < 0 ||
        nowMs < sceneAtMs_ || nowMs < arousalSampledAtMs ||
        nowMs - sceneAtMs_ > kFreshMs || nowMs - arousalSampledAtMs > kFreshMs ||
        nowMs - clearSinceMs_ < kClearDelayMs) return std::nullopt;
    const auto event = BasicSosEvent(arousal);
    // OSL Aroused 2.9.3 (ActorStateManager::UpdateSOSAnimation) sends an event
    // only when the bend bucket differs from the last one it applied, and never
    // repeats an unchanged one: a repeat replays the SOS blend. SPS matches that.
    // The applied state is still cleared by scenes, failures, unload and resets
    // (as OSL clears its cache on scenes and load), so the pose is re-asserted
    // after those.
    if (!event || applied_ == event) return std::nullopt;
    return event;
}

void BasicNpcPolicy::Complete(const std::string& event, BasicPositionResult result, std::int64_t)
{
    if (result == BasicPositionResult::applied) {
        applied_ = event;
    } else {
        // A scene may have started after the initial query. Require fresh clear
        // observations and settling again, not a retry against a cached clear.
        scene_ = ActorSceneStatus::unknown;
        clearSinceMs_ = -1;
        applied_.reset();
    }
}

void BasicNpcPolicy::Reset() { *this = BasicNpcPolicy{}; }

}
