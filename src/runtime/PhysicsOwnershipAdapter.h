#pragma once

#include <RE/Skyrim.h>

#include <cstdint>

namespace SPS::Runtime {

bool OrderedFsmpBridgeAvailable();
bool ResetPlayerPhysics(
    RE::Actor* actor,
    bool full,
    bool fsmpActorApiAvailable,
    std::int64_t allowedAfterMs,
    std::int64_t nowMs);
bool SetPlayerPhysicsOwner(
    RE::Actor* actor,
    bool useCBPC,
    bool fsmpActorApiAvailable,
    std::int64_t allowedAfterMs,
    std::int64_t nowMs);
bool ReleasePlayerPhysics(
    RE::Actor* actor,
    bool fsmpActorApiAvailable,
    std::int64_t allowedAfterMs,
    std::int64_t nowMs);

}
