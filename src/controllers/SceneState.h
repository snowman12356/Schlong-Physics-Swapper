#pragma once

#include <atomic>
#include <cstdint>

namespace SPS::Controllers {

struct SexLabState {
    std::atomic<int> threadID{ -1 };
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

    void ResetSession()
    {
        ++sexLab.queryGeneration;
        ++sexLab.roleGeneration;
        ++ostim.roleGeneration;
        sexLab.threadID.store(-1);
        sexLab.active.store(false);
        sexLab.valid.store(false);
        sexLab.connected.store(false);
        sexLab.endedMs.store(0);
        sexLab.entryStateValid.store(false);
        sexLab.entryCBPC.store(false);
        sexLab.role.store(0);
        sexLab.roleValid.store(false);
        sexLab.queryPending.store(false);
        sexLab.roleQueryPending.store(false);
        sexLab.queryRetryAfterMs.store(0);
        sexLab.roleRetryAfterMs.store(0);
        sexLab.lastTopMs.store(0);
        sexLab.bottomCandidateSinceMs.store(0);
        ostim.active.store(false);
        ostim.connected.store(false);
        ostim.endedMs.store(0);
        ostim.entryStateValid.store(false);
        ostim.entryCBPC.store(false);
        ostim.role.store(0);
        ostim.roleValid.store(false);
        ostim.roleQueryPending.store(false);
        ostim.roleRetryAfterMs.store(0);
        ppa.sceneActive.store(false);
        ppa.sceneRole.store(0);
        ppa.sceneRoleValid.store(false);
        ppa.lastUpdateMs.store(0);
        ppa.lastTopMs.store(0);
        ppa.bottomCandidateSinceMs.store(0);
    }
};

}
