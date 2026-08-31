#include "PhysicsDecision.h"

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

    if (failures == 0) {
        std::cout << "All SPS physics-decision tests passed.\n";
    }
    return failures == 0 ? 0 : 1;
}
