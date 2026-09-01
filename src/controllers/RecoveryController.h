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
    [[nodiscard]] RecoverySnapshot Read() const;

private:
    ActorContext& context_;
};

}
