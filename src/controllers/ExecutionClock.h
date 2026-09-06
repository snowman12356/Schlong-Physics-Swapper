#pragma once

#include <cstdint>
#include <mutex>

namespace SPS::Controllers {

// Recovery deadlines count executable game time, not time spent in menus.
class ExecutionClock {
public:
    void SetPaused(std::int64_t realNow, bool paused)
    {
        std::scoped_lock lock(lock_);
        if (paused == paused_) return;
        if (paused) pauseStarted_ = realNow;
        else pausedTotal_ += realNow - pauseStarted_;
        paused_ = paused;
    }

    std::int64_t Now(std::int64_t realNow) const
    {
        std::scoped_lock lock(lock_);
        return (paused_ ? pauseStarted_ : realNow) - pausedTotal_;
    }

    bool Paused() const
    {
        std::scoped_lock lock(lock_);
        return paused_;
    }

private:
    mutable std::mutex lock_;
    bool paused_{ false };
    std::int64_t pauseStarted_{ 0 };
    std::int64_t pausedTotal_{ 0 };
};

}
