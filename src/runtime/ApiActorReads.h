#pragma once

#include <cstdint>

namespace SPS::Runtime {

enum class ApiActorRead { ready, notReady, unsupported };

// Backing for the V3 public API. Game thread only. Neither call starts a
// Papyrus stack per call; the scene read answers from a cache that is refreshed
// through the existing scene bridge at most once a second per actor.
// The player is not handled here.
ApiActorRead ReadApiActorArousal(std::uint32_t formID, float* arousal);
ApiActorRead ReadApiActorScene(std::uint32_t formID, bool* inScene,
    std::int64_t sceneEndDelayMs, std::int64_t allowedAfterMs);
// V5. The actor's role in a SexLab/OStim scene: 0 unknown, 1 receiving, 2
// penetrating. Same cache and rules as the scene read; the first call returns
// notReady. Also valid for managed NPCs. The player is not handled here.
ApiActorRead ReadApiActorSceneRole(std::uint32_t formID, int* role, std::int64_t allowedAfterMs);
// Drops every cached probe (save load / new game boundary).
void ResetApiActorReads();

}
