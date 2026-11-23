#include "ManagedNpcRuntime.h"

#include "ActorBridgeAdapter.h"
#include "Compatibility.h"
#include "PapyrusGateway.h"
#include "PhysicsDecision.h"
#include "PhysicsOwnershipAdapter.h"
#include "PositionBackendAdapter.h"
#include "SPSAPI.h"
#include "ShaftPoseTracker.h"
#include "SosPoseAngle.h"
#include "controllers/ManagedActorRegistry.h"
#include "controllers/PhysicsOwnershipController.h"
#include "controllers/PoseTargets.h"

#include <SKSE/SKSE.h>
#include <SimpleIni.h>
#include <fmt/format.h>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <set>
#include <unordered_map>

namespace SPS::Runtime {
namespace {
using namespace Controllers;

ManagedActorRegistry registry{ kManagedNpcCapacity };

// Per-actor driver state. Keyed by ticket so removal and re-add cannot inherit
// a previous actor's pending ownership generation. The ownership controller is
// built per tick from the actor's own ActorContext, because it binds directly to
// that context's PhysicsOwnershipState.
constexpr int kUnknownIndex = -100;   // no SOS stage has been sent yet
constexpr int kBottomIndex = -9;      // SOSBend-9, where a fall releases CBPC
constexpr std::int64_t kFallHoldMaxMs = 15000;
constexpr std::int64_t kRisePrimeMaxMs = 3000;  // never delay a rise longer than this

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
    // Timed rise and fall for the event channel, mirroring the player: the bend
    // moves from curBend toward the arousal target at the player's Rise/Fall
    // speed, one SOS event per visible stage.
    float curBend{ 0.0F };
    int sentIndex{ kUnknownIndex };
    std::int64_t lastStepMs{ 0 };
    bool positionInFlight{ false };
    // Latched erect state for the angle's hysteresis band.
    bool erectHeld{ false };
    // When CBPC owner is held until the angle has been lowered to the bottom.
    std::int64_t fallHoldSinceMs{ -1 };
    // Next time the soft (SMP) pose may be re-sent after a failed attempt.
    std::int64_t softPoseRetryAt{ 0 };
    // The graph was put on the ramp's first stage (SOSBend-9) before a soft to
    // CBPC switch, so the rise does not snap up to the erect pose first.
    bool risePrimed{ false };
    std::int64_t risePrimeStartedAt{ 0 };
    // After a real CBPC to SMP handoff the shaft gets one FSMP reset (what the
    // user otherwise runs by hand), like the player. It waits until the resting
    // pose has been sent and the shaft bones have stopped moving, so SMP does not
    // bake a pose that is still blending. 0 means no reset is scheduled.
    std::int64_t softResetDueAt{ 0 };
    std::int64_t softResetUntil{ 0 };
    std::int64_t stableWaitStart{ 0 };
    ShaftPoseTracker shaftPose;
    std::uint64_t switched{ 0 }, applied{ 0 }, deferred{ 0 }, failed{ 0 }, blocked{ 0 };
    // Log throttling: changes are always logged; repeats of the same request or
    // the same blocked reason at most every kRepeatLogMs.
    ActorSceneStatus loggedScene{ ActorSceneStatus::unknown };
    bool sceneLogged{ false };
    int loggedRequestCBPC{ -1 };
    std::int64_t requestLoggedAt{ -1 };
    std::int64_t blockedLoggedAt{ -1 };
    // Menu "set arousal" test write. Queued by FormID, sent by Service on the
    // game thread, retried at most once a second until it lands or expires.
    std::optional<float> requestedArousalWrite;
    std::int64_t writeStartedAt{ -1 };
    std::int64_t writeRetryAt{ 0 };
    std::int64_t writeExpiresAt{ 0 };
    // A poll that started before the last write finished may return the old
    // value; it must not overwrite the freshly written one.
    std::int64_t lastWriteDoneAt{ -1 };
};
std::unordered_map<std::uint64_t, Driver> drivers;
ManagedNpcTickSink tickSink;
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
// The membership kept in the save's SPSW co-save (record MNPC). The registry is
// rebuilt on every game load, so this list is what survives: it is filled by the
// co-save load and by successful Add, and shrinks only on an explicit Remove or a
// save revert. Both guarded by statusLock.
std::set<std::uint32_t> memberIds;
bool restorePending{ false };
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

// ---- Per-NPC settings ----
// Kept apart from the player's Settings. Stored by the NPC's base record
// ("Plugin.esp|LocalID") so they survive reloads; a runtime-only base falls back
// to this session. Game thread only (form lookups).
constexpr auto kNpcIni = "Data/SKSE/Plugins/SchlongPhysicsSwapper_NPCs.ini";
std::mutex optionsLock;
std::unordered_map<std::string, ManagedNpcOptions> savedOptions;
std::unordered_map<std::uint32_t, ManagedNpcOptions> liveOptions;
bool savedOptionsLoaded{ false };

ManagedNpcOptions ClampOptions(ManagedNpcOptions o)
{
    o.erectAt = std::clamp(o.erectAt, 0.0F, 100.0F);
    o.angle = std::clamp(o.angle, 1, 20);
    o.riseSeconds = std::clamp(o.riseSeconds, 1, 30);
    o.fallSeconds = std::clamp(o.fallSeconds, 1, 30);
    return o;
}

std::string StableKey(std::uint32_t formID)
{
    const auto* actor = RE::TESForm::LookupByID<RE::Actor>(formID);
    const auto* base = actor ? actor->GetActorBase() : nullptr;
    const auto* file = base ? base->GetFile(0) : nullptr;
    if (!file) return {};
    return fmt::format("{}|{:06X}", file->GetFilename(), base->GetLocalFormID());
}

void LoadSavedOptions()  // optionsLock held
{
    if (savedOptionsLoaded) return;
    savedOptionsLoaded = true;
    CSimpleIniA ini;
    ini.SetUnicode();
    if (ini.LoadFile(kNpcIni) < 0) return;
    CSimpleIniA::TNamesDepend sections;
    ini.GetAllSections(sections);
    for (const auto& section : sections) {
        ManagedNpcOptions o;
        o.erectAt = static_cast<float>(ini.GetDoubleValue(section.pItem, "ErectAt", o.erectAt));
        o.angle = static_cast<int>(ini.GetLongValue(section.pItem, "Angle", o.angle));
        o.followArousal = ini.GetBoolValue(section.pItem, "FollowArousal", o.followArousal);
        o.riseSeconds = static_cast<int>(ini.GetLongValue(section.pItem, "RiseSeconds", o.riseSeconds));
        o.fallSeconds = static_cast<int>(ini.GetLongValue(section.pItem, "FallSeconds", o.fallSeconds));
        savedOptions[section.pItem] = ClampOptions(o);
    }
}

ManagedNpcOptions OptionsFor(std::uint32_t formID)
{
    std::scoped_lock lock(optionsLock);
    if (const auto found = liveOptions.find(formID); found != liveOptions.end()) return found->second;
    ManagedNpcOptions o;
    const auto key = StableKey(formID);
    if (!key.empty()) {
        LoadSavedOptions();
        if (const auto saved = savedOptions.find(key); saved != savedOptions.end()) o = saved->second;
    }
    liveOptions[formID] = o;
    return o;
}

// Builds the engine settings for one NPC from its own options. Only the shared
// technical choices (bend method) come from the player's configuration.
ManagedNpcSettings EffectiveSettings(const ManagedNpcSettings& shared, const ManagedNpcOptions& o)
{
    ManagedNpcSettings s = shared;
    s.mode = 0;
    s.threshold = o.erectAt;
    s.hysteresis = 5.0F;
    s.erectBend = o.angle;
    s.gradualErection = true;
    s.arousalBasedErection = o.followArousal;
    s.erectionStartArousal = 20.0F;
    s.erectionDurationMs = o.riseSeconds * 1000;
    s.softeningDurationMs = o.fallSeconds * 1000;
    return s;
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
    std::vector<std::uint32_t> adds, removes, restore;
    std::string rejection;
    {
        std::scoped_lock lock(statusLock);
        adds.swap(pendingAdds);
        removes.swap(pendingRemoves);
        rejection.swap(lastRejection);
        if (restorePending) {
            restorePending = false;
            restore.assign(memberIds.begin(), memberIds.end());
        }
    }
    if (!adds.empty() || !removes.empty() || !restore.empty()) nearbyScanAtMs = 0;  // Refresh the list now.
    // Members remembered by the save. No NPC-eligibility check: the NPC may be in
    // another cell right now, and the tick already keeps membership for an actor
    // that is not loaded and starts work when it is. Only an actor that no longer
    // exists is dropped.
    for (const auto formID : restore) {
        if (registry.Find(formID)) continue;
        const auto actor = RE::TESForm::LookupByID<RE::Actor>(formID);
        const auto added = actor ? registry.Add(formID) : ActorRegistrationResult{};
        if (added.status == ActorRegistrationStatus::added) {
            SKSE::log::info("Managed NPC {} ({:08X}) restored from this save",
                ActorName(actor), formID);
        } else if (added.status == ActorRegistrationStatus::capacityReached) {
            SKSE::log::info("Managed NPC {:08X} not restored: the managed NPC limit is reached; kept in the save", formID);
        } else if (added.status != ActorRegistrationStatus::alreadyRegistered) {
            SKSE::log::warn("Managed NPC {:08X} dropped from this save's list: the actor no longer exists", formID);
            std::scoped_lock lock(statusLock);
            memberIds.erase(formID);
        }
    }
    for (const auto formID : removes) {
        { std::scoped_lock lock(statusLock); memberIds.erase(formID); }
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
            case ActorRegistrationStatus::added: {
                std::scoped_lock lock(statusLock);
                memberIds.insert(formID);
                break;
            }
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
    next.capacity = registry.Capacity();
    next.rejection = lastRejection;
    next.nearby = nearbyCache;
    for (const auto& snapshot : registry.Snapshot()) {
        ManagedNpcActorStatus actor;
        actor.formID = snapshot.ticket.formID;
        actor.options = OptionsFor(snapshot.ticket.formID);
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

// Returns true when the request was queued (or one was already pending).
bool QueueOwnership(Driver& driver, RE::Actor* actor,
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
        return false;
    }
    if (begin.status == OwnershipBeginStatus::alreadyPending) return true;
    const int requested = wantCBPC ? 1 : 0;
    if (purpose == OwnershipPurpose::resetSoft) {
        SKSE::log::info("Managed NPC {} ({:08X}) requesting a soft SMP reset", ActorName(actor), ticket.formID);
    } else if (driver.loggedRequestCBPC != requested || driver.requestLoggedAt < 0 ||
        now - driver.requestLoggedAt >= kRepeatLogMs) {
        driver.loggedRequestCBPC = requested;
        driver.requestLoggedAt = now;
        SKSE::log::info("Managed NPC {} ({:08X}) arousal {}: requesting {}", ActorName(actor), ticket.formID,
            driver.arousal ? fmt::format("{:.0f}", *driver.arousal) : std::string("unknown"),
            wantCBPC ? "CBPC (erect)" : "SMP (soft)");
    }
    const auto generation = begin.generation;
    // Preparation 1 stops both engines first; 5 is the reset-only soft refresh
    // (SPS_FSMPBridge.SetActorOwnerV3).
    const int preparation = purpose == OwnershipPurpose::resetLoad ? 1 :
        (purpose == OwnershipPurpose::resetSoft ? 5 : 0);
    const auto result = SetActorPhysicsOwner(lease, actor, wantCBPC,
        preparation, FsmpActorApiAvailable(),
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
                if (completion.purpose == OwnershipPurpose::resetSoft) {
                    // Reset only: ownership is unchanged, so this is not a switch.
                    state.softResetDueAt = 0;
                    if (completion.success)
                        SKSE::log::info("Managed NPC {} ({:08X}) soft SMP reset done; SMP proof {}/{}",
                            name, ticket.formID, readback.dynamicBonesAfter, readback.boneCount);
                    else {
                        ++state.failed;
                        SKSE::log::warn("Managed NPC {} ({:08X}) soft SMP reset did not complete",
                            name, ticket.formID);
                    }
                    return;
                }
                if (completion.success) {
                    ++state.switched;
                    // A real CBPC to SMP handoff: schedule the one soft reset. The
                    // resting pose is sent by Service after this; the reset waits
                    // for it and for the shaft to settle.
                    if (!wantCBPC && completion.previousKnown && completion.previousCBPC) {
                        const auto done = NowMs();
                        state.softResetDueAt = done + 2500;
                        state.softResetUntil = done + 12000;
                        state.stableWaitStart = 0;
                        state.shaftPose.Reset();
                    }
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
        // A reset that never ran changed nothing, so it must not mark the owner
        // unknown (a failed completion would force a full re-handoff). The reset
        // is simply retried while its window lasts.
        if (purpose == OwnershipPurpose::resetSoft) controller.ResetPending();
        else (void)controller.Complete(generation, false, now);
        ++driver.blocked;
        logBlocked("the physics script was busy or unavailable (player work, another handoff, or bridge not ready)");
        return false;
    }
    return true;
}

void Service(ActorWorkTicket ticket, ActorContext& context, RE::Actor* actor, const ActorWorkLease& lease,
    const ManagedNpcSettings& sharedSettings, std::int64_t allowedAfterMs, std::int64_t now)
{
    const ManagedNpcSettings settings =
        EffectiveSettings(sharedSettings, OptionsFor(ticket.formID));
    auto& driver = DriverFor(ticket);
    auto& slot = arousalSlots[Key(ticket)];
    auto& sceneSlot = sceneSlots[Key(ticket)];
    auto controller = PhysicsOwnershipController{ context.physics };

    if (controller.Read().pending) {
        // Drop a handoff that never returned instead of waiting forever.
        (void)controller.Expire(now, 15000);
    }
    if (driver.requestedArousalWrite && now >= driver.writeExpiresAt) {
        driver.requestedArousalWrite.reset();
        driver.writeStartedAt = -1;
        ++driver.failed;
        SKSE::log::warn("Managed NPC {:08X}: OSL arousal write expired without completing", ticket.formID);
    }
    if (driver.requestedArousalWrite && driver.writeStartedAt < 0 && now >= driver.writeRetryAt) {
        const auto requested = *driver.requestedArousalWrite;
        driver.writeStartedAt = now;
        if (!QueueActorArousalWrite(lease, actor, requested, allowedAfterMs,
            [ticket, started = now, requested](ActorWorkTicket, auto result) {
                const auto found = drivers.find(Key(ticket));
                if (found == drivers.end() || !registry.Resolve(ticket)) return;
                auto& tracked = found->second;
                if (tracked.writeStartedAt != started) return;
                tracked.writeStartedAt = -1;
                if (tracked.requestedArousalWrite &&
                    std::abs(*tracked.requestedArousalWrite - requested) < 0.01F)
                    tracked.requestedArousalWrite.reset();
                if (!result) {
                    ++tracked.failed;
                    SKSE::log::warn("Managed NPC {:08X}: OSL arousal write did not complete", ticket.formID);
                    return;
                }
                tracked.arousal = *result;
                tracked.lastWriteDoneAt = NowMs();
                tracked.arousalAt = tracked.lastWriteDoneAt;
                SKSE::log::info("Managed NPC {:08X}: OSL arousal set to {:.1f} (requested {:.1f})",
                    ticket.formID, *result, requested);
            })) {
            driver.writeStartedAt = -1;
            driver.writeRetryAt = now + 1000;
        }
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
                if (started < found->second.lastWriteDoneAt) return;
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
                // A query that could not be dispatched says nothing about the
                // scene. Marking it unknown made the pose logic below forget the
                // angle and replay the whole rise every time the script engine
                // was busy, so keep the last known scene and try again next poll.
                (void)sceneSlot.Finish(started);
                ++driver.blocked;
                if (driver.blockedLoggedAt < 0 || now - driver.blockedLoggedAt >= kRepeatLogMs) {
                    driver.blockedLoggedAt = now;
                    SKSE::log::info("Managed NPC {} ({:08X}): scene check not sent (script engine busy); keeping the last known scene",
                        ActorName(actor), ticket.formID);
                }
            }
        }
    }

    if (tickSink) tickSink(ticket.formID, driver.arousal, allowedAfterMs);

    // A scene owns positioning: never send a background ownership change.
    if (driver.scene != ActorSceneStatus::clear) {
        // The scene's own animations replace the pose, so the recorded angle is
        // no longer evidence of the bone. Forget it, otherwise the unchanged
        // event is never re-sent once the scene ends and the angle is lost.
        driver.appliedEvent.clear();
        driver.appliedBend = -1;
        driver.sentIndex = kUnknownIndex;
        driver.curBend = 0.0F;
        driver.lastStepMs = 0;
        driver.fallHoldSinceMs = -1;
        return;
    }
    if (!driver.arousal || driver.arousalAt < 0 ||
        now - driver.arousalAt > 3000 || now < driver.arousalAt) return;
    if (controller.Read().pending) return;

    const auto ownership = controller.Read();
    const Core::NormalDecisionState decision{
        *driver.arousal, true, ownership.known, ownership.usingCBPC, false };
    const bool eventChannel = settings.bendMethod != SPS::Runtime::kBendMethodPose;
    bool wantCBPC = Core::NormalSettingsWantCBPC(settings, decision);
    // Like the player, lower the angle while CBPC still owns the bones and only
    // hand over to SMP once the lowest stage was sent. Releasing first would let
    // SMP settle the shaft into its own pose before the fall. Bounded, so an
    // unavailable position bridge can never keep CBPC forever.
    if (eventChannel && settings.gradualErection && !wantCBPC && ownership.known &&
        ownership.usingCBPC) {
        const bool atBottom = driver.sentIndex == kBottomIndex && driver.curBend <= 0.0F;
        if (driver.fallHoldSinceMs < 0) driver.fallHoldSinceMs = now;
        if (!atBottom && now - driver.fallHoldSinceMs < kFallHoldMaxMs) wantCBPC = true;
    } else {
        driver.fallHoldSinceMs = -1;
    }
    // An unknown owner must be established before this actor can be trusted to
    // hold a pose, so the first decision always dispatches.
    if (!ownership.known || ownership.usingCBPC != wantCBPC) {
        // Like the player (PrimeLowestStageForRise): a soft actor rests on the
        // flaccid clip, and CBPC then follows the graph's erect pose until the
        // ramp's first event, which showed as a snap up then back down. Put the
        // graph on SOSBend-9 first and switch once it is applied. Bounded, so an
        // unavailable position bridge never holds a rise.
        if (wantCBPC && ownership.known && !ownership.usingCBPC && eventChannel &&
            settings.gradualErection && !driver.risePrimed) {
            if (driver.risePrimeStartedAt == 0) driver.risePrimeStartedAt = now;
            if (now - driver.risePrimeStartedAt < kRisePrimeMaxMs) {
                if (!driver.positionInFlight) {
                    const std::string primeEvent =
                        std::string(SPS::Core::PoseTargets::EventName(0, TngLoaded()));
                    driver.positionInFlight = true;
                    if (!QueueActorPosition(lease, actor, primeEvent, std::nullopt, allowedAfterMs,
                            [ticket](ActorWorkTicket, bool applied) {
                                const auto found = drivers.find(Key(ticket));
                                if (found == drivers.end()) return;
                                found->second.positionInFlight = false;
                                if (applied) found->second.risePrimed = true;
                            })) {
                        driver.positionInFlight = false;
                        ++driver.blocked;
                    }
                }
                return;
            }
        }
        driver.risePrimed = false;
        driver.risePrimeStartedAt = 0;
        driver.appliedEvent.clear();
        // The new owner may rebuild the node, so the recorded pose is no longer
        // evidence that the current bone matches it.
        driver.appliedBend = -1;
        driver.sentIndex = kUnknownIndex;
        driver.curBend = 0.0F;
        driver.lastStepMs = 0;
        driver.fallHoldSinceMs = -1;
        driver.softResetDueAt = 0;
        driver.stableWaitStart = 0;
        (void)QueueOwnership(driver, actor, lease, wantCBPC,
            OwnershipPurpose::switchOwner, allowedAfterMs, now, ticket, controller);
        return;
    }
    if (!wantCBPC) {
        driver.risePrimed = false;
        driver.risePrimeStartedAt = 0;
        // SMP owns the physics, but the graph keeps whatever bend was last sent,
        // and the handoff or a finished scene can leave it above the bottom. Like
        // the player (ApplyRequestedSoftBend after each soft handoff), put the
        // graph on SOSBend-9 once per ownership change so the NPC stays fully down.
        if (eventChannel && ownership.known && !ownership.usingCBPC &&
            driver.sentIndex != kBottomIndex && !driver.positionInFlight &&
            now >= driver.softPoseRetryAt) {
            driver.softPoseRetryAt = now + 3000;
            // SMP owns the bones here, so use the true hanging clip (what OSL sends
            // for a soft actor); SOSBend-9 would leave the shaft tilted out.
            const std::string bottomEvent = TngLoaded() ?
                std::string(SPS::Core::PoseTargets::EventName(0, true)) : std::string("SOSFlaccid");
            driver.positionInFlight = true;
            if (!QueueActorPosition(lease, actor, bottomEvent, std::nullopt, allowedAfterMs,
                    [ticket, bottomEvent](ActorWorkTicket, bool applied) {
                        const auto found = drivers.find(Key(ticket));
                        if (found == drivers.end()) return;
                        found->second.positionInFlight = false;
                        if (!applied) { ++found->second.deferred; return; }
                        found->second.sentIndex = kBottomIndex;
                        found->second.appliedEvent = bottomEvent;
                        ++found->second.applied;
                        SKSE::log::info("Managed NPC {} ({:08X}) soft pose held: {}",
                            ActorName(RE::TESForm::LookupByID<RE::Actor>(ticket.formID)), ticket.formID, bottomEvent);
                    })) {
                driver.positionInFlight = false;
                ++driver.blocked;
            }
        }
        if (driver.softResetDueAt > 0 && now >= driver.softResetDueAt) {
            if (now >= driver.softResetUntil) {
                driver.softResetDueAt = 0;
                driver.stableWaitStart = 0;
                SKSE::log::warn("Managed NPC {} ({:08X}) soft SMP reset skipped: it could not run in time",
                    ActorName(actor), ticket.formID);
            } else if (!driver.positionInFlight && (!eventChannel || driver.sentIndex == kBottomIndex)) {
                // Reset only once the resting pose has been sent and the shaft has
                // stopped moving, so SMP does not bake a half-blended pose. The
                // wait is capped at 6 s; after that the reset runs anyway.
                if (driver.stableWaitStart == 0) driver.stableWaitStart = now;
                const auto waited = now - driver.stableWaitStart;
                if (waited >= 6000 || driver.shaftPose.Stable(actor->Get3D(), now)) {
                    if (waited >= 6000)
                        SKSE::log::info("Managed NPC {} ({:08X}) soft SMP reset: shaft pose still moving after 6 s, resetting anyway",
                            ActorName(actor), ticket.formID);
                    else
                        SKSE::log::info("Managed NPC {} ({:08X}) soft SMP reset: shaft pose stable after {} ms",
                            ActorName(actor), ticket.formID, waited);
                    driver.stableWaitStart = 0;
                    if (!QueueOwnership(driver, actor, lease, false, OwnershipPurpose::resetSoft,
                            allowedAfterMs, now, ticket, controller))
                        driver.softResetDueAt = now + 1000;
                }
            }
        }
        return;
    }

    // SPS pose angle: continuous, idempotent, and independent of the SOS event
    // contract, so it is written directly instead of being queued. The graph
    // events it replaces cannot be re-asserted safely from a tick because they
    // are relative and would repeat the same blend.
    if (settings.bendMethod == SPS::Runtime::kBendMethodPose) {
        const int bend = Controllers::ManagedPoseBend(*driver.arousal,
            { settings.threshold, settings.erectBend, settings.arousalBasedErection,
                settings.erectionStartArousal });
        if (bend == driver.appliedBend) return;
        if (SPS::Runtime::SosPose::Apply(actor, bend,
                SPS::Runtime::SosPose::ConfiguredDegrees())) {
            driver.appliedBend = bend;
            ++driver.applied;
        } else ++driver.blocked;
        return;
    }

    // CBPC owns the pose. Move the angle toward the arousal target the way the
    // player does: at the Rise time when it grows and the Fall time when it
    // shrinks (full travel 0 to the erect bend takes exactly that time), with
    // gradual erection off jumping straight to the target. One verified SOS
    // event is sent per visible stage change.
    // The same hysteresis band the physics owner already has: once erect, stay
    // erect until arousal falls hysteresis points below the threshold. Without it
    // an NPC whose arousal hovers around the threshold flips target every few
    // seconds and replays the angle over and over. A gradient (rise with arousal)
    // is continuous, so it needs none.
    float effectiveThreshold = settings.threshold;
    if (!settings.arousalBasedErection) {
        if (*driver.arousal >= settings.threshold) driver.erectHeld = true;
        else if (*driver.arousal < settings.threshold - settings.hysteresis) driver.erectHeld = false;
        if (driver.erectHeld) effectiveThreshold = settings.threshold - settings.hysteresis;
    }
    const int target = Controllers::ManagedPoseBend(*driver.arousal,
        { effectiveThreshold, settings.erectBend, settings.arousalBasedErection,
            settings.erectionStartArousal });
    if (target < 0) return;
    const float elapsedMs = driver.lastStepMs > 0 ?
        static_cast<float>(std::clamp<std::int64_t>(now - driver.lastStepMs, 0, 1000)) : 0.0F;
    driver.lastStepMs = now;
    if (settings.gradualErection) {
        const float travel = static_cast<float>(std::max(1, std::min(settings.erectBend, 20)));
        const float riseMs = static_cast<float>(std::clamp(settings.erectionDurationMs, 500, 30000));
        const float fallMs = static_cast<float>(std::clamp(settings.softeningDurationMs, 500, 30000));
        const float goal = static_cast<float>(target);
        if (driver.curBend < goal)
            driver.curBend = std::min(goal, driver.curBend + elapsedMs * travel / riseMs);
        else if (driver.curBend > goal)
            driver.curBend = std::max(goal, driver.curBend - elapsedMs * travel / fallMs);
    } else {
        driver.curBend = static_cast<float>(target);
    }
    const bool tng = TngLoaded();
    const int bendNow = std::clamp(static_cast<int>(std::lround(driver.curBend)), 0, 20);
    const int index = SPS::Core::PoseTargets::EventIndex(bendNow, tng);
    if (index == driver.sentIndex || driver.positionInFlight) return;
    const std::string bendEvent = SPS::Core::PoseTargets::EventName(bendNow, tng);
    driver.positionInFlight = true;
    if (!QueueActorPosition(lease, actor, bendEvent, std::nullopt, allowedAfterMs,
            [ticket, event = bendEvent, index, target](ActorWorkTicket, bool applied) {
                const auto found = drivers.find(Key(ticket));
                if (found == drivers.end()) return;
                found->second.positionInFlight = false;
                const auto name = ActorName(RE::TESForm::LookupByID<RE::Actor>(ticket.formID));
                if (applied) {
                    found->second.sentIndex = index;
                    found->second.appliedEvent = event;
                    ++found->second.applied;
                    // Log the start and the end of a move, not every stage.
                    if (index == kBottomIndex || index == SPS::Core::PoseTargets::EventIndex(target, TngLoaded()))
                        SKSE::log::info("Managed NPC {} ({:08X}) angle sent: {}", name, ticket.formID, event);
                } else {
                    ++found->second.deferred;
                    SKSE::log::info("Managed NPC {} ({:08X}) angle {} deferred (scene or stale target)",
                        name, ticket.formID, event);
                }
            })) {
        driver.positionInFlight = false;
        ++driver.blocked;
        if (driver.blockedLoggedAt < 0 || now - driver.blockedLoggedAt >= kRepeatLogMs) {
            driver.blockedLoggedAt = now;
            SKSE::log::warn("Managed NPC {} ({:08X}) angle {} not sent: position bridge busy or unavailable",
                ActorName(actor), ticket.formID, bendEvent);
        }
    }
}

} // namespace

void TickManagedNpcs(bool enabled, const ManagedNpcSettings& settings,
    std::int64_t allowedAfterMs, bool playerBusy)
{
    // Lowering the limit keeps NPCs that are already managed; it only refuses new ones.
    registry.SetCapacity(settings.managedNpcUpToEight ? kManagedNpcMaxCapacity : kManagedNpcCapacity);
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
            if (!registry.SetLoaded(ticket, eligible)) {
                // The registry no longer knows this actor, so it must not come
                // back from the saved list after the next load either.
                { std::scoped_lock lock(statusLock); memberIds.erase(ticket.formID); }
                Remove(ticket);
                continue;
            }
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
    { std::scoped_lock lock(optionsLock); liveOptions.clear(); }
    nearbyCache.clear();
    nearbyScanAtMs = 0;
    lastCrosshairFormID = 0;
    std::scoped_lock lock(statusLock);
    status = {};
    // Only the live registry was dropped (a game load, or the feature switched
    // off or changed). The saved membership stays and is re-added on the next
    // enabled tick.
    restorePending = !memberIds.empty();
}

std::vector<std::uint32_t> ReadManagedNpcMembership()
{
    std::scoped_lock lock(statusLock);
    return { memberIds.begin(), memberIds.end() };
}

void RestoreManagedNpcMembership(const std::vector<std::uint32_t>& formIDs)
{
    std::scoped_lock lock(statusLock);
    memberIds.clear();
    for (const auto formID : formIDs) {
        if (memberIds.size() >= kManagedNpcMaxCapacity) break;
        if (formID != 0 && formID != SPS::API::kPlayerFormID) memberIds.insert(formID);
    }
    restorePending = !memberIds.empty();
}

void ClearManagedNpcMembership()
{
    std::scoped_lock lock(statusLock);
    memberIds.clear();
    restorePending = false;
}

bool QueueManagedNpcAdd(std::uint32_t formID)
{
    if (formID == 0 || formID == SPS::API::kPlayerFormID) return false;
    std::scoped_lock lock(statusLock);
    if (std::find(pendingAdds.begin(), pendingAdds.end(), formID) != pendingAdds.end()) return false;
    pendingAdds.push_back(formID);
    return true;
}

bool QueueManagedNpcArousalWrite(std::uint32_t formID, float value)
{
    const auto requested = Core::ValidArousalReading(value);
    if (!requested || !OslArousedLoaded()) return false;
    const auto found = registry.Find(formID);
    if (!found) return false;
    auto& driver = DriverFor(found->ticket);
    driver.requestedArousalWrite = *requested;
    driver.writeRetryAt = 0;
    driver.writeExpiresAt = NowMs() + 5000;
    return true;
}

bool SetManagedNpcOptions(std::uint32_t formID, const ManagedNpcOptions& options, bool save)
{
    if (!registry.Find(formID)) return false;
    const auto clamped = ClampOptions(options);
    const auto key = StableKey(formID);
    {
        std::scoped_lock lock(optionsLock);
        liveOptions[formID] = clamped;
        if (key.empty() || !save) return true;  // session only, or still dragging
        LoadSavedOptions();
        savedOptions[key] = clamped;
        CSimpleIniA ini;
        ini.SetUnicode();
        (void)ini.LoadFile(kNpcIni);
        ini.SetDoubleValue(key.c_str(), "ErectAt", clamped.erectAt);
        ini.SetLongValue(key.c_str(), "Angle", clamped.angle);
        ini.SetBoolValue(key.c_str(), "FollowArousal", clamped.followArousal);
        ini.SetLongValue(key.c_str(), "RiseSeconds", clamped.riseSeconds);
        ini.SetLongValue(key.c_str(), "FallSeconds", clamped.fallSeconds);
        if (ini.SaveFile(kNpcIni) < 0)
            SKSE::log::warn("Could not save managed NPC settings to {}", kNpcIni);
    }
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

std::optional<ManagedNpcApiState> ReadManagedNpcApiState(std::uint32_t formID)
{
    const auto found = registry.Find(formID);
    if (!found) return std::nullopt;
    ManagedNpcApiState state;
    if (auto* context = registry.Resolve(found->ticket)) {
        const auto ownership = PhysicsOwnershipController{ context->physics }.Read();
        state.physicsKnown = ownership.known && !ownership.pending;
        state.usingCBPC = ownership.usingCBPC;
    }
    if (const auto driver = drivers.find(Key(found->ticket)); driver != drivers.end() && driver->second.arousal)
        state.arousal = driver->second.arousal;
    return state;
}

void SetManagedNpcTickSink(ManagedNpcTickSink sink)
{
    tickSink = std::move(sink);
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
