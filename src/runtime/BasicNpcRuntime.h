#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace SPS::Runtime {

struct BasicNpcActorStatus {
    std::uint32_t formID{ 0 };
    std::string name;
    std::optional<float> arousal;
};

struct BasicNpcStatus {
    std::string state{ "Off" };
    unsigned tracked{ 0 }, waiting{ 0 }, inScene{ 0 }, pending{ 0 };
    std::uint64_t completed{ 0 }, deferred{ 0 }, failed{ 0 }, blocked{ 0 };
    std::vector<BasicNpcActorStatus> actors;
};

// Game-thread entry points; status is copied for the menu thread.
void TickBasicNpcs(bool enabled, bool providerSosDisabled, std::int64_t allowedAfterMs, bool playerBusy);
void ResetBasicNpcs();
// Thread-safe event notification; invalidates only that actor's current lease.
void InvalidateBasicNpc(std::uint32_t formID);
BasicNpcStatus ReadBasicNpcStatus();
// Thread-safe immediate revoke when controls are disabled. Re-enabling always
// creates new lifetimes on the game thread, never revives old queued commands.
void ConfigureBasicNpcCommands(bool enabled);
// Game-thread test action. Writes the real OSL value for an actor already
// owned by the bounded basic pool; it does not enroll or discover a new actor.
bool QueueBasicNpcOslArousalWrite(std::uint32_t formID, float value,
    std::int64_t allowedAfterMs);

}
