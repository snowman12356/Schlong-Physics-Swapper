#include "ManagedActorRegistry.h"

#include "SPSAPI.h"

#include <algorithm>
#include <limits>

namespace SPS::Controllers {

ManagedActorRegistry::ManagedActorRegistry(std::size_t capacity) : capacity_(capacity) {}
ManagedActorRegistry::~ManagedActorRegistry() { ResetSession(); }

std::optional<std::uint64_t> ManagedActorRegistry::NextGeneration()
{
    return NextActorWorkGeneration();
}

ActorRegistrationResult ManagedActorRegistry::Add(std::uint32_t formID)
{
    if (formID == 0 || formID == SPS::API::kPlayerFormID) return {};
    if (const auto found = actors_.find(formID); found != actors_.end()) {
        return { ActorRegistrationStatus::alreadyRegistered, found->second.ticket };
    }
    if (actors_.size() >= capacity_) return { ActorRegistrationStatus::capacityReached, {} };
    const auto generation = NextGeneration();
    if (!generation) return { ActorRegistrationStatus::generationExhausted, {} };
    const ActorWorkTicket ticket{ formID, *generation };
    actors_.emplace(formID, Entry{ ticket, nullptr, nullptr });
    return { ActorRegistrationStatus::added, ticket };
}

std::optional<ManagedActorSnapshot> ManagedActorRegistry::Find(std::uint32_t formID) const
{
    const auto found = actors_.find(formID);
    if (found == actors_.end()) return std::nullopt;
    return ManagedActorSnapshot{ found->second.ticket, found->second.context != nullptr };
}

std::vector<ManagedActorSnapshot> ManagedActorRegistry::Snapshot() const
{
    std::vector<ManagedActorSnapshot> result;
    result.reserve(actors_.size());
    for (const auto& item : actors_) {
        const auto& entry = item.second;
        result.push_back({ entry.ticket, entry.context != nullptr });
    }
    std::ranges::sort(result, {}, [](const auto& entry) { return entry.ticket.formID; });
    return result;
}

std::size_t ManagedActorRegistry::Size() const { return actors_.size(); }

std::optional<ActorWorkTicket> ManagedActorRegistry::SetLoaded(ActorWorkTicket ticket, bool loaded)
{
    const auto found = actors_.find(ticket.formID);
    if (found == actors_.end() || found->second.ticket != ticket) return std::nullopt;
    auto& entry = found->second;
    if ((entry.context != nullptr) == loaded) return entry.ticket;
    const auto generation = NextGeneration();
    if (!generation) {
        // No further work can be issued safely, including after a requested
        // unload. Keep membership, but make every previous ticket unusable.
        if (entry.active) entry.active->store(false);
        entry.context.reset();
        entry.active.reset();
        entry.ticket.generation = 0;
        return std::nullopt;
    }
    auto context = loaded ? std::make_unique<ActorContext>(ticket.formID) : nullptr;
    auto active = loaded ? std::make_shared<std::atomic<bool>>(true) : nullptr;
    if (entry.active) entry.active->store(false);
    entry.context = std::move(context);
    entry.active = std::move(active);
    entry.ticket.generation = *generation;
    return entry.ticket;
}

ActorContext* ManagedActorRegistry::Resolve(ActorWorkTicket ticket)
{
    const auto found = actors_.find(ticket.formID);
    if (ticket.generation == 0 || found == actors_.end() || found->second.ticket != ticket) return nullptr;
    return found->second.context.get();
}

const ActorContext* ManagedActorRegistry::Resolve(ActorWorkTicket ticket) const
{
    const auto found = actors_.find(ticket.formID);
    if (ticket.generation == 0 || found == actors_.end() || found->second.ticket != ticket) return nullptr;
    return found->second.context.get();
}

bool ManagedActorRegistry::Remove(ActorWorkTicket ticket)
{
    const auto found = actors_.find(ticket.formID);
    if (found == actors_.end() || found->second.ticket != ticket) return false;
    if (found->second.active) found->second.active->store(false);
    actors_.erase(found);
    return true;
}

ActorWorkLease ManagedActorRegistry::Lease(ActorWorkTicket ticket) const
{
    const auto found = actors_.find(ticket.formID);
    if (ticket.generation == 0 || found == actors_.end() || found->second.ticket != ticket ||
        !found->second.context) return {};
    return { ticket, found->second.active };
}

void ManagedActorRegistry::ResetSession()
{
    for (const auto& item : actors_) {
        if (item.second.active) item.second.active->store(false);
    }
    actors_.clear();
}

}
