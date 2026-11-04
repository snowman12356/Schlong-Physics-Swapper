#pragma once

#include "ManagedActorRegistry.h"

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace SPS::Controllers {

enum class ActorRequestKind { arousal, arousalWrite, position, scene, ownership };

struct ActorRequest {
    ActorWorkLease lease;
    std::uint32_t handle{ 0 };
    ActorRequestKind kind{ ActorRequestKind::arousal };
    std::int64_t expiresAtMs{ 0 };
    bool cancelled{ false };
    std::uintptr_t rootIdentity{ 0 };
};

// Bounded records shared by the game thread and Papyrus guards. Cancellation
// invalidates a request but retains its slot until the callback retires.
class ActorRequestLedger {
public:
    explicit ActorRequestLedger(std::size_t capacity) : capacity_(capacity) {}
    [[nodiscard]] bool Prepare(const std::string& token, ActorRequest request, std::int64_t now);
    [[nodiscard]] bool Current(const std::string& token, ActorRequestKind kind,
        std::uint32_t formID, std::uint32_t handle, std::int64_t now, std::uintptr_t rootIdentity = 0) const;
    [[nodiscard]] std::optional<ActorRequest> Read(const std::string& token,
        ActorRequestKind kind, std::int64_t now) const;
    [[nodiscard]] std::optional<ActorRequest> Finish(const std::string& token, std::int64_t now);
    void Discard(const std::string& token);
    [[nodiscard]] std::vector<std::string> Cancel(ActorWorkTicket ticket);
    void ResetSession();
    [[nodiscard]] std::size_t Size() const;
private:
    [[nodiscard]] static bool Valid(const ActorRequest& request, std::int64_t now);
    // Position and ownership both mutate physics under the shared engine gate.
    [[nodiscard]] static bool HoldsEngineLease(ActorRequestKind kind);
    const std::size_t capacity_;
    mutable std::mutex lock_;
    std::unordered_map<std::string, ActorRequest> requests_;
};

}
