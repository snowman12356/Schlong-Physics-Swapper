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
};

struct PositionState {
    std::atomic<int> appliedBend{ -1 };
    std::atomic<int> requestedBend{ -1 };
    std::atomic<int> lastMethod{ -1 };
    std::atomic<bool> lastSucceeded{ false };
    std::atomic<bool> automaticSuspended{ false };
    std::atomic<unsigned> repairs{ 0 };
};

struct RecoveryState {
    std::atomic<unsigned> loadSmpResets{ 0 };
    std::atomic<std::int64_t> lastLoadSmpResetMs{ 0 };
};

struct ActorContext {
    explicit ActorContext(std::uint32_t actorFormID) : formID(actorFormID) {}

    std::uint32_t formID;
    PhysicsOwnershipState physics{};
    PositionState position{};
    RecoveryState recovery{};
};

}
