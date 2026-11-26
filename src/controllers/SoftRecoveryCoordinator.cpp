#include "SoftRecoveryCoordinator.h"

namespace SPS::Controllers {

void SoftRecoveryCoordinator::Request(std::string_view reason, bool bake, bool force, std::int64_t now)
{
    std::scoped_lock guard(lock_);
    if (!confirm_ && !bake_) {
        firstMs_ = now;
        merged_ = 0;
        dispatchFailures_ = 0;
        longWaitLogged_ = false;
        force_ = false;
        reasons_.clear();
    }
    if (bake) bake_ = true;
    else confirm_ = true;
    force_ = force_ || force;
    lastMs_ = now;
    ++sequence_;
    ++merged_;
    if (reasons_.find(reason) == std::string::npos) {
        if (!reasons_.empty()) reasons_ += ", ";
        reasons_ += reason;
    }
}

void SoftRecoveryCoordinator::ClearLocked()
{
    confirm_ = false;
    bake_ = false;
    force_ = false;
    merged_ = 0;
    dispatchFailures_ = 0;
    longWaitLogged_ = false;
    reasons_.clear();
}

void SoftRecoveryCoordinator::Clear()
{
    std::scoped_lock guard(lock_);
    ClearLocked();
}

SoftRecoveryCoordinator::Plan SoftRecoveryCoordinator::Evaluate(const Inputs& in)
{
    Plan plan;
    bool confirm = false, bake = false, force = false;
    std::int64_t firstMs = 0, lastMs = 0, overSince = 0;
    {
        std::scoped_lock guard(lock_);
        if (in.sceneBusy) sceneOverSinceMs_ = 0;
        else if (sceneOverSinceMs_ == 0) sceneOverSinceMs_ = in.now;
        overSince = sceneOverSinceMs_;
        if (!confirm_ && !bake_) return plan;
        // The max wait counts from when the scene was last over, not from a
        // request made during the scene.
        if (in.sceneBusy && !force_) firstMs_ = in.now;
        confirm = confirm_;
        bake = bake_;
        force = force_;
        firstMs = firstMs_;
        lastMs = lastMs_;
        plan.bakeRequested = bake_;
        plan.sequence = sequence_;
        plan.merged = merged_;
        plan.reasons = reasons_;
        if (!in.enabled || (in.ownerKnown && in.ownerCBPC)) {
            // Erect now (or SPS off): the soft recovery no longer applies.
            ClearLocked();
            return plan;
        }
    }

    const bool sceneSettled = !in.sceneBusy && overSince > 0 && in.now - overSince >= kSceneSettleMs;
    const bool quiet = in.now - lastMs >= kQuietMs || in.now - firstMs >= kMaxWaitMs;
    // A rebuild waits for the scene; a plain check does not.
    const bool bakeNow = bake && (force || sceneSettled);
    const bool confirmNow = !bakeNow && confirm;
    const char* waitingFor = !in.ownerKnown ? "the physics owner" :
        (!bakeNow && !confirmNow) ? (in.sceneBusy ? in.sceneBusy : "the scene cleanup") :
        !quiet ? nullptr :
        in.handoffPending ? "the current physics handoff" :
        in.animating ? "the angle animation" :
        !in.papyrusReady ? "Papyrus" : nullptr;
    if (waitingFor || !quiet) {
        plan.action = Action::wait;
        plan.waitingFor = waitingFor;
        if (waitingFor && in.now - firstMs >= kLongWaitMs) {
            std::scoped_lock guard(lock_);
            if (!longWaitLogged_) {
                longWaitLogged_ = true;
                plan.logLongWait = true;
            }
        }
        return plan;
    }
    plan.action = bakeNow ? Action::bake : Action::confirm;
    return plan;
}

void SoftRecoveryCoordinator::OnDispatched(const Plan& plan, bool didBake)
{
    std::scoped_lock guard(lock_);
    if (sequence_ == plan.sequence) {
        // Everything requested so far is covered by this transaction. A bake
        // stops CBPC and rebuilds SMP, so it also covers any plain check.
        confirm_ = false;
        if (didBake || !plan.bakeRequested) bake_ = false;
        if (!bake_) {
            force_ = false;
            merged_ = 0;
            reasons_.clear();
        }
    } else if (didBake) {
        // Newer triggers keep waiting for another round, but this bake already
        // covered everything before them.
        confirm_ = false;
    }
}

bool SoftRecoveryCoordinator::OnDispatchFailed(bool ownerKnown)
{
    // A refusal while busy is retried next round. A dispatch that started and
    // failed marks the owner unknown: count it so it cannot loop forever.
    if (ownerKnown) return false;
    std::scoped_lock guard(lock_);
    if (++dispatchFailures_ >= kMaxDispatchFailures) {
        confirm_ = false;
        bake_ = false;
        return true;
    }
    return false;
}

SoftRecoveryCoordinator::ProofFollowUp SoftRecoveryCoordinator::OnSmpProof(bool certain, std::int64_t now)
{
    std::scoped_lock guard(lock_);
    if (certain) {
        smpReproofs_ = 0;
        return ProofFollowUp::none;
    }
    if (now - lastReproofMs_ > kRetryDecayMs) smpReproofs_ = 0;
    lastReproofMs_ = now;
    const int attempt = ++smpReproofs_;
    if (attempt <= kMaxRetries) return ProofFollowUp::recheck;
    return attempt == kMaxRetries + 1 ? ProofFollowUp::giveUp : ProofFollowUp::none;
}

bool SoftRecoveryCoordinator::AllowBakeRetry(std::int64_t now)
{
    std::scoped_lock guard(lock_);
    if (now - lastFailedBakeMs_ > kRetryDecayMs) failedBakes_ = 0;
    lastFailedBakeMs_ = now;
    return ++failedBakes_ <= kMaxRetries;
}

void SoftRecoveryCoordinator::OnBakeSucceeded()
{
    std::scoped_lock guard(lock_);
    failedBakes_ = 0;
}

bool SoftRecoveryCoordinator::BakeClean(int dynamicBefore, int dynamicAfter, int boneCount,
    bool previousBakeAborted)
{
    // The SMP-off step must have found the shaft dynamic (so it reached the body
    // skeleton and the shaft took the resting clip), unless the previous bake
    // stopped early and may have left it kinematic; every bone must be dynamic
    // after the rebuild.
    return boneCount > 0 && dynamicAfter == boneCount &&
        (previousBakeAborted || dynamicBefore == boneCount);
}

bool SoftRecoveryCoordinator::CheckBake(int dynamicBefore, int dynamicAfter, int boneCount)
{
    std::scoped_lock guard(lock_);
    const bool clean = BakeClean(dynamicBefore, dynamicAfter, boneCount, lastBakeAborted_);
    lastBakeAborted_ = false;
    return clean;
}

bool SoftRecoveryCoordinator::CbpcReleaseWanted() const
{
    std::scoped_lock guard(lock_);
    return !cbpcReleased_ && cbpcReleaseAttempts_ < kMaxCbpcReleaseAttempts;
}

void SoftRecoveryCoordinator::OnCbpcReleaseQueued()
{
    std::scoped_lock guard(lock_);
    ++cbpcReleaseAttempts_;
}

void SoftRecoveryCoordinator::OnCbpcReleased()
{
    std::scoped_lock guard(lock_);
    cbpcReleased_ = true;
}

void SoftRecoveryCoordinator::ResetSession()
{
    std::scoped_lock guard(lock_);
    ClearLocked();
    failedBakes_ = 0;
    lastBakeAborted_ = false;
    smpReproofs_ = 0;
    cbpcReleased_ = false;
    cbpcReleaseAttempts_ = 0;
}

}
