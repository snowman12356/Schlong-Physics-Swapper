#pragma once

#include "ActorContext.h"

#include <cstdint>

namespace SPS::Controllers {

struct RecoverySnapshot {
    unsigned loadSmpResets{ 0 };
    std::int64_t lastLoadSmpResetMs{ 0 };
    std::int64_t lastOwnerRestorationMs{ 0 };
};

class RecoveryController {
public:
    explicit RecoveryController(ActorContext& context);

    void ResetTransient();
    void ScheduleExternalOwnerRepair(std::int64_t now, int delayMs);
    void ScheduleNodeRefreshFollowup(std::int64_t now);
    bool RetryNodeRefreshFollowup(std::int64_t now);

    [[nodiscard]] bool ClaimPostSwitchVerification(std::int64_t now);
    [[nodiscard]] bool ClaimExternalOwnerRepair(std::int64_t now);
    [[nodiscard]] bool ClaimLoadSmpReset(std::int64_t now);
    [[nodiscard]] bool ClaimSoftHandoffReset(std::int64_t now, bool usingCBPC);
    [[nodiscard]] bool ClaimSoftAngleRefresh(std::int64_t now, bool usingCBPC);
    [[nodiscard]] bool ClaimNodeCbpcReacquire(std::int64_t now, bool usingCBPC);
    [[nodiscard]] bool ClaimSoftConfirmation(std::int64_t now, bool usingCBPC);
    [[nodiscard]] bool ClaimCbpcConfirmation(std::int64_t now, bool usingCBPC);
    [[nodiscard]] bool ClaimNodeRefresh(std::int64_t now);
    [[nodiscard]] bool ClaimNodeRefreshFollowup(std::int64_t now);
    [[nodiscard]] bool ClaimErectMeshReplay(std::int64_t now);

    [[nodiscard]] RecoverySnapshot Read() const;

private:
    [[nodiscard]] static bool ClaimIfDue(
        std::atomic<std::int64_t>& dueMs, std::int64_t now);

    ActorContext& context_;
};

}
