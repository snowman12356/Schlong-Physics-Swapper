#include "controllers/BasicNpcPool.h"
#include "controllers/ManagedActorRegistry.h"
#include "controllers/ActorRequestLedger.h"
#include <iostream>
#include <set>

int RunBasicNpcPoolTests()
{
    using namespace SPS::Controllers;
    int failures = 0;
    const auto expect = [&](bool value, const char* message) {
        if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
    };
    BasicNpcPool pool;
    auto permission = std::make_shared<std::atomic<bool>>(true);
    expect(!pool.Add(0, 1, 10) && !pool.Add(0x14, 1, 10) && !pool.Add(100, 0, 10) && !pool.Add(100, 1, 0),
        "invalid identities, player and absent 3D cannot join the basic pool");
    for (std::uint32_t i = 0; i < BasicNpcPool::kCapacity; ++i)
        expect(pool.Add(100 + i, 200 + i, 300 + i, permission) != nullptr, "eligible basic actor admitted");
    expect(!pool.Add(999, 999, 999) && pool.Tickets().size() == 8, "basic actor count is strictly bounded");
    const auto firstTicket = pool.Find(100)->work.Ticket();
    const auto lease = pool.Find(firstTicket)->work.Lease();
    expect(pool.Add(100, 200, 300)->work.Ticket() == firstTicket && !pool.Add(100, 201, 300) && !pool.Add(100, 200, 301),
        "duplicate discovery keeps identity; changed handle or 3D cannot reuse it");
    std::set<std::uint32_t> visited;
    for (std::int64_t time = 0; time < 2000; time += 250) {
        auto* entry = pool.Next(time);
        expect(entry != nullptr, "one actor serviced each scheduled tick");
        if (entry) visited.insert(entry->work.Ticket().formID);
        expect(pool.Next(time + 1) == nullptr, "repeat ticks cannot multiply actor work");
    }
    expect(visited.size() == 8 && pool.Next(2000)->work.Ticket() == firstTicket,
        "all eight actors receive a turn within two seconds");

    ManagedActorRegistry managed{ 1 };
    const auto managedTicket = managed.SetLoaded(managed.Add(100).ticket, true);
    expect(managedTicket && *managedTicket != firstTicket && !managed.Resolve(firstTicket),
        "basic and managed lifetimes cannot alias even for the same FormID");
    ActorRequestLedger ledger{ 1 };
    expect(ledger.Prepare("root", { lease, 200, ActorRequestKind::position, 5000, false, 300 }, 100) &&
        ledger.Current("root", ActorRequestKind::position, 100, 200, 200, 300) &&
        !ledger.Current("root", ActorRequestKind::position, 100, 200, 200, 301),
        "changed live 3D rejects a pending request before the next discovery tick");
    permission->store(false);
    expect(!lease.Valid() && !pool.Find(firstTicket) && !ledger.Finish("root", 300),
        "disabling permission immediately revokes queued commands and results");
    pool.Reset();
    auto* replacement = pool.Add(100, 200, 300, std::make_shared<std::atomic<bool>>(true));
    expect(replacement && replacement->work.Ticket() != firstTicket && !lease.Valid() && !pool.Find(firstTicket),
        "re-enable makes a new lifetime and never revives an old lease");
    if (!replacement) return failures + 1;
    const auto readdedTicket = replacement->work.Ticket();
    const auto replacementLease = replacement->work.Lease();
    expect(!pool.Remove(firstTicket) && pool.Remove(readdedTicket) && !replacementLease.Valid(),
        "old removal cannot remove replacement; current removal invalidates work");

    BasicNpcRequestSlot slot;
    expect(slot.Begin(100) && !slot.Begin(200) && !slot.Finish(101, 300),
        "one pending request and exact callback identity required");
    expect(slot.Finish(100, 500) && !slot.Begin(2099) && slot.Begin(2100),
        "successful callback retires its slot but obeys the sample budget");
    expect(!slot.Finish(2100, 12100) && slot.Expire(12100) && !slot.Begin(13000) && slot.Begin(14100),
        "missing and expired callbacks back off without marking data fresh");
    expect(!slot.Finish(2100, 14200) && slot.Finish(14100, 14200),
        "late result cannot clear the next request");

    auto* entry = pool.Add(101, 201, 301);
    if (!entry) return failures + 1;
    entry->arousal = 100.0F;
    entry->arousalAt = 0;
    entry->policy.ObserveScene(ActorSceneStatus::clear, 0);
    expect(entry->arousalRequest.Begin(0) && entry->sceneRequest.Begin(0) && entry->positionRequest.Begin(0),
        "separate bounded slots available for reads and position");
    entry->Expire(10000);
    expect(!entry->arousal && entry->scene == ActorSceneStatus::unknown && entry->failed == 3 &&
        !entry->policy.Decide(100, 10000, 10000, true, false, true),
        "lost callbacks clear old input and require fresh settled scene evidence");
    const auto unloadedLease = entry->work.Lease();
    unloadedLease.Revoke();
    expect(!pool.Find(unloadedLease.Ticket()), "unload event can revoke a lease before the next game tick");
    // Exercise every supported population with frame-boundary jitter. Sampling
    // must remain fresh at six/seven actors as well as exactly at the cap.
    for (std::uint32_t count = 1; count <= BasicNpcPool::kCapacity; ++count) {
        BasicNpcPool cadence;
        for (std::uint32_t i = 0; i < count; ++i) (void)cadence.Add(100 + i, 200 + i, 300 + i);
        for (std::int64_t tick = 0; tick < 80; ++tick) {
            const auto now = tick * 250 - (tick % 2 ? 10 : 0);
            auto* current = cadence.Next(now);
            expect(current != nullptr, "frame jitter does not skip a scheduled actor turn");
            if (!current) continue;
            if (current->sceneRequest.Begin(now, 1000)) {
                expect(current->sceneAt < 0 || now - current->sceneAt <= BasicNpcPolicy::kFreshMs,
                    "scene samples remain within freshness budget at every actor count");
                current->sceneAt = now;
                expect(current->sceneRequest.Finish(now, now), "cadence sample completes");
            }
        }
    }
    return failures;
}
