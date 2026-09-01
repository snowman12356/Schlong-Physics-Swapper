#pragma once

#include <RE/Skyrim.h>

#include <cstdint>
#include <functional>

namespace SPS::Runtime {

enum class OwnershipDispatchResult {
    failed,
    queued
};

using OwnershipCompletion = std::function<void(bool)>;

bool OrderedFsmpBridgeAvailable();
bool ResetPlayerPhysics(
    RE::Actor* actor,
    bool full,
    bool fsmpActorApiAvailable,
    std::int64_t allowedAfterMs,
    std::int64_t nowMs);
OwnershipDispatchResult SetPlayerPhysicsOwner(
    RE::Actor* actor,
    bool useCBPC,
    bool fsmpActorApiAvailable,
    std::int64_t allowedAfterMs,
    std::int64_t nowMs,
    OwnershipCompletion completion);
bool ReleasePlayerPhysics(
    RE::Actor* actor,
    bool fsmpActorApiAvailable,
    std::int64_t allowedAfterMs,
    std::int64_t nowMs);

}
