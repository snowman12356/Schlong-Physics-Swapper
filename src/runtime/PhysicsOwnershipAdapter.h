#pragma once

#include "controllers/ActorWorkLifetime.h"

#include <RE/Skyrim.h>

#include <cstdint>
#include <functional>
#include <string>

namespace SPS::Runtime {

// Paired SPS_FSMPBridge.pex must report this exact version (bridge v12).
inline constexpr int kPhysicsBridgeVersion = 12;

// Ownership timeouts. A stack that was queued but never entered is retired
// after the queued limit (cancelling it is safe: it cannot enter afterwards).
// A stack that has entered is left to finish until the hard limit; its own
// waits are bounded, so it returns well before that on a healthy game.
inline constexpr std::int64_t kOwnershipQueuedTimeoutMs = 5000;
// Counted from Begin, so it includes up to 5 s of queueing; the longest stack
// (bounded 10 s barrier wait plus the bake's own waits) fits with margin.
inline constexpr std::int64_t kOwnershipRunningTimeoutMs = 30000;
// NPC ownership requests stay valid for the stack as long as the NPC
// controller waits for it.
inline constexpr std::int64_t kActorOwnershipValidityMs = kOwnershipRunningTimeoutMs;

enum class OwnershipDispatchResult {
    failed,
    queued
};

// Live read-back from the bridge. `acknowledged` is the old boolean result:
// the stack ran to completion and the player 3D was present. It is execution
// evidence, not proof that either engine changed state.
// `dynamicBonesAfter` is a second SMP enable, taken after the handoff has
// asked FSMP to make the bones dynamic. FSMP records the live state and does
// not change a bone that is already dynamic, so a count equal to `boneCount`
// proves every managed bone was dynamic. -1 means this handoff did not sample.
// A smaller count does not prove the remaining bones are kinematic.
struct OwnershipReadback {
    bool acknowledged{ false };
    int dynamicBonesBefore{ -1 };
    int boneCount{ 0 };
    int dynamicBonesAfter{ -1 };
};

using OwnershipCompletion = std::function<void(OwnershipReadback)>;

bool OrderedFsmpBridgeAvailable();
bool RegisterPhysicsBridge(RE::BSScript::IVirtualMachine* vm);
void CancelPhysicsOperation();
bool PhysicsOperationBusy();
// True while a dispatched ownership stack is running (entered, not yet
// returned). A running stack is never cancelled by the ordinary timeout: a
// cancel between engine calls can leave the shaft half handed over.
bool PhysicsOperationExecuting();
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

// Actor-specific ownership dispatch for explicitly managed NPCs. This reuses
// the same ordered operation gate and bridge version as the player path, so an
// NPC handoff can never overlap a player handoff. The caller must hold a live
// managed-actor lease; the bridge re-verifies the actor before every mutation
// and reports -1 when the target changed or the stack went stale.
//
// `preparation` follows SPS_FSMPBridge.SetActorOwnerV3: 0 ordinary handoff,
// 1 full reset. The NPC path deliberately omits the player-only SOS bend and
// soft-pose alignment steps, which depend on player-scoped settings.
OwnershipDispatchResult SetActorPhysicsOwner(
    Controllers::ActorWorkLease lease,
    RE::Actor* actor,
    bool useCBPC,
    int preparation,
    bool fsmpActorApiAvailable,
    std::int64_t allowedAfterMs,
    std::int64_t nowMs,
    OwnershipCompletion completion);

// Removing or re-adding an NPC must revoke its ownership token without force-
// releasing a stack that is still executing. Cancelled tokens keep their gate
// lease until the real callback retires.
void CancelActorOwnership(Controllers::ActorWorkTicket ticket);

}
