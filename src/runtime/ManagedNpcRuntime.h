#pragma once

#include "Settings.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace SPS::Runtime {

// The managed NPC reuses the player's decision rules through the same
// Core::Settings policy, so the two paths cannot silently drift apart.
// Player-only fields (random, morning, refractory, sleep) are simply never read
// for an NPC; they describe player time-of-day and sleep assumptions.
using ManagedNpcSettings = Core::Settings;

// The only settings a managed NPC has. They are its own, separate from the
// player's: nothing here reads or changes the player page. Saved per NPC (by its
// base record, so they survive reloads) in SchlongPhysicsSwapper_NPCs.ini.
struct ManagedNpcOptions {
    float erectAt{ 60.0F };       // Arousal at which the NPC is fully erect.
    int angle{ 14 };              // Erect angle, 1 to 20.
    bool followArousal{ false };  // Angle grows with arousal instead of jumping.
    int riseSeconds{ 3 };         // Time to rise, 1 to 30.
    int fallSeconds{ 5 };         // Time to fall, 1 to 30.
};

struct ManagedNpcActorStatus {
    std::uint32_t formID{ 0 };
    ManagedNpcOptions options;
    std::string name;
    std::string state{ "Unknown" };
    // "Soft (SMP)", "Erect (CBPC)", "Switching" or "Unknown". Command state, not
    // proof of visible motion.
    std::string physics{ "Unknown" };
    std::optional<float> arousal;
    bool loaded{ false };
};

// A loaded NPC near the player, offered in the menu so it can be managed with
// one click instead of aiming the crosshair.
struct ManagedNpcCandidate {
    std::uint32_t formID{ 0 };
    std::string name;
    float distance{ 0.0F };
    bool managed{ false };
    std::string unavailable;  // Why it cannot be managed; empty when it can.
};

struct ManagedNpcStatus {
    std::string state{ "Off" };
    std::size_t capacity{ 1 };
    std::string rejection;  // Last Add/Remove refusal, cleared on success.
    std::uint64_t switched{ 0 }, applied{ 0 }, deferred{ 0 }, failed{ 0 }, blocked{ 0 };
    std::vector<ManagedNpcActorStatus> actors;
    std::vector<ManagedNpcCandidate> nearby;  // Closest first, at most kNearbyLimit.
};

inline constexpr std::size_t kManagedNpcNearbyLimit = 8;

// One fully managed NPC alongside the player, per the Stage 3 milestone. The
// player is never counted against this limit and is never a target here.
inline constexpr std::size_t kManagedNpcCapacity = 1;
// The limit when the user turns on "Allow up to 8 managed NPCs". Each managed NPC
// runs its own SMP/CBPC handoffs and Papyrus polls, so this is opt-in.
inline constexpr std::size_t kManagedNpcMaxCapacity = 8;

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
// Game thread. Writes the real OSL arousal value of a managed NPC through the
// same bridge call as the basic NPC test sliders. False if OSL is absent, the
// value is invalid or the actor is not managed.
bool QueueManagedNpcArousalWrite(std::uint32_t formID, float value);
// Game thread. Stores (clamped) and saves one managed NPC's own settings.
// `save` false applies it now without writing the file (used while dragging).
bool SetManagedNpcOptions(std::uint32_t formID, const ManagedNpcOptions& options, bool save = true);

// Per-save membership, stored by ActorIdentity's SPSW co-save (record MNPC). The
// live registry is rebuilt on every game load, so the saved list is re-added on
// the next enabled tick. Thread-safe. An actor that no longer exists is dropped.
std::vector<std::uint32_t> ReadManagedNpcMembership();
void RestoreManagedNpcMembership(const std::vector<std::uint32_t>& formIDs);
void ClearManagedNpcMembership();

// Thread-safe invalidation, mirroring the basic bridge's event hook.
void InvalidateManagedNpc(std::uint32_t formID);

// True when this actor is enrolled in full management. The basic bridge must
// skip these actors so the two paths never command the same target.
bool IsManagedNpc(std::uint32_t formID);

// Game thread. A managed NPC equipped or removed armour that can cover or
// replace the genitals. After a short debounce its driver re-asserts the
// current owner (an ordinary handoff to the same owner, no reset), because
// FSMP rebuilds a dynamic shaft for the new mesh. Ignored for other actors.
void NotifyManagedNpcEquipment(std::uint32_t formID);

// Public API V5. Game thread. The physics owner SPS currently selected for a
// managed NPC and its last accepted arousal. Nullopt when the actor is not managed.
struct ManagedNpcApiState {
    bool physicsKnown{ false };
    bool usingCBPC{ false };
    std::optional<float> arousal;
    // The scene role the managed driver already read (1 receiving, 2
    // penetrating). Unset outside an active scene or before the first read, so
    // the API falls back to its own probe.
    std::optional<int> sceneRole;
};
std::optional<ManagedNpcApiState> ReadManagedNpcApiState(std::uint32_t formID);
// Called once per service tick for every managed NPC while set, so the public
// API can raise NPC events without ManagedNpcRuntime knowing about listeners.
using ManagedNpcTickSink = std::function<void(std::uint32_t formID, std::optional<float> arousal,
    std::int64_t allowedAfterMs)>;
void SetManagedNpcTickSink(ManagedNpcTickSink sink);

ManagedNpcStatus ReadManagedNpcStatus();
// Game-thread helper resolving the current crosshair actor, or 0.
std::uint32_t ResolveManagedNpcTarget();

}
