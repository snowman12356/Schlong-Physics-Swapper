#include "PhysicsDecision.h"
#include "SettingsStore.h"

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
    sexLab.recentPPARoleValid = true;
    sexLab.recentPPARole = SPS::Core::SceneRole::penetrating;
    Expect(SPS::Core::SexLabSceneWantsCBPC(settings, sexLab), "recent PPA penetrating role wins transient disagreement");

    sexLab.roleValid = false;
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

    if (failures == 0) {
        std::cout << "All SPS physics-decision tests passed.\n";
    }
    return failures == 0 ? 0 : 1;
}
