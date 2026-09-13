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
            !state_.pendingSuperseded.load() && state_.pendingCBPC.load() == targetCBPC &&
                    state_.pendingPurpose.load() == static_cast<int>(purpose) ?
                OwnershipBeginStatus::alreadyPending : OwnershipBeginStatus::blocked,
            state_.requestGeneration.load()
        };
    }

    if (purpose == OwnershipPurpose::maintainCBPC &&
        !CanMaintainCBPC(targetCBPC, false, now)) {
        return {};
    }

    auto generation = state_.requestGeneration.fetch_add(1) + 1;
    if (generation == 0) {
        generation = state_.requestGeneration.fetch_add(1) + 1;
    }
    state_.pendingCBPC.store(targetCBPC);
    state_.pendingPurpose.store(static_cast<int>(purpose));
    state_.pendingSoftTransition.store(softTransition);
    state_.pendingSuperseded.store(false);
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
    state_.pendingSuperseded.store(false);
    state_.pendingSinceMs.store(0);
    state_.smpOffReassertAfterMs.store(0);
}

bool PhysicsOwnershipController::Supersede(bool targetCBPC)
{
    if (!state_.pending.load() || state_.pendingCBPC.load() == targetCBPC ||
        state_.pendingSuperseded.exchange(true)) return false;

    // The old stack may already have changed one engine. Do not restore the
    // cached owner or let its eventual success acknowledge the obsolete target.
    state_.known.store(false);
    state_.smpConnected.store(false);
    state_.cbpcConnected.store(false);
    return true;
}

bool PhysicsOwnershipController::CanMaintainCBPC(
    bool wantsCBPC, bool relaxing, std::int64_t now) const
{
    return wantsCBPC && !relaxing && state_.known.load() && state_.usingCBPC.load() &&
        !state_.pending.load() && now >= state_.retryAfterMs.load() &&
        now >= state_.smpOffReassertAfterMs.load();
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
        state_.failures.load(),
        state_.pendingSuperseded.load()
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
        static_cast<OwnershipPurpose>(state_.pendingPurpose.load()),
        state_.pendingSuperseded.load()
    };
    state_.pending.store(false);
    state_.pendingPurpose.store(0);
    state_.pendingSoftTransition.store(false);
    state_.pendingSuperseded.store(false);
    state_.pendingSinceMs.store(0);

    if (result.superseded) {
        // A policy change is not an engine failure. The adapter retains the
        // execution lease if this completion came from the timeout path.
        result.success = false;
        state_.retryAfterMs.store(0);
        return result;
    }

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
    // Leave a full second after any completed transaction. Routine maintenance
    // uses the normal tick and never queues extra work behind an active stack.
    state_.smpOffReassertAfterMs.store(now + 1000);
    if (result.purpose == OwnershipPurpose::switchOwner) {
        state_.usingCBPC.store(result.targetCBPC);
        state_.known.store(true);
        state_.lastSwitchMs.store(now);
        ++state_.successes;
    }
    return result;
}

}
