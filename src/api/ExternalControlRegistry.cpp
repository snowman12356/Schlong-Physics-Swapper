#include "ExternalControlRegistry.h"

#include <algorithm>
#include <ranges>

namespace SPS::APIControl {

namespace {

template <class Handle>
Handle NextNonZero(Handle& next)
{
    auto result = next++;
    if (result == 0) {
        result = next++;
    }
    return result;
}

}

Request ExternalControlRegistry::Add(
    const SPS::API::PhysicsRequest& request,
    std::int64_t nowMs)
{
    std::scoped_lock lock(lock_);
    Request stored;
    stored.handle = NextNonZero(nextRequestHandle_);
    stored.state = request.state;
    stored.expiresAtMs = request.durationMilliseconds > 0 ?
        nowMs + request.durationMilliseconds : 0;
    if (request.requesterName && request.requesterName[0] != '\0') {
        stored.requester = request.requesterName;
        if (stored.requester.size() > 64) {
            stored.requester.resize(64);
        }
    }
    requests_.emplace(stored.handle, stored);
    ++accepted_;
    return stored;
}

std::optional<std::string> ExternalControlRegistry::Release(SPS::API::RequestHandle handle)
{
    std::scoped_lock lock(lock_);
    const auto found = requests_.find(handle);
    if (found == requests_.end()) {
        return std::nullopt;
    }
    auto requester = found->second.requester;
    requests_.erase(found);
    ++released_;
    return requester;
}

std::optional<Request> ExternalControlRegistry::Active(std::int64_t nowMs)
{
    std::scoped_lock lock(lock_);
    for (auto it = requests_.begin(); it != requests_.end();) {
        if (it->second.expiresAtMs > 0 && nowMs >= it->second.expiresAtMs) {
            ++released_;
            it = requests_.erase(it);
        } else {
            ++it;
        }
    }
    if (requests_.empty()) {
        return std::nullopt;
    }
    const auto selected = std::ranges::max_element(requests_, {}, [](const auto& entry) {
        return entry.first;
    });
    return selected->second;
}

std::size_t ExternalControlRegistry::Clear()
{
    std::scoped_lock lock(lock_);
    const auto count = requests_.size();
    requests_.clear();
    released_ += static_cast<unsigned>(count);
    return count;
}

std::size_t ExternalControlRegistry::Count() const
{
    std::scoped_lock lock(lock_);
    return requests_.size();
}

Stats ExternalControlRegistry::GetStats() const
{
    std::scoped_lock lock(lock_);
    return { accepted_, released_, requests_.size() };
}

SPS::API::ListenerHandle ExternalControlRegistry::RegisterListener(
    SPS::API::StateChangedCallback callback,
    void* userData)
{
    if (!callback) {
        return 0;
    }
    std::scoped_lock lock(lock_);
    const auto handle = NextNonZero(nextListenerHandle_);
    listeners_.emplace(handle, Listener{ callback, userData });
    return handle;
}

bool ExternalControlRegistry::UnregisterListener(SPS::API::ListenerHandle handle)
{
    std::scoped_lock lock(lock_);
    return listeners_.erase(handle) > 0;
}

std::vector<Listener> ExternalControlRegistry::Listeners() const
{
    std::scoped_lock lock(lock_);
    std::vector<Listener> copy;
    copy.reserve(listeners_.size());
    for (const auto& [_, listener] : listeners_) {
        copy.push_back(listener);
    }
    return copy;
}

}
