#pragma once

#include "Settings.h"
#include <optional>
#include <string_view>

namespace SPS::Core {

struct NormalDecisionState {
    float arousal{ 0.0F };
    bool arousalValid{ false };
    bool ownerKnown{ false };
    bool usingCBPC{ false };
    bool spontaneousErectionActive{ false };
};

enum class SceneRole : int {
    unknown = 0,
    receiving = 1,
    penetrating = 2
};

struct SceneDecisionState {
    bool active{ false };
    bool currentCBPC{ false };
    bool entryStateValid{ false };
    bool entryCBPC{ false };
    bool roleValid{ false };
    SceneRole role{ SceneRole::unknown };
    NormalDecisionState normal{};
};

struct SexLabDecisionState : SceneDecisionState {
    bool recentPPARoleValid{ false };
    SceneRole recentPPARole{ SceneRole::unknown };
};

// Pure policy: it decides the requested owner but never touches Skyrim,
// Papyrus, FSMP or CBPC. Keeping this separate makes the most important SPS
// behaviour testable without starting the game.
bool NormalSettingsWantCBPC(const Settings& settings, const NormalDecisionState& state);
bool SexLabSceneWantsCBPC(const Settings& settings, const SexLabDecisionState& state);
bool OStimSceneWantsCBPC(const Settings& settings, const SceneDecisionState& state);
bool SoftHandoffNeedsRetry(bool animating, bool ownerKnown, bool usingCBPC);
bool MaintenanceWantsCBPC(bool normalTarget, int manualTest, bool testActive);
bool ManualPhysicsTestHoldsControl(int pendingAction, bool pendingActive, bool visibleActive);
std::optional<float> ValidArousalReading(float reading);
bool IsSexLabThreadEvent(std::string_view name);
bool IsSexLabPhysicsReloadEvent(std::string_view name);
bool MatchesPlayerSceneThread(std::string_view argument, int playerThread);

}
