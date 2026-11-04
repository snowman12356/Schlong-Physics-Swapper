#pragma once

#include "Settings.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace SPS::Runtime {

// The managed NPC reuses the player's decision rules through the same
// Core::Settings policy, so the two paths cannot silently drift apart.
// Player-only fields (random, morning, refractory, sleep) are simply never read
// for an NPC; they describe player time-of-day and sleep assumptions.
using ManagedNpcSettings = Core::Settings;

struct ManagedNpcActorStatus {
    std::uint32_t formID{ 0 };
    std::string name;
    std::string state{ "Unknown" };
    std::optional<float> arousal;
    bool loaded{ false };
};

struct ManagedNpcStatus {
    std::string state{ "Off" };
    std::size_t capacity{ 1 };
    std::string rejection;  // Last Add/Remove refusal, cleared on success.
    std::uint64_t switched{ 0 }, applied{ 0 }, deferred{ 0 }, failed{ 0 }, blocked{ 0 };
    std::vector<ManagedNpcActorStatus> actors;
};

// One fully managed NPC alongside the player, per the Stage 3 milestone. The
// player is never counted against this limit and is never a target here.
inline constexpr std::size_t kManagedNpcCapacity = 1;

// Game-thread entry point. `playerBusy` keeps managed work behind the player's
// own pending handoff because both share one engine operation gate.
void TickManagedNpcs(bool enabled, const ManagedNpcSettings& settings,
    std::int64_t allowedAfterMs, bool playerBusy);
void ResetManagedNpcs();
// Thread-safe immediate revoke when the feature is disabled. Re-enabling starts
// new lifetimes on the game thread and never revives queued work.
void ConfigureManagedNpcCommands(bool enabled);

// Thread-safe. Add/Remove are queued and applied on the game thread because the
// registry is game-thread-only. A refusal is reported through the status.
bool QueueManagedNpcAdd(std::uint32_t formID);
bool QueueManagedNpcRemove(std::uint32_t formID);

// Thread-safe invalidation, mirroring the basic bridge's event hook.
void InvalidateManagedNpc(std::uint32_t formID);

// True when this actor is enrolled in full management. The basic bridge must
// skip these actors so the two paths never command the same target.
bool IsManagedNpc(std::uint32_t formID);

ManagedNpcStatus ReadManagedNpcStatus();
// Game-thread helper resolving the current crosshair actor, or 0.
std::uint32_t ResolveManagedNpcTarget();

}
