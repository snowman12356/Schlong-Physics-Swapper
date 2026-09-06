#pragma once

#include "SceneState.h"

#include <RE/Skyrim.h>

#include <cstdint>
#include <functional>
#include <string>

namespace SPS::Controllers {

struct OwnerSnapshot {
    bool known{ false };
    bool usingCBPC{ false };
};

class SceneController {
public:
    using ReadyHook = std::function<bool()>;
    using RecordHook = std::function<void(std::string, bool)>;
    using ActionHook = std::function<void()>;
    using OwnerHook = std::function<OwnerSnapshot()>;
    using SexLabTransitionHook = std::function<void(bool, bool, std::int64_t)>;

    void Configure(
        ReadyHook papyrusReady,
        RecordHook record,
        ActionHook evaluate,
        OwnerHook owner,
        SexLabTransitionHook sexLabTransition);

    void QuerySexLab();
    void QuerySexLabRole();
    void QueryOStimRole();
    void InvalidateQueries();
    void ResetSession();
    void ResetPPA(int ignoreMs = 0);

    [[nodiscard]] SceneState& State();
    [[nodiscard]] const SceneState& State() const;

    void AcceptSexLabResult(std::uint64_t generation, int result);
    void AcceptSexLabRoleResult(std::uint64_t generation, int result);
    void AcceptOStimRoleResult(std::uint64_t generation, int result);

private:
    void QueueEvaluation(bool querySexLabRole = false);
    void Record(std::string message, bool error = false) const;

    SceneState state_{};
    ReadyHook papyrusReady_;
    RecordHook record_;
    ActionHook evaluate_;
    OwnerHook owner_;
    SexLabTransitionHook sexLabTransition_;
};

}
