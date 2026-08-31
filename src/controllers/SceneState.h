#pragma once

#include <atomic>
#include <cstdint>

namespace SPS::Controllers {

struct SexLabState {
    std::atomic<bool> active{ false };
    std::atomic<bool> valid{ false };
    std::atomic<bool> connected{ false };
    std::atomic<bool> queryPending{ false };
    std::atomic<std::int64_t> queryStartedMs{ 0 };
    std::atomic<std::int64_t> queryRetryAfterMs{ 0 };
    std::atomic<std::uint64_t> queryGeneration{ 0 };
    std::atomic<std::int64_t> endedMs{ 0 };
    std::atomic<bool> entryCBPC{ false };
    std::atomic<bool> entryStateValid{ false };
    std::atomic<int> role{ 0 };
    std::atomic<bool> roleValid{ false };
    std::atomic<bool> roleQueryPending{ false };
    std::atomic<std::int64_t> roleQueryStartedMs{ 0 };
    std::atomic<std::int64_t> roleRetryAfterMs{ 0 };
    std::atomic<std::int64_t> lastTopMs{ 0 };
    std::atomic<std::int64_t> bottomCandidateSinceMs{ 0 };
    std::atomic<std::uint64_t> roleGeneration{ 0 };
};

struct OStimState {
    std::atomic<bool> active{ false };
    std::atomic<bool> connected{ false };
    std::atomic<int> role{ 0 };
    std::atomic<bool> roleValid{ false };
    std::atomic<bool> roleQueryPending{ false };
    std::atomic<std::int64_t> roleQueryStartedMs{ 0 };
    std::atomic<std::int64_t> roleRetryAfterMs{ 0 };
    std::atomic<std::uint64_t> roleGeneration{ 0 };
    std::atomic<std::int64_t> endedMs{ 0 };
    std::atomic<bool> entryCBPC{ false };
    std::atomic<bool> entryStateValid{ false };
};

struct PPAState {
    std::atomic<bool> apiConnected{ false };
    std::atomic<bool> sceneActive{ false };
    std::atomic<int> sceneRole{ 0 };
    std::atomic<bool> sceneRoleValid{ false };
    std::atomic<std::int64_t> lastUpdateMs{ 0 };
    std::atomic<std::int64_t> lastTopMs{ 0 };
    std::atomic<std::int64_t> bottomCandidateSinceMs{ 0 };
    std::atomic<std::int64_t> ignoreUntilMs{ 0 };
};

struct SceneState {
    SexLabState sexLab{};
    OStimState ostim{};
    PPAState ppa{};
};

}
