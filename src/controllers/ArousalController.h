#pragma once

#include <RE/Skyrim.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>

namespace SPS::Controllers {

class ArousalController {
public:
    using RecordHook = std::function<void(std::string, bool)>;
    using ActionHook = std::function<void()>;
    using ReadyHook = std::function<bool()>;

    // `changed` (optional) runs on the game thread, in the same task as and just
    // before `evaluate`, whenever an accepted reading changed by 0.5 or more.
    void Configure(ReadyHook papyrusReady, RecordHook record, ActionHook clearError, ActionHook evaluate,
        ActionHook changed = {});
    void Query(bool force = false);
    void Invalidate();
    void ResetReading();

    [[nodiscard]] float Value() const;
    [[nodiscard]] bool Valid() const;
    [[nodiscard]] bool Connected() const;
    // Coherent value/validity pair for read-only callers on other threads.
    [[nodiscard]] std::optional<float> Reading() const;

    void AcceptResult(std::uint64_t generation, float result);
    void AcceptExternalReading(float reading);

private:
    void ApplyReading(float reading);
    void QueueEvaluation();
    void Record(std::string message, bool error) const;

    std::atomic<float> value_{ 0.0F };
    mutable std::mutex readingLock_;
    std::atomic<bool> valid_{ false };
    std::atomic<bool> connected_{ false };
    std::atomic<bool> queryPending_{ false };
    std::atomic<std::int64_t> queryStartedMs_{ 0 };
    std::atomic<std::int64_t> retryAfterMs_{ 0 };
    std::atomic<std::int64_t> nextQueryMs_{ 0 };
    std::atomic<std::uint64_t> generation_{ 0 };
    ReadyHook papyrusReady_;
    RecordHook record_;
    ActionHook clearError_;
    ActionHook evaluate_;
    ActionHook changed_;
};

}
