#include "PhysicsDecision.h"
#include "SettingsStore.h"
#include "api/ExternalControlRegistry.h"
#include "controllers/PositionController.h"
#include "controllers/PhysicsOwnershipController.h"
#include "controllers/RecoveryController.h"
#include "controllers/ExecutionClock.h"
#include "controllers/PapyrusOperationGate.h"
#include "controllers/SceneState.h"
#include <limits>
#include "diagnostics/ActivityLog.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

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
    SPS::Core::Settings settings;
    SPS::Core::NormalDecisionState state;

    settings.mode = 1;
    state.arousal = 100.0F;
    state.arousalValid = true;
    Expect(!SPS::Core::NormalSettingsWantCBPC(settings, state), "manual soft always requests SMP");

    settings.mode = 2;
    state.arousal = 0.0F;
    Expect(SPS::Core::NormalSettingsWantCBPC(settings, state), "manual erect always requests CBPC");

    settings.mode = 0;
    state = {};
    Expect(!SPS::Core::NormalSettingsWantCBPC(settings, state), "unknown startup state defaults safely to SMP");
    state.ownerKnown = true;
    state.usingCBPC = true;
    Expect(SPS::Core::NormalSettingsWantCBPC(settings, state), "invalid arousal preserves known owner");

    state = {};
    state.arousalValid = true;
    state.arousal = 59.9F;
    Expect(!SPS::Core::NormalSettingsWantCBPC(settings, state), "below threshold stays SMP");
    state.arousal = 60.0F;
    Expect(SPS::Core::NormalSettingsWantCBPC(settings, state), "threshold switches to CBPC");

    state.usingCBPC = true;
    state.arousal = 55.0F;
    Expect(!SPS::Core::NormalSettingsWantCBPC(settings, state), "hysteresis boundary returns to SMP");
    state.arousal = 55.1F;
    Expect(SPS::Core::NormalSettingsWantCBPC(settings, state), "hysteresis prevents early return");

    settings.arousalBasedErection = true;
    settings.erectionStartArousal = 20.0F;
    state.usingCBPC = false;
    state.arousal = 19.9F;
    Expect(!SPS::Core::NormalSettingsWantCBPC(settings, state), "gradual erection waits for its start point");
    state.arousal = 20.0F;
    Expect(SPS::Core::NormalSettingsWantCBPC(settings, state), "gradual erection starts at configured arousal");

    state.arousal = 0.0F;
    state.spontaneousErectionActive = true;
    Expect(SPS::Core::NormalSettingsWantCBPC(settings, state), "spontaneous erection requests CBPC");

    SPS::Core::SexLabDecisionState sexLab;
    sexLab.currentCBPC = false;
    settings.sexLabRoleSwitching = false;
    Expect(SPS::Core::SexLabSceneWantsCBPC(settings, sexLab), "disabled SexLab role switching keeps legacy erect behaviour");

    settings.sexLabRoleSwitching = true;
    sexLab.active = false;
    sexLab.currentCBPC = true;
    Expect(SPS::Core::SexLabSceneWantsCBPC(settings, sexLab), "SexLab end delay preserves confirmed owner");

    sexLab.active = true;
    sexLab.roleValid = true;
    sexLab.role = SPS::Core::SceneRole::penetrating;
    Expect(SPS::Core::SexLabSceneWantsCBPC(settings, sexLab), "SexLab penetrating role requests CBPC");

    settings.sexLabBottomBehavior = 2;
    sexLab.role = SPS::Core::SceneRole::receiving;
    Expect(!SPS::Core::SexLabSceneWantsCBPC(settings, sexLab), "SexLab receiving role can force SMP");

    settings.sexLabBottomBehavior = 0;
    sexLab.entryStateValid = true;
    sexLab.entryCBPC = true;
    Expect(SPS::Core::SexLabSceneWantsCBPC(settings, sexLab), "SexLab receiving role preserves entry owner");

    sexLab.role = SPS::Core::SceneRole::receiving;
    sexLab.entryCBPC = false;
    sexLab.recentPPARoleValid = true;
    sexLab.recentPPARole = SPS::Core::SceneRole::penetrating;
    Expect(!SPS::Core::SexLabSceneWantsCBPC(settings, sexLab), "valid SexLab receiving role wins conflicting PPA role");

    sexLab.role = SPS::Core::SceneRole::penetrating;
    sexLab.recentPPARole = SPS::Core::SceneRole::receiving;
    Expect(SPS::Core::SexLabSceneWantsCBPC(settings, sexLab), "valid SexLab penetrating role wins conflicting PPA role");

    sexLab.role = SPS::Core::SceneRole::unknown;
    sexLab.recentPPARole = SPS::Core::SceneRole::penetrating;
    sexLab.currentCBPC = false;
    Expect(!SPS::Core::SexLabSceneWantsCBPC(settings, sexLab), "valid unknown SexLab role preserves the configured fallback despite PPA");

    sexLab.roleValid = false;
    Expect(SPS::Core::SexLabSceneWantsCBPC(settings, sexLab), "PPA penetrating role is used when the SexLab role query is unavailable");

    settings.sexLabBottomBehavior = 2;
    sexLab.recentPPARole = SPS::Core::SceneRole::receiving;
    Expect(!SPS::Core::SexLabSceneWantsCBPC(settings, sexLab), "PPA receiving role is used when the SexLab role query is unavailable");

    sexLab.recentPPARoleValid = false;
    settings.sexLabUnknownRole = 1;
    Expect(!SPS::Core::SexLabSceneWantsCBPC(settings, sexLab), "unknown SexLab role can force SMP");
    settings.sexLabUnknownRole = 2;
    Expect(SPS::Core::SexLabSceneWantsCBPC(settings, sexLab), "unknown SexLab role can force CBPC");

    SPS::Core::SceneDecisionState ostim;
    settings.ostimRoleSwitching = true;
    settings.sexLabUnknownRole = 0;
    ostim.active = true;
    ostim.currentCBPC = false;
    ostim.roleValid = true;
    ostim.role = SPS::Core::SceneRole::penetrating;
    Expect(SPS::Core::OStimSceneWantsCBPC(settings, ostim), "OStim penetrating role requests CBPC");
    settings.sexLabBottomBehavior = 3;
    ostim.role = SPS::Core::SceneRole::receiving;
    Expect(SPS::Core::OStimSceneWantsCBPC(settings, ostim), "OStim receiving role follows configured behaviour");
    settings.ostimRoleSwitching = false;
    Expect(SPS::Core::OStimSceneWantsCBPC(settings, ostim), "disabled OStim role switching keeps legacy erect behaviour");

    const auto testRoot = std::filesystem::temp_directory_path() /
        ("sps-core-tests-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto currentIni = testRoot / "SchlongPhysicsSwapper.ini";
    const auto legacyIni = testRoot / "UBEPhysicsSwitch.ini";

    SPS::Core::Settings saved;
    saved.enabled = false;
    saved.threshold = 73.5F;
    saved.hysteresis = 7.5F;
    saved.mode = 2;
    saved.erectBend = 18;
    saved.flaccidAngleControl = true;
    saved.flaccidBend = 3;
    saved.pollMs = 2250;
    saved.sexLabBottomBehavior = 3;
    saved.ostimRoleSwitching = false;
    saved.erectionStartArousal = 31.5F;
    saved.randomErections = true;
    saved.verboseLogging = true;
    Expect(SPS::Core::SaveSettings(currentIni, saved), "settings save succeeds");
    const auto roundTrip = SPS::Core::LoadSettings(currentIni, legacyIni);
    Expect(roundTrip.settings == saved, "settings round trip preserves every value");
    Expect(!roundTrip.migratedLegacy && !roundTrip.shouldWriteCurrent, "current settings do not trigger migration");

    std::filesystem::remove(currentIni);
    saved.threshold = 42.0F;
    Expect(SPS::Core::SaveSettings(legacyIni, saved), "legacy settings save succeeds");
    const auto migrated = SPS::Core::LoadSettings(currentIni, legacyIni);
    Expect(migrated.settings == saved, "legacy settings load preserves values");
    Expect(migrated.migratedLegacy && migrated.shouldWriteCurrent, "legacy settings request one-time migration");

    std::filesystem::create_directories(testRoot);
    {
        std::ofstream invalid(currentIni);
        invalid << "[Migration]\nPreferLegacyIfPresent=false\n"
                   "[General]\nArousalThreshold=999\nHysteresis=-10\nMode=99\nPollMilliseconds=1\n"
                   "[Position]\nErectBend=99\nFlaccidBend=-5\nMaxAutomaticFailures=0\n";
    }
    const auto clamped = SPS::Core::LoadSettings(currentIni, legacyIni).settings;
    Expect(clamped.threshold == 100.0F, "threshold clamps to supported maximum");
    Expect(clamped.hysteresis == 0.0F, "hysteresis clamps to supported minimum");
    Expect(clamped.mode == 2, "mode clamps to supported range");
    Expect(clamped.pollMs == 250, "poll interval clamps to safe minimum");
    Expect(clamped.flaccidBend == 0, "soft bend clamps to supported minimum");
    Expect(clamped.maxBendFailures == 1, "automatic failures clamp to safe minimum");
    std::filesystem::remove_all(testRoot);

    SPS::Diagnostics::ActivityLog activity("starting");
    activity.Record("normal event", false);
    activity.Record("SPS-010: owner failed", true);
    auto activitySnapshot = activity.Read();
    Expect(activitySnapshot.lastAction == "SPS-010: owner failed", "activity stores the latest action");
    Expect(activitySnapshot.lastError == "SPS-010: owner failed", "activity stores the latest error");
    Expect(!activity.ClearErrorWithPrefixes({ "SPS-002:" }), "unrelated recovery does not clear an error");
    Expect(activity.ClearErrorWithPrefixes({ "SPS-010:", "SPS-020:" }), "matching recovery clears an error");
    for (int index = 0; index < 20; ++index) {
        activity.Record("event " + std::to_string(index), false);
    }
    activitySnapshot = activity.Read();
    Expect(activitySnapshot.recent.size() == 12, "activity history remains bounded");

    SPS::Controllers::ActorContext actorContext(0x14);
    SPS::Controllers::PositionController position(actorContext.position);
    SPS::Controllers::RecoveryController recovery(actorContext);

    Expect(position.CheckAutomatic(1000, true).allowed, "first automatic position repair is allowed");
    Expect(position.CheckAutomatic(1100, true).allowed, "second automatic position repair is allowed");
    Expect(position.CheckAutomatic(1200, true).allowed, "third automatic position repair is allowed");
    const auto bouncePause = position.CheckAutomatic(1300, true);
    Expect(!bouncePause.allowed && bouncePause.pauseStarted,
        "bounce guard pauses the fourth rapid automatic repair");
    Expect(!position.CheckAutomatic(2000, true).allowed,
        "bounce guard rejects repairs during its pause");
    Expect(position.CheckAutomatic(6300, true).allowed,
        "bounce guard resumes repairs after its pause");

    auto dispatch = position.CompleteDispatch(12, 0, false, true, 2, 7000);
    Expect(!dispatch.pauseStarted && actorContext.position.retryDueMs.load() == 8500,
        "first failed automatic position dispatch uses the short retry");
    dispatch = position.CompleteDispatch(12, 0, false, true, 2, 7100);
    Expect(dispatch.pauseStarted && actorContext.position.automaticSuspended.load() &&
        actorContext.position.retryDueMs.load() == 12100,
        "bounded position failures suspend automatic repair once");
    dispatch = position.CompleteDispatch(12, 0, true, true, 2, 13000);
    Expect(dispatch.recovered && actorContext.position.appliedBend.load() == 12 &&
        !actorContext.position.automaticSuspended.load(),
        "successful position dispatch clears the failure suspension");

    const auto animationGeneration = position.BeginAnimation(18, true, 14000);
    auto positionSnapshot = position.Read();
    Expect(positionSnapshot.animating && positionSnapshot.relaxing &&
        positionSnapshot.animationTargetBend == 18 && animationGeneration > 0,
        "position controller owns animation startup state");
    position.AcceptAnimationStep(10, 1, 14500);
    position.CompleteAnimation(18);
    positionSnapshot = position.Read();
    Expect(!positionSnapshot.animating && positionSnapshot.appliedBend == 18 &&
        positionSnapshot.lastMethod == 1,
        "position controller owns successful animation completion state");
    position.CancelAnimation();
    Expect(!position.Read().relaxing,
        "cancelling an animation clears its relaxation state");

    actorContext.physics.lastSwitchMs.store(1);
    actorContext.physics.retryAfterMs.store(2);
    actorContext.position.settleDueMs.store(3);
    actorContext.recovery.loadSmpResetDueMs.store(4);
    actorContext.recovery.softConfirmationDueMs.store(5);
    actorContext.recovery.nodeRefreshDueMs.store(6);
    actorContext.recovery.startupReconcileDueMs.store(7);
    actorContext.recovery.externalOwnerRepairDueMs.store(8);
    actorContext.recovery.ignoreNodeEventsUntilMs.store(9);
    recovery.ResetTransient();
    Expect(actorContext.physics.lastSwitchMs.load() == 0 &&
        actorContext.physics.retryAfterMs.load() == 0 &&
        actorContext.position.settleDueMs.load() == 0 &&
        actorContext.recovery.loadSmpResetDueMs.load() == 0 &&
        actorContext.recovery.softConfirmationDueMs.load() == 0 &&
        actorContext.recovery.nodeRefreshDueMs.load() == 0 &&
        actorContext.recovery.startupReconcileDueMs.load() == 0 &&
        actorContext.recovery.externalOwnerRepairDueMs.load() == 0 &&
        actorContext.recovery.ignoreNodeEventsUntilMs.load() == 0,
        "recovery controller clears actor-scoped transient timers together");
    recovery.ScheduleExternalOwnerRepair(20000, 1);
    Expect(actorContext.recovery.externalOwnerRepairDueMs.load() == 20100 &&
        actorContext.recovery.externalOwnerRepairUntilMs.load() == 30100,
        "external owner repair scheduling keeps bounded delays");
    Expect(!recovery.ClaimExternalOwnerRepair(20099) &&
        actorContext.recovery.externalOwnerRepairDueMs.load() == 20100,
        "recovery timers remain queued before their due time");
    Expect(recovery.ClaimExternalOwnerRepair(20100) &&
        actorContext.recovery.externalOwnerRepairDueMs.load() == 0 &&
        !recovery.ClaimExternalOwnerRepair(20100),
        "a due recovery timer can be claimed exactly once");

    actorContext.recovery.softConfirmationDueMs.store(21000);
    Expect(!recovery.ClaimSoftConfirmation(22000, true) &&
        actorContext.recovery.softConfirmationDueMs.load() == 21000,
        "soft confirmation remains queued while CBPC owns the actor");
    Expect(recovery.ClaimSoftConfirmation(22000, false) &&
        actorContext.recovery.softConfirmationDueMs.load() == 0,
        "soft confirmation is claimed after SMP becomes the owner");

    actorContext.recovery.cbpcConfirmationDueMs.store(23000);
    Expect(!recovery.ClaimCbpcConfirmation(24000, false) &&
        actorContext.recovery.cbpcConfirmationDueMs.load() == 23000,
        "CBPC confirmation remains queued while SMP owns the actor");
    Expect(recovery.ClaimCbpcConfirmation(24000, true) &&
        actorContext.recovery.cbpcConfirmationDueMs.load() == 0,
        "CBPC confirmation is claimed after CBPC becomes the owner");

    actorContext.position.settleDueMs.store(25000);
    Expect(!position.ClaimSettle(26000, false, true) &&
        !position.ClaimSettle(26000, true, false) &&
        actorContext.position.settleDueMs.load() == 25000,
        "position settle remains queued until ownership and intent agree");
    Expect(position.ClaimSettle(26000, true, true) &&
        actorContext.position.settleDueMs.load() == 0,
        "position settle is claimed once ownership and intent agree");

    actorContext.position.confirmationDueMs.store(27000);
    Expect(!position.ClaimConfirmation(26999, true, true) &&
        actorContext.position.confirmationDueMs.load() == 27000,
        "position confirmation remains queued before its due time");
    Expect(position.ClaimConfirmation(27000, true, true) &&
        actorContext.position.confirmationDueMs.load() == 0,
        "position confirmation is claimed exactly once");

    SPS::Controllers::PhysicsOwnershipController ownership(actorContext.physics);
    const auto firstOwnerRequest = ownership.Begin(
        true, SPS::Controllers::OwnershipPurpose::switchOwner, false, 30000);
    Expect(firstOwnerRequest.status == SPS::Controllers::OwnershipBeginStatus::started,
        "ownership controller starts one staged handoff");
    Expect(ownership.Begin(
        true, SPS::Controllers::OwnershipPurpose::switchOwner, false, 30010).status ==
        SPS::Controllers::OwnershipBeginStatus::alreadyPending,
        "duplicate ownership target reuses the pending handoff");
    Expect(ownership.Begin(
        false, SPS::Controllers::OwnershipPurpose::switchOwner, false, 30010).status ==
        SPS::Controllers::OwnershipBeginStatus::blocked,
        "opposite ownership target waits for the pending handoff");
    const auto ownerCompleted = ownership.Complete(
        firstOwnerRequest.generation, true, 30300);
    Expect(ownerCompleted.matched && ownerCompleted.success &&
        actorContext.physics.known.load() && actorContext.physics.usingCBPC.load() &&
        actorContext.physics.successes.load() == 1,
        "completed ownership callback commits the selected owner");

    // An unreported FSMP reset does not change SPS's cached owner. Maintenance
    // must remain eligible after confirmation has ended, without a new event.
    SPS::Controllers::PhysicsOwnershipState safeguardState;
    SPS::Controllers::PhysicsOwnershipController safeguard(safeguardState);
    using Purpose = SPS::Controllers::OwnershipPurpose;
    using BeginStatus = SPS::Controllers::OwnershipBeginStatus;
    Expect(!safeguard.CanMaintainCBPC(true, false, 30000),
        "unknown ownership cannot dispatch an SMP-off-only safeguard");
    const auto erect = safeguard.Begin(true, Purpose::switchOwner, false, 30000);
    Expect(!safeguard.CanMaintainCBPC(true, false, 30001),
        "safeguard cannot overtake an active handoff");
    Expect(safeguard.Complete(erect.generation, true, 30300).success,
        "safeguard setup completes the erect handoff");
    Expect(!safeguard.CanMaintainCBPC(true, false, 31299) &&
        safeguard.CanMaintainCBPC(true, false, 31300),
        "safeguard waits one second after transaction completion");
    Expect(!safeguard.CanMaintainCBPC(false, false, 90000) &&
        !safeguard.CanMaintainCBPC(true, true, 90000),
        "a receiving/soft decision or ongoing relaxation suppresses SMP-off even with cached CBPC");
    Expect(safeguard.CanMaintainCBPC(true, false, 90000),
        "cached erect owner still gets SMP-off after unreported dynamics resumption");
    Expect(safeguard.Begin(false, Purpose::maintainCBPC, false, 90000).status == BeginStatus::blocked,
        "SMP-off maintenance rejects a soft target");
    const auto maintain = safeguard.Begin(true, Purpose::maintainCBPC, false, 90000);
    Expect(maintain.status == BeginStatus::started &&
        !safeguard.CanMaintainCBPC(true, false, 91000),
        "one safeguard occupies the existing ownership transaction slot");
    Expect(safeguard.Begin(false, Purpose::resetMesh, false, 90001).status == BeginStatus::blocked,
        "reset cannot overlap a running SMP-off command");
    Expect(safeguard.Complete(maintain.generation, true, 90200).success &&
        safeguardState.known.load() && safeguardState.usingCBPC.load() &&
        safeguardState.successes.load() == 1 && safeguardState.lastSwitchMs.load() == 30300,
        "maintenance completion preserves owner, switch count and switch cooldown");
    Expect(!safeguard.CanMaintainCBPC(true, false, 91199) &&
        safeguard.CanMaintainCBPC(true, false, 91200),
        "repeated fast polls cannot flood maintenance dispatch");
    const auto delayedMaintain = safeguard.Begin(true, Purpose::maintainCBPC, false, 91200);
    Expect(safeguard.Expire(96200, 5000).matched && !safeguardState.known.load() &&
        !safeguard.CanMaintainCBPC(true, false, 98000),
        "timed-out maintenance requires full owner recovery instead of continuing SMP-off-only work");
    Expect(!safeguard.Complete(delayedMaintain.generation, true, 96300).matched,
        "late safeguard success cannot overwrite a timeout");
    const auto soft = safeguard.Begin(false, Purpose::switchOwner, false, 98000);
    Expect(safeguard.Complete(soft.generation, true, 98500).success &&
        !safeguard.CanMaintainCBPC(true, false, 100000),
        "soft/disabled owner cannot receive SMP-off maintenance");
    const auto recoveredErect = safeguard.Begin(true, Purpose::switchOwner, false, 101000);
    Expect(safeguard.Complete(recoveredErect.generation, true, 101300).success,
        "normal handoff restores eligibility after maintenance failure");
    const auto savedMaintain = safeguard.Begin(true, Purpose::maintainCBPC, false, 102300);
    safeguard.ResetPending();
    safeguardState.known.store(false);
    Expect(!safeguard.Complete(savedMaintain.generation, true, 103000).matched &&
        !safeguard.CanMaintainCBPC(true, false, 103000),
        "load invalidation rejects old maintenance callbacks and waits for a fresh owner");

    const auto staleOwnerRequest = ownership.Begin(
        false, SPS::Controllers::OwnershipPurpose::switchOwner, true, 31000);
    ownership.ResetPending();
    Expect(!ownership.Complete(staleOwnerRequest.generation, true, 31100).matched &&
        actorContext.physics.usingCBPC.load(),
        "late ownership callback cannot mutate a reset game session");
    const auto expiringOwnerRequest = ownership.Begin(
        false, SPS::Controllers::OwnershipPurpose::switchOwner, true, 32000);
    Expect(!ownership.Expire(32999, 1000).matched,
        "pending ownership handoff remains active before its timeout");
    const auto ownerExpired = ownership.Expire(33000, 1000);
    Expect(ownerExpired.matched && !ownerExpired.success && ownerExpired.softTransition &&
        !actorContext.physics.pending.load() && !actorContext.physics.known.load() &&
        actorContext.physics.usingCBPC.load() && actorContext.physics.failures.load() == 1,
        "timed-out ownership handoff invalidates confirmation without guessing a new owner");
    Expect(!ownership.Complete(expiringOwnerRequest.generation, true, 33100).matched,
        "callback arriving after ownership timeout is ignored");

    // The old relaxation branch required known=true after failure had set it false.
    Expect(SPS::Core::SoftHandoffNeedsRetry(false, false, true),
        "timed-out relaxation can reacquire unknown ownership");
    Expect(SPS::Core::SoftHandoffNeedsRetry(false, false, false),
        "unknown ownership retries even if its stale cache says SMP");
    Expect(!SPS::Core::SoftHandoffNeedsRetry(true, true, true) &&
        !SPS::Core::SoftHandoffNeedsRetry(false, true, false),
        "running relaxation and completed soft ownership do not restart the handoff");
    Expect(SPS::Core::MaintenanceWantsCBPC(false, 1, true),
        "erect test retains its pose at zero normal arousal");
    Expect(!SPS::Core::MaintenanceWantsCBPC(true, 0, true) &&
        SPS::Core::MaintenanceWantsCBPC(true, 0, false),
        "soft test overrides high arousal only during its visible test window");

    SPS::Controllers::ExecutionClock executionClock;
    const auto menuRequest = ownership.Begin(true,
        SPS::Controllers::OwnershipPurpose::switchOwner, false, executionClock.Now(40000));
    executionClock.SetPaused(40200, true);
    Expect(executionClock.Now(100000) == 40200 &&
        !ownership.Expire(executionClock.Now(100000), 5000).matched,
        "one minute in a paused menu does not expire a live Papyrus handoff");
    executionClock.SetPaused(100000, false);
    Expect(executionClock.Now(100300) == 40500 &&
        ownership.Complete(menuRequest.generation, true, executionClock.Now(100300)).success,
        "handoff resumes with its original active-time budget");

    SPS::Controllers::PapyrusOperationGate gate;
    Expect(gate.Prepare("old") && gate.Enter("old"), "old Papyrus stack acquires its lease");
    gate.Cancel();
    Expect(!gate.Current("old") && !gate.Prepare("new"),
        "timeout cancels old instructions without letting replacement overtake running stack");
    gate.Complete("old");
    Expect(gate.Prepare("new") && !gate.Enter("old") && gate.Enter("new"),
        "delayed old entry cannot execute after replacement starts");
    gate.Complete("old");
    Expect(gate.Current("new"), "late old callback cannot release replacement lease");
    Expect(!gate.BarrierPassed("new"), "reset restoration waits for the FSMP game task");
    gate.PassBarrier("old");
    Expect(!gate.BarrierPassed("new"), "stale reset barrier cannot acknowledge current rebuild");
    gate.PassBarrier("new");
    Expect(gate.BarrierPassed("new"), "current FSMP task barrier permits restoration");
    gate.ResetSession();
    Expect(!gate.Current("new") && !gate.Enter("new") && gate.Prepare("other-save"),
        "saved stacks cannot acquire a new session's operation");
    gate.Cancel();
    Expect(gate.Prepare("not-started") , "queued-only cancellation releases its slot");
    gate.Cancel();
    Expect(!gate.Enter("not-started"), "timed-out queued stack has no side effects when it finally starts");

    // A newer scene/arousal/disabled target must win even if the obsolete
    // stack reports success, or the request flips back before it has returned.
    for (const auto purpose : { Purpose::switchOwner, Purpose::confirmSoft,
             Purpose::confirmCBPC, Purpose::restore, Purpose::resetLoad,
             Purpose::resetSoft, Purpose::resetAngle, Purpose::resetMesh,
             Purpose::reconnectMesh, Purpose::maintainCBPC }) {
        for (const bool target : { false, true }) {
            if (purpose == Purpose::maintainCBPC && !target) continue;
            for (const bool entered : { false, true }) {
                SPS::Controllers::PhysicsOwnershipState latestState;
                SPS::Controllers::PhysicsOwnershipController latest(latestState);
                SPS::Controllers::PapyrusOperationGate latestGate;
                latestState.known.store(true);
                latestState.usingCBPC.store(target);
                latestState.smpConnected.store(true);
                latestState.cbpcConnected.store(true);
                const auto old = latest.Begin(target, purpose, !target, 1000);
                Expect(old.status == BeginStatus::started && latestGate.Prepare("obsolete"),
                    "supersession starts an ordered physics operation");
                if (entered) Expect(latestGate.Enter("obsolete"), "simulated bridge starts work");
                Expect(!latest.Supersede(target) && !latest.Read().pendingSuperseded,
                    "unchanged policy retains its current transaction");
                Expect(latest.Supersede(!target), "opposite policy supersedes every operation purpose");
                latestGate.Cancel();
                Expect(latest.Read().pending && latest.Read().pendingSuperseded &&
                    !latestState.known.load() && !latestState.smpConnected.load() &&
                    !latestState.cbpcConnected.load() && !latestGate.Current("obsolete"),
                    "supersession cancels future steps without guessing the partially changed owner");
                Expect(latestGate.Busy() == entered,
                    "only an entered cancelled stack retains its execution lease");
                Expect(!latest.Supersede(target) &&
                    latest.Begin(target, purpose, false, 1100).status == BeginStatus::blocked,
                    "rapid reversal cannot revive the obsolete request");
                const auto retired = latest.Complete(old.generation, true, 1200);
                Expect(retired.matched && retired.superseded && !retired.success &&
                    !latest.Read().pending && !latestState.known.load() &&
                    latestState.successes.load() == 0 && latestState.failures.load() == 0 &&
                    latestState.retryAfterMs.load() == 0,
                    "late obsolete success neither commits ownership nor reports an engine failure");
                latestGate.Complete("obsolete");
                Expect(!latestGate.Busy() && latestGate.Prepare("replacement") &&
                    latestGate.Enter("replacement"), "replacement runs after the old stack retires");
                const auto replacement = latest.Begin(!target, Purpose::switchOwner, false, 1300);
                Expect(!latest.Complete(old.generation, true, 1350).matched,
                    "old completion cannot overwrite replacement generation");
                Expect(latest.Complete(replacement.generation, true, 1400).success &&
                    latestState.known.load() && latestState.usingCBPC.load() == !target,
                    "fresh ordered handoff acknowledges the latest requested owner");
            }
        }
    }

    SPS::Controllers::PhysicsOwnershipState drainingState;
    SPS::Controllers::PhysicsOwnershipController draining(drainingState);
    SPS::Controllers::PapyrusOperationGate drainingGate;
    const auto drainingRequest = draining.Begin(true, Purpose::switchOwner, false, 1000);
    Expect(drainingGate.Prepare("draining") && drainingGate.Enter("draining") &&
        draining.Supersede(false), "superseded running stack awaits disposal");
    drainingGate.Cancel();
    Expect(draining.Expire(6000, 5000).superseded && drainingGate.Busy() &&
        !draining.Read().pending && !drainingGate.Prepare("too-early"),
        "controller timeout must not allow replacement to overtake a cancelled running stack");
    Expect(drainingState.failures.load() == 0 &&
        !draining.Complete(drainingRequest.generation, true, 6100).matched,
        "cancelled timeout and late callback do not publish obsolete results");
    drainingGate.Complete("draining");
    Expect(!drainingGate.Busy(), "retirement permits deferred dispatch without repeated failures");
    const auto beforeLoad = draining.Begin(true, Purpose::switchOwner, false, 7000);
    Expect(draining.Supersede(false), "pre-load operation is cancelled");
    draining.ResetPending();
    Expect(!draining.Read().pendingSuperseded &&
        !draining.Complete(beforeLoad.generation, true, 7100).matched,
        "load clears supersession state and rejects previous-save callbacks");

    Expect(SPS::Core::ManualPhysicsTestHoldsControl(0, true, false) &&
        SPS::Core::ManualPhysicsTestHoldsControl(1, true, false),
        "both queued tests hold normal control while their handoff is pending");
    Expect(!SPS::Core::ManualPhysicsTestHoldsControl(0, false, false) &&
        !SPS::Core::ManualPhysicsTestHoldsControl(2, true, false) &&
        !SPS::Core::ManualPhysicsTestHoldsControl(-1, true, false),
        "expired tests, repair and absent tests do not suppress normal control");
    Expect(SPS::Core::ManualPhysicsTestHoldsControl(-1, false, true),
        "completed manual test retains its existing visible test window");

    const auto resetting = ownership.Begin(false,
        SPS::Controllers::OwnershipPurpose::resetMesh, false, 50000);
    Expect(ownership.Begin(true, SPS::Controllers::OwnershipPurpose::switchOwner,
        false, 50100).status == SPS::Controllers::OwnershipBeginStatus::blocked,
        "opposite handoff waits for reset plus owner restoration to complete");
    Expect(ownership.Complete(resetting.generation, true, 51000).success,
        "reset transaction completes through the same ownership controller");
    recovery.ScheduleNodeRefreshFollowup(60000);
    Expect(recovery.RetryNodeRefreshFollowup(71999) &&
        !recovery.RetryNodeRefreshFollowup(72000) &&
        actorContext.recovery.nodeRefreshFollowupDueMs.load() == 0,
        "equipment follow-up cannot extend its deadline on every retry");
    Expect(!recovery.RetryNodeRefreshFollowup(73000), "expired follow-up stays stopped");
    recovery.ScheduleNodeRefreshFollowup(74000);
    Expect(recovery.RetryNodeRefreshFollowup(75000), "a new equipment episode can retry again");

    SPS::Controllers::SceneState scenes;
    scenes.sexLab.active.store(true);
    scenes.sexLab.threadID.store(4);
    scenes.sexLab.endedMs.store(20);
    scenes.sexLab.entryStateValid.store(true);
    scenes.ostim.active.store(true);
    const auto previousSceneGeneration = scenes.sexLab.queryGeneration.load();
    scenes.ResetSession();
    Expect(!scenes.sexLab.active.load() && scenes.sexLab.threadID.load() == -1 &&
        scenes.sexLab.endedMs.load() == 0 && !scenes.sexLab.entryStateValid.load() &&
        !scenes.ostim.active.load() && scenes.sexLab.queryGeneration.load() != previousSceneGeneration,
        "load reset removes old scene identity and invalidates its callbacks");
    Expect(SPS::Core::IsSexLabThreadEvent("StageStart") &&
        SPS::Core::IsSexLabPhysicsReloadEvent("StageStart") &&
        SPS::Core::IsSexLabPhysicsReloadEvent("AnimationEnd") &&
        !SPS::Core::IsSexLabThreadEvent("HookStageStart") &&
        !SPS::Core::IsSexLabThreadEvent("unrelated"),
        "native event routing uses SexLab's actual event names");
    Expect(SPS::Core::MatchesPlayerSceneThread("0", 0) &&
        SPS::Core::MatchesPlayerSceneThread("4", 4) &&
        !SPS::Core::MatchesPlayerSceneThread("5", 4) &&
        !SPS::Core::MatchesPlayerSceneThread("4", -1),
        "only a known matching player thread admits stage recovery");
    Expect(!SPS::Core::MatchesPlayerSceneThread("4junk", 4) &&
        !SPS::Core::MatchesPlayerSceneThread("", 0) &&
        !SPS::Core::MatchesPlayerSceneThread("-1", -1) &&
        !SPS::Core::MatchesPlayerSceneThread("999999999999999999", 4),
        "malformed or overflowing thread IDs cannot trigger player recovery");
    Expect(!SPS::Core::ValidArousalReading(-1.0F) &&
        !SPS::Core::ValidArousalReading(std::numeric_limits<float>::quiet_NaN()) &&
        !SPS::Core::ValidArousalReading(std::numeric_limits<float>::infinity()) &&
        SPS::Core::ValidArousalReading(0.0F).value_or(-1.0F) == 0.0F &&
        SPS::Core::ValidArousalReading(105.0F).value_or(-1.0F) == 100.0F,
        "missing/invalid arousal is distinct from genuine zero arousal");

    SPS::APIControl::ExternalControlRegistry registry;
    SPS::API::PhysicsRequest apiRequest;
    apiRequest.state = SPS::API::PhysicsState::SMP;
    apiRequest.requesterName = "first requester";
    const auto firstRequest = registry.Add(apiRequest, 1000);
    apiRequest.state = SPS::API::PhysicsState::CBPC;
    apiRequest.durationMilliseconds = 500;
    apiRequest.requesterName = "second requester";
    const auto secondRequest = registry.Add(apiRequest, 1000);
    Expect(registry.Active(1200)->handle == secondRequest.handle, "newest API request has priority");
    Expect(registry.Active(1600)->handle == firstRequest.handle, "expired API request releases automatically");
    Expect(registry.Release(firstRequest.handle).value_or("") == "first requester", "API release returns requester label");
    Expect(!registry.Release(firstRequest.handle).has_value(), "API request cannot be released twice");
    const auto registryStats = registry.GetStats();
    Expect(registryStats.accepted == 2 && registryStats.released == 2 && registryStats.active == 0,
        "API registry keeps accurate bounded counters");

    if (failures == 0) {
        std::cout << "All SPS physics-decision tests passed.\n";
    }
    return failures == 0 ? 0 : 1;
}
