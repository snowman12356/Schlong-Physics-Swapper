#include "BasicNpcPool.h"
#include "SPSAPI.h"
#include <algorithm>

namespace SPS::Controllers {

bool BasicNpcRequestSlot::Begin(std::int64_t now, std::int64_t retryMs)
{
    if (now < 0 || startedAt >= 0 || now < retryAt) return false;
    startedAt = now;
    retryAt = now + retryMs;
    return true;
}

bool BasicNpcRequestSlot::Finish(std::int64_t started, std::int64_t now)
{
    if (started < 0 || startedAt != started || now < started || now - started >= kTimeoutMs) return false;
    startedAt = -1;
    return true;
}

bool BasicNpcRequestSlot::Expire(std::int64_t now)
{
    if (startedAt < 0 || now < startedAt || now - startedAt < kTimeoutMs) return false;
    startedAt = -1;
    retryAt = now + 2000;
    return true;
}

void BasicNpcEntry::Expire(std::int64_t now)
{
    if (arousalRequest.Expire(now)) { arousal.reset(); ++failed; }
    if (arousalWriteRequest.Expire(now)) ++failed;
    if (sceneRequest.Expire(now)) {
        scene = ActorSceneStatus::unknown;
        sceneAt = now;
        policy.ObserveScene(scene, now);
        ++failed;
    }
    if (positionRequest.Expire(now)) {
        policy.Complete({}, BasicPositionResult::failed, now);
        ++failed;
    }
}

BasicNpcEntry* BasicNpcPool::Add(std::uint32_t formID, std::uint32_t handle, std::uintptr_t root,
    std::shared_ptr<std::atomic<bool>> permission)
{
    if (!formID || formID == SPS::API::kPlayerFormID || !handle || !root) return nullptr;
    if (auto* existing = Find(formID)) return existing->handle == handle && existing->root == root ? existing : nullptr;
    for (auto& entry : entries_) {
        if (entry) continue;
        auto candidate = std::make_unique<BasicNpcEntry>(formID, handle, root, std::move(permission));
        if (!candidate->work.Lease().Valid()) return nullptr;
        entry = std::move(candidate);
        return entry.get();
    }
    return nullptr;
}

BasicNpcEntry* BasicNpcPool::Find(std::uint32_t formID)
{
    for (auto& entry : entries_) if (entry && entry->work.Ticket().formID == formID) return entry.get();
    return nullptr;
}

BasicNpcEntry* BasicNpcPool::Find(ActorWorkTicket ticket)
{
    auto* entry = Find(ticket.formID);
    return entry && entry->work.Ticket() == ticket && entry->work.Lease().Valid() ? entry : nullptr;
}

std::vector<ActorWorkTicket> BasicNpcPool::Tickets() const
{
    std::vector<ActorWorkTicket> result;
    for (const auto& entry : entries_) if (entry) result.push_back(entry->work.Ticket());
    return result;
}

BasicNpcEntry* BasicNpcPool::Next(std::int64_t now)
{
    // The worker sleeps 250 ms, but game tasks land on frames. Preserve cadence
    // with a small frame tolerance so 240/260 ms arrivals don't skip half the
    // actors' turns. The worker still queues at most one coalesced task per wake.
    constexpr std::int64_t frameTolerance = 25;
    if (now < 0 || now + frameTolerance < nextAt_) return nullptr;
    nextAt_ = std::max(nextAt_ + kTickMs, now + kTickMs - frameTolerance);
    for (std::size_t i = 0; i < kCapacity; ++i) {
        auto* entry = entries_[cursor_].get();
        cursor_ = (cursor_ + 1) % kCapacity;
        if (entry && entry->work.Lease().Valid()) return entry;
    }
    return nullptr;
}

bool BasicNpcPool::Remove(ActorWorkTicket ticket)
{
    for (auto& entry : entries_) {
        if (entry && entry->work.Ticket() == ticket) { entry.reset(); return true; }
    }
    return false;
}

void BasicNpcPool::Reset()
{
    for (auto& entry : entries_) entry.reset();
    cursor_ = 0;
    nextAt_ = 0;
}

}
