#pragma once

#include "controllers/ManagedActorRegistry.h"
#include "controllers/BasicNpcPolicy.h"

#include <RE/Skyrim.h>
#include <functional>
#include <optional>
#include <string>

namespace SPS::Runtime {

using ActorArousalCompletion = std::function<void(Controllers::ActorWorkTicket, std::optional<float>)>;
using ActorArousalWriteCompletion = std::function<void(Controllers::ActorWorkTicket, std::optional<float>)>;
using ActorPositionCompletion = std::function<void(Controllers::ActorWorkTicket, bool)>;
using ActorSceneCompletion = std::function<void(Controllers::ActorWorkTicket, Controllers::ActorSceneStatus)>;
// 0 unknown, 1 receiving, 2 penetrating, -1 the bridge call was stale or failed.
using ActorSceneRoleCompletion = std::function<void(Controllers::ActorWorkTicket, int)>;
using BasicPositionCompletion = std::function<void(Controllers::ActorWorkTicket, Controllers::BasicPositionResult)>;

// These are building blocks, not automatic NPC enrollment or policy. Call on
// the game thread with a current registry lease and verified live actor.
// Completions return to that thread and carry a ticket, never a context pointer.
// The caller must resolve that ticket before updating its actor controllers.
bool QueryActorArousal(Controllers::ActorWorkLease lease, RE::Actor* actor,
    std::int64_t allowedAfterMs, ActorArousalCompletion completion);
// Writes the actual OSL value through OSLArousedNative.SetArousal. This is an
// explicit test action, not an SPS-only override. The player is accepted only
// when the caller supplies a deliberately player-enabled lifetime.
bool QueueActorArousalWrite(Controllers::ActorWorkLease lease, RE::Actor* actor,
    float value, std::int64_t allowedAfterMs, ActorArousalWriteCompletion completion);
bool QueueActorPosition(Controllers::ActorWorkLease lease, RE::Actor* actor,
    std::string eventName, std::optional<int> nativeBend,
    std::int64_t allowedAfterMs, ActorPositionCompletion completion);
bool QueryActorScene(Controllers::ActorWorkLease lease, RE::Actor* actor,
    std::int64_t allowedAfterMs, ActorSceneCompletion completion);
// The actor's role in a SexLab or OStim scene (SPS_PositionBridge v5).
bool QueryActorSceneRole(Controllers::ActorWorkLease lease, RE::Actor* actor,
    std::int64_t allowedAfterMs, ActorSceneRoleCompletion completion);
// Store request-start time with arousal/scene samples; callback arrival is not
// a new observation time. False dispatch, stale/absent callbacks and unknown
// results never establish permission to write.
// Caller must satisfy BasicNpcPolicy first. The bridge checks scene ownership
// again immediately before writing; this is not managed physics or enrollment.
bool QueueBasicActorPosition(Controllers::ActorWorkLease lease, RE::Actor* actor,
    std::string eventName, std::int64_t allowedAfterMs, BasicPositionCompletion completion);
void CancelActorBridgeRequests(Controllers::ActorWorkTicket ticket);
// Call at the existing save/new-game reset boundary alongside physics reset.
void ResetActorBridgeRequests();
bool RegisterActorBridgeGuards(RE::BSScript::IVirtualMachine* vm);

}
