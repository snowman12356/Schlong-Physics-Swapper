#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace SPS::Controllers {

// Zero stays unknown so a missing Papyrus result cannot mean clear.
enum class ActorSceneStatus : int { unknown = 0, clear = 1, active = 2 };
enum class BasicPositionResult : int { failed = -1, deferred = 0, applied = 1 };

// Source-backed legacy OSL mapping, independent of player SPS thresholds.
[[nodiscard]] std::optional<std::string> BasicSosEvent(float arousal);

// Per-actor policy only. The eventual basic scheduler owns these separately
// from full-management contexts and supplies active-time timestamps.
class BasicNpcPolicy {
public:
    static constexpr std::int64_t kFreshMs = 2500;
    static constexpr std::int64_t kClearDelayMs = 1500;
    static constexpr std::int64_t kRefreshMs = 10000;
    void ObserveScene(ActorSceneStatus status, std::int64_t sampledAtMs);
    [[nodiscard]] std::optional<std::string> Decide(float arousal, std::int64_t arousalSampledAtMs,
        std::int64_t nowMs, bool enabled, bool fullyManaged, bool providerSosDisabled) const;
    void Complete(const std::string& event, BasicPositionResult result, std::int64_t nowMs);
    void Reset();
private:
    ActorSceneStatus scene_{ ActorSceneStatus::unknown };
    std::int64_t sceneAtMs_{ -1 };
    std::int64_t clearSinceMs_{ -1 };
    std::optional<std::string> applied_;
    std::int64_t appliedAtMs_{ -1 };
};

}
