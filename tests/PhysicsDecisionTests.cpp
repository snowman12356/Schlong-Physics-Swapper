#include "PhysicsDecision.h"
#include "SettingsStore.h"
#include "api/ExternalControlRegistry.h"
#include "controllers/PositionController.h"
#include "controllers/PhysicsOwnershipController.h"
#include "controllers/RecoveryController.h"
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
