#include "PhysicsDecision.h"

#include <algorithm>

namespace SPS::Core {

namespace {

bool BottomWantsCBPC(const Settings& settings, const SceneDecisionState& state)
{
    switch (settings.sexLabBottomBehavior) {
    case 1:
        return NormalSettingsWantCBPC(settings, state.normal);
    case 2:
        return false;
    case 3:
        return true;
    default:
        return state.entryStateValid ? state.entryCBPC :
                                       NormalSettingsWantCBPC(settings, state.normal);
    }
}

}

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

bool SexLabSceneWantsCBPC(const Settings& settings, const SexLabDecisionState& state)
{
    if (!settings.sexLabRoleSwitching) {
        return true;
    }
    if (!state.active) {
        return state.currentCBPC;
    }

    if (state.roleValid) {
        if (state.role == SceneRole::receiving) {
            return BottomWantsCBPC(settings, state);
        }
        if (state.role == SceneRole::penetrating) {
            return true;
        }
        if (settings.sexLabUnknownRole == 1) {
            return false;
        }
        if (settings.sexLabUnknownRole == 2) {
            return true;
        }
        return state.currentCBPC;
    }
    if (state.recentPPARoleValid) {
        if (state.recentPPARole == SceneRole::penetrating) {
            return true;
        }
        if (state.recentPPARole == SceneRole::receiving) {
            return BottomWantsCBPC(settings, state);
        }
    }
    if (settings.sexLabUnknownRole == 1) {
        return false;
    }
    if (settings.sexLabUnknownRole == 2) {
        return true;
    }
    return state.currentCBPC;
}

bool OStimSceneWantsCBPC(const Settings& settings, const SceneDecisionState& state)
{
    if (!settings.ostimRoleSwitching) {
        return true;
    }
    if (!state.active) {
        return state.currentCBPC;
    }
    if (state.roleValid) {
        if (state.role == SceneRole::receiving) {
            return BottomWantsCBPC(settings, state);
        }
        if (state.role == SceneRole::penetrating) {
            return true;
        }
    }
    if (settings.sexLabUnknownRole == 1) {
        return false;
    }
    if (settings.sexLabUnknownRole == 2) {
        return true;
    }
    return state.currentCBPC;
}

}
