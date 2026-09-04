#include "PhysicsOwnershipController.h"

#include <algorithm>

namespace SPS::Controllers {

PhysicsOwnershipController::PhysicsOwnershipController(PhysicsOwnershipState& state) :
    state_(state) {}

OwnershipBeginResult PhysicsOwnershipController::Begin(
    bool targetCBPC, OwnershipPurpose purpose,
    bool softTransition, std::int64_t now)
{
    if (state_.pending.load()) {
        return {
            state_.pendingCBPC.load() == targetCBPC &&
                    state_.pendingPurpose.load() == static_cast<int>(purpose) ?
                OwnershipBeginStatus::alreadyPending : OwnershipBeginStatus::blocked,
            state_.requestGeneration.load()
        };
    }

    auto generation = state_.requestGeneration.fetch_add(1) + 1;
    if (generation == 0) {
        generation = state_.requestGeneration.fetch_add(1) + 1;
    }
    state_.pendingCBPC.store(targetCBPC);
    state_.pendingPurpose.store(static_cast<int>(purpose));
    state_.pendingSoftTransition.store(softTransition);
    state_.pendingSinceMs.store(now);
    state_.pending.store(true);
    return { OwnershipBeginStatus::started, generation };
}

OwnershipCompletion PhysicsOwnershipController::Complete(
    std::uint64_t generation, bool success, std::int64_t now)
{
    return Finish(generation, success, now);
}

OwnershipCompletion PhysicsOwnershipController::Expire(
    std::int64_t now, std::int64_t timeoutMs)
{
    if (!state_.pending.load() ||
        now - state_.pendingSinceMs.load() < std::max<std::int64_t>(1, timeoutMs)) {
        return {};
    }
    return Finish(state_.requestGeneration.load(), false, now);
}

void PhysicsOwnershipController::ResetPending()
{
    state_.requestGeneration.fetch_add(1);
    state_.pending.store(false);
    state_.pendingPurpose.store(0);
    state_.pendingSoftTransition.store(false);
    state_.pendingSinceMs.store(0);
}

PhysicsOwnershipSnapshot PhysicsOwnershipController::Read() const
{
    return {
        state_.known.load(),
        state_.usingCBPC.load(),
        state_.smpConnected.load(),
        state_.cbpcConnected.load(),
        state_.pending.load(),
        state_.pendingCBPC.load(),
        static_cast<OwnershipPurpose>(state_.pendingPurpose.load()),
        state_.pendingSinceMs.load(),
        state_.successes.load(),
        state_.failures.load()
    };
}

OwnershipCompletion PhysicsOwnershipController::Finish(
    std::uint64_t generation, bool success, std::int64_t now)
{
    if (!state_.pending.load() || generation != state_.requestGeneration.load()) {
        return {};
    }

    OwnershipCompletion result{
        true,
        success,
        state_.pendingCBPC.load(),
        state_.known.load(),
        state_.usingCBPC.load(),
        state_.pendingSoftTransition.load(),
        static_cast<OwnershipPurpose>(state_.pendingPurpose.load())
    };
    state_.pending.store(false);
    state_.pendingPurpose.store(0);
    state_.pendingSoftTransition.store(false);
    state_.pendingSinceMs.store(0);

    if (!success) {
        ++state_.failures;
        // A failed callback normally means no work ran, but a timeout can race
        // a late Papyrus completion. The previous owner is no longer safe to
        // treat as confirmed; force the next policy evaluation to reassert the
        // requested state with a fresh ordered transaction.
        state_.known.store(false);
        state_.retryAfterMs.store(now + 1000);
        return result;
    }

    state_.retryAfterMs.store(0);
    if (result.purpose == OwnershipPurpose::switchOwner) {
        state_.usingCBPC.store(result.targetCBPC);
        state_.known.store(true);
        state_.lastSwitchMs.store(now);
        ++state_.successes;
    }
    return result;
}

}
