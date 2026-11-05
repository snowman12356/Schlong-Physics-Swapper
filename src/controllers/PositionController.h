#pragma once

#include "ActorContext.h"

#include <cstdint>

namespace SPS::Controllers {

struct AutomaticPositionResult {
    bool allowed{ false };
    bool pauseStarted{ false };
};

struct PositionDispatchResult {
    bool recovered{ false };
    bool pauseStarted{ false };
};

struct PositionSnapshot {
    int appliedBend{ -1 };
    int requestedBend{ -1 };
    int lastMethod{ -1 };
    int animationTargetBend{ 0 };
    bool lastSucceeded{ false };
    bool automaticSuspended{ false };
    bool animating{ false };
    bool relaxing{ false };
    unsigned repairs{ 0 };
    std::int64_t lastApplyMs{ 0 };
    std::int64_t guardUntilMs{ 0 };
};

class PositionController {
public:
    explicit PositionController(PositionState& state);

    void ResetAutomaticRecovery();
    [[nodiscard]] AutomaticPositionResult CheckAutomatic(
        std::int64_t now, bool bounceGuard);
    [[nodiscard]] PositionDispatchResult CompleteDispatch(
        int bend, int method, bool accepted, bool automatic,
        int maxFailures, std::int64_t now);

    void CancelAnimation();
    [[nodiscard]] std::uint64_t BeginAnimation(
        int targetBend, bool relaxing, std::int64_t now);
    void FailAnimationStart();
    void FailAnimationStep(std::int64_t retryDueMs);
    void AcceptAnimationStep(int bend, int method, std::int64_t now);
    void CompleteAnimation(int targetBend);
    void CompleteRelaxationFallback(
        int targetBend, bool accepted, std::int64_t now);

    [[nodiscard]] bool ClaimSettle(
        std::int64_t now, bool usingCBPC, bool targetWantsCBPC);
    [[nodiscard]] bool ClaimConfirmation(
        std::int64_t now, bool usingCBPC, bool targetWantsCBPC);

    [[nodiscard]] PositionSnapshot Read() const;

private:
    [[nodiscard]] static bool ClaimIfDue(
        std::atomic<std::int64_t>& dueMs, std::int64_t now);

    PositionState& state_;
};

}
