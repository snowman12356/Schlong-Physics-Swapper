#include "ActivityLog.h"

#include <algorithm>
#include <ranges>
#include <utility>

namespace SPS::Diagnostics {

ActivityLog::ActivityLog(std::string initialAction) :
    lastAction_(std::move(initialAction))
{}

void ActivityLog::Record(std::string message, bool error)
{
    std::scoped_lock lock(lock_);
    lastAction_ = message;
    if (error) {
        lastError_ = message;
    }
    recent_.push_front(std::move(message));
    while (recent_.size() > 12) {
        recent_.pop_back();
    }
}

bool ActivityLog::ClearErrorWithPrefixes(std::initializer_list<std::string_view> prefixes)
{
    std::scoped_lock lock(lock_);
    if (std::ranges::any_of(prefixes, [&](std::string_view prefix) {
            return lastError_.starts_with(prefix);
        })) {
        lastError_.clear();
        return true;
    }
    return false;
}

ActivitySnapshot ActivityLog::Read() const
{
    std::scoped_lock lock(lock_);
    return { lastAction_, lastError_, recent_ };
}

}
