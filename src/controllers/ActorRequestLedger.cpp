#include "ActorRequestLedger.h"

namespace SPS::Controllers {

bool ActorRequestLedger::Valid(const ActorRequest& request, std::int64_t now)
{
    return !request.cancelled && request.lease.Valid() && request.handle != 0 &&
        now >= 0 && now < request.expiresAtMs;
}

bool ActorRequestLedger::Prepare(const std::string& token, ActorRequest request, std::int64_t now)
{
    std::scoped_lock lock(lock_);
    if (token.empty() || !Valid(request, now) || requests_.contains(token) ||
        requests_.size() >= capacity_) return false;
    // Even a cancelled/expired stack must return before the same actor/kind
    // can queue again. An actor being re-added cannot bypass this bound.
    for (const auto& item : requests_) {
        if (item.second.lease.Ticket().formID == request.lease.Ticket().formID &&
            item.second.kind == request.kind) return false;
    }
    requests_.emplace(token, std::move(request));
    return true;
}

bool ActorRequestLedger::Current(const std::string& token, ActorRequestKind kind,
    std::uint32_t formID, std::uint32_t handle, std::int64_t now, std::uintptr_t rootIdentity) const
{
    std::scoped_lock lock(lock_);
    const auto found = requests_.find(token);
    return found != requests_.end() && found->second.kind == kind &&
        found->second.lease.Ticket().formID == formID && found->second.handle == handle &&
        found->second.rootIdentity == rootIdentity && Valid(found->second, now);
}

std::optional<ActorRequest> ActorRequestLedger::Read(const std::string& token,
    ActorRequestKind kind, std::int64_t now) const
{
    std::scoped_lock lock(lock_);
    const auto found = requests_.find(token);
    if (found == requests_.end() || found->second.kind != kind || !Valid(found->second, now)) return std::nullopt;
    return found->second;
}

std::optional<ActorRequest> ActorRequestLedger::Finish(const std::string& token, std::int64_t now)
{
    std::scoped_lock lock(lock_);
    const auto found = requests_.find(token);
    if (found == requests_.end()) return std::nullopt;
    const auto result = Valid(found->second, now) ? std::optional{ found->second } : std::nullopt;
    requests_.erase(found);
    return result;
}

void ActorRequestLedger::Discard(const std::string& token)
{
    std::scoped_lock lock(lock_);
    requests_.erase(token);
}

std::vector<std::string> ActorRequestLedger::Cancel(ActorWorkTicket ticket)
{
    std::scoped_lock lock(lock_);
    std::vector<std::string> mutations;
    for (auto& item : requests_) {
        auto& request = item.second;
        if (request.lease.Ticket() == ticket) {
            request.cancelled = true;
            if (request.kind == ActorRequestKind::position) mutations.push_back(item.first);
        }
    }
    return mutations;
}

void ActorRequestLedger::ResetSession()
{
    std::scoped_lock lock(lock_);
    requests_.clear();
}

std::size_t ActorRequestLedger::Size() const
{
    std::scoped_lock lock(lock_);
    return requests_.size();
}

}
