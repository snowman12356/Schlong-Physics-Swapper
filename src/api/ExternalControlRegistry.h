#pragma once

#include "SPSAPI.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace SPS::APIControl {

struct Request {
    SPS::API::RequestHandle handle{ 0 };
    SPS::API::PhysicsState state{ SPS::API::PhysicsState::Unknown };
    std::int64_t expiresAtMs{ 0 };
    std::string requester{ "Unnamed plugin" };
};

struct Listener {
    SPS::API::StateChangedCallback callback{ nullptr };
    void* userData{ nullptr };
};

struct ActorListener {
    SPS::API::ActorEventCallback callback{ nullptr };
    void* userData{ nullptr };
};

struct Stats {
    unsigned accepted{ 0 };
    unsigned released{ 0 };
    std::size_t active{ 0 };
};

class ExternalControlRegistry {
public:
    Request Add(const SPS::API::PhysicsRequest& request, std::int64_t nowMs);
    std::optional<std::string> Release(SPS::API::RequestHandle handle);
    std::optional<Request> Active(std::int64_t nowMs);
    std::size_t Clear();
    [[nodiscard]] std::size_t Count() const;
    [[nodiscard]] Stats GetStats() const;

    SPS::API::ListenerHandle RegisterListener(
        SPS::API::StateChangedCallback callback,
        void* userData);
    bool UnregisterListener(SPS::API::ListenerHandle handle);
    [[nodiscard]] std::vector<Listener> Listeners() const;

    // V4 rise holds. Separate from the physics requests: they never choose an
    // owner, they only delay the arousal-driven rise. Handles come from the same
    // counter, so a hold handle can never match a physics request.
    Request AddRiseHold(const SPS::API::RiseHoldRequest& request, std::int64_t nowMs);
    std::optional<std::string> ReleaseRiseHold(SPS::API::RequestHandle handle);
    [[nodiscard]] bool RiseHoldActive(std::int64_t nowMs);
    std::size_t ClearRiseHolds();

    // V4 actor event listeners.
    SPS::API::ListenerHandle RegisterActorListener(
        SPS::API::ActorEventCallback callback, void* userData);
    bool UnregisterActorListener(SPS::API::ListenerHandle handle);
    [[nodiscard]] std::vector<ActorListener> ActorListeners() const;
    [[nodiscard]] bool HasActorListeners() const;

    // V5 NPC event listeners, kept apart from the player's V4 listeners.
    SPS::API::ListenerHandle RegisterNpcListener(
        SPS::API::ActorEventCallback callback, void* userData);
    bool UnregisterNpcListener(SPS::API::ListenerHandle handle);
    [[nodiscard]] std::vector<ActorListener> NpcListeners() const;
    [[nodiscard]] bool HasNpcListeners() const;

private:
    mutable std::mutex lock_;
    std::unordered_map<SPS::API::RequestHandle, Request> requests_;
    std::unordered_map<SPS::API::RequestHandle, Request> riseHolds_;
    std::unordered_map<SPS::API::ListenerHandle, Listener> listeners_;
    std::unordered_map<SPS::API::ListenerHandle, ActorListener> actorListeners_;
    std::unordered_map<SPS::API::ListenerHandle, ActorListener> npcListeners_;
    SPS::API::RequestHandle nextRequestHandle_{ 1 };
    SPS::API::ListenerHandle nextListenerHandle_{ 1 };
    unsigned accepted_{ 0 };
    unsigned released_{ 0 };
};

}
