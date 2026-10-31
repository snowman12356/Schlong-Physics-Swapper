#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

namespace SPS::Controllers {

struct ActorWorkTicket {
    std::uint32_t formID{ 0 };
    std::uint64_t generation{ 0 };
    bool operator==(const ActorWorkTicket&) const = default;
};

// Shared across basic and managed owners, including newly constructed owners
// after a save change. A ticket must never alias another controller's work.
[[nodiscard]] std::optional<std::uint64_t> NextActorWorkGeneration();

class ActorWorkLease {
public:
    ActorWorkLease() = default;
    [[nodiscard]] ActorWorkTicket Ticket() const { return ticket_; }
    void Revoke() const { if (active_) active_->store(false); }
    [[nodiscard]] bool Valid() const {
        return active_ && active_->load() && (!permission_ || permission_->load());
    }
private:
    friend class ManagedActorRegistry;
    friend class ActorWorkOwner;
    ActorWorkLease(ActorWorkTicket ticket, std::shared_ptr<std::atomic<bool>> active,
        std::shared_ptr<std::atomic<bool>> permission = {}) :
        ticket_(ticket), active_(std::move(active)), permission_(std::move(permission)) {}
    ActorWorkTicket ticket_{};
    std::shared_ptr<std::atomic<bool>> active_;
    std::shared_ptr<std::atomic<bool>> permission_;
};

// Basic actors need cancellable lifetimes, not a full physics ActorContext.
class ActorWorkOwner {
public:
    explicit ActorWorkOwner(std::uint32_t formID, std::shared_ptr<std::atomic<bool>> permission = {},
        bool allowPlayer = false);
    ~ActorWorkOwner() { Invalidate(); }
    ActorWorkOwner(const ActorWorkOwner&) = delete;
    ActorWorkOwner& operator=(const ActorWorkOwner&) = delete;
    [[nodiscard]] ActorWorkLease Lease() const { return { ticket_, active_, permission_ }; }
    [[nodiscard]] ActorWorkTicket Ticket() const { return ticket_; }
    void Invalidate() { if (active_) active_->store(false); }
private:
    ActorWorkTicket ticket_{};
    std::shared_ptr<std::atomic<bool>> active_;
    std::shared_ptr<std::atomic<bool>> permission_;
};

}
