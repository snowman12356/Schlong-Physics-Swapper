#pragma once

#include <RE/Skyrim.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

namespace SPS::Controllers {

class ArousalController {
public:
    using RecordHook = std::function<void(std::string, bool)>;
    using ActionHook = std::function<void()>;
    using ReadyHook = std::function<bool()>;

    void Configure(ReadyHook papyrusReady, RecordHook record, ActionHook clearError, ActionHook evaluate);
    void Query(bool force = false);
    void Invalidate();
    void ResetReading();

    [[nodiscard]] float Value() const;
    [[nodiscard]] bool Valid() const;
    [[nodiscard]] bool Connected() const;

    void AcceptResult(std::uint64_t generation, RE::BSScript::Variable result);
    void AcceptExternalReading(float reading);

private:
    void ApplyReading(float reading);
    void QueueEvaluation();
    void Record(std::string message, bool error) const;

    std::atomic<float> value_{ 0.0F };
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
};

}
