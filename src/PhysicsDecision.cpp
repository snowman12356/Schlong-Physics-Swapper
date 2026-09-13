#include "PhysicsDecision.h"

#include <algorithm>
#include <charconv>
#include <cmath>

namespace SPS::Core {

bool SoftHandoffNeedsRetry(bool animating, bool ownerKnown, bool usingCBPC)
{
    return !animating && (!ownerKnown || usingCBPC);
}

bool MaintenanceWantsCBPC(bool normalTarget, int manualTest, bool testActive)
{
    return testActive && manualTest >= 0 ? manualTest == 1 : normalTarget;
}

bool ManualPhysicsTestHoldsControl(int pendingAction, bool pendingActive, bool visibleActive)
{
    return visibleActive || (pendingActive && (pendingAction == 0 || pendingAction == 1));
}

std::optional<float> ValidArousalReading(float reading)
{
    if (!std::isfinite(reading) || reading < 0.0F) return std::nullopt;
    return std::clamp(reading, 0.0F, 100.0F);
}

bool IsSexLabThreadEvent(std::string_view name)
{
    return name == "AnimationStart" || name == "AnimationStarting" ||
        name == "AnimationChange" || name == "StageStart" || name == "StageEnd" ||
        name == "ActorsRelocated" || name == "ActorChangeEnd" ||
        name == "AnimationEnding" || name == "AnimationEnd";
}

bool IsSexLabPhysicsReloadEvent(std::string_view name)
{
    return name == "AnimationStart" || name == "AnimationChange" ||
        name == "StageStart" || name == "AnimationEnd";
}

bool MatchesPlayerSceneThread(std::string_view argument, int playerThread)
{
    if (argument.empty() || playerThread < 0) return false;
    int threadID = -1;
    const auto parsed = std::from_chars(argument.data(), argument.data() + argument.size(), threadID);
    return parsed.ec == std::errc{} && parsed.ptr == argument.data() + argument.size() &&
        threadID == playerThread;
}

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
