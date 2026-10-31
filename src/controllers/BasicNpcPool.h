#pragma once

#include "ActorWorkLifetime.h"
#include "BasicNpcPolicy.h"
#include <array>
#include <vector>

namespace SPS::Controllers {

// Active-time slots handle dropped callbacks without inventing observations.
// The bridge ledger still retains the real stack until it retires.
struct BasicNpcRequestSlot {
    static constexpr std::int64_t kTimeoutMs = 10000;
    std::int64_t startedAt{ -1 };
    std::int64_t retryAt{ 0 };
    [[nodiscard]] bool Begin(std::int64_t now, std::int64_t retryMs = 2000);
    [[nodiscard]] bool Finish(std::int64_t started, std::int64_t now);
    [[nodiscard]] bool Expire(std::int64_t now);
};

struct BasicNpcEntry {
    BasicNpcEntry(std::uint32_t formID, std::uint32_t handleValue, std::uintptr_t rootValue,
        std::shared_ptr<std::atomic<bool>> permission) :
        work(formID, std::move(permission)), handle(handleValue), root(rootValue) {}
    ActorWorkOwner work;
    const std::uint32_t handle;
    const std::uintptr_t root;
    BasicNpcPolicy policy;
    BasicNpcRequestSlot arousalRequest, arousalWriteRequest, sceneRequest, positionRequest;
    std::optional<float> requestedArousalWrite;
    std::optional<float> arousal;
    std::int64_t arousalAt{ -1 };
    ActorSceneStatus scene{ ActorSceneStatus::unknown };
    std::int64_t sceneAt{ -1 };
    std::uint64_t completed{ 0 }, deferred{ 0 }, failed{ 0 };
    void Expire(std::int64_t now);
};

// Game-thread-only, transient basic behavior. Full managed membership remains
// separate. Next services at most one actor per tick, with a hard actor cap.
class BasicNpcPool {
public:
    static constexpr std::size_t kCapacity = 8;
    static constexpr std::int64_t kTickMs = 250;
    [[nodiscard]] BasicNpcEntry* Add(std::uint32_t formID, std::uint32_t handle, std::uintptr_t root,
        std::shared_ptr<std::atomic<bool>> permission = {});
    [[nodiscard]] BasicNpcEntry* Find(ActorWorkTicket ticket);
    [[nodiscard]] BasicNpcEntry* Find(std::uint32_t formID);
    [[nodiscard]] std::vector<ActorWorkTicket> Tickets() const;
    [[nodiscard]] BasicNpcEntry* Next(std::int64_t now);
    bool Remove(ActorWorkTicket ticket);
    void Reset();
private:
    std::array<std::unique_ptr<BasicNpcEntry>, kCapacity> entries_;
    std::size_t cursor_{ 0 };
    std::int64_t nextAt_{ 0 };
};

}
