#include "ActorBridgeAdapter.h"

#include "Compatibility.h"
#include "PapyrusGateway.h"
#include "PhysicsOwnershipAdapter.h"
#include "PhysicsDecision.h"
#include "SPSAPI.h"
#include "controllers/ActorRequestLedger.h"

#include <SKSE/SKSE.h>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <utility>

namespace SPS::Runtime {
namespace {
using Kind = Controllers::ActorRequestKind;
using Completion = std::function<void(Controllers::ActorWorkTicket, float)>;

// This is an internal ceiling on outstanding calls, not a user-facing managed
// actor limit. There is also at most one outstanding call per actor/kind.
Controllers::ActorRequestLedger requests{ 64 };

struct VersionProbe {
    explicit VersionProbe(int expectedVersion) : expected(expectedVersion) {}
    const int expected;
    std::mutex lock;
    int version{ 0 };
    bool pending{ false };
    std::int64_t retryAtMs{ 0 };
    std::uint64_t generation{ 0 };
    void Reset()
    {
        std::scoped_lock guard(lock);
        ++generation;
        version = 0;
        pending = false;
        retryAtMs = 0;
    }
};
VersionProbe arousalVersion{ 3 };
VersionProbe positionVersion{ 6 };
VersionProbe ostimVersion{ 3 };

class VersionCallback final : public RE::BSScript::IStackCallbackFunctor {
public:
    VersionCallback(VersionProbe& probe, std::uint64_t generation) : probe_(probe), generation_(generation) {}
    ~VersionCallback() override
    {
        std::scoped_lock guard(probe_.lock);
        if (probe_.generation == generation_) probe_.pending = false;
    }
    void operator()(RE::BSScript::Variable value) override
    {
        // Probes are process-lifetime objects, not removable actor controllers.
        // Keep the generation check and write together across a session reset.
        std::scoped_lock guard(probe_.lock);
        if (probe_.generation != generation_) return;
        const int version = value.IsInt() ? value.GetSInt() : -1;
        probe_.version = version == probe_.expected ? version : -1;
        probe_.pending = false;
        if (version != probe_.expected) SKSE::log::error("SPS NPC bridge version mismatch; install the paired DLL and scripts");
    }
    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
private:
    VersionProbe& probe_;
    std::uint64_t generation_;
};

bool BridgeReady(const char* script, VersionProbe& probe, std::int64_t allowedAfterMs)
{
    const auto now = NowMs();
    std::uint64_t generation;
    {
        std::scoped_lock guard(probe.lock);
        if (probe.version != 0) return probe.version == probe.expected;
        // A stalled VM query keeps its one slot until return/disposal or a real
        // session reset; elapsed time alone must not queue another probe.
        if (probe.pending || now < probe.retryAtMs || !PapyrusReady(allowedAfterMs, now)) return false;
        generation = ++probe.generation;
        probe.pending = true;
        probe.retryAtMs = now + 10000;
    }
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{
        new VersionCallback(probe, generation)
    };
    if (!DispatchStaticWithCallback(script, "GetSPSBridgeVersion", allowedAfterMs, now, std::move(callback))) {
        std::scoped_lock guard(probe.lock);
        if (probe.generation == generation) probe.pending = false;
    }
    return false;
}

bool AvailableActor(RE::Actor* actor, bool allowPlayer = false)
{
    return actor && (allowPlayer || actor->GetFormID() != SPS::API::kPlayerFormID) &&
        !actor->IsChild() && !actor->IsDead() && !actor->IsDeleted() &&
        !actor->IsDisabled() && actor->Is3DLoaded() && actor->Get3D();
}

bool Guard(const RE::BSFixedString& token, RE::Actor* actor, Kind kind)
{
    if (!AvailableActor(actor, kind == Kind::arousalWrite) || ExecutionPaused()) return false;
    return requests.Current(token.c_str(), kind, actor->GetFormID(),
        actor->GetHandle().native_handle(), NowMs(), reinterpret_cast<std::uintptr_t>(actor->Get3D()));
}

bool ArousalRequestCurrent(RE::StaticFunctionTag*, RE::BSFixedString token, RE::Actor* actor)
{
    return Guard(token, actor, Kind::arousal);
}
bool ArousalWriteRequestCurrent(RE::StaticFunctionTag*, RE::BSFixedString token, RE::Actor* actor)
{
    return Guard(token, actor, Kind::arousalWrite);
}
bool PositionRequestCurrent(RE::StaticFunctionTag*, RE::BSFixedString token, RE::Actor* actor)
{
    return Guard(token, actor, Kind::position);
}
bool SceneRequestCurrent(RE::StaticFunctionTag*, RE::BSFixedString token, RE::Actor* actor)
{
    return Guard(token, actor, Kind::scene);
}

RE::Actor* ResolveRequest(const RE::BSFixedString& token, Kind kind)
{
    const auto request = requests.Read(token.c_str(), kind, NowMs());
    if (!request) return nullptr;
    const auto actor = RE::Actor::LookupByHandle(request->handle);
    return Guard(token, actor.get(), kind) ? actor.get() : nullptr;
}

RE::Actor* ResolveArousalRequest(RE::StaticFunctionTag*, RE::BSFixedString token)
{
    return ResolveRequest(token, Kind::arousal);
}
RE::Actor* ResolveArousalWriteRequest(RE::StaticFunctionTag*, RE::BSFixedString token)
{
    return ResolveRequest(token, Kind::arousalWrite);
}
RE::Actor* ResolvePositionRequest(RE::StaticFunctionTag*, RE::BSFixedString token)
{
    return ResolveRequest(token, Kind::position);
}
RE::Actor* ResolveSceneRequest(RE::StaticFunctionTag*, RE::BSFixedString token)
{
    return ResolveRequest(token, Kind::scene);
}

bool QueueResult(std::string token, RE::ActorHandle handle, float value, Completion completion)
{
    if (auto* tasks = SKSE::GetTaskInterface()) {
        tasks->AddTask([token = std::move(token), handle, value, completion = std::move(completion)] {
            const auto request = requests.Finish(token, NowMs());
            if (!request || !request->lease.Valid()) return;
            const auto actor = handle.get();
            if (!AvailableActor(actor.get(), request->kind == Kind::arousalWrite) ||
                actor->GetFormID() != request->lease.Ticket().formID ||
                actor->GetHandle().native_handle() != request->handle ||
                reinterpret_cast<std::uintptr_t>(actor->Get3D()) != request->rootIdentity) return;
            completion(request->lease.Ticket(), value);
        });
        return true;
    }
    requests.Discard(token);
    return false;
}

class ActorCallback final : public RE::BSScript::IStackCallbackFunctor {
public:
    ActorCallback(std::string token, RE::ActorHandle handle, Kind kind, Completion completion,
        bool booleanResult = false) :
        token_(std::move(token)), handle_(handle), kind_(kind), completion_(std::move(completion)),
        booleanResult_(booleanResult) {}
    ~ActorCallback() override
    {
        if (kind_ == Kind::position) CompletePhysicsOperation(token_);
        if (!transferred_.load()) requests.Discard(token_);
    }
    void operator()(RE::BSScript::Variable result) override
    {
        if (called_.exchange(true)) return;
        if (kind_ == Kind::position) CompletePhysicsOperation(token_);
        const float value = booleanResult_ ?
            (result.IsBool() && result.GetBool() ? 1.0F : 0.0F) :
            (result.IsFloat() ? result.GetFloat() :
                (result.IsInt() ? static_cast<float>(result.GetSInt()) : -1.0F));
        transferred_.store(QueueResult(token_, handle_, value, std::move(completion_)));
    }
    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
private:
    std::string token_;
    RE::ActorHandle handle_;
    Kind kind_;
    Completion completion_;
    bool booleanResult_;
    std::atomic<bool> called_{ false };
    std::atomic<bool> transferred_{ false };
};

bool TargetReady(const Controllers::ActorWorkLease& lease, RE::Actor* actor,
    std::int64_t allowedAfterMs, bool allowPlayer = false)
{
    return lease.Valid() && AvailableActor(actor, allowPlayer) && actor->GetFormID() == lease.Ticket().formID &&
        PapyrusReady(allowedAfterMs, NowMs());
}

// Accept only the SOS events used by the position adapters, not arbitrary
// animation events passed through this internal bridge.
bool PositionEvent(const std::string& event)
{
    if (event.empty() || event == "SOSFlaccid") return true;
    for (int bend = -9; bend <= 9; ++bend) {
        if (event == "SOSBend" + std::to_string(bend)) return true;
    }
    return false;
}

struct SceneSources {
    bool osl;
    bool sexLab;
    bool ostim;
};

bool NativeSceneQueryAvailable(const char* script, const char* function)
{
    // Check the linked VM function, not just a DLL/file name. An unbound Bool
    // native could otherwise look like false and incorrectly permit a write.
    auto* vm = VM();
    RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> type;
    if (!vm || !vm->GetScriptObjectType(RE::BSFixedString(script), type) || !type ||
        type->linkedValid != RE::BSScript::ObjectTypeInfo::LinkValidState::kLinkedValid) return false;
    const auto* functions = type->GetGlobalFuncIter();
    if (!functions) return false;
    for (std::uint32_t i = 0; i < type->GetNumGlobalFuncs(); ++i) {
        const auto& candidate = functions[i].func;
        if (!candidate || candidate->GetName() != function) continue;
        if (!candidate->GetIsNative() || !candidate->GetIsStatic() ||
            !candidate->GetReturnType().IsBool() || candidate->GetParamCount() != 1) return false;
        RE::BSFixedString name;
        RE::BSScript::TypeInfo parameter;
        candidate->GetParam(0, name, parameter);
        if (!parameter.IsObject() || !parameter.GetTypeInfo() ||
            RE::BSFixedString(parameter.GetTypeInfo()->GetName()) != "Actor") return false;
        // Linked native functions derive from NativeFunctionBase; script
        // functions report GetIsNative=false. HasStub verifies the callable.
        return static_cast<RE::BSScript::NF_util::NativeFunctionBase*>(candidate.get())->HasStub();
    }
    return false;
}

std::optional<SceneSources> AvailableSceneSources(std::int64_t allowedAfterMs)
{
    // Provider presence and provider readiness are different. A present but
    // missing bridge/DLL must block background writes, not count as no scene.
    SceneSources sources{ OslArousedLoaded() || PluginLoaded({ "OSLAroused.esp", "OAroused.esp" }),
        PluginLoaded({ "SexLab.esm" }) || ModuleLoaded(L"SexLabUtil.dll"),
        PluginLoaded({ "OStim.esp" }) || ModuleLoaded(L"OStim.dll") };
    if (!BridgeReady("SPS_PositionBridge", positionVersion, allowedAfterMs)) return std::nullopt;
    if (sources.osl && (!OslArousedLoaded() ||
        !NativeSceneQueryAvailable("OSLArousedNative", "IsInScene"))) return std::nullopt;
    if (sources.sexLab && (!ModuleLoaded(L"SexLabUtil.dll") ||
        !std::filesystem::exists("Data/Scripts/SexLabFramework.pex"))) return std::nullopt;
    if (sources.ostim && (!ModuleLoaded(L"OStim.dll") ||
        !std::filesystem::exists("Data/Scripts/OActor.pex") ||
        !std::filesystem::exists("Data/Scripts/SPS_OStimBridge.pex") ||
        !NativeSceneQueryAvailable("OActor", "IsInOStim") ||
        !BridgeReady("SPS_OStimBridge", ostimVersion, allowedAfterMs))) return std::nullopt;
    return sources;
}
}

bool RegisterActorBridgeGuards(RE::BSScript::IVirtualMachine* vm)
{
    if (!vm) return false;
    vm->RegisterFunction("ActorRequestCurrent", "SPS_ArousalBridge", ArousalRequestCurrent);
    vm->RegisterFunction("ArousalWriteRequestCurrent", "SPS_ArousalBridge", ArousalWriteRequestCurrent);
    vm->RegisterFunction("ActorRequestCurrent", "SPS_PositionBridge", PositionRequestCurrent);
    vm->RegisterFunction("ResolveActorRequest", "SPS_ArousalBridge", ResolveArousalRequest);
    vm->RegisterFunction("ResolveArousalWriteRequest", "SPS_ArousalBridge", ResolveArousalWriteRequest);
    vm->RegisterFunction("ResolveActorRequest", "SPS_PositionBridge", ResolvePositionRequest);
    vm->RegisterFunction("SceneRequestCurrent", "SPS_PositionBridge", SceneRequestCurrent);
    vm->RegisterFunction("ResolveSceneRequest", "SPS_PositionBridge", ResolveSceneRequest);
    return true;
}

bool QueryActorArousal(Controllers::ActorWorkLease lease, RE::Actor* actor,
    std::int64_t allowedAfterMs, ActorArousalCompletion completion)
{
    if (!completion || !TargetReady(lease, actor, allowedAfterMs)) return false;
    std::optional<float> reading;
    bool papyrus = false;
    if (SloArousedLoaded() || ClassicArousedLoaded()) {
        auto* faction = RE::TESForm::LookupByEditorID<RE::TESFaction>("sla_Arousal");
        if (faction && actor->IsInFaction(faction)) {
            reading = Core::ValidArousalReading(static_cast<float>(
                actor->GetFactionRank(faction, false)));
        }
    } else if (OslArousedLoaded()) {
        reading = ReadOslArousal(actor);
        papyrus = !reading.has_value();
        if (papyrus && !BridgeReady("SPS_ArousalBridge", arousalVersion, allowedAfterMs)) return false;
    } else {
        return false;
    }

    const auto now = NowMs();
    const auto handle = actor->GetHandle();
    const auto token = CreatePhysicsOperationToken();
    if (!requests.Prepare(token, { lease, handle.native_handle(), Kind::arousal, now + 10000, false, reinterpret_cast<std::uintptr_t>(actor->Get3D()) }, now)) return false;
    Completion deliver = [completion = std::move(completion)](auto ticket, float value) {
        completion(ticket, Core::ValidArousalReading(value));
    };
    if (!papyrus) return QueueResult(token, handle, reading.value_or(-1.0F), std::move(deliver));

    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{
        new ActorCallback(token, handle, Kind::arousal, std::move(deliver))
    };
    if (DispatchStaticWithCallback("SPS_ArousalBridge", "GetActorArousal", allowedAfterMs, now,
            std::move(callback), RE::BSFixedString(token))) return true;
    requests.Discard(token);
    return false;
}

bool QueueActorArousalWrite(Controllers::ActorWorkLease lease, RE::Actor* actor,
    float value, std::int64_t allowedAfterMs, ActorArousalWriteCompletion completion)
{
    const auto requested = Core::ValidArousalReading(value);
    const bool player = actor && actor->GetFormID() == SPS::API::kPlayerFormID;
    if (!completion || !requested || !OslArousedLoaded() ||
        !TargetReady(lease, actor, allowedAfterMs, player) ||
        !BridgeReady("SPS_ArousalBridge", arousalVersion, allowedAfterMs)) return false;

    const auto now = NowMs();
    const auto handle = actor->GetHandle();
    const auto token = CreatePhysicsOperationToken();
    if (!requests.Prepare(token, { lease, handle.native_handle(), Kind::arousalWrite,
            now + 10000, false, reinterpret_cast<std::uintptr_t>(actor->Get3D()) }, now)) return false;
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{
        new ActorCallback(token, handle, Kind::arousalWrite,
            [completion = std::move(completion)](auto ticket, float result) {
                completion(ticket, Core::ValidArousalReading(result));
            })
    };
    if (DispatchStaticWithCallback("SPS_ArousalBridge", "SetActorArousal", allowedAfterMs, now,
            std::move(callback), RE::BSFixedString(token), *requested)) return true;
    requests.Discard(token);
    return false;
}

bool QueueActorPosition(Controllers::ActorWorkLease lease, RE::Actor* actor,
    std::string eventName, std::optional<int> nativeBend,
    std::int64_t allowedAfterMs, ActorPositionCompletion completion)
{
    // nativeBend is intentionally ignored: SOS AE 4.0.0 removed the native angle
    // call, and SPS now owns the angle through SosPoseAngle. The parameter is
    // kept so existing callers (basic and managed NPC paths) compile unchanged
    // and so the legacy request shape stays visible in the call sites.
    (void)nativeBend;
    if (!completion || !TargetReady(lease, actor, allowedAfterMs) || !PositionEvent(eventName) ||
        eventName.empty() ||
        !PositionBackendAvailable() ||
        !OrderedFsmpBridgeAvailable() ||
        !BridgeReady("SPS_PositionBridge", positionVersion, allowedAfterMs)) return false;
    const auto now = NowMs();
    const auto handle = actor->GetHandle();
    const auto token = CreatePhysicsOperationToken();
    if (!requests.Prepare(token, { lease, handle.native_handle(), Kind::position, now + 10000, false, reinterpret_cast<std::uintptr_t>(actor->Get3D()) }, now)) return false;
    if (!PreparePhysicsOperation(token)) {
        requests.Discard(token);
        return false;
    }
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{
        new ActorCallback(token, handle, Kind::position,
            [completion = std::move(completion)](auto ticket, float value) { completion(ticket, value == 1.0F); }, true)
    };
    if (DispatchStaticWithCallback("SPS_PositionBridge", "ApplyActorPosition", allowedAfterMs, now,
            std::move(callback), RE::BSFixedString(token), RE::BSFixedString(eventName),
            false)) return true;
    requests.Discard(token);
    CompletePhysicsOperation(token);
    return false;
}

bool QueryActorScene(Controllers::ActorWorkLease lease, RE::Actor* actor,
    std::int64_t allowedAfterMs, ActorSceneCompletion completion)
{
    if (!completion || !TargetReady(lease, actor, allowedAfterMs)) return false;
    const auto sources = AvailableSceneSources(allowedAfterMs);
    if (!sources) return false;
    const auto now = NowMs();
    const auto handle = actor->GetHandle();
    const auto token = CreatePhysicsOperationToken();
    if (!requests.Prepare(token, { lease, handle.native_handle(), Kind::scene, now + 10000, false, reinterpret_cast<std::uintptr_t>(actor->Get3D()) }, now)) return false;
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{
        new ActorCallback(token, handle, Kind::scene,
            [completion = std::move(completion)](auto ticket, float value) {
                using Status = Controllers::ActorSceneStatus;
                completion(ticket, value == 1.0F ? Status::clear : value == 2.0F ? Status::active : Status::unknown);
            })
    };
    if (DispatchStaticWithCallback("SPS_PositionBridge", "GetActorSceneState", allowedAfterMs, now,
            std::move(callback), RE::BSFixedString(token), sources->osl, sources->sexLab, sources->ostim)) return true;
    requests.Discard(token);
    return false;
}

bool QueryActorSceneRole(Controllers::ActorWorkLease lease, RE::Actor* actor,
    std::int64_t allowedAfterMs, ActorSceneRoleCompletion completion)
{
    if (!completion || !TargetReady(lease, actor, allowedAfterMs)) return false;
    const auto sources = AvailableSceneSources(allowedAfterMs);
    if (!sources) return false;
    // Only SexLab and OStim describe a role; OSL has no such concept.
    if (!sources->sexLab && !sources->ostim) return false;
    const auto now = NowMs();
    const auto handle = actor->GetHandle();
    const auto token = CreatePhysicsOperationToken();
    if (!requests.Prepare(token, { lease, handle.native_handle(), Kind::scene, now + 10000, false, reinterpret_cast<std::uintptr_t>(actor->Get3D()) }, now)) return false;
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{
        new ActorCallback(token, handle, Kind::scene,
            [completion = std::move(completion)](auto ticket, float value) {
                // -1 is a stale or failed bridge call and must not read as "no role".
                completion(ticket, value == 1.0F ? 1 : value == 2.0F ? 2 : value == 0.0F ? 0 : -1);
            })
    };
    if (DispatchStaticWithCallback("SPS_PositionBridge", "GetActorSceneRole", allowedAfterMs, now,
            std::move(callback), RE::BSFixedString(token), sources->sexLab, sources->ostim)) return true;
    requests.Discard(token);
    return false;
}

bool QueueBasicActorPosition(Controllers::ActorWorkLease lease, RE::Actor* actor,
    std::string eventName, std::int64_t allowedAfterMs, BasicPositionCompletion completion)
{
    if (!completion || !TargetReady(lease, actor, allowedAfterMs) || eventName.empty() ||
        !PositionEvent(eventName) || !PositionBackendAvailable() || !OrderedFsmpBridgeAvailable()) return false;
    const auto sources = AvailableSceneSources(allowedAfterMs);
    if (!sources) return false;
    const auto now = NowMs();
    const auto handle = actor->GetHandle();
    const auto token = CreatePhysicsOperationToken();
    if (!requests.Prepare(token, { lease, handle.native_handle(), Kind::position, now + 10000, false, reinterpret_cast<std::uintptr_t>(actor->Get3D()) }, now)) return false;
    if (!PreparePhysicsOperation(token)) {
        requests.Discard(token);
        return false;
    }
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{
        new ActorCallback(token, handle, Kind::position,
            [completion = std::move(completion)](auto ticket, float value) {
                using Result = Controllers::BasicPositionResult;
                completion(ticket, value == 1.0F ? Result::applied : value == 0.0F ? Result::deferred : Result::failed);
            })
    };
    if (DispatchStaticWithCallback("SPS_PositionBridge", "ApplyBasicActorPosition", allowedAfterMs, now,
            std::move(callback), RE::BSFixedString(token), RE::BSFixedString(eventName),
            sources->osl, sources->sexLab, sources->ostim)) return true;
    requests.Discard(token);
    CompletePhysicsOperation(token);
    return false;
}

void CancelActorBridgeRequests(Controllers::ActorWorkTicket ticket)
{
    for (const auto& token : requests.Cancel(ticket)) CancelPhysicsOperation(token);
}

void ResetActorBridgeRequests()
{
    requests.ResetSession();
    arousalVersion.Reset();
    positionVersion.Reset();
    ostimVersion.Reset();
}

}
