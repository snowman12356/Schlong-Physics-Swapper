#include "RecoveryController.h"

#include <algorithm>

namespace SPS::Controllers {

RecoveryController::RecoveryController(ActorContext& context) : context_(context) {}

void RecoveryController::ResetTransient()
{
    context_.physics.lastSwitchMs.store(0);
    context_.physics.retryAfterMs.store(0);
    context_.position.settleDueMs.store(0);
    context_.position.confirmationDueMs.store(0);
    context_.position.retryDueMs.store(0);

    auto& recovery = context_.recovery;
    recovery.loadSmpResetDueMs.store(0);
    recovery.softHandoffResetDueMs.store(0);
    recovery.softHandoffResetUntilMs.store(0);
    recovery.softAngleRefreshDueMs.store(0);
    recovery.softConfirmationDueMs.store(0);
    recovery.softConfirmationUntilMs.store(0);
    recovery.cbpcConfirmationDueMs.store(0);
    recovery.cbpcConfirmationUntilMs.store(0);
    recovery.nodeRefreshDueMs.store(0);
    recovery.nodeRefreshFollowupDueMs.store(0);
    recovery.nodeRefreshFollowupUntilMs.store(0);
    recovery.nodeCbpcReacquireDueMs.store(0);
    recovery.nodeCbpcReacquireUntilMs.store(0);
    recovery.erectMeshReplayDueMs.store(0);
    recovery.startupReconcileDueMs.store(0);
    recovery.startupReconcileUntilMs.store(0);
    recovery.postSwitchVerificationDueMs.store(0);
    recovery.postSwitchVerificationUntilMs.store(0);
    recovery.externalOwnerRepairDueMs.store(0);
    recovery.externalOwnerRepairUntilMs.store(0);
    recovery.ignoreNodeEventsUntilMs.store(0);
}

void RecoveryController::ScheduleExternalOwnerRepair(
    std::int64_t now, int delayMs)
{
    const auto due = now + std::clamp(delayMs, 100, 5000);
    context_.recovery.externalOwnerRepairDueMs.store(due);
    context_.recovery.externalOwnerRepairUntilMs.store(due + 10000);
}

void RecoveryController::ScheduleNodeRefreshFollowup(std::int64_t now)
{
    context_.recovery.nodeRefreshFollowupDueMs.store(now + 1500);
    context_.recovery.nodeRefreshFollowupUntilMs.store(now + 12000);
}

bool RecoveryController::RetryNodeRefreshFollowup(std::int64_t now)
{
    if (now < context_.recovery.nodeRefreshFollowupUntilMs.load()) {
        context_.recovery.nodeRefreshFollowupDueMs.store(now + 1000);
        return true;
    }
    context_.recovery.nodeRefreshFollowupDueMs.store(0);
    context_.recovery.nodeRefreshFollowupUntilMs.store(0);
    return false;
}

bool RecoveryController::ClaimPostSwitchVerification(std::int64_t now)
{
    return ClaimIfDue(context_.recovery.postSwitchVerificationDueMs, now);
}

bool RecoveryController::ClaimExternalOwnerRepair(std::int64_t now)
{
    return ClaimIfDue(context_.recovery.externalOwnerRepairDueMs, now);
}

bool RecoveryController::ClaimLoadSmpReset(std::int64_t now)
{
    return ClaimIfDue(context_.recovery.loadSmpResetDueMs, now);
}

bool RecoveryController::ClaimSoftHandoffReset(
    std::int64_t now, bool usingCBPC)
{
    return !usingCBPC && ClaimIfDue(context_.recovery.softHandoffResetDueMs, now);
}

bool RecoveryController::ClaimSoftAngleRefresh(
    std::int64_t now, bool usingCBPC)
{
    return !usingCBPC && ClaimIfDue(context_.recovery.softAngleRefreshDueMs, now);
}

bool RecoveryController::ClaimNodeCbpcReacquire(
    std::int64_t now, bool usingCBPC)
{
    return usingCBPC && ClaimIfDue(context_.recovery.nodeCbpcReacquireDueMs, now);
}

bool RecoveryController::ClaimSoftConfirmation(
    std::int64_t now, bool usingCBPC)
{
    return !usingCBPC && ClaimIfDue(context_.recovery.softConfirmationDueMs, now);
}

bool RecoveryController::ClaimCbpcConfirmation(
    std::int64_t now, bool usingCBPC)
{
    return usingCBPC && ClaimIfDue(context_.recovery.cbpcConfirmationDueMs, now);
}

bool RecoveryController::ClaimNodeRefresh(std::int64_t now)
{
    return ClaimIfDue(context_.recovery.nodeRefreshDueMs, now);
}

bool RecoveryController::ClaimNodeRefreshFollowup(std::int64_t now)
{
    return ClaimIfDue(context_.recovery.nodeRefreshFollowupDueMs, now);
}

bool RecoveryController::ClaimErectMeshReplay(std::int64_t now)
{
    return ClaimIfDue(context_.recovery.erectMeshReplayDueMs, now);
}

bool RecoveryController::ClaimIfDue(
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

RecoverySnapshot RecoveryController::Read() const
{
    return {
        context_.recovery.loadSmpResets.load(),
        context_.recovery.lastLoadSmpResetMs.load(),
        context_.recovery.lastOwnerRestorationMs.load()
    };
}

}
