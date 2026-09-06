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
bool RegisterPhysicsBridge(RE::BSScript::IVirtualMachine* vm);
void CancelPhysicsOperation();
void ResetPhysicsOperations();
OwnershipDispatchResult SetPlayerPhysicsOwner(
    RE::Actor* actor,
    bool useCBPC,
    int preparation,
    int softBend,
    bool fsmpActorApiAvailable,
    std::int64_t allowedAfterMs,
    std::int64_t nowMs,
    OwnershipCompletion completion);

}
