#pragma once

#include "Settings.h"

namespace SPS::Core {

struct NormalDecisionState {
    float arousal{ 0.0F };
    bool arousalValid{ false };
    bool ownerKnown{ false };
    bool usingCBPC{ false };
    bool spontaneousErectionActive{ false };
};

// Pure policy: it decides the requested owner but never touches Skyrim,
// Papyrus, FSMP or CBPC. Keeping this separate makes the most important SPS
// behaviour testable without starting the game.
bool NormalSettingsWantCBPC(const Settings& settings, const NormalDecisionState& state);

}
