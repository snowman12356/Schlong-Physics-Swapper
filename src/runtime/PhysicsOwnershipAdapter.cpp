#include "PhysicsOwnershipAdapter.h"

#include "controllers/ActorRequestLedger.h"
#include "controllers/ManagedActorRegistry.h"
#include "PapyrusGateway.h"
#include "SPSAPI.h"
#include "controllers/PapyrusOperationGate.h"

#include <SKSE/SKSE.h>
#include <atomic>
#include <random>
#include <string>
#include <utility>

namespace SPS::Runtime {
namespace {

Controllers::PapyrusOperationGate operationGate;
// Actor-scoped ownership tokens. Separate from ActorBridgeAdapter's ledger so
// removing an NPC's arousal work cannot silently release an ownership lease.
Controllers::ActorRequestLedger ownershipRequests{ 8 };
std::atomic<int> bridgeVersion{ 0 };
std::atomic<bool> versionPending{ false };
std::atomic<std::int64_t> versionStartedMs{ 0 };
std::atomic<std::int64_t> versionRetryAfterMs{ 0 };
std::atomic<std::uint64_t> versionGeneration{ 0 };

std::string NewOperationToken()
{
    // Saved static stacks must not collide with tokens from a fresh process.
    static const auto processID = [] {
        std::random_device random;
        return std::to_string(random()) + "-" + std::to_string(random());
    }();
    static std::atomic<std::uint64_t> next{ 0 };
    return processID + "-" + std::to_string(++next);
}

bool EnterOperation(RE::StaticFunctionTag*, RE::BSFixedString token)
{
    return operationGate.Enter(token.c_str());
}

bool OperationCurrent(RE::StaticFunctionTag*, RE::BSFixedString token)
{
    return operationGate.Current(token.c_str());
}

bool QueueResetBarrier(RE::StaticFunctionTag*, RE::BSFixedString token)
{
    if (!operationGate.Current(token.c_str())) return false;
    if (auto* tasks = SKSE::GetTaskInterface()) {
        // FSMP has already queued its rebuild. SKSE processes its task before
        // this FIFO barrier, so Papyrus never restores ownership ahead of it.
        tasks->AddTask([value = std::string(token.c_str())] {
            operationGate.PassBarrier(value);
        });
        return true;
    }
    return false;
}

bool ResetBarrierPassed(RE::StaticFunctionTag*, RE::BSFixedString token)
{
    return operationGate.BarrierPassed(token.c_str());
}

namespace {

// Actor-scoped ownership guards. The player never resolves through these, so a
// failed NPC lookup can only return null, never the player.
bool ActorAvailable(RE::Actor* actor)
{
    return actor && actor->GetFormID() != SPS::API::kPlayerFormID &&
        !actor->IsChild() && !actor->IsDead() && !actor->IsDeleted() &&
        !actor->IsDisabled() && actor->Is3DLoaded() && actor->Get3D() &&
        !ExecutionPaused();
}

}

bool OwnershipRequestCurrent(RE::StaticFunctionTag*, RE::BSFixedString token, RE::Actor* actor)
{
    if (!ActorAvailable(actor)) return false;
    return ownershipRequests.Current(token.c_str(),
        Controllers::ActorRequestKind::ownership, actor->GetFormID(),
        actor->GetHandle().native_handle(), NowMs(),
        reinterpret_cast<std::uintptr_t>(actor->Get3D()));
}

RE::Actor* ResolveOwnershipRequest(RE::StaticFunctionTag*, RE::BSFixedString token)
{
    const auto request = ownershipRequests.Read(token.c_str(),
        Controllers::ActorRequestKind::ownership, NowMs());
    if (!request) return nullptr;
    const auto actor = RE::Actor::LookupByHandle(request->handle);
    return ActorAvailable(actor.get()) ? actor.get() : nullptr;
}

class BridgeVersionCallback final : public RE::BSScript::IStackCallbackFunctor {
public:
    explicit BridgeVersionCallback(std::uint64_t generation) : generation_(generation) {}
    void operator()(RE::BSScript::Variable result) override
    {
        if (generation_ != versionGeneration.load()) return;
        const int version = result.IsInt() ? result.GetSInt() : -1;
        bridgeVersion.store(version == kPhysicsBridgeVersion ? kPhysicsBridgeVersion : -1);
        versionPending.store(false);
        if (version != kPhysicsBridgeVersion) SKSE::log::error("SPS-003: Physics bridge ABI mismatch; install the paired DLL and scripts");
    }
    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
private:
    std::uint64_t generation_;
};

// The bridge returns Int[4]:
//   [0] 1 = operation still valid and the player 3D was present,
//   [1] managed bones that were dynamic at a disable sample (-1 = unsampled),
//   [2] managed bone array length,
//   [3] managed bones dynamic on the post-enable sample (-1 = not an SMP handoff).
// Verified against Faster HDT-SMP src/dhdtPapyrusFunctions.cpp: each entry is the
// bone's state before that call, and a bone already in the requested state is
// left unchanged. Only a true entry proves the bone is dynamic.
OwnershipReadback ReadBridgeReadback(const RE::BSScript::Variable& value)
{
    OwnershipReadback readback{};
    if (!value.IsArray()) return readback;
    const auto array = value.GetArray();
    if (!array) return readback;

    const auto size = array->size();
    const auto at = [&array, size](std::uint32_t index) -> int {
        if (index >= size) return 0;
        const auto& element = (*array)[index];
        return element.IsInt() ? element.GetSInt() : 0;
    };

    readback.acknowledged = at(0) != 0;
    readback.dynamicBonesBefore = size > 1 ? at(1) : -1;
    readback.boneCount = size > 2 ? at(2) : 0;
    readback.dynamicBonesAfter = size > 3 ? at(3) : -1;
    return readback;
}

class OwnerCompletionCallback final : public RE::BSScript::IStackCallbackFunctor {
public:
    OwnerCompletionCallback(std::string token, OwnershipCompletion completion,
        bool actorRequest = false) :
        token_(std::move(token)), completion_(std::move(completion)), actorRequest_(actorRequest) {}
    ~OwnerCompletionCallback() override { Release(); }
    void operator()(RE::BSScript::Variable result) override
    {
        // Cancellation retains the lease until the stack really returns.
        Release();
        if (completion_) completion_(ReadBridgeReadback(result));
    }
    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
private:
    void Release()
    {
        operationGate.Complete(token_);
        // The NPC's ownership request must stay readable until the stack has
        // returned: SetActorOwnerV3 resolves its target through it on entry and
        // re-checks it around every engine call. Releasing it at dispatch made
        // every NPC handoff resolve None and fail.
        if (actorRequest_) ownershipRequests.Discard(token_);
    }
    std::string token_;
    OwnershipCompletion completion_;
    bool actorRequest_{ false };
};
}

bool RegisterPhysicsBridge(RE::BSScript::IVirtualMachine* vm)
{
    // The four lease guards only touch the mutex-guarded operation gate (and
    // SKSE's thread-safe task queue), so they may run on the script thread.
    // Without this flag each call waits for a main-thread frame, and a bridge
    // stack makes about twenty of them, which stretched transactions towards
    // their timeout. The actor guards below read actor state, so they keep
    // the default main-thread synchronisation.
    vm->RegisterFunction("EnterOperation", "SPS_FSMPBridge", EnterOperation, true);
    vm->RegisterFunction("OperationCurrent", "SPS_FSMPBridge", OperationCurrent, true);
    vm->RegisterFunction("QueueResetBarrier", "SPS_FSMPBridge", QueueResetBarrier, true);
    vm->RegisterFunction("ResetBarrierPassed", "SPS_FSMPBridge", ResetBarrierPassed, true);
    vm->RegisterFunction("OwnershipRequestCurrent", "SPS_FSMPBridge",
        OwnershipRequestCurrent);
    vm->RegisterFunction("ResolveOwnershipRequest", "SPS_FSMPBridge",
        ResolveOwnershipRequest);
    return true;
}

void CancelPhysicsOperation() { operationGate.Cancel(); }
std::string CreatePhysicsOperationToken() { return NewOperationToken(); }
bool PreparePhysicsOperation(const std::string& token) { return operationGate.Prepare(token); }
void CompletePhysicsOperation(const std::string& token) { operationGate.Complete(token); }
void CancelPhysicsOperation(const std::string& token) { operationGate.Cancel(token); }
bool PhysicsOperationBusy() { return operationGate.Busy(); }
bool PhysicsOperationExecuting() { return operationGate.Executing(); }
void ResetPhysicsOperations()
{
    operationGate.ResetSession();
    ownershipRequests.ResetSession();
    ++versionGeneration;
    versionPending.store(false);
    versionRetryAfterMs.store(0);
}

bool OrderedFsmpBridgeAvailable()
{
    if (bridgeVersion.load() != 0) return bridgeVersion.load() == kPhysicsBridgeVersion;
    const auto now = NowMs();
    if (now < versionRetryAfterMs.load()) return false;
    if (versionPending.load() && now - versionStartedMs.load() < 5000) return false;
    if (!PapyrusReady(0, now)) return false;
    const auto generation = ++versionGeneration;
    versionPending.store(true);
    versionStartedMs.store(now);
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{
        new BridgeVersionCallback(generation)
    };
    if (!DispatchStaticWithCallback("SPS_FSMPBridge", "GetSPSBridgeVersion", 0, now, std::move(callback))) {
        versionPending.store(false);
        versionRetryAfterMs.store(now + 10000);
        SKSE::log::warn("SPS-003: Physics bridge version query could not be dispatched; retrying after a delay");
    }
    return false;
}

OwnershipDispatchResult SetPlayerPhysicsOwner(
    RE::Actor* actor, bool useCBPC, int preparation, int softBend,
    bool fsmpActorApiAvailable, std::int64_t allowedAfterMs,
    std::int64_t nowMs, OwnershipCompletion completion)
{
    if (!actor || actor != RE::PlayerCharacter::GetSingleton() ||
        !fsmpActorApiAvailable || !completion || !OrderedFsmpBridgeAvailable())
        return OwnershipDispatchResult::failed;
    const auto token = NewOperationToken();
    if (!operationGate.Prepare(token)) return OwnershipDispatchResult::failed;
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{
        new OwnerCompletionCallback(token, std::move(completion))
    };
    if (DispatchStaticWithCallback("SPS_FSMPBridge", "SetPlayerOwnerV3",
            allowedAfterMs, nowMs, std::move(callback),
            RE::BSFixedString(token), useCBPC, preparation, softBend))
        return OwnershipDispatchResult::queued;
    operationGate.Complete(token);
    return OwnershipDispatchResult::failed;
}

OwnershipDispatchResult SetActorPhysicsOwner(
    Controllers::ActorWorkLease lease, RE::Actor* actor, bool useCBPC,
    int preparation, bool fsmpActorApiAvailable, std::int64_t allowedAfterMs,
    std::int64_t nowMs, OwnershipCompletion completion)
{
    // The player has its own path with SOS bend and soft-pose steps; sharing it
    // here would apply player settings to an NPC.
    if (!actor || actor == RE::PlayerCharacter::GetSingleton() || !lease.Valid() ||
        actor->GetFormID() != lease.Ticket().formID || !fsmpActorApiAvailable ||
        !completion || !OrderedFsmpBridgeAvailable())
        return OwnershipDispatchResult::failed;
    const auto handle = actor->GetHandle();
    const auto token = NewOperationToken();
    const auto identity = reinterpret_cast<std::uintptr_t>(actor->Get3D());
    // Ownership work is serialized with every other engine mutation, so it must
    // take the same ledger slot as the player path rather than a parallel gate.
    if (!ownershipRequests.Prepare(token, { lease, handle.native_handle(),
            Controllers::ActorRequestKind::ownership, nowMs + kActorOwnershipValidityMs, false, identity }, nowMs))
        return OwnershipDispatchResult::failed;
    if (!operationGate.Prepare(token)) {
        ownershipRequests.Discard(token);
        return OwnershipDispatchResult::failed;
    }
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{
        new OwnerCompletionCallback(token, std::move(completion), true)
    };
    // The callback releases both the gate lease and the ownership request when
    // the stack returns (or is disposed), so the request stays resolvable for
    // the whole bridge call. Expiry still bounds a stack that never returns;
    // it matches the NPC controller's hard timeout so the stack is never
    // invalidated while the controller still waits for it.
    if (DispatchStaticWithCallback("SPS_FSMPBridge", "SetActorOwnerV3",
            allowedAfterMs, nowMs, std::move(callback),
            RE::BSFixedString(token), useCBPC, preparation)) {
        return OwnershipDispatchResult::queued;
    }
    ownershipRequests.Discard(token);
    operationGate.Complete(token);
    return OwnershipDispatchResult::failed;
}

void CancelActorOwnership(Controllers::ActorWorkTicket ticket)
{
    // The ledger invalidates the guard immediately; the operation gate keeps
    // the lease until the dispatched stack really returns.
    for (const auto& token : ownershipRequests.Cancel(ticket)) {
        operationGate.Cancel(token);
    }
}
}
