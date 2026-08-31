#pragma once

#include <deque>
#include <initializer_list>
#include <mutex>
#include <string>
#include <string_view>

namespace SPS::Diagnostics {

struct ActivitySnapshot {
    std::string lastAction;
    std::string lastError;
    std::deque<std::string> recent;
};

class ActivityLog {
public:
    explicit ActivityLog(std::string initialAction = "Waiting for a loaded game");

    void Record(std::string message, bool error);
    bool ClearErrorWithPrefixes(std::initializer_list<std::string_view> prefixes);
    [[nodiscard]] ActivitySnapshot Read() const;

private:
    mutable std::mutex lock_;
    std::deque<std::string> recent_;
    std::string lastAction_;
    std::string lastError_;
};

}
