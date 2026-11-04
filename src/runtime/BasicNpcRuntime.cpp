#include "BasicNpcRuntime.h"
#include "ManagedNpcRuntime.h"
#include "ActorBridgeAdapter.h"
#include "Compatibility.h"
#include "PapyrusGateway.h"
#include "PhysicsOwnershipAdapter.h"
#include "PhysicsDecision.h"
#include "controllers/BasicNpcPool.h"
#include "SPSAPI.h"

#include <SKSE/SKSE.h>
#include <algorithm>
#include <cmath>
#include <mutex>

namespace SPS::Runtime {
namespace {
using namespace Controllers;
BasicNpcPool pool;
std::uint32_t discoveryCursor{ 0 };
std::int64_t discoveryAt{ 0 };
std::uint64_t blocked{ 0 };
std::mutex statusLock;
BasicNpcStatus status;
std::shared_ptr<std::atomic<bool>> permission;
bool configuredEnabled{ false };
std::vector<ActorWorkLease> eventLeases;  // bounded by the pool; protected by statusLock

bool Eligible(RE::Actor* actor, RE::PlayerCharacter* player)
{
    if (!actor || !player || !player->Is3DLoaded() || actor->GetFormID() == SPS::API::kPlayerFormID ||
        actor->IsChild() || actor->IsDead() || actor->IsDeleted() || actor->IsDisabled() ||
        !actor->Is3DLoaded()) return false;
    // An enrolled NPC belongs to full management. Both paths must never issue
    // competing commands to the same actor.
    if (IsManagedNpc(actor->GetFormID())) return false;
    const auto* race = actor->GetRace();
    auto* root = actor->Get3D();
    if (!race || !race->HasKeywordString("ActorTypeNPC") || !root) return false;
    const auto location = actor->GetPosition();
    const auto origin = player->GetPosition();
    const float x = location.x - origin.x, y = location.y - origin.y, z = location.z - origin.z;
    const float distanceSquared = x * x + y * y + z * z;
    if (!std::isfinite(distanceSquared) || distanceSquared > 3000.0F * 3000.0F) return false;
    // A positioning prerequisite only, never proof of an equipped SMP mesh.
    return root->GetObjectByName(RE::BSFixedString("NPC Genitals01 [Gen01]")) != nullptr;
}

void Remove(ActorWorkTicket ticket)
{
    // Invalidate the actor before cancelling its request tokens. Neither action
    // releases an executing shared engine stack ahead of its real completion.
    pool.Remove(ticket);
    CancelActorBridgeRequests(ticket);
    CancelActorOwnership(ticket);
    std::scoped_lock lock(statusLock);
    std::erase_if(eventLeases, [ticket](const auto& lease) { return lease.Ticket() == ticket; });
}

void Publish(const char* state, std::int64_t now)
{
    BasicNpcStatus next;
    next.state = state;
    next.blocked = blocked;
    for (const auto ticket : pool.Tickets()) {
        const auto* entry = pool.Find(ticket);
        if (!entry) continue;
        ++next.tracked;
        if (entry->scene == ActorSceneStatus::active) ++next.inScene;
        if (!entry->arousal || now - entry->arousalAt > BasicNpcPolicy::kFreshMs ||
            entry->scene == ActorSceneStatus::unknown || now - entry->sceneAt > BasicNpcPolicy::kFreshMs) ++next.waiting;
        next.pending += static_cast<unsigned>(entry->arousalRequest.startedAt >= 0) +
            static_cast<unsigned>(entry->arousalWriteRequest.startedAt >= 0) +
            static_cast<unsigned>(entry->sceneRequest.startedAt >= 0) +
            static_cast<unsigned>(entry->positionRequest.startedAt >= 0);
        next.completed += entry->completed;
        next.deferred += entry->deferred;
        next.failed += entry->failed;
        const auto actor = RE::Actor::LookupByHandle(entry->handle);
        const char* displayName = actor ? actor->GetDisplayFullName() : nullptr;
        next.actors.push_back({ ticket.formID,
            displayName && displayName[0] != '\0' ? displayName : "Unnamed NPC", entry->arousal });
    }
    std::scoped_lock lock(statusLock);
    status = std::move(next);
}

void Service(BasicNpcEntry& entry, RE::Actor* actor, std::int64_t now,
    std::int64_t allowedAfterMs, bool playerBusy)
{
    entry.Expire(now);
    const auto lease = entry.work.Lease();
    if (!playerBusy && entry.requestedArousalWrite && entry.arousalWriteRequest.Begin(now, 1000)) {
        const auto requested = *entry.requestedArousalWrite;
        if (!QueueActorArousalWrite(lease, actor, requested, allowedAfterMs,
            [started = now, requested](auto ticket, auto result) {
                auto* current = pool.Find(ticket);
                const auto completedAt = NowMs();
                if (!current || !current->arousalWriteRequest.Finish(started, completedAt)) return;
                if (!result) {
                    if (current->requestedArousalWrite &&
                        std::abs(*current->requestedArousalWrite - requested) < 0.01F)
                        current->requestedArousalWrite.reset();
                    ++current->failed;
                    SKSE::log::warn("OSL arousal write for NPC {:08X} did not complete", ticket.formID);
                    return;
                }
                current->arousal = *result;
                current->arousalAt = completedAt;
                if (current->requestedArousalWrite &&
                    std::abs(*current->requestedArousalWrite - requested) < 0.01F)
                    current->requestedArousalWrite.reset();
                SKSE::log::info("OSL arousal set for NPC {:08X}: {:.1f} (requested {:.1f})",
                    ticket.formID, *result, requested);
            })) {
            (void)entry.arousalWriteRequest.Finish(now, now);
        }
    }
    if (!playerBusy && !PhysicsOperationBusy() && entry.arousal) {
        const auto event = entry.policy.Decide(*entry.arousal, entry.arousalAt, now, true, false, true);
        if (event && entry.positionRequest.Begin(now)) {
            if (!QueueBasicActorPosition(lease, actor, *event, allowedAfterMs,
                [started = now, event = *event](auto ticket, auto result) {
                    auto* current = pool.Find(ticket);
                    const auto completedAt = NowMs();
                    if (!current || !current->positionRequest.Finish(started, completedAt)) return;
                    current->policy.Complete(event, result, completedAt);
                    if (result == BasicPositionResult::applied) ++current->completed;
                    else if (result == BasicPositionResult::deferred) ++current->deferred;
                    else ++current->failed;
                })) {
                (void)entry.positionRequest.Finish(now, now);
                entry.policy.Complete(*event, BasicPositionResult::failed, now);
                ++blocked;
            }
        }
    }
    if (entry.arousalRequest.Begin(now, 1000)) {
        if (!QueryActorArousal(lease, actor, allowedAfterMs, [started = now](auto ticket, auto value) {
            auto* current = pool.Find(ticket);
            if (!current || !current->arousalRequest.Finish(started, NowMs())) return;
            current->arousal = value;
            current->arousalAt = started;
        })) {
            (void)entry.arousalRequest.Finish(now, now);
            entry.arousal.reset();
            ++blocked;
        }
    }
    if (entry.sceneRequest.Begin(now, 1000)) {
        if (!QueryActorScene(lease, actor, allowedAfterMs, [started = now](auto ticket, auto value) {
            auto* current = pool.Find(ticket);
            if (!current || !current->sceneRequest.Finish(started, NowMs())) return;
            current->scene = value;
            current->sceneAt = started;
            current->policy.ObserveScene(value, started);
        })) {
            (void)entry.sceneRequest.Finish(now, now);
            entry.scene = ActorSceneStatus::unknown;
            entry.sceneAt = now;
            entry.policy.ObserveScene(entry.scene, now);
            ++blocked;
        }
    }
}
}

BasicNpcStatus ReadBasicNpcStatus()
{
    std::scoped_lock lock(statusLock);
    return status;
}

void ConfigureBasicNpcCommands(bool enabled)
{
    std::scoped_lock lock(statusLock);
    configuredEnabled = enabled;
    if (permission) permission->store(false);
}

void ResetBasicNpcs()
{
    { std::scoped_lock lock(statusLock); if (permission) permission->store(false); }
    for (const auto ticket : pool.Tickets()) Remove(ticket);
    pool.Reset();
    discoveryCursor = 0;
    discoveryAt = 0;
    blocked = 0;
    std::scoped_lock lock(statusLock);
    permission.reset();
    eventLeases.clear();
    status = {};
}

void InvalidateBasicNpc(std::uint32_t formID)
{
    std::scoped_lock lock(statusLock);
    for (const auto& lease : eventLeases) if (lease.Ticket().formID == formID) lease.Revoke();
}

void TickBasicNpcs(bool enabled, bool providerSosDisabled, std::int64_t allowedAfterMs, bool playerBusy)
{
    const auto now = NowMs();
    bool revoked;
    {
        std::scoped_lock lock(statusLock);
        enabled = enabled && configuredEnabled;
        revoked = permission && !permission->load();
    }
    if (!enabled || !providerSosDisabled || revoked) ResetBasicNpcs();
    if (!enabled || !providerSosDisabled) {
        Publish(enabled ? "Confirm that OSL SOS control is disabled" : "Off", now);
        return;
    }
    if (!PapyrusReady(allowedAfterMs, now)) { Publish("Waiting for the game", now); return; }
    // Initial live scope is the verified OSL mapping. Other provider/fallback
    // paths remain unavailable here until their coexistence is established.
    if (!OslArousedLoaded() || SloArousedLoaded() || ClassicArousedLoaded() || !PositionBackendAvailable()) {
        ResetBasicNpcs();
        Publish("Requires OSL Aroused and SOS or TNG", now);
        return;
    }
    std::shared_ptr<std::atomic<bool>> sessionPermission;
    {
        std::scoped_lock lock(statusLock);
        // Re-read under the same lock as configuration changes: an older tick
        // must not create fresh permission after the menu has disabled it.
        if (!configuredEnabled) return;
        if (!permission) permission = std::make_shared<std::atomic<bool>>(true);
        sessionPermission = permission;
    }
    auto* player = RE::PlayerCharacter::GetSingleton();
    for (const auto ticket : pool.Tickets()) {
        const auto* entry = pool.Find(ticket);
        if (!entry) { Remove(ticket); continue; }
        const auto actor = RE::Actor::LookupByHandle(entry->handle);
        if (!Eligible(actor.get(), player) || actor->GetFormID() != ticket.formID ||
            reinterpret_cast<std::uintptr_t>(actor->Get3D()) != entry->root) Remove(ticket);
    }
    // At most 32 existing high-process handles per second. No world/form scan,
    // no retained process-list iterators, and never more than eight basic NPCs.
    if (now >= discoveryAt && pool.Tickets().size() < BasicNpcPool::kCapacity) {
        discoveryAt = now + 1000;
        if (auto* lists = RE::ProcessLists::GetSingleton()) {
            const auto count = lists->highActorHandles.size();
            const auto budget = std::min<std::size_t>(32, count);
            for (std::size_t i = 0; i < budget && pool.Tickets().size() < BasicNpcPool::kCapacity; ++i) {
                discoveryCursor %= count;
                const auto actor = lists->highActorHandles[discoveryCursor++].get();
                if (Eligible(actor.get(), player) && !pool.Find(actor->GetFormID())) {
                    if (auto* added = pool.Add(actor->GetFormID(), actor->GetHandle().native_handle(),
                        reinterpret_cast<std::uintptr_t>(actor->Get3D()), sessionPermission)) {
                        std::scoped_lock lock(statusLock);
                        eventLeases.push_back(added->work.Lease());
                    }
                }
            }
        }
    }
    if (auto* entry = pool.Next(now)) {
        const auto actor = RE::Actor::LookupByHandle(entry->handle);
        if (Eligible(actor.get(), player)) Service(*entry, actor.get(), now, allowedAfterMs, playerBusy);
    }
    Publish(playerBusy ? "Waiting for player work" : "Checking nearby NPCs", now);
}

bool QueueBasicNpcOslArousalWrite(std::uint32_t formID, float value,
    std::int64_t allowedAfterMs)
{
    (void)allowedAfterMs;
    const auto requested = Core::ValidArousalReading(value);
    auto* entry = pool.Find(formID);
    if (!entry || !requested || !OslArousedLoaded()) return false;
    const auto actor = RE::Actor::LookupByHandle(entry->handle);
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!Eligible(actor.get(), player) || actor->GetFormID() != formID ||
        reinterpret_cast<std::uintptr_t>(actor->Get3D()) != entry->root) return false;
    entry->requestedArousalWrite = *requested;
    return true;
}

}
