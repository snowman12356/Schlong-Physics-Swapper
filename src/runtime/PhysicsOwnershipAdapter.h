#pragma once

#include <RE/Skyrim.h>

#include <cstdint>
#include <functional>
#include <string>

namespace SPS::Runtime {

// Paired SPS_FSMPBridge.pex must report this exact version (bridge v7).
inline constexpr int kPhysicsBridgeVersion = 7;

enum class OwnershipDispatchResult {
    failed,
    queued
};

// Live read-back from the bridge. `acknowledged` is the old boolean result:
// the stack ran to completion and the player 3D was present. It is execution
// evidence, not proof that either engine changed state.
// `dynamicBonesBefore` is FSMP's own previous-state read-back for the managed
// bones: above zero proves SMP still owned bone(s) at the moment of a disable
// call (drift). It is -1 whenever this path did not sample, and a zero or false
// entry must never be read as proof that SMP released a bone.
struct OwnershipReadback {
    bool acknowledged{ false };
    int dynamicBonesBefore{ -1 };
    int boneCount{ 0 };
};

using OwnershipCompletion = std::function<void(OwnershipReadback)>;

bool OrderedFsmpBridgeAvailable();
bool RegisterPhysicsBridge(RE::BSScript::IVirtualMachine* vm);
void CancelPhysicsOperation();
bool PhysicsOperationBusy();
void ResetPhysicsOperations();
// Actor position stacks share the existing physics execution lease. Scoped
// cancellation must never cancel an unrelated player's operation.
std::string CreatePhysicsOperationToken();
bool PreparePhysicsOperation(const std::string& token);
void CompletePhysicsOperation(const std::string& token);
void CancelPhysicsOperation(const std::string& token);
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
