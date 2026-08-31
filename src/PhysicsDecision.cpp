#include "PhysicsDecision.h"

#include <algorithm>

namespace SPS::Core {

bool NormalSettingsWantCBPC(const Settings& settings, const NormalDecisionState& state)
{
    if (settings.mode == 1) {
        return false;
    }
    if (settings.mode == 2) {
        return true;
    }
    if (state.spontaneousErectionActive) {
        return true;
    }
    if (!state.arousalValid) {
        return state.ownerKnown ? state.usingCBPC : false;
    }

    const float erectionPoint = settings.arousalBasedErection ?
        std::min(settings.threshold, std::clamp(settings.erectionStartArousal, 0.0F, 99.0F)) :
        settings.threshold;
    if (state.usingCBPC) {
        return state.arousal > erectionPoint - settings.hysteresis;
    }
    return state.arousal >= erectionPoint;
}

}
