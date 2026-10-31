#pragma once

#include "ActorContext.h"

#include <cstdint>

namespace SPS::Controllers {

enum class OwnershipPurpose : int {
    switchOwner = 1,
    confirmSoft = 2,
    confirmCBPC = 3,
    restore = 4,
    resetLoad = 5,
    resetSoft = 6,
    resetAngle = 7,
    resetMesh = 8,
    reconnectMesh = 9,
    maintainCBPC = 10
};

enum class OwnershipBeginStatus {
    started,
    alreadyPending,
    blocked
};

struct OwnershipBeginResult {
    OwnershipBeginStatus status{ OwnershipBeginStatus::blocked };
    std::uint64_t generation{ 0 };
};

struct OwnershipEvidence {
    bool success{ false };
    // FSMP's previous-state read-back for the managed bones at the moment of a
    // disable call. Above zero proves SMP still owned bone(s) (drift). -1 means
    // this path never sampled. A zero/false entry is ambiguous ("kinematic or
    // not found") and is never treated as proof that SMP released a bone.
    int dynamicBonesBefore{ -1 };
    int boneCount{ 0 };
};

struct OwnershipCompletion {
    bool matched{ false };
    bool success{ false };
    bool targetCBPC{ false };
    bool previousKnown{ false };
    bool previousCBPC{ false };
    bool softTransition{ false };
    OwnershipPurpose purpose{ OwnershipPurpose::switchOwner };
    bool superseded{ false };
    // Execution evidence only: the bridge stack completed and the player 3D
    // was present. It does not prove either engine changed state.
    int dynamicBonesBefore{ -1 };
    int boneCount{ 0 };
    // True when a disable was issued while FSMP reported managed bones still
    // dynamic. This is the only passive drift proof SPS has.
    bool smpDriftDetected{ false };
};

struct PhysicsOwnershipSnapshot {
    bool known{ false };
    bool usingCBPC{ false };
    // These describe the owner SPS selected and corroborate it with accepted
    // commands. They are not read-back of live engine state: CBPC exposes none,
    // and FSMP can only prove a bone was still dynamic, never that it stopped.
    bool smpConnected{ false };
    bool cbpcConnected{ false };
    bool pending{ false };
    bool pendingCBPC{ false };
    OwnershipPurpose pendingPurpose{ OwnershipPurpose::switchOwner };
    std::int64_t pendingSinceMs{ 0 };
    unsigned successes{ 0 };
    unsigned failures{ 0 };
    bool pendingSuperseded{ false };
    int lastDynamicBonesBefore{ -1 };
    unsigned smpDriftDetections{ 0 };
};

class PhysicsOwnershipController {
public:
    explicit PhysicsOwnershipController(PhysicsOwnershipState& state);

    [[nodiscard]] OwnershipBeginResult Begin(
        bool targetCBPC, OwnershipPurpose purpose,
        bool softTransition, std::int64_t now);
    [[nodiscard]] OwnershipCompletion Complete(
        std::uint64_t generation, bool success, std::int64_t now);
    [[nodiscard]] OwnershipCompletion Complete(
        std::uint64_t generation, const OwnershipEvidence& evidence, std::int64_t now);
    [[nodiscard]] OwnershipCompletion Expire(
        std::int64_t now, std::int64_t timeoutMs);
    void ResetPending();
    // Invalidate an opposite request, retaining its slot until completion.
    [[nodiscard]] bool Supersede(bool targetCBPC);
    [[nodiscard]] bool CanMaintainCBPC(
        bool wantsCBPC, bool relaxing, std::int64_t now) const;
    [[nodiscard]] PhysicsOwnershipSnapshot Read() const;

private:
    [[nodiscard]] OwnershipCompletion Finish(
        std::uint64_t generation, const OwnershipEvidence& evidence, std::int64_t now);

    PhysicsOwnershipState& state_;
};

}
