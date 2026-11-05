#include "ManagedNpcRuntime.h"

#include "ActorBridgeAdapter.h"
#include "Compatibility.h"
#include "PapyrusGateway.h"
#include "PhysicsDecision.h"
#include "PhysicsOwnershipAdapter.h"
#include "PositionBackendAdapter.h"
#include "SPSAPI.h"
#include "SosPoseAngle.h"
#include "controllers/ManagedActorRegistry.h"
#include "controllers/PhysicsOwnershipController.h"

#include <SKSE/SKSE.h>
#include <fmt/format.h>
#include <algorithm>
#include <mutex>
#include <unordered_map>

namespace SPS::Runtime {
namespace {
using namespace Controllers;

ManagedActorRegistry registry{ kManagedNpcCapacity };

// Per-actor driver state. Keyed by ticket so removal and re-add cannot inherit
// a previous actor's pending ownership generation. The ownership controller is
// built per tick from the actor's own ActorContext, because it binds directly to
// that context's PhysicsOwnershipState.
struct Driver {
    std::optional<float> arousal;
    std::int64_t arousalAt{ -1 };
    ActorSceneStatus scene{ ActorSceneStatus::unknown };
    std::int64_t sceneAt{ -1 };
    std::int64_t nextPollMs{ 0 };
    // Last event the bridge accepted. Repeating it every tick would spam the
    // animation graph and is the main source of needless POS work.
    std::string appliedEvent;
    // Last bend written through the pose channel. -1 means nothing applied yet.
    int appliedBend{ -1 };
    std::uint64_t switched{ 0 }, applied{ 0 }, deferred{ 0 }, failed{ 0 }, blocked{ 0 };
    // Log throttling: changes are always logged; repeats of the same request or
    // the same blocked reason at most every kRepeatLogMs.
    ActorSceneStatus loggedScene{ ActorSceneStatus::unknown };
    bool sceneLogged{ false };
    int loggedRequestCBPC{ -1 };
    std::int64_t requestLoggedAt{ -1 };
    std::int64_t blockedLoggedAt{ -1 };
};
std::unordered_map<std::uint64_t, Driver> drivers;
constexpr std::int64_t kRepeatLogMs = 10000;
std::string lastLoggedState;

const char* SceneName(ActorSceneStatus scene)
{
    switch (scene) {
    case ActorSceneStatus::clear: return "no scene";
    case ActorSceneStatus::active: return "in a scene";
    default: return "scene unknown";
    }
}

std::mutex statusLock;
ManagedNpcStatus status;
std::vector<std::uint32_t> pendingAdds;
std::vector<std::uint32_t> pendingRemoves;
std::string lastRejection;
bool configuredEnabled{ false };

// At most one outstanding arousal and one scene query per driver.
struct Slot {
    std::int64_t startedAt{ -1 };
    [[nodiscard]] bool Begin(std::int64_t now) { if (startedAt >= 0) return false; startedAt = now; return true; }
    [[nodiscard]] bool Finish(std::int64_t started) { if (startedAt != started) return false; startedAt = -1; return true; }
};
std::unordered_map<std::uint64_t, Slot> arousalSlots;
std::unordered_map<std::uint64_t, Slot> sceneSlots;

std::uint64_t Key(ActorWorkTicket ticket) { return (ticket.generation << 32) ^ ticket.formID; }

// Nearby list for the menu. Game-thread only; refreshed at most once a second.
std::vector<ManagedNpcCandidate> nearbyCache;
std::int64_t nearbyScanAtMs{ 0 };
constexpr float kNearbyRange = 4096.0F;

// Crosshair "who is managed" hint: one notification when the crosshair moves
// onto a managed NPC, repeated for the same NPC at most every few seconds.
std::uint32_t lastCrosshairFormID{ 0 };
std::int64_t lastCrosshairToldMs{ 0 };
constexpr std::int64_t kCrosshairRepeatMs = 8000;

std::string ActorName(RE::Actor* actor)
{
    const char* name = actor ? actor->GetDisplayFullName() : nullptr;
    return name && name[0] != '\0' ? name : "Unnamed NPC";
}

// Verified vanilla native: Debug.Notification(string) global (vanilla-papyrus
// Debug.psc). Dispatched like the other SPS Papyrus calls; dropped silently
// while the VM is not ready, since it is only feedback.
void Notify(const std::string& text, std::int64_t allowedAfterMs)
{
    (void)DispatchStatic("Debug", "Notification", allowedAfterMs, NowMs(),
        RE::BSFixedString(text.c_str()));
}

std::string PhysicsLabel(ActorContext* context)
{
    if (!context) return "Not loaded";
    const auto ownership = PhysicsOwnershipController{ context->physics }.Read();
    if (ownership.pending) return "Switching";
    if (!ownership.known) return "Unknown";
    return ownership.usingCBPC ? "Erect (CBPC)" : "Soft (SMP)";
}

Driver& DriverFor(ActorWorkTicket ticket)
{
    auto& driver = drivers[Key(ticket)];
    // A fresh key starts from default state; the ownership state lives in the
    // actor context and is reset by the registry when 3D availability changes.
    return driver;
}

void DropDriver(ActorWorkTicket ticket)
{
    const auto key = Key(ticket);
    drivers.erase(key);
    arousalSlots.erase(key);
    sceneSlots.erase(key);
}

bool Eligible(RE::Actor* actor, RE::PlayerCharacter* player)
{
    if (!actor || !player || actor->GetFormID() == SPS::API::kPlayerFormID ||
        actor->IsChild() || actor->IsDead() || actor->IsDeleted() || actor->IsDisabled() ||
        !actor->Is3DLoaded()) return false;
    const auto* race = actor->GetRace();
    auto* root = actor->Get3D();
    if (!race || !race->HasKeywordString("ActorTypeNPC") || !root) return false;
    // Same positioning prerequisite as the basic bridge. Bones alone are not
    // evidence of an equipped SMP mesh, so this never claims SMP compatibility.
    return root->GetObjectByName(RE::BSFixedString("NPC Genitals01 [Gen01]")) != nullptr;
}

const char* RejectionReason(std::uint32_t formID)
{
    if (formID == 0 || formID == SPS::API::kPlayerFormID) return "That is not an NPC.";
    const auto actor = RE::TESForm::LookupByID<RE::Actor>(formID);
    if (!actor) return "No such actor.";
    if (actor->IsChild()) return "Children are not supported.";
    if (actor->IsDead()) return "That actor is dead.";
    if (actor->IsDeleted() || actor->IsDisabled()) return "That actor is deleted or disabled.";
    const auto* race = actor->GetRace();
    if (!race || !race->HasKeywordString("ActorTypeNPC")) return "That actor is not an NPC.";
    if (!actor->Is3DLoaded() || !actor->Get3D()) return "That NPC is not loaded; get closer first.";
    if (!actor->Get3D()->GetObjectByName(RE::BSFixedString("NPC Genitals01 [Gen01]")))
        return "That NPC has no SOS/TNG node, so SPS cannot position it.";
    return nullptr;
}

// Bounded: one pass over the high-process handles per second with cheap
// checks, then the node lookup only for the closest few. No world scan.
void ScanNearby(RE::PlayerCharacter* player, std::int64_t now)
{
    if (now < nearbyScanAtMs) return;
    nearbyScanAtMs = now + 1000;
    nearbyCache.clear();
    auto* lists = RE::ProcessLists::GetSingleton();
    if (!lists || !player) return;
    const auto origin = player->GetPosition();
    struct Found { RE::Actor* actor; float distance; };
    std::vector<Found> found;
    for (const auto& handle : lists->highActorHandles) {
        const auto actor = handle.get();
        if (!actor || actor.get() == player || !actor->Is3DLoaded() ||
            actor->IsDead() || actor->IsChild() || actor->IsDisabled()) continue;
        const auto* race = actor->GetRace();
        if (!race || !race->HasKeywordString("ActorTypeNPC")) continue;
        const float distance = origin.GetDistance(actor->GetPosition());
        if (distance <= kNearbyRange) found.push_back({ actor.get(), distance });
    }
    std::sort(found.begin(), found.end(),
        [](const Found& a, const Found& b) { return a.distance < b.distance; });
    if (found.size() > kManagedNpcNearbyLimit) found.resize(kManagedNpcNearbyLimit);
    for (const auto& item : found) {
        ManagedNpcCandidate candidate;
        candidate.formID = item.actor->GetFormID();
        candidate.name = ActorName(item.actor);
        candidate.distance = item.distance;
        candidate.managed = registry.Find(candidate.formID).has_value();
        if (!candidate.managed) {
            if (const auto* reason = RejectionReason(candidate.formID)) candidate.unavailable = reason;
        }
        nearbyCache.push_back(std::move(candidate));
    }
}

// Tells the player, in game, when the crosshair lands on a managed NPC.
void HintCrosshairTarget(std::int64_t allowedAfterMs, std::int64_t now)
{
    std::uint32_t target = 0;
    if (auto* data = RE::CrosshairPickData::GetSingleton()) {
        if (const auto ref = data->GetActiveTarget().get()) {
            if (const auto* npc = ref->As<RE::Actor>()) target = npc->GetFormID();
        }
    }
    const auto entry = target ? registry.Find(target) : std::nullopt;
    if (!entry) {
        if (target != lastCrosshairFormID) lastCrosshairFormID = 0;
        return;
    }
    if (target == lastCrosshairFormID && now - lastCrosshairToldMs < kCrosshairRepeatMs) return;
    lastCrosshairFormID = target;
    lastCrosshairToldMs = now;
    const auto actor = RE::TESForm::LookupByID<RE::Actor>(target);
    Notify(fmt::format("{}: managed by SPS - {}", ActorName(actor),
        PhysicsLabel(registry.Resolve(entry->ticket))), allowedAfterMs);
}

void Remove(ActorWorkTicket ticket)
{
    // Invalidate before cancelling tokens so a late callback cannot rebuild the
    // driver, and never force-release an executing stack: ownership cancellation
    // keeps the gate lease until the real completion.
    (void)registry.Remove(ticket);
    CancelActorBridgeRequests(ticket);
    CancelActorOwnership(ticket);
    DropDriver(ticket);
}

void ApplyQueuedMembership(std::int64_t allowedAfterMs)
{
    std::vector<std::uint32_t> adds, removes;
    std::string rejection;
    {
        std::scoped_lock lock(statusLock);
        adds.swap(pendingAdds);
        removes.swap(pendingRemoves);
        rejection.swap(lastRejection);
    }
    if (!adds.empty() || !removes.empty()) nearbyScanAtMs = 0;  // Refresh the list now.
    for (const auto formID : removes) {
        const auto found = registry.Find(formID);
        if (found) {
            Remove(found->ticket);
            SKSE::log::info("Managed NPC {} ({:08X}) removed",
                ActorName(RE::TESForm::LookupByID<RE::Actor>(formID)), formID);
            Notify(fmt::format("SPS no longer manages {}",
                ActorName(RE::TESForm::LookupByID<RE::Actor>(formID))), allowedAfterMs);
        } else rejection = "That NPC is not managed.";
    }
    for (const auto formID : adds) {
        const auto* reason = RejectionReason(formID);
        if (!reason) {
            const auto added = registry.Add(formID);
            switch (added.status) {
            case ActorRegistrationStatus::invalidActor: reason = "That NPC cannot be managed."; break;
            case ActorRegistrationStatus::alreadyRegistered: reason = "That NPC is already managed."; break;
            case ActorRegistrationStatus::capacityReached:
                reason = "The managed NPC limit is reached. Remove one first."; break;
            case ActorRegistrationStatus::generationExhausted: reason = "SPS could not create a new session for that NPC."; break;
            case ActorRegistrationStatus::added: break;
            }
        }
        const auto name = ActorName(RE::TESForm::LookupByID<RE::Actor>(formID));
        if (reason) {
            rejection = reason;
            SKSE::log::warn("Managed NPC {} ({:08X}) refused: {}", name, formID, reason);
            Notify(fmt::format("SPS cannot manage {}: {}", name, reason), allowedAfterMs);
        } else {
            SKSE::log::info("Managed NPC {} ({:08X}) added", name, formID);
            Notify(fmt::format("SPS now manages {}", name), allowedAfterMs);
        }
    }
    std::scoped_lock lock(statusLock);
    lastRejection = std::move(rejection);
}

void Publish(const char* state)
{
    if (lastLoggedState != state) {
        SKSE::log::info("Managed NPC runtime: {}", state);
        lastLoggedState = state;
    }
    ManagedNpcStatus next;
    next.state = state;
    next.capacity = kManagedNpcCapacity;
    next.rejection = lastRejection;
    next.nearby = nearbyCache;
    for (const auto& snapshot : registry.Snapshot()) {
        ManagedNpcActorStatus actor;
        actor.formID = snapshot.ticket.formID;
        actor.loaded = snapshot.loaded;
        actor.name = ActorName(RE::TESForm::LookupByID<RE::Actor>(snapshot.ticket.formID));
        actor.physics = snapshot.loaded ? PhysicsLabel(registry.Resolve(snapshot.ticket)) : "Not loaded";
        if (const auto driver = drivers.find(Key(snapshot.ticket)); driver != drivers.end()) {
            const auto& value = driver->second;
            actor.arousal = value.arousal;
            actor.state = !snapshot.loaded ? "Not loaded" :
                value.arousal ? "Managing" : "Waiting for arousal";
            next.switched += value.switched;
            next.applied += value.applied;
            next.deferred += value.deferred;
            next.failed += value.failed;
            next.blocked += value.blocked;
        } else {
            actor.state = snapshot.loaded ? "Starting" : "Not loaded";
        }
        next.actors.push_back(std::move(actor));
    }
    std::scoped_lock lock(statusLock);
    status = std::move(next);
}

void QueueOwnership(Driver& driver, RE::Actor* actor,
    const ActorWorkLease& lease, bool wantCBPC, OwnershipPurpose purpose,
    std::int64_t allowedAfterMs, std::int64_t now, ActorWorkTicket ticket,
    PhysicsOwnershipController& controller)
{
    const auto begin = controller.Begin(wantCBPC, purpose, false, now);
    const auto logBlocked = [&](const char* why) {
        if (driver.blockedLoggedAt >= 0 && now - driver.blockedLoggedAt < kRepeatLogMs) return;
        driver.blockedLoggedAt = now;
        SKSE::log::warn("Managed NPC {} ({:08X}) {} handoff not sent: {}", ActorName(actor),
            ticket.formID, wantCBPC ? "CBPC" : "SMP", why);
    };
    if (begin.status == OwnershipBeginStatus::blocked) {
        ++driver.blocked;
        logBlocked("the NPC's ownership controller refused it (retry cooldown or invalid state)");
        return;
    }
    if (begin.status == OwnershipBeginStatus::alreadyPending) return;
    const int requested = wantCBPC ? 1 : 0;
    if (driver.loggedRequestCBPC != requested || driver.requestLoggedAt < 0 ||
        now - driver.requestLoggedAt >= kRepeatLogMs) {
        driver.loggedRequestCBPC = requested;
        driver.requestLoggedAt = now;
        SKSE::log::info("Managed NPC {} ({:08X}) arousal {}: requesting {}", ActorName(actor), ticket.formID,
            driver.arousal ? fmt::format("{:.0f}", *driver.arousal) : std::string("unknown"),
            wantCBPC ? "CBPC (erect)" : "SMP (soft)");
    }
    const auto generation = begin.generation;
    const auto result = SetActorPhysicsOwner(lease, actor, wantCBPC,
        purpose == OwnershipPurpose::resetLoad ? 1 : 0, FsmpActorApiAvailable(),
        allowedAfterMs, now, [ticket, generation, wantCBPC](OwnershipReadback readback) {
            // The Papyrus callback runs on a script thread, but the registry and
            // drivers are game-thread-only (as on the player path, which also
            // queues its completion as a task).
            auto* tasks = SKSE::GetTaskInterface();
            if (!tasks) return;
            tasks->AddTask([ticket, generation, wantCBPC, readback] {
                auto* current = registry.Resolve(ticket);
                const auto found = drivers.find(Key(ticket));
                if (!current || found == drivers.end()) return;
                auto& state = found->second;
                // Completion must be applied through the same context the request
                // began on, so a reloaded actor cannot accept the old generation.
                const auto completion = PhysicsOwnershipController{ current->physics }.Complete(generation,
                    { readback.acknowledged, readback.dynamicBonesBefore, readback.boneCount,
                        readback.dynamicBonesAfter },
                    NowMs());
                if (!completion.matched) return;
                const auto name = ActorName(RE::TESForm::LookupByID<RE::Actor>(ticket.formID));
                if (completion.success) {
                    ++state.switched;
                    if (!wantCBPC && readback.boneCount > 0 && readback.dynamicBonesAfter == readback.boneCount)
                        SKSE::log::info("Managed NPC {} ({:08X}) switched to SMP; SMP proof {}/{}",
                            name, ticket.formID, readback.dynamicBonesAfter, readback.boneCount);
                    else
                        SKSE::log::info("Managed NPC {} ({:08X}) switched to {}", name, ticket.formID,
                            wantCBPC ? "CBPC (erect)" : "SMP (soft), not proven");
                } else {
                    ++state.failed;
                    SKSE::log::warn("Managed NPC {} ({:08X}) handoff to {} did not complete (bridge returned {})",
                        name, ticket.formID, wantCBPC ? "CBPC" : "SMP",
                        readback.acknowledged ? "acknowledged" : "not done");
                }
            });
        });
    if (result != OwnershipDispatchResult::queued) {
        (void)controller.Complete(generation, false, now);
        ++driver.blocked;
        logBlocked("the physics script was busy or unavailable (player work, another handoff, or bridge not ready)");
    }
}

void Service(ActorWorkTicket ticket, ActorContext& context, RE::Actor* actor, const ActorWorkLease& lease,
    const ManagedNpcSettings& settings, std::int64_t allowedAfterMs, std::int64_t now)
{
    auto& driver = DriverFor(ticket);
    auto& slot = arousalSlots[Key(ticket)];
    auto& sceneSlot = sceneSlots[Key(ticket)];
    auto controller = PhysicsOwnershipController{ context.physics };

    if (controller.Read().pending) {
        // Drop a handoff that never returned instead of waiting forever.
        (void)controller.Expire(now, 15000);
    }
    if (now >= driver.nextPollMs) {
        driver.nextPollMs = now + 1000;
        if (slot.Begin(now)) {
            const auto started = now;
            if (!QueryActorArousal(lease, actor, allowedAfterMs, [ticket, started](ActorWorkTicket, auto value) {
                auto& entry = arousalSlots[Key(ticket)];
                if (!entry.Finish(started)) return;
                const auto found = drivers.find(Key(ticket));
                if (found == drivers.end() || !registry.Resolve(ticket)) return;
                found->second.arousal = value;
                found->second.arousalAt = started;
            })) {
                (void)slot.Finish(started);
                driver.arousal.reset();
                ++driver.blocked;
            }
        }
        if (sceneSlot.Begin(now)) {
            const auto started = now;
            if (!QueryActorScene(lease, actor, allowedAfterMs, [ticket, started](ActorWorkTicket, auto value) {
                auto& entry = sceneSlots[Key(ticket)];
                if (!entry.Finish(started)) return;
                const auto found = drivers.find(Key(ticket));
                if (found == drivers.end() || !registry.Resolve(ticket)) return;
                auto& tracked = found->second;
                if (!tracked.sceneLogged || tracked.loggedScene != value) {
                    tracked.sceneLogged = true;
                    tracked.loggedScene = value;
                    SKSE::log::info("Managed NPC {} ({:08X}): {}",
                        ActorName(RE::TESForm::LookupByID<RE::Actor>(ticket.formID)), ticket.formID,
                        SceneName(value));
                }
                tracked.scene = value;
                tracked.sceneAt = started;
            })) {
                (void)sceneSlot.Finish(started);
                driver.scene = ActorSceneStatus::unknown;
                driver.sceneAt = now;
                ++driver.blocked;
            }
        }
    }

    // A scene owns positioning: never send a background ownership change.
    if (driver.scene != ActorSceneStatus::clear) return;
    if (!driver.arousal || driver.arousalAt < 0 ||
        now - driver.arousalAt > 3000 || now < driver.arousalAt) return;
    if (controller.Read().pending) return;

    const auto ownership = controller.Read();
    const Core::NormalDecisionState decision{
        *driver.arousal, true, ownership.known, ownership.usingCBPC, false };
    const bool wantCBPC = Core::NormalSettingsWantCBPC(settings, decision);
    // An unknown owner must be established before this actor can be trusted to
    // hold a pose, so the first decision always dispatches.
    if (!ownership.known || ownership.usingCBPC != wantCBPC) {
        driver.appliedEvent.clear();
        // The new owner may rebuild the node, so the recorded pose is no longer
        // evidence that the current bone matches it.
        driver.appliedBend = -1;
        QueueOwnership(driver, actor, lease, wantCBPC,
            OwnershipPurpose::switchOwner, allowedAfterMs, now, ticket, controller);
        return;
    }
    if (!wantCBPC) return;  // SMP owns the pose; no position command is needed.

    // SPS pose angle: continuous, idempotent, and independent of the SOS event
    // contract, so it is written directly instead of being queued. The graph
    // events it replaces cannot be re-asserted safely from a tick because they
    // are relative and would repeat the same blend.
    if (settings.bendMethod == SPS::Runtime::kBendMethodPose) {
        const int bend = Controllers::ManagedPoseBend(*driver.arousal,
            { settings.threshold, settings.erectBend });
        if (bend == driver.appliedBend) return;
        if (SPS::Runtime::SosPose::Apply(actor, bend,
                SPS::Runtime::SosPose::ConfiguredDegrees())) {
            driver.appliedBend = bend;
            ++driver.applied;
        } else ++driver.blocked;
        return;
    }

    // CBPC owns the pose. Reuse the verified SOS event contract the basic
    // bridge already uses, converted from this NPC's own bend.
    const auto bendEvent = Controllers::ManagedSosEvent(*driver.arousal,
        { settings.threshold, settings.erectBend }, TngLoaded());
    if (!bendEvent || *bendEvent == driver.appliedEvent) return;
    if (!QueueActorPosition(lease, actor, *bendEvent, std::nullopt, allowedAfterMs,
            [ticket, event = *bendEvent](ActorWorkTicket, bool applied) {
                const auto found = drivers.find(Key(ticket));
                if (found == drivers.end()) return;
                const auto name = ActorName(RE::TESForm::LookupByID<RE::Actor>(ticket.formID));
                if (applied) {
                    found->second.appliedEvent = event;
                    ++found->second.applied;
                    SKSE::log::info("Managed NPC {} ({:08X}) angle sent: {}", name, ticket.formID, event);
                } else {
                    ++found->second.deferred;
                    SKSE::log::info("Managed NPC {} ({:08X}) angle {} deferred (scene or stale target)",
                        name, ticket.formID, event);
                }
            })) {
        ++driver.blocked;
        if (driver.blockedLoggedAt < 0 || now - driver.blockedLoggedAt >= kRepeatLogMs) {
            driver.blockedLoggedAt = now;
            SKSE::log::warn("Managed NPC {} ({:08X}) angle {} not sent: position bridge busy or unavailable",
                ActorName(actor), ticket.formID, *bendEvent);
        }
    }
}

} // namespace

void TickManagedNpcs(bool enabled, const ManagedNpcSettings& settings,
    std::int64_t allowedAfterMs, bool playerBusy)
{
    if (!enabled) {
        if (registry.Size() != 0) ResetManagedNpcs();
        Publish("Off");
        return;
    }
    {
        // Re-read under the same lock as configuration changes: an older tick
        // must not act after the menu disabled the feature.
        std::scoped_lock lock(statusLock);
        enabled = enabled && configuredEnabled;
    }
    if (!enabled) {
        if (registry.Size() != 0) ResetManagedNpcs();
        nearbyCache.clear();
        Publish("Off");
        return;
    }
    ApplyQueuedMembership(allowedAfterMs);
    auto* player = RE::PlayerCharacter::GetSingleton();
    const auto now = NowMs();
    if (!ExecutionPaused()) {
        ScanNearby(player, now);
        HintCrosshairTarget(allowedAfterMs, now);
    }
    if (registry.Size() == 0) { Publish("No managed NPC"); return; }
    if (!PapyrusReady(allowedAfterMs, now)) { Publish("Waiting for the game"); return; }
    if (!FsmpActorApiAvailable() || !OrderedFsmpBridgeAvailable() || !PositionBackendAvailable()) {
        Publish("Requires Faster HDT-SMP and SOS or TNG");
        return;
    }
    if (playerBusy) { Publish("Waiting for player work"); return; }

    for (const auto& snapshot : registry.Snapshot()) {
        const auto ticket = snapshot.ticket;
        const auto actorRef = RE::TESForm::LookupByID<RE::Actor>(ticket.formID);
        const bool eligible = Eligible(actorRef, player);
        // Availability changes give the actor a fresh generation and reset its
        // transient state, so in-flight work can never land on the new 3D.
        if (eligible != snapshot.loaded) {
            SKSE::log::info("Managed NPC {} ({:08X}) {}", ActorName(actorRef), ticket.formID,
                eligible ? "loaded; physics state will be re-established" :
                    "not available (unloaded, disabled or dead); membership kept, no work queued");
            if (!registry.SetLoaded(ticket, eligible)) { Remove(ticket); continue; }
            DropDriver(ticket);
            continue;
        }
        // An unloaded actor keeps its membership; only a live context is
        // serviced, and no work is queued while its 3D is absent.
        if (!eligible) continue;
        auto* context = registry.Resolve(ticket);
        if (!context) continue;
        Service(ticket, *context, actorRef, registry.Lease(ticket), settings, allowedAfterMs, now);
    }
    Publish("Managing NPC");
}

void ResetManagedNpcs()
{
    for (const auto& snapshot : registry.Snapshot()) Remove(snapshot.ticket);
    registry.ResetSession();
    drivers.clear();
    arousalSlots.clear();
    sceneSlots.clear();
    nearbyCache.clear();
    nearbyScanAtMs = 0;
    lastCrosshairFormID = 0;
    std::scoped_lock lock(statusLock);
    status = {};
}

bool QueueManagedNpcAdd(std::uint32_t formID)
{
    if (formID == 0 || formID == SPS::API::kPlayerFormID) return false;
    std::scoped_lock lock(statusLock);
    if (std::find(pendingAdds.begin(), pendingAdds.end(), formID) != pendingAdds.end()) return false;
    pendingAdds.push_back(formID);
    return true;
}

void ConfigureManagedNpcCommands(bool enabled)
{
    std::scoped_lock lock(statusLock);
    configuredEnabled = enabled;
}

bool QueueManagedNpcRemove(std::uint32_t formID)
{
    std::scoped_lock lock(statusLock);
    if (std::find(pendingRemoves.begin(), pendingRemoves.end(), formID) != pendingRemoves.end()) return false;
    pendingRemoves.push_back(formID);
    return true;
}

void InvalidateManagedNpc(std::uint32_t formID)
{
    const auto found = registry.Find(formID);
    if (!found) return;
    // The pending ownership generation lives in the actor's own context, so it
    // must be reset there rather than in the transient driver record.
    if (auto* context = registry.Resolve(found->ticket))
        PhysicsOwnershipController{ context->physics }.ResetPending();
}

bool IsManagedNpc(std::uint32_t formID)
{
    return registry.Find(formID).has_value();
}

ManagedNpcStatus ReadManagedNpcStatus()
{
    std::scoped_lock lock(statusLock);
    return status;
}

std::uint32_t ResolveManagedNpcTarget()
{
    auto* data = RE::CrosshairPickData::GetSingleton();
    if (!data) return 0;
    const auto handle = data->GetActiveTarget();
    const auto actor = handle.get();
    if (!actor) return 0;
    const auto* npc = actor->As<RE::Actor>();
    return npc ? npc->GetFormID() : 0;
}

}
