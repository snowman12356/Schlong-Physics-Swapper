#include "SceneController.h"

#include "runtime/Compatibility.h"
#include "runtime/PapyrusGateway.h"

#include <SKSE/SKSE.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <utility>

namespace logger = SKSE::log;

namespace SPS::Controllers {

namespace {

std::int64_t NowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

const char* RoleName(int role)
{
    switch (role) {
    case 1:
        return "Receiving / bottom";
    case 2:
        return "Penetrating / top";
    default:
        return "Not identified";
    }
}

class SexLabCallback final : public RE::BSScript::IStackCallbackFunctor {
public:
    SexLabCallback(SceneController& owner, std::uint64_t generation) :
        owner_(owner), generation_(generation) {}
    void operator()(RE::BSScript::Variable result) override
    {
        owner_.AcceptSexLabResult(generation_, result);
    }
    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

private:
    SceneController& owner_;
    std::uint64_t generation_;
};

class SexLabRoleCallback final : public RE::BSScript::IStackCallbackFunctor {
public:
    SexLabRoleCallback(SceneController& owner, std::uint64_t generation) :
        owner_(owner), generation_(generation) {}
    void operator()(RE::BSScript::Variable result) override
    {
        owner_.AcceptSexLabRoleResult(generation_, result);
    }
    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

private:
    SceneController& owner_;
    std::uint64_t generation_;
};

class OStimRoleCallback final : public RE::BSScript::IStackCallbackFunctor {
public:
    OStimRoleCallback(SceneController& owner, std::uint64_t generation) :
        owner_(owner), generation_(generation) {}
    void operator()(RE::BSScript::Variable result) override
    {
        owner_.AcceptOStimRoleResult(generation_, result);
    }
    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

private:
    SceneController& owner_;
    std::uint64_t generation_;
};

}

void SceneController::Configure(
    ReadyHook papyrusReady,
    RecordHook record,
    ActionHook evaluate,
    OwnerHook owner,
    SexLabTransitionHook sexLabTransition)
{
    papyrusReady_ = std::move(papyrusReady);
    record_ = std::move(record);
    evaluate_ = std::move(evaluate);
    owner_ = std::move(owner);
    sexLabTransition_ = std::move(sexLabTransition);
}

void SceneController::QuerySexLab()
{
    if (!Runtime::ModuleLoaded(L"SexLabUtil.dll") ||
        !std::filesystem::exists("Data/Scripts/SPS_SexLabBridge.pex")) {
        state_.sexLab.valid.store(false);
        state_.sexLab.connected.store(false);
        return;
    }
    const auto now = NowMs();
    if (now < state_.sexLab.queryRetryAfterMs.load()) {
        return;
    }
    if (state_.sexLab.queryPending.load()) {
        if (now - state_.sexLab.queryStartedMs.load() < 5000) {
            return;
        }
        state_.sexLab.queryGeneration.fetch_add(1);
        state_.sexLab.queryPending.store(false);
        state_.sexLab.valid.store(false);
        state_.sexLab.queryRetryAfterMs.store(now + 5000);
        return;
    }
    if (!papyrusReady_ || !papyrusReady_()) {
        state_.sexLab.queryRetryAfterMs.store(now + 1000);
        return;
    }
    if (state_.sexLab.queryPending.exchange(true)) {
        return;
    }
    state_.sexLab.queryStartedMs.store(now);
    auto* vm = Runtime::VM();
    if (!vm) {
        state_.sexLab.queryPending.store(false);
        return;
    }
    const auto generation = state_.sexLab.queryGeneration.fetch_add(1) + 1;
    auto* args = RE::MakeFunctionArguments();
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{
        new SexLabCallback(*this, generation)
    };
    if (!vm->DispatchStaticCall("SPS_SexLabBridge", "IsPlayerActive", args, callback)) {
        state_.sexLab.queryPending.store(false);
        state_.sexLab.valid.store(false);
        state_.sexLab.connected.store(false);
        state_.sexLab.queryRetryAfterMs.store(now + 5000);
    }
}

void SceneController::QuerySexLabRole()
{
    if (!state_.sexLab.active.load() ||
        !std::filesystem::exists("Data/Scripts/SPS_SexLabBridge.pex")) {
        state_.sexLab.roleValid.store(false);
        state_.sexLab.roleQueryPending.store(false);
        return;
    }
    const auto now = NowMs();
    if (now < state_.sexLab.roleRetryAfterMs.load()) {
        return;
    }
    if (state_.sexLab.roleQueryPending.load()) {
        if (now - state_.sexLab.roleQueryStartedMs.load() < 3000) {
            return;
        }
        state_.sexLab.roleGeneration.fetch_add(1);
        state_.sexLab.roleQueryPending.store(false);
        state_.sexLab.roleValid.store(false);
        state_.sexLab.roleRetryAfterMs.store(now + 3000);
        return;
    }
    if (!papyrusReady_ || !papyrusReady_()) {
        state_.sexLab.roleRetryAfterMs.store(now + 1000);
        return;
    }
    if (state_.sexLab.roleQueryPending.exchange(true)) {
        return;
    }
    state_.sexLab.roleQueryStartedMs.store(now);
    auto* vm = Runtime::VM();
    if (!vm) {
        state_.sexLab.roleQueryPending.store(false);
        return;
    }
    const auto generation = state_.sexLab.roleGeneration.load();
    auto* args = RE::MakeFunctionArguments();
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{
        new SexLabRoleCallback(*this, generation)
    };
    if (!vm->DispatchStaticCall("SPS_SexLabBridge", "GetPlayerRole", args, callback)) {
        state_.sexLab.roleQueryPending.store(false);
        state_.sexLab.roleValid.store(false);
        state_.sexLab.roleRetryAfterMs.store(now + 3000);
    }
}

void SceneController::QueryOStimRole()
{
    if (!state_.ostim.active.load() ||
        !std::filesystem::exists("Data/Scripts/SPS_OStimBridge.pex")) {
        state_.ostim.roleValid.store(false);
        state_.ostim.roleQueryPending.store(false);
        return;
    }
    const auto now = NowMs();
    if (now < state_.ostim.roleRetryAfterMs.load()) {
        return;
    }
    if (state_.ostim.roleQueryPending.load()) {
        if (now - state_.ostim.roleQueryStartedMs.load() < 3000) {
            return;
        }
        state_.ostim.roleGeneration.fetch_add(1);
        state_.ostim.roleQueryPending.store(false);
        state_.ostim.roleValid.store(false);
        state_.ostim.roleRetryAfterMs.store(now + 3000);
        return;
    }
    if (!papyrusReady_ || !papyrusReady_()) {
        state_.ostim.roleRetryAfterMs.store(now + 1000);
        return;
    }
    if (state_.ostim.roleQueryPending.exchange(true)) {
        return;
    }
    state_.ostim.roleQueryStartedMs.store(now);
    auto* vm = Runtime::VM();
    if (!vm) {
        state_.ostim.roleQueryPending.store(false);
        return;
    }
    const auto generation = state_.ostim.roleGeneration.load();
    auto* args = RE::MakeFunctionArguments();
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{
        new OStimRoleCallback(*this, generation)
    };
    if (!vm->DispatchStaticCall("SPS_OStimBridge", "GetPlayerRole", args, callback)) {
        state_.ostim.roleQueryPending.store(false);
        state_.ostim.roleValid.store(false);
        state_.ostim.connected.store(false);
        state_.ostim.roleRetryAfterMs.store(now + 3000);
    }
}

void SceneController::InvalidateQueries()
{
    state_.sexLab.queryGeneration.fetch_add(1);
    state_.sexLab.roleGeneration.fetch_add(1);
    state_.ostim.roleGeneration.fetch_add(1);
    state_.sexLab.queryPending.store(false);
    state_.sexLab.roleQueryPending.store(false);
    state_.ostim.roleQueryPending.store(false);
    state_.sexLab.queryRetryAfterMs.store(0);
    state_.sexLab.roleRetryAfterMs.store(0);
    state_.ostim.roleRetryAfterMs.store(0);
}

void SceneController::ResetPPA(int ignoreMs)
{
    state_.ppa.sceneActive.store(false);
    state_.ppa.sceneRole.store(0);
    state_.ppa.sceneRoleValid.store(false);
    state_.ppa.lastUpdateMs.store(0);
    state_.ppa.lastTopMs.store(0);
    state_.ppa.bottomCandidateSinceMs.store(0);
    state_.ppa.ignoreUntilMs.store(ignoreMs > 0 ? NowMs() + ignoreMs : 0);
}

SceneState& SceneController::State()
{
    return state_;
}

const SceneState& SceneController::State() const
{
    return state_;
}

void SceneController::AcceptSexLabResult(std::uint64_t generation, RE::BSScript::Variable result)
{
    if (generation != state_.sexLab.queryGeneration.load()) {
        return;
    }
    if (result.IsBool()) {
        const bool active = result.GetBool();
        const bool previous = state_.sexLab.active.exchange(active);
        state_.sexLab.valid.store(true);
        state_.sexLab.connected.store(true);
        const auto now = NowMs();
        if (previous && !active) {
            state_.sexLab.endedMs.store(now);
            state_.sexLab.entryStateValid.store(false);
            state_.sexLab.roleGeneration.fetch_add(1);
            state_.sexLab.role.store(0);
            state_.sexLab.roleValid.store(false);
            state_.sexLab.roleQueryPending.store(false);
            state_.sexLab.lastTopMs.store(0);
            state_.sexLab.bottomCandidateSinceMs.store(0);
            ResetPPA(2000);
            Record("SexLab scene ended; post-scene hold started");
        } else if (!previous && active) {
            const auto owner = owner_ ? owner_() : OwnerSnapshot{};
            state_.sexLab.entryCBPC.store(owner.usingCBPC);
            state_.sexLab.entryStateValid.store(owner.known);
            // P+ can report the actor active just before its native collision
            // thread has registered. Querying the role in that narrow window
            // produces a noisy "Thread instance not found" Papyrus stack.
            state_.sexLab.roleRetryAfterMs.store(now + 500);
            state_.sexLab.lastTopMs.store(0);
            state_.sexLab.bottomCandidateSinceMs.store(0);
            Record(std::string("SexLab scene detected; receiving state locked as ") +
                (owner.usingCBPC ? "hard (CBPC)" : "flaccid (SMP)"));
        }
        if (previous != active && sexLabTransition_) {
            sexLabTransition_(previous, active, now);
        }
    } else {
        state_.sexLab.valid.store(false);
        state_.sexLab.queryRetryAfterMs.store(NowMs() + 5000);
    }
    state_.sexLab.queryPending.store(false);
    QueueEvaluation(state_.sexLab.active.load());
}

void SceneController::AcceptSexLabRoleResult(std::uint64_t generation, RE::BSScript::Variable result)
{
    if (generation != state_.sexLab.roleGeneration.load()) {
        return;
    }
    bool valid = false;
    int role = 0;
    if (result.IsInt()) {
        role = std::clamp(result.GetSInt(), 0, 2);
        valid = true;
    }
    const auto now = NowMs();
    if (valid && role == 2) {
        state_.sexLab.lastTopMs.store(now);
        state_.sexLab.bottomCandidateSinceMs.store(0);
    } else if (valid && role == 1 && state_.sexLab.roleValid.load() &&
        state_.sexLab.role.load() == 2) {
        const auto candidateSince = state_.sexLab.bottomCandidateSinceMs.load();
        if (candidateSince == 0) {
            state_.sexLab.bottomCandidateSinceMs.store(now);
            state_.sexLab.roleQueryPending.store(false);
            return;
        }
        if (now - candidateSince < 1250 || now - state_.sexLab.lastTopMs.load() < 1250) {
            state_.sexLab.roleQueryPending.store(false);
            return;
        }
    } else if (!valid || role != 1) {
        state_.sexLab.bottomCandidateSinceMs.store(0);
    }
    const int previous = state_.sexLab.role.exchange(role);
    const bool wasValid = state_.sexLab.roleValid.exchange(valid);
    state_.sexLab.roleQueryPending.store(false);
    if (valid && (!wasValid || previous != role)) {
        Record(std::string("SexLab role: ") + RoleName(role));
    }
    if (!valid) {
        logger::warn("SexLab role bridge returned an invalid value");
    }
    QueueEvaluation();
}

void SceneController::AcceptOStimRoleResult(std::uint64_t generation, RE::BSScript::Variable result)
{
    if (generation != state_.ostim.roleGeneration.load()) {
        return;
    }
    bool valid = false;
    int role = 0;
    if (result.IsInt()) {
        role = std::clamp(result.GetSInt(), 0, 2);
        valid = true;
    }
    const int previous = state_.ostim.role.exchange(role);
    const bool wasValid = state_.ostim.roleValid.exchange(valid);
    state_.ostim.roleQueryPending.store(false);
    if (valid) {
        state_.ostim.connected.store(true);
        state_.ostim.roleRetryAfterMs.store(0);
        if (!wasValid || previous != role) {
            Record(std::string("OStim role: ") + RoleName(role));
        }
    } else {
        state_.ostim.roleRetryAfterMs.store(NowMs() + 3000);
        logger::warn("OStim role bridge returned an invalid value");
    }
    QueueEvaluation();
}

void SceneController::QueueEvaluation(bool querySexLabRole)
{
    if (auto* tasks = SKSE::GetTaskInterface()) {
        tasks->AddTask([this, querySexLabRole] {
            if (querySexLabRole) {
                QuerySexLabRole();
            }
            if (evaluate_) {
                evaluate_();
            }
        });
    }
}

void SceneController::Record(std::string message, bool error) const
{
    if (record_) {
        record_(std::move(message), error);
    }
}

}
