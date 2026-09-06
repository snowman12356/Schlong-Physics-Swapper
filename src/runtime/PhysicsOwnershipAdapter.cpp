#include "PhysicsOwnershipAdapter.h"
#include "PapyrusGateway.h"
#include "controllers/PapyrusOperationGate.h"

#include <SKSE/SKSE.h>
#include <atomic>
#include <random>
#include <string>
#include <utility>

namespace SPS::Runtime {
namespace {

Controllers::PapyrusOperationGate operationGate;
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

class BridgeVersionCallback final : public RE::BSScript::IStackCallbackFunctor {
public:
    explicit BridgeVersionCallback(std::uint64_t generation) : generation_(generation) {}
    void operator()(RE::BSScript::Variable result) override
    {
        if (generation_ != versionGeneration.load()) return;
        const int version = result.IsInt() ? result.GetSInt() : -1;
        bridgeVersion.store(version == 3 ? 3 : -1);
        versionPending.store(false);
        if (version != 3) SKSE::log::error("SPS-003: Physics bridge ABI mismatch; install the paired DLL and scripts");
    }
    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
private:
    std::uint64_t generation_;
};

class OwnerCompletionCallback final : public RE::BSScript::IStackCallbackFunctor {
public:
    OwnerCompletionCallback(std::string token, OwnershipCompletion completion) :
        token_(std::move(token)), completion_(std::move(completion)) {}
    ~OwnerCompletionCallback() override { operationGate.Complete(token_); }
    void operator()(RE::BSScript::Variable result) override
    {
        // Cancellation retains the lease until the stack really returns.
        operationGate.Complete(token_);
        if (completion_) completion_(result.IsBool() && result.GetBool());
    }
    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
private:
    std::string token_;
    OwnershipCompletion completion_;
};
}

bool RegisterPhysicsBridge(RE::BSScript::IVirtualMachine* vm)
{
    vm->RegisterFunction("EnterOperation", "SPS_FSMPBridge", EnterOperation);
    vm->RegisterFunction("OperationCurrent", "SPS_FSMPBridge", OperationCurrent);
    vm->RegisterFunction("QueueResetBarrier", "SPS_FSMPBridge", QueueResetBarrier);
    vm->RegisterFunction("ResetBarrierPassed", "SPS_FSMPBridge", ResetBarrierPassed);
    return true;
}

void CancelPhysicsOperation() { operationGate.Cancel(); }
void ResetPhysicsOperations()
{
    operationGate.ResetSession();
    ++versionGeneration;
    versionPending.store(false);
    versionRetryAfterMs.store(0);
}

bool OrderedFsmpBridgeAvailable()
{
    if (bridgeVersion.load() != 0) return bridgeVersion.load() == 3;
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
}
