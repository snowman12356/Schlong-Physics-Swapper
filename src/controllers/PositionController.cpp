#include "PositionController.h"

#include <algorithm>

namespace SPS::Controllers {

PositionController::PositionController(PositionState& state) : state_(state) {}

void PositionController::ResetAutomaticRecovery()
{
    state_.automaticSuspended.store(false);
    state_.consecutiveFailures.store(0);
    state_.failureReported.store(false);
    state_.retryDueMs.store(0);
    state_.guardCount.store(0);
    state_.guardUntilMs.store(0);
    state_.guardWindowStartMs.store(0);
}

AutomaticPositionResult PositionController::CheckAutomatic(
    std::int64_t now, bool bounceGuard)
{
    if (state_.automaticSuspended.load()) {
        if (now < state_.guardUntilMs.load()) {
            return {};
        }
        state_.automaticSuspended.store(false);
    }
    if (!bounceGuard) {
        return { true, false };
    }
    if (now < state_.guardUntilMs.load()) {
        return {};
    }
    const auto windowStart = state_.guardWindowStartMs.load();
    if (windowStart == 0 || now - windowStart > 3000) {
        state_.guardWindowStartMs.store(now);
        state_.guardCount.store(0);
    }
    if (state_.guardCount.fetch_add(1) + 1 > 3) {
        state_.guardUntilMs.store(now + 5000);
        state_.guardCount.store(0);
        return { false, true };
    }
    return { true, false };
}

PositionDispatchResult PositionController::CompleteDispatch(
    int bend, int method, bool accepted, bool automatic,
    int maxFailures, std::int64_t now)
{
    state_.lastMethod.store(method);
    if (accepted) {
        state_.appliedBend.store(bend);
        state_.lastApplyMs.store(now);
        state_.lastSucceeded.store(true);
        const int previousFailures = state_.consecutiveFailures.exchange(0);
        const bool failureWasReported = state_.failureReported.exchange(false);
        state_.retryDueMs.store(0);
        state_.automaticSuspended.store(false);
        return { previousFailures > 0 || failureWasReported, false };
    }

    state_.lastSucceeded.store(false);
    state_.retryDueMs.store(now + 1500);
    if (!automatic) {
        return {};
    }

    const int failures = state_.consecutiveFailures.fetch_add(1) + 1;
    if (failures < std::max(1, maxFailures)) {
        return {};
    }

    state_.consecutiveFailures.store(0);
    state_.guardUntilMs.store(now + 5000);
    state_.automaticSuspended.store(true);
    state_.retryDueMs.store(now + 5000);
    return { false, !state_.failureReported.exchange(true) };
}

void PositionController::CancelAnimation()
{
    state_.animating.store(false);
    state_.animationGeneration.fetch_add(1);
    state_.relaxing.store(false);
}

std::uint64_t PositionController::BeginAnimation(
    int targetBend, bool relaxing, std::int64_t now)
{
    CancelAnimation();
    state_.relaxing.store(relaxing);
    const auto generation = state_.animationGeneration.load();
    state_.animationTargetBend.store(targetBend);
    state_.animationLastQueuedBend.store(-1);
    state_.animationStartMs.store(now);
    state_.animating.store(true);
    state_.requestedBend.store(targetBend);
    state_.appliedBend.store(-1);
    return generation;
}

void PositionController::FailAnimationStart()
{
    state_.animating.store(false);
    state_.relaxing.store(false);
    state_.appliedBend.store(-1);
    state_.lastMethod.store(-1);
    state_.lastSucceeded.store(false);
}

void PositionController::FailAnimationStep(std::int64_t retryDueMs)
{
    CancelAnimation();
    state_.appliedBend.store(-1);
    state_.lastSucceeded.store(false);
    state_.retryDueMs.store(retryDueMs);
}

void PositionController::AcceptAnimationStep(
    int bend, int method, std::int64_t now)
{
    state_.appliedBend.store(bend);
    state_.lastMethod.store(method);
    state_.lastSucceeded.store(true);
    state_.retryDueMs.store(0);
    state_.consecutiveFailures.store(0);
    state_.failureReported.store(false);
    state_.automaticSuspended.store(false);
    state_.lastApplyMs.store(now);
}

void PositionController::CompleteAnimation(int targetBend)
{
    state_.animating.store(false);
    state_.appliedBend.store(targetBend);
}

void PositionController::CompleteRelaxationFallback(
    int targetBend, bool accepted, std::int64_t now)
{
    CancelAnimation();
    state_.relaxing.store(true);
    state_.requestedBend.store(targetBend);
    state_.appliedBend.store(targetBend);
    state_.lastSucceeded.store(accepted);
    state_.retryDueMs.store(0);
    state_.consecutiveFailures.store(0);
    state_.failureReported.store(false);
    if (accepted) {
        state_.lastMethod.store(1);
        state_.lastApplyMs.store(now);
    }
}

bool PositionController::ClaimSettle(
    std::int64_t now, bool usingCBPC, bool targetWantsCBPC)
{
    return usingCBPC && targetWantsCBPC && ClaimIfDue(state_.settleDueMs, now);
}

bool PositionController::ClaimConfirmation(
    std::int64_t now, bool usingCBPC, bool targetWantsCBPC)
{
    return usingCBPC && targetWantsCBPC && ClaimIfDue(state_.confirmationDueMs, now);
}

bool PositionController::ClaimIfDue(
    std::atomic<std::int64_t>& dueMs, std::int64_t now)
{
    auto due = dueMs.load();
    while (due > 0 && now >= due) {
        if (dueMs.compare_exchange_weak(due, 0)) {
            return true;
        }
    }
    return false;
}

PositionSnapshot PositionController::Read() const
{
    return {
        state_.appliedBend.load(),
        state_.requestedBend.load(),
        state_.lastMethod.load(),
        state_.animationTargetBend.load(),
        state_.lastSucceeded.load(),
        state_.automaticSuspended.load(),
        state_.animating.load(),
        state_.relaxing.load(),
        state_.repairs.load(),
        state_.lastApplyMs.load(),
        state_.guardUntilMs.load()
    };
}

}
