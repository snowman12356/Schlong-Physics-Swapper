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
// Drops every cached probe (save load / new game boundary).
void ResetApiActorReads();

}
