#include "ActorWorkLifetime.h"
#include "SPSAPI.h"
#include <limits>

namespace SPS::Controllers {

std::optional<std::uint64_t> NextActorWorkGeneration()
{
    static std::atomic<std::uint64_t> generation{ 0 };
    auto previous = generation.load();
    while (previous != std::numeric_limits<std::uint64_t>::max()) {
        if (generation.compare_exchange_weak(previous, previous + 1)) return previous + 1;
    }
    return std::nullopt;
}

ActorWorkOwner::ActorWorkOwner(std::uint32_t formID, std::shared_ptr<std::atomic<bool>> permission,
    bool allowPlayer) :
    permission_(std::move(permission))
{
    if (!formID || (formID == SPS::API::kPlayerFormID && !allowPlayer)) return;
    if (const auto generation = NextActorWorkGeneration()) {
        ticket_ = { formID, *generation };
        active_ = std::make_shared<std::atomic<bool>>(true);
    }
}

}
