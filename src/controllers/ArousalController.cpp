#include "ArousalController.h"
#include "PhysicsDecision.h"

#include "runtime/Compatibility.h"
#include "runtime/PapyrusGateway.h"

#include <SKSE/SKSE.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>

namespace logger = SKSE::log;

namespace SPS::Controllers {

namespace {

std::int64_t NowMs()
{
    return Runtime::NowMs();
}

class ArousalCallback final : public RE::BSScript::IStackCallbackFunctor {
public:
    ArousalCallback(ArousalController& owner, std::uint64_t generation) :
        owner_(owner), generation_(generation) {}

    void operator()(RE::BSScript::Variable result) override
    {
        const float value = result.IsFloat() ? result.GetFloat() :
            (result.IsInt() ? static_cast<float>(result.GetSInt()) : -1.0F);
        if (auto* tasks = SKSE::GetTaskInterface()) {
            tasks->AddTask([owner = &owner_, generation = generation_, value] {
                owner->AcceptResult(generation, value);
            });
        }
    }

    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

private:
    ArousalController& owner_;
    std::uint64_t generation_;
};

}

void ArousalController::Configure(
    ReadyHook papyrusReady,
    RecordHook record,
    ActionHook clearError,
    ActionHook evaluate)
{
    papyrusReady_ = std::move(papyrusReady);
    record_ = std::move(record);
    clearError_ = std::move(clearError);
    evaluate_ = std::move(evaluate);
}

void ArousalController::Query(bool force)
{
    const auto now = NowMs();
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player || Runtime::JournalMenuOpen()) {
        return;
    }

    if (Runtime::SloArousedLoaded() || Runtime::ClassicArousedLoaded()) {
        generation_.fetch_add(1);
        queryPending_.store(false);
        retryAfterMs_.store(0);
        nextQueryMs_.store(0);
        auto* faction = RE::TESForm::LookupByEditorID<RE::TESFaction>("sla_Arousal");
        if (!faction) {
            valid_.store(false);
            connected_.store(false);
            Record("SPS-002: The arousal faction was not found", true);
            return;
        }
        const float reading = static_cast<float>(
            std::clamp(player->GetFactionRank(faction, true), 0, 100));
        const auto previous = value_.exchange(reading);
        const auto wasValid = valid_.exchange(true);
        connected_.store(true);
        if (clearError_) {
            clearError_();
        }
        const bool changed = !wasValid || std::abs(previous - reading) >= 0.5F;
        if (changed) {
            logger::info("{} arousal: {:.1f}", Runtime::ArousalProviderName(), reading);
            QueueEvaluation();
        }
        return;
    }

    if (!Runtime::OslArousedLoaded()) {
        queryPending_.store(false);
        valid_.store(false);
        connected_.store(false);
        nextQueryMs_.store(0);
        return;
    }
    if (!force && now < nextQueryMs_.load()) {
        return;
    }
    if (now < retryAfterMs_.load()) {
        return;
    }
    if (const auto reading = Runtime::ReadOslArousal(player)) {
        AcceptExternalReading(*reading);
        return;
    }
    if (queryPending_.load()) {
        if (now - queryStartedMs_.load() < 10000) {
            return;
        }
        generation_.fetch_add(1);
        queryPending_.store(false);
        const bool haveLastReading = valid_.load();
        retryAfterMs_.store(now + 3000);
        Record(haveLastReading ?
            "Arousal response was delayed; keeping the last reading and trying again" :
            "Arousal response is still loading; SPS will try again",
            false);
        return;
    }
    if (!papyrusReady_ || !papyrusReady_()) {
        retryAfterMs_.store(now + 1000);
        return;
    }
    if (queryPending_.exchange(true)) {
        return;
    }
    queryStartedMs_.store(now);
    nextQueryMs_.store(now + 1000);
    auto* vm = Runtime::VM();
    if (!vm) {
        queryPending_.store(false);
        return;
    }
    const auto generation = generation_.fetch_add(1) + 1;
    const auto dispatch = [&] {
        auto* args = RE::MakeFunctionArguments();
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{
            new ArousalCallback(*this, generation)
        };
        return vm->DispatchStaticCall(
            "SPS_ArousalBridge", "GetPlayerArousal", args, callback);
    };
    if (!dispatch()) {
        queryPending_.store(false);
        const bool haveLastReading = valid_.load();
        connected_.store(false);
        retryAfterMs_.store(now + 3000);
        Record(haveLastReading ?
            "Arousal provider is temporarily unavailable; keeping the last reading" :
            "SPS-002: Could not connect to an arousal provider",
            !haveLastReading);
    }
}

void ArousalController::Invalidate()
{
    generation_.fetch_add(1);
    queryPending_.store(false);
    retryAfterMs_.store(0);
    nextQueryMs_.store(0);
}

void ArousalController::ResetReading()
{
    valid_.store(false);
}

float ArousalController::Value() const
{
    return value_.load();
}

bool ArousalController::Valid() const
{
    return valid_.load();
}

bool ArousalController::Connected() const
{
    return connected_.load();
}

void ArousalController::AcceptResult(std::uint64_t generation, float result)
{
    if (generation != generation_.load()) {
        return;
    }
    if (const auto reading = Core::ValidArousalReading(result)) {
        ApplyReading(*reading);
    } else {
        const bool haveLastReading = valid_.load();
        retryAfterMs_.store(NowMs() + 3000);
        Record(haveLastReading ?
            "Arousal provider returned a delayed or invalid response; keeping the last reading" :
            "SPS-002: Arousal provider returned an invalid value",
            !haveLastReading);
    }
    queryPending_.store(false);
}

void ArousalController::AcceptExternalReading(float reading)
{
    const auto validReading = Core::ValidArousalReading(reading);
    if (!validReading) return;
    generation_.fetch_add(1);
    queryPending_.store(false);
    ApplyReading(*validReading);
}

void ArousalController::ApplyReading(float reading)
{
    const auto previous = value_.exchange(reading);
    const auto wasValid = valid_.exchange(true);
    const bool changed = !wasValid || std::abs(previous - reading) >= 0.5F;
    connected_.store(true);
    retryAfterMs_.store(0);
    nextQueryMs_.store(NowMs() + (changed ? 1000 : 5000));
    if (clearError_) {
        clearError_();
    }
    if (changed) {
        logger::info("{} arousal: {:.1f}", Runtime::ArousalProviderName(), reading);
        QueueEvaluation();
    }
}

void ArousalController::QueueEvaluation()
{
    if (!evaluate_) {
        return;
    }
    if (auto* tasks = SKSE::GetTaskInterface()) {
        tasks->AddTask([this] {
            if (evaluate_) {
                evaluate_();
            }
        });
    }
}

void ArousalController::Record(std::string message, bool error) const
{
    if (record_) {
        record_(std::move(message), error);
    }
}

}
