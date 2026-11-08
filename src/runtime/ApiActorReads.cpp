#include "ApiActorReads.h"

#include "ActorBridgeAdapter.h"
#include "Compatibility.h"
#include "PapyrusGateway.h"
#include "PhysicsDecision.h"
#include "SPSAPI.h"
#include "controllers/ManagedActorRegistry.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace SPS::Runtime {

namespace {

using Controllers::ActorSceneStatus;
using Controllers::ActorWorkTicket;

constexpr std::size_t kProbeCapacity = 32;
constexpr std::int64_t kRefreshMs = 1000;
constexpr std::int64_t kStaleMs = 5000;      // an answer older than this is NotReady
constexpr std::int64_t kIdleDropMs = 30000;  // not asked about for this long: dropped
constexpr std::int64_t kInFlightMs = 12000;  // the request ledger expires at 10 s

RE::Actor* LiveActor(std::uint32_t formID)
{
    if (formID == 0 || formID == SPS::API::kPlayerFormID) return nullptr;
    auto* actor = RE::TESForm::LookupByID<RE::Actor>(formID);
    if (!actor || actor->IsDeleted() || actor->IsDisabled() || !actor->Is3DLoaded()) return nullptr;
    return actor;
}

struct Probe {
    ActorWorkTicket ticket{};
    ActorSceneStatus status{ ActorSceneStatus::unknown };
    std::int64_t sampledAt{ -1 };
    std::int64_t lastActiveAt{ -1 };
    std::int64_t lastCalledAt{ 0 };
    std::int64_t inFlight{ -1 };
};

// Reuses the registry only for its leases, which the scene bridge requires.
Controllers::ManagedActorRegistry registry{ kProbeCapacity };
std::unordered_map<std::uint32_t, Probe> probes;

void Drop(std::uint32_t formID)
{
    const auto found = probes.find(formID);
    if (found == probes.end()) return;
    CancelActorBridgeRequests(found->second.ticket);
    (void)registry.Remove(found->second.ticket);
    probes.erase(found);
}

void SweepIdle(std::int64_t now)
{
    std::vector<std::uint32_t> idle;
    for (const auto& [formID, probe] : probes)
        if (now - probe.lastCalledAt > kIdleDropMs) idle.push_back(formID);
    for (const auto formID : idle) Drop(formID);
}

Probe* Acquire(std::uint32_t formID, std::int64_t now)
{
    if (const auto found = probes.find(formID); found != probes.end()) {
        found->second.lastCalledAt = now;
        return &found->second;
    }
    if (probes.size() >= kProbeCapacity) {
        const auto oldest = std::min_element(probes.begin(), probes.end(),
            [](const auto& a, const auto& b) { return a.second.lastCalledAt < b.second.lastCalledAt; });
        if (oldest != probes.end()) Drop(oldest->first);
    }
    const auto added = registry.Add(formID);
    if (added.status != Controllers::ActorRegistrationStatus::added) return nullptr;
    const auto loaded = registry.SetLoaded(added.ticket, true);
    if (!loaded) {
        (void)registry.Remove(added.ticket);
        return nullptr;
    }
    Probe probe;
    probe.ticket = *loaded;
    probe.lastCalledAt = now;
    return &(probes[formID] = probe);
}

}

ApiActorRead ReadApiActorArousal(std::uint32_t formID, float* arousal)
{
    auto* actor = LiveActor(formID);
    if (!actor) return ApiActorRead::unsupported;
    std::optional<float> reading;
    if (SloArousedLoaded() || ClassicArousedLoaded()) {
        auto* faction = RE::TESForm::LookupByEditorID<RE::TESFaction>("sla_Arousal");
        if (faction && actor->IsInFaction(faction))
            reading = Core::ValidArousalReading(static_cast<float>(actor->GetFactionRank(faction, false)));
    } else if (OslArousedLoaded()) {
        if (const auto value = ReadOslArousal(actor)) reading = Core::ValidArousalReading(*value);
    }
    if (!reading) return ApiActorRead::notReady;
    *arousal = *reading;
    return ApiActorRead::ready;
}

ApiActorRead ReadApiActorScene(std::uint32_t formID, bool* inScene,
    std::int64_t sceneEndDelayMs, std::int64_t allowedAfterMs)
{
    const auto now = NowMs();
    auto* actor = LiveActor(formID);
    if (!actor) {
        Drop(formID);
        return ApiActorRead::unsupported;
    }
    SweepIdle(now);
    auto* probe = Acquire(formID, now);
    if (!probe) return ApiActorRead::notReady;

    if (probe->inFlight >= 0 && now - probe->inFlight > kInFlightMs) probe->inFlight = -1;
    if (probe->inFlight < 0 && (probe->sampledAt < 0 || now - probe->sampledAt >= kRefreshMs)) {
        probe->inFlight = now;
        const auto ticket = probe->ticket;
        const auto started = now;
        const bool queued = QueryActorScene(registry.Lease(ticket), actor, allowedAfterMs,
            [formID, ticket, started](ActorWorkTicket, ActorSceneStatus value) {
                const auto found = probes.find(formID);
                if (found == probes.end() || found->second.ticket.generation != ticket.generation ||
                    found->second.inFlight != started) return;
                auto& tracked = found->second;
                tracked.inFlight = -1;
                // An unknown result keeps the previous answer and is retried on
                // the next call; it never means "not in a scene".
                if (value == ActorSceneStatus::unknown) return;
                tracked.status = value;
                tracked.sampledAt = started;
                if (value == ActorSceneStatus::active) tracked.lastActiveAt = started;
            });
        if (!queued) probe->inFlight = -1;
    }

    if (probe->status == ActorSceneStatus::unknown || probe->sampledAt < 0 ||
        now - probe->sampledAt > kStaleMs) return ApiActorRead::notReady;
    const bool held = probe->lastActiveAt >= 0 && now - probe->lastActiveAt < sceneEndDelayMs;
    *inScene = probe->status == ActorSceneStatus::active || held;
    return ApiActorRead::ready;
}

void ResetApiActorReads()
{
    for (const auto& [formID, probe] : probes) CancelActorBridgeRequests(probe.ticket);
    probes.clear();
    registry.ResetSession();
}

}
