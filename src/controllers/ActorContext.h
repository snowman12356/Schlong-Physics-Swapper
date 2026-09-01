#pragma once

#include <atomic>
#include <cstdint>

namespace SPS::Controllers {

struct PhysicsOwnershipState {
    std::atomic<bool> usingCBPC{ false };
    std::atomic<bool> known{ false };
    std::atomic<bool> smpConnected{ false };
    std::atomic<bool> cbpcConnected{ false };
    std::atomic<bool> sosConnected{ false };
    std::atomic<std::int64_t> lastSwitchMs{ 0 };
    std::atomic<std::int64_t> retryAfterMs{ 0 };
    std::atomic<unsigned> successes{ 0 };
    std::atomic<unsigned> failures{ 0 };
    std::atomic<bool> pending{ false };
    std::atomic<bool> pendingCBPC{ false };
    std::atomic<int> pendingPurpose{ 0 };
    std::atomic<bool> pendingSoftTransition{ false };
    std::atomic<std::int64_t> pendingSinceMs{ 0 };
    std::atomic<std::uint64_t> requestGeneration{ 0 };
};

struct PositionState {
    std::atomic<int> appliedBend{ -1 };
    std::atomic<int> requestedBend{ -1 };
    std::atomic<int> lastMethod{ -1 };
    std::atomic<bool> lastSucceeded{ false };
    std::atomic<bool> automaticSuspended{ false };
    std::atomic<unsigned> repairs{ 0 };
    std::atomic<std::int64_t> lastApplyMs{ 0 };
    std::atomic<std::int64_t> settleDueMs{ 0 };
    std::atomic<std::int64_t> confirmationDueMs{ 0 };
    std::atomic<std::int64_t> retryDueMs{ 0 };
    std::atomic<std::int64_t> animationStartMs{ 0 };
    std::atomic<int> animationTargetBend{ 0 };
    std::atomic<int> animationLastQueuedBend{ -1 };
    std::atomic<std::uint64_t> animationGeneration{ 0 };
    std::atomic<bool> animating{ false };
    std::atomic<bool> relaxing{ false };
    std::atomic<std::int64_t> guardUntilMs{ 0 };
    std::atomic<std::int64_t> guardWindowStartMs{ 0 };
    std::atomic<int> guardCount{ 0 };
    std::atomic<int> consecutiveFailures{ 0 };
    std::atomic<bool> failureReported{ false };
};

struct RecoveryState {
    std::atomic<unsigned> loadSmpResets{ 0 };
    std::atomic<std::int64_t> lastLoadSmpResetMs{ 0 };
    std::atomic<std::int64_t> loadSmpResetDueMs{ 0 };
    std::atomic<std::int64_t> loadSmpResetRestoreDueMs{ 0 };
    std::atomic<std::int64_t> softHandoffResetDueMs{ 0 };
    std::atomic<std::int64_t> softHandoffResetUntilMs{ 0 };
    std::atomic<std::int64_t> softHandoffResetRestoreDueMs{ 0 };
    std::atomic<std::int64_t> softAngleRefreshDueMs{ 0 };
    std::atomic<std::int64_t> softAngleRefreshRestoreDueMs{ 0 };
    std::atomic<std::int64_t> softConfirmationDueMs{ 0 };
    std::atomic<std::int64_t> softConfirmationUntilMs{ 0 };
    std::atomic<std::int64_t> cbpcConfirmationDueMs{ 0 };
    std::atomic<std::int64_t> cbpcConfirmationUntilMs{ 0 };
    std::atomic<std::int64_t> nodeRefreshDueMs{ 0 };
    std::atomic<std::int64_t> nodeRefreshFollowupDueMs{ 0 };
    std::atomic<std::int64_t> nodeCbpcReacquireDueMs{ 0 };
    std::atomic<std::int64_t> nodeCbpcReacquireUntilMs{ 0 };
    std::atomic<std::int64_t> erectMeshReplayDueMs{ 0 };
    std::atomic<std::int64_t> nodeSmpResetRestoreDueMs{ 0 };
    std::atomic<std::int64_t> startupReconcileDueMs{ 0 };
    std::atomic<std::int64_t> startupReconcileUntilMs{ 0 };
    std::atomic<std::int64_t> postSwitchVerificationDueMs{ 0 };
    std::atomic<std::int64_t> postSwitchVerificationUntilMs{ 0 };
    std::atomic<std::int64_t> externalOwnerRepairDueMs{ 0 };
    std::atomic<std::int64_t> externalOwnerRepairUntilMs{ 0 };
    std::atomic<std::int64_t> lastOwnerRestorationMs{ 0 };
    std::atomic<std::int64_t> ignoreNodeEventsUntilMs{ 0 };
};

struct SpontaneousState {
    std::atomic<std::int64_t> randomNextMs{ 0 };
    std::atomic<std::int64_t> randomUntilMs{ 0 };
    std::atomic<bool> randomManualTest{ false };
    std::atomic<std::int64_t> refractoryUntilMs{ 0 };
    std::atomic<std::int64_t> morningDueMs{ 0 };
    std::atomic<bool> morningActive{ false };
    std::atomic<float> sleepWaitStartedHours{ -1.0F };
};

struct ActorContext {
    explicit ActorContext(std::uint32_t actorFormID) : formID(actorFormID) {}

    std::uint32_t formID;
    PhysicsOwnershipState physics{};
    PositionState position{};
    RecoveryState recovery{};
    SpontaneousState spontaneous{};
};

}
