#include "controllers/ActorRequestLedger.h"
#include "controllers/PapyrusOperationGate.h"
#include "SPSAPI.h"

#include <iostream>
#include <string_view>

int RunActorRequestLedgerTests()
{
    using namespace SPS::Controllers;
    using Kind = ActorRequestKind;
    int failures = 0;
    const auto expect = [&](bool condition, std::string_view message) {
        if (!condition) {
            ++failures;
            std::cerr << "FAIL: " << message << '\n';
        }
    };

    ManagedActorRegistry registry{ 2 };
    constexpr std::uint32_t actorID = 0xFF001234;
    constexpr std::uint32_t otherID = 0x112233;
    constexpr std::uint32_t handle = 17;
    const auto added = registry.Add(actorID);
    expect(!registry.Lease(added.ticket).Valid(), "unloaded registration cannot issue a lease");
    const auto loaded = registry.SetLoaded(added.ticket, true);
    const auto other = registry.SetLoaded(registry.Add(otherID).ticket, true);
    if (!loaded || !other) return failures + 1;
    const auto lease = registry.Lease(*loaded);
    const auto otherLease = registry.Lease(*other);
    expect(lease.Valid() && lease.Ticket() == *loaded, "lease retains its actual high FormID and generation");
    ActorWorkOwner rejectedPlayerOwner{ SPS::API::kPlayerFormID };
    ActorWorkOwner allowedPlayerOwner{ SPS::API::kPlayerFormID, {}, true };
    expect(!rejectedPlayerOwner.Lease().Valid() && allowedPlayerOwner.Lease().Valid(),
        "player test work requires an explicit player-enabled lifetime");

    ActorRequestLedger ledger{ 2 };
    const ActorRequest read{ lease, handle, Kind::arousal, 1000 };
    const ActorRequest position{ lease, handle, Kind::position, 1000 };
    ActorRequestLedger sceneLedger{ 4 };
    expect(sceneLedger.Prepare("scene", { lease, handle, Kind::scene, 1000 }, 100) &&
        sceneLedger.Prepare("arousal", read, 100) &&
        sceneLedger.Prepare("arousal-write", { lease, handle, Kind::arousalWrite, 1000 }, 100) &&
        sceneLedger.Prepare("bend", position, 100),
        "scene, arousal read, arousal write and position each have one independent request slot");
    expect(!sceneLedger.Prepare("scene-again", { lease, handle, Kind::scene, 1000 }, 100) &&
        !sceneLedger.Current("scene", Kind::position, actorID, handle, 100),
        "scene query cannot accumulate or authorize a mutation");
    const auto sceneCancelled = sceneLedger.Cancel(*loaded);
    expect(sceneCancelled.size() == 1 && sceneCancelled[0] == "bend" &&
        !sceneLedger.Finish("scene", 200) && !sceneLedger.Finish("arousal", 200) &&
        !sceneLedger.Finish("arousal-write", 200),
        "actor cancellation drops scene results while identifying only actual engine work");
    expect(!ledger.Prepare("invalid", { {}, handle, Kind::arousal, 1000 }, 100), "missing lifetime cannot prepare work");
    expect(!ledger.Prepare("zero-handle", { lease, 0, Kind::arousal, 1000 }, 100), "invalid live handle cannot prepare work");
    expect(!ledger.Prepare("expired", read, 1000) && !ledger.Prepare("", read, 100), "expired or nameless requests are rejected");
    expect(ledger.Prepare("read", read, 100) && ledger.Prepare("position", position, 100),
        "one arousal request and one position request can coexist");
    expect(!ledger.Prepare("another-read", read, 100) && !ledger.Prepare("read", position, 100),
        "duplicate tokens and repeated actor/kind work are rejected");
    expect(!ledger.Prepare("over-capacity", { otherLease, 18, Kind::arousal, 1000 }, 100),
        "global request count is bounded");
    expect(ledger.Current("read", Kind::arousal, actorID, handle, 999), "correct actor and kind pass the native guard");
    const auto target = ledger.Read("read", Kind::arousal, 200);
    expect(target && target->handle == handle && target->lease.Ticket().formID == actorID &&
        !ledger.Read("read", Kind::position, 200) && !ledger.Read("read", Kind::arousal, 1000),
        "native target resolution is tied to the stored handle, action and deadline");
    expect(!ledger.Current("read", Kind::position, actorID, handle, 200) &&
        !ledger.Current("read", Kind::arousal, otherID, handle, 200) &&
        !ledger.Current("read", Kind::arousal, actorID, 99, 200) &&
        !ledger.Current("read", Kind::arousal, actorID, handle, 1000),
        "wrong action, FormID, live handle and expired request all fail the guard");

    // Ownership is an engine mutation, so it shares the actor's single mutation
    // slot with position work and can never authorize player-only dispatch.
    ActorRequestLedger ownershipLedger{ 3 };
    expect(ownershipLedger.Prepare("own", { lease, handle, Kind::ownership, 1000 }, 100),
        "managed actor can prepare an ownership handoff");
    expect(!ownershipLedger.Prepare("own-position", { lease, handle, Kind::position, 1000 }, 100),
        "ownership and position work cannot both hold the actor's mutation slot");
    expect(ownershipLedger.Current("own", Kind::ownership, actorID, handle, 200) &&
        !ownershipLedger.Current("own", Kind::position, actorID, handle, 200),
        "ownership guard does not authorize a position dispatch");
    expect(ownershipLedger.Finish("own", 200).has_value(),
        "ownership completion is delivered to the waiting caller");
    expect(ownershipLedger.Prepare("own-position", { lease, handle, Kind::position, 1000 }, 300),
        "position work resumes once ownership retires");

    PapyrusOperationGate gate;
    expect(gate.Prepare("position") && gate.Enter("position"), "NPC mutation acquires the existing engine gate");
    const auto cancelled = ledger.Cancel(*loaded);
    expect(cancelled.size() == 1 && cancelled[0] == "position", "cancellation identifies only that actor's mutation token");
    for (const auto& token : cancelled) gate.Cancel(token);
    expect(!ledger.Current("position", Kind::position, actorID, handle, 200) &&
        !ledger.Current("read", Kind::arousal, actorID, handle, 200), "cancellation rejects both kinds of stale work");
    expect(gate.Busy() && !gate.Prepare("player") && !ledger.Prepare("replacement", read, 200),
        "cancelled work retains its engine lease and request slots until retirement");
    expect(!ledger.Finish("read", 200) && !ledger.Finish("position", 200),
        "cancelled callbacks retire without delivering a result");
    gate.Complete("position");
    expect(gate.Prepare("player") && gate.Enter("player"), "player handoff can proceed once the NPC stack retires");
    gate.Cancel("position");
    expect(gate.Current("player"), "late NPC cancellation cannot cancel a newer player's operation");
    gate.Complete("player");

    expect(ledger.Prepare("before-unload", read, 200), "new query can start after previous work retires");
    const auto unloaded = registry.SetLoaded(*loaded, false);
    expect(unloaded && !lease.Valid() && !ledger.Current("before-unload", Kind::arousal, actorID, handle, 201),
        "3D loss invalidates worker-visible permission without accessing a context");
    if (!unloaded) return failures + 1;
    const auto reloaded = registry.SetLoaded(*unloaded, true);
    if (!reloaded) return failures + 1;
    const auto newLease = registry.Lease(*reloaded);
    const ActorRequest newRead{ newLease, handle, Kind::arousal, 2000 };
    expect(!ledger.Prepare("too-soon", newRead, 300), "reloaded actor cannot pile queries behind an old suspended stack");
    expect(!ledger.Finish("before-unload", 300) && ledger.Prepare("new-read", newRead, 300),
        "old completion is discarded before replacement query becomes available");
    const auto delivered = ledger.Finish("new-read", 400);
    expect(delivered && delivered->lease.Valid(), "valid completion preserves the original callback identity");
    expect(registry.Remove(*reloaded), "actor can be removed after completion was queued");
    expect(delivered && !delivered->lease.Valid(), "removal also invalidates a result already handed to a queued game task");

    const auto readded = registry.SetLoaded(registry.Add(actorID).ticket, true);
    if (!readded) return failures + 1;
    const auto finalLease = registry.Lease(*readded);
    expect(ledger.Prepare("old-save", { finalLease, handle, Kind::position, 3000 }, 1000), "pre-save request is registered");
    registry.ResetSession();
    ledger.ResetSession();
    expect(!finalLease.Valid() && ledger.Size() == 0 && !ledger.Finish("old-save", 1001),
        "session reset rejects pending callbacks and cancels retained views");

    ActorWorkLease afterOwnerDestruction;
    {
        ManagedActorRegistry temporary{ 1 };
        const auto temporaryLoaded = temporary.SetLoaded(temporary.Add(actorID).ticket, true);
        if (!temporaryLoaded) return failures + 1;
        afterOwnerDestruction = temporary.Lease(*temporaryLoaded);
        expect(afterOwnerDestruction.Valid(), "temporary owner grants a valid view");
    }
    expect(!afterOwnerDestruction.Valid(), "destroying the owner cancels retained views without dangling controller pointers");
    ledger.Discard("unknown-token");
    expect(ledger.Size() == 0, "late disposal of an absent token is harmless");
    return failures;
}
