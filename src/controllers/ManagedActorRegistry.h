#pragma once

#include "ActorContext.h"
#include "ActorWorkLifetime.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace SPS::Controllers {

enum class ActorRegistrationStatus {
    added,
    alreadyRegistered,
    invalidActor,
    capacityReached,
    generationExhausted
};

struct ActorRegistrationResult {
    ActorRegistrationStatus status{ ActorRegistrationStatus::invalidActor };
    ActorWorkTicket ticket{};
};

struct ManagedActorSnapshot {
    ActorWorkTicket ticket{};
    bool loaded{ false };
};

// Game-thread-only native state for explicitly managed NPCs. The existing
// player context stays outside this registry and does not consume its limit.
// Actor/mesh eligibility, live handles, persistence and physics cleanup belong
// to the runtime caller. This class neither dispatches nor releases an engine
// lease: cancellation/removal must still respect the shared operation gate.
class ManagedActorRegistry {
public:
    explicit ManagedActorRegistry(std::size_t capacity);
    ~ManagedActorRegistry();
    ManagedActorRegistry(const ManagedActorRegistry&) = delete;
    ManagedActorRegistry& operator=(const ManagedActorRegistry&) = delete;
    ManagedActorRegistry(ManagedActorRegistry&&) = delete;
    ManagedActorRegistry& operator=(ManagedActorRegistry&&) = delete;

    [[nodiscard]] ActorRegistrationResult Add(std::uint32_t formID);
    [[nodiscard]] std::optional<ManagedActorSnapshot> Find(std::uint32_t formID) const;
    [[nodiscard]] std::vector<ManagedActorSnapshot> Snapshot() const;
    [[nodiscard]] std::size_t Size() const;

    // Registration starts unloaded. Only a verified runtime availability
    // change may activate it. Every change gives work a fresh generation and
    // fresh transient state; same-state notifications do not invalidate work.
    [[nodiscard]] std::optional<ActorWorkTicket> SetLoaded(ActorWorkTicket ticket, bool loaded);
    [[nodiscard]] ActorContext* Resolve(ActorWorkTicket ticket);
    [[nodiscard]] const ActorContext* Resolve(ActorWorkTicket ticket) const;
    [[nodiscard]] ActorWorkLease Lease(ActorWorkTicket ticket) const;
    [[nodiscard]] bool Remove(ActorWorkTicket ticket);
    void ResetSession();

private:
    struct Entry {
        ActorWorkTicket ticket{};
        std::unique_ptr<ActorContext> context;
        std::shared_ptr<std::atomic<bool>> active;
    };

    [[nodiscard]] std::optional<std::uint64_t> NextGeneration();

    const std::size_t capacity_;
    std::unordered_map<std::uint32_t, Entry> actors_;
};

}
