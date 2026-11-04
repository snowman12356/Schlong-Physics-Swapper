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

// The subset of settings that decides a managed NPC's pose. Kept here, in the
// game-independent core, so the mapping is testable without Skyrim types.
struct ManagedPoseProfile {
    float threshold{ 60.0F };
    int erectBend{ 14 };
};

// A managed NPC uses its own threshold and configured erect bend, not the
// ordinary-SOS fallback above, which must not inherit player SPS thresholds.
// `tngBackend` selects the event range. Both backends name SOSBend-9 .. SOSBend9.
// SOSBend-9 is the lowest angle.
[[nodiscard]] std::optional<std::string> ManagedSosEvent(float arousal,
    const ManagedPoseProfile& profile, bool tngBackend);

// Bend for the SPS pose channel. Unlike ManagedSosEvent this is a numeric angle
// rather than a stage name, because the pose channel is continuous. Below the
// threshold the NPC holds the neutral reference; at or above it, the configured
// erect bend. Returns -1 for an unknown arousal so no pose is invented.
[[nodiscard]] int ManagedPoseBend(float arousal, const ManagedPoseProfile& profile);

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
