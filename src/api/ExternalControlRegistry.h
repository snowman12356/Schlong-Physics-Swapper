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

private:
    mutable std::mutex lock_;
    std::unordered_map<SPS::API::RequestHandle, Request> requests_;
    std::unordered_map<SPS::API::ListenerHandle, Listener> listeners_;
    SPS::API::RequestHandle nextRequestHandle_{ 1 };
    SPS::API::ListenerHandle nextListenerHandle_{ 1 };
    unsigned accepted_{ 0 };
    unsigned released_{ 0 };
};

}
