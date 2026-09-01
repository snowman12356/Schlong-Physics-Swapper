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
    recovery.loadSmpResetRestoreDueMs.store(0);
    recovery.softHandoffResetDueMs.store(0);
    recovery.softHandoffResetUntilMs.store(0);
    recovery.softHandoffResetRestoreDueMs.store(0);
    recovery.softAngleRefreshDueMs.store(0);
    recovery.softAngleRefreshRestoreDueMs.store(0);
    recovery.softConfirmationDueMs.store(0);
    recovery.softConfirmationUntilMs.store(0);
    recovery.cbpcConfirmationDueMs.store(0);
    recovery.cbpcConfirmationUntilMs.store(0);
    recovery.nodeRefreshDueMs.store(0);
    recovery.nodeRefreshFollowupDueMs.store(0);
    recovery.nodeCbpcReacquireDueMs.store(0);
    recovery.nodeCbpcReacquireUntilMs.store(0);
    recovery.erectMeshReplayDueMs.store(0);
    recovery.nodeSmpResetRestoreDueMs.store(0);
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

RecoverySnapshot RecoveryController::Read() const
{
    return {
        context_.recovery.loadSmpResets.load(),
        context_.recovery.lastLoadSmpResetMs.load(),
        context_.recovery.lastOwnerRestorationMs.load()
    };
}

}
