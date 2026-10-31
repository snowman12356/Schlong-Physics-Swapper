#include "BasicNpcPolicy.h"
#include "PhysicsDecision.h"

#include <algorithm>

namespace SPS::Controllers {

std::optional<std::string> BasicSosEvent(float arousal)
{
    const auto reading = Core::ValidArousalReading(arousal);
    if (!reading) return std::nullopt;
    const int bend = static_cast<int>(*reading) / 4 - 14;
    return bend < -9 ? "SOSFlaccid" : "SOSBend" + std::to_string(std::min(bend, 9));
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
    if (!event || (applied_ == event && nowMs >= appliedAtMs_ &&
        nowMs - appliedAtMs_ < kRefreshMs)) return std::nullopt;
    return event;
}

void BasicNpcPolicy::Complete(const std::string& event, BasicPositionResult result, std::int64_t nowMs)
{
    if (result == BasicPositionResult::applied) {
        applied_ = event;
        appliedAtMs_ = nowMs;
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
