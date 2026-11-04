#include "SPSAPI.h"
#include "controllers/ManagedActorRegistry.h"
#include "controllers/PapyrusOperationGate.h"
#include "controllers/PhysicsOwnershipController.h"
#include "controllers/PositionController.h"
#include "controllers/RecoveryController.h"

#include <iostream>
#include <string_view>

int RunActorRequestLedgerTests();

namespace {
int failures = 0;
void Expect(bool condition, std::string_view name)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << name << '\n';
    }
}
}

int main()
{
    using namespace SPS::Controllers;
    using Status = ActorRegistrationStatus;
    constexpr std::uint32_t firstID = 0x1234;
    constexpr std::uint32_t secondID = 0x5678;

    ManagedActorRegistry disabled{ 0 };
    Expect(disabled.Add(firstID).status == Status::capacityReached,
        "zero capacity cannot enroll NPCs");
    ManagedActorRegistry registry{ 2 };
    Expect(registry.Add(0).status == Status::invalidActor &&
        registry.Add(SPS::API::kPlayerFormID).status == Status::invalidActor && registry.Size() == 0,
        "invalid IDs and the existing player context do not consume NPC slots");
    const auto first = registry.Add(firstID);
    const auto second = registry.Add(secondID);
    Expect(first.status == Status::added && second.status == Status::added && registry.Size() == 2,
        "two explicit NPC registrations are independent");
    Expect(registry.Resolve(first.ticket) == nullptr && !registry.Find(firstID)->loaded,
        "registration alone cannot issue work to unverified 3D");
    Expect(registry.Add(0x9000).status == Status::capacityReached,
        "unloaded membership still counts toward the hard limit");

    const auto firstLoaded = registry.SetLoaded(first.ticket, true);
    const auto secondLoaded = registry.SetLoaded(second.ticket, true);
    if (!firstLoaded || !secondLoaded) {
        std::cerr << "FAIL: loaded contexts could not be created\n";
        return 1;
    }
    Expect(registry.Resolve(first.ticket) == nullptr,
        "work from before 3D activation cannot enter the new context");
    auto* firstContext = registry.Resolve(*firstLoaded);
    auto* secondContext = registry.Resolve(*secondLoaded);
    Expect(firstContext && secondContext && firstContext != secondContext &&
        firstContext->formID == firstID && secondContext->formID == secondID,
        "live contexts carry the intended independent actor identity");
    if (!firstContext || !secondContext) return 1;
    {
        PhysicsOwnershipController firstOwner{ firstContext->physics };
        PhysicsOwnershipController secondOwner{ secondContext->physics };
        const auto erect = firstOwner.Begin(true, OwnershipPurpose::switchOwner, false, 100);
        const auto soft = secondOwner.Begin(false, OwnershipPurpose::switchOwner, false, 100);
        Expect(firstOwner.Complete(erect.generation, true, 200).success &&
            secondOwner.Complete(soft.generation, true, 200).success,
            "existing ownership controllers complete independently");
        Expect(firstOwner.Read().usingCBPC && !secondOwner.Read().usingCBPC &&
            firstOwner.Read().known && secondOwner.Read().known,
            "opposite actor states do not overwrite one another");
        PositionController firstPosition{ firstContext->position };
        const auto animation = firstPosition.BeginAnimation(14, false, 250);
        RecoveryController firstRecovery{ *firstContext };
        firstRecovery.ScheduleNodeRefreshFollowup(300);
        Expect(animation != 0 && firstContext->position.animating.load() && !secondContext->position.animating.load() &&
            firstContext->recovery.nodeRefreshFollowupDueMs.load() != 0 &&
            secondContext->recovery.nodeRefreshFollowupDueMs.load() == 0,
            "position and recovery work are actor-specific");
    }
    Expect(registry.Add(firstID).ticket == *firstLoaded &&
        registry.Add(firstID).status == Status::alreadyRegistered &&
        registry.Resolve(*firstLoaded)->physics.known.load(),
        "duplicate enrollment at capacity preserves the existing state");
    Expect(registry.SetLoaded(*firstLoaded, true) == firstLoaded,
        "duplicate loaded events preserve valid callbacks");
    Expect(registry.Resolve({ secondID, firstLoaded->generation }) == nullptr,
        "an actor generation cannot be used to address another actor");
    Expect(!registry.SetLoaded(first.ticket, false) && !registry.Remove(first.ticket),
        "outdated availability/removal requests cannot affect the current registration");

    const auto unloaded = registry.SetLoaded(*firstLoaded, false);
    Expect(unloaded && !registry.Resolve(*firstLoaded) && !registry.Find(firstID)->loaded && registry.Size() == 2,
        "unload invalidates work without losing managed membership");
    if (!unloaded) return 1;
    const auto reloaded = registry.SetLoaded(*unloaded, true);
    if (!reloaded) return 1;
    const auto* fresh = registry.Resolve(*reloaded);
    Expect(fresh && !fresh->physics.known.load() && !fresh->physics.pending.load() &&
        !fresh->position.animating.load() && fresh->recovery.nodeRefreshFollowupDueMs.load() == 0,
        "new 3D does not inherit cached ownership, animations or recovery timers");
    Expect(!registry.Resolve(*firstLoaded) && !registry.SetLoaded(*firstLoaded, false),
        "late callbacks and unload notices cannot mutate the replacement 3D context");
    Expect(registry.Resolve(*secondLoaded)->physics.known.load(),
        "one actor unloading leaves the other actor unchanged");

    PapyrusOperationGate engineGate;
    Expect(engineGate.Prepare("old-stack") && engineGate.Enter("old-stack"),
        "an existing engine stack holds its execution lease");
    engineGate.Cancel();
    Expect(registry.Remove(*reloaded), "explicit removal discards the retired native context");
    const auto readded = registry.Add(firstID);
    const auto newLoaded = registry.SetLoaded(readded.ticket, true);
    Expect(newLoaded && !registry.Resolve(*reloaded) && !registry.Remove(*reloaded),
        "remove/re-add cannot resurrect earlier actor work");
    if (!newLoaded) return 1;
    Expect(engineGate.Busy() && !engineGate.Prepare("replacement"),
        "registry changes do not force-release a cancelled running engine stack");
    engineGate.Complete("old-stack");
    Expect(engineGate.Prepare("replacement"), "replacement engine work waits for the old stack to retire");
    engineGate.Complete("replacement");

    const auto snapshot = registry.Snapshot();
    Expect(snapshot.size() == 2 && snapshot[0].ticket.formID == firstID &&
        snapshot[1].ticket.formID == secondID && snapshot[0].loaded && snapshot[1].loaded,
        "diagnostic snapshots contain a stable ordering and current availability");
    const auto priorSaveTicket = *newLoaded;
    registry.ResetSession();
    Expect(registry.Size() == 0 && registry.Snapshot().empty() && !registry.Resolve(priorSaveTicket) &&
        !registry.Find(secondID), "save reset clears membership and all old contexts");
    const auto nextSave = registry.Add(firstID);
    const auto nextSaveLoaded = registry.SetLoaded(nextSave.ticket, true);
    Expect(nextSaveLoaded && nextSaveLoaded != newLoaded && !registry.Resolve(priorSaveTicket) &&
        !registry.SetLoaded(priorSaveTicket, false) && !registry.Remove(priorSaveTicket),
        "same FormID in another save cannot accept old-save callbacks");
    const ManagedActorRegistry& readOnly = registry;
    Expect(nextSaveLoaded && readOnly.Resolve(*nextSaveLoaded) && !readOnly.Resolve(priorSaveTicket),
        "read-only lookup observes the same generation checks");

    failures += RunActorRequestLedgerTests();
    extern int RunBasicNpcPolicyTests();
    failures += RunBasicNpcPolicyTests();
    extern int RunBasicNpcPoolTests();
    failures += RunBasicNpcPoolTests();
    extern int RunPoseAngleTests();
    failures += RunPoseAngleTests();
    extern int RunPoseTargetsTests();
    failures += RunPoseTargetsTests();
    if (failures) return 1;
    std::cout << "All actor lifetime, request, NPC policy, pose angle and pose target tests passed\n";
    return 0;
}
