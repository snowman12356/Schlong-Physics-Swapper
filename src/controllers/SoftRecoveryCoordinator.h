#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>

namespace SPS::Controllers {

// Soft (SMP) physics recovery decisions for the player, kept free of game types
// so they can be tested. plugin.cpp supplies the live inputs and performs the
// side effects (the ownership transaction, log lines, ignore windows).
//
// While the player is soft, every recovery trigger (scene end, fall to soft,
// soft confirmation, external SMP reset, mesh or outfit change, game load, the
// post-switch check, manual repair) only posts a request here, and at most one
// SMP transaction runs per round:
//  - a rebuild (clean rest bake, bridge preparation 6) waits until the scene has
//    been fully over for kSceneSettleMs and no trigger arrived for kQuietMs; a
//    manual repair (force) skips the scene wait;
//  - a plain ownership check may run during a scene, after the quiet period.
// Before this, each trigger ran its own transaction and retried every second;
// after one scene the log showed six SMP transactions in 20 s, each re-baking
// SMP's rest pose from a shaft that was still moving.
//
// It also holds the bounded follow-ups for two dependency quirks:
//  - FSMP's actor calls can land on the player's first-person skeleton and do
//    nothing, so an SMP proof below N gets at most two plain re-checks, and a
//    bake must prove itself clean, with at most two retries;
//  - CBPC clears its per-node stop flags on every game load, so CBPC is released
//    once per load while the first owner decision waits.
class SoftRecoveryCoordinator {
public:
    static constexpr std::int64_t kQuietMs = 1500;
    static constexpr std::int64_t kSceneSettleMs = 2000;
    // Once the scene is over, run even if triggers keep arriving.
    static constexpr std::int64_t kMaxWaitMs = 10000;
    static constexpr std::int64_t kLongWaitMs = 60000;
    static constexpr int kMaxDispatchFailures = 3;
    static constexpr int kMaxRetries = 2;
    static constexpr std::int64_t kRetryDecayMs = 120000;
    static constexpr int kMaxCbpcReleaseAttempts = 3;

    struct Inputs {
        std::int64_t now{ 0 };
        // Why the player's scene is not over yet, or nullptr when it is.
        const char* sceneBusy{ nullptr };
        bool enabled{ true };
        bool ownerKnown{ false };
        bool ownerCBPC{ false };
        bool handoffPending{ false };
        bool animating{ false };
        bool papyrusReady{ true };
    };

    enum class Action {
        none,     // nothing requested, or the request no longer applies
        wait,     // requested, not yet
        confirm,  // run a plain ownership check (confirmSoft)
        bake      // run a clean rest rebuild (bakeSoft)
    };

    struct Plan {
        Action action{ Action::none };
        // What the round is waiting for; nullptr when it only waits for quiet.
        const char* waitingFor{ nullptr };
        // True once per round, when the wait first passes kLongWaitMs.
        bool logLongWait{ false };
        // Snapshot of the round, passed back to OnDispatched.
        bool bakeRequested{ false };
        std::uint64_t sequence{ 0 };
        int merged{ 0 };
        std::string reasons;
    };

    enum class ProofFollowUp { none, recheck, giveUp };

    void Request(std::string_view reason, bool bake, bool force, std::int64_t now);
    void Clear();
    [[nodiscard]] Plan Evaluate(const Inputs& in);
    // The planned transaction was queued. didBake can be false for a planned bake
    // when FSMP's actor API is unavailable (a plain check ran instead).
    void OnDispatched(const Plan& plan, bool didBake);
    // The planned transaction could not be queued. Returns true when the round
    // was dropped after kMaxDispatchFailures failed dispatches.
    [[nodiscard]] bool OnDispatchFailed(bool ownerKnown);

    // A completed SMP handoff's proof. recheck: ask for a plain check; giveUp:
    // log once (the third unproven proof within kRetryDecayMs).
    [[nodiscard]] ProofFollowUp OnSmpProof(bool certain, std::int64_t now);
    // A bake failed or was not proven. True: request another bake; false: the
    // retry budget is spent.
    [[nodiscard]] bool AllowBakeRetry(std::int64_t now);
    void OnBakeSucceeded();
    void NoteBakeAborted() { lastBakeAborted_ = true; }
    // Whether a finished bake is clean. Consumes the aborted flag.
    [[nodiscard]] bool CheckBake(int dynamicBefore, int dynamicAfter, int boneCount);
    [[nodiscard]] static bool BakeClean(int dynamicBefore, int dynamicAfter, int boneCount,
        bool previousBakeAborted);

    [[nodiscard]] bool CbpcReleaseWanted() const;
    void OnCbpcReleaseQueued();
    void OnCbpcReleased();

    // Game load or new game: forget the round and every budget.
    void ResetSession();

private:
    mutable std::mutex lock_;
    bool confirm_{ false };
    bool bake_{ false };
    bool force_{ false };           // manual repair: do not wait for the scene to end
    std::int64_t firstMs_{ 0 };     // first request since the scene was last over
    std::int64_t lastMs_{ 0 };
    std::uint64_t sequence_{ 0 };   // bumped by every request
    int merged_{ 0 };
    int dispatchFailures_{ 0 };     // failed dispatches in this round
    bool longWaitLogged_{ false };
    std::string reasons_;
    std::int64_t sceneOverSinceMs_{ 0 };

    int failedBakes_{ 0 };
    std::int64_t lastFailedBakeMs_{ 0 };
    bool lastBakeAborted_{ false };
    int smpReproofs_{ 0 };
    std::int64_t lastReproofMs_{ 0 };
    bool cbpcReleased_{ false };
    int cbpcReleaseAttempts_{ 0 };

    void ClearLocked();
};

}
