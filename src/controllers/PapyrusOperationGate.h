#pragma once

#include <mutex>
#include <string>
#include <string_view>

namespace SPS::Controllers {

// Cancellation invalidates queued work but retains the running stack's lease
// until its callback. A replacement cannot overtake an old engine mutation.
class PapyrusOperationGate {
public:
    bool Busy() const
    {
        std::scoped_lock lock(lock_);
        return !expected_.empty() || !executing_.empty();
    }
    bool Prepare(std::string token)
    {
        std::scoped_lock lock(lock_);
        if (!expected_.empty() || !executing_.empty() || token.empty()) return false;
        expected_ = std::move(token);
        barrierPassed_ = false;
        return true;
    }
    bool Enter(std::string_view token)
    {
        std::scoped_lock lock(lock_);
        if (token.empty() || token != expected_ || !executing_.empty()) return false;
        executing_ = token;
        return true;
    }
    bool Current(std::string_view token) const
    {
        std::scoped_lock lock(lock_);
        return !token.empty() && token == expected_ && token == executing_;
    }
    void PassBarrier(std::string_view token)
    {
        std::scoped_lock lock(lock_);
        if (token == expected_ && token == executing_) barrierPassed_ = true;
    }
    bool BarrierPassed(std::string_view token) const
    {
        std::scoped_lock lock(lock_);
        return !token.empty() && token == expected_ && token == executing_ && barrierPassed_;
    }
    void Cancel()
    {
        std::scoped_lock lock(lock_);
        expected_.clear();
    }
    void Complete(std::string_view token)
    {
        std::scoped_lock lock(lock_);
        if (token == executing_) executing_.clear();
        if (token == expected_) expected_.clear();
    }
    void ResetSession()
    {
        std::scoped_lock lock(lock_);
        expected_.clear();
        executing_.clear();
        barrierPassed_ = false;
    }
private:
    mutable std::mutex lock_;
    std::string expected_;
    std::string executing_;
    bool barrierPassed_{ false };
};

}
