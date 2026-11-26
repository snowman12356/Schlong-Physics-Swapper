# SPS compatibility API (V1 to V5)

The SPS API lets another SKSE plugin cooperate with Schlong Physics Swapper
without linking against it or making it a required dependency.

V1 can:

- check whether SPS supports an actor;
- read the player's current SMP/CBPC state;
- temporarily ask SPS to hold SMP or CBPC;
- notify SPS after rebuilding/resetting player physics;
- receive a callback after SPS changes the physics state.

V2 preserves the complete V1 interface and adds read-only access to the live
arousal value SPS is currently using and SPS's configured erect threshold. It
does not query an arousal provider, change physics ownership or alter settings.

The structures already carry an actor FormID so the interface can be extended
to selected NPCs later. SPS V1 supports only the player (`0x14`). Always call
`IsActorSupported` instead of assuming that every actor is supported.

## Connecting safely

Copy `SPSAPI.h` into your project. Discover SPS at runtime so your mod still
loads when SPS is not installed:

```cpp
#include "SPSAPI.h"
#include <Windows.h>

const SPS::API::InterfaceV1* GetSPSAPI()
{
    const auto module = ::GetModuleHandleW(SPS::API::kPluginDLL);
    if (!module) {
        return nullptr;
    }

    const auto requestAPI = reinterpret_cast<SPS::API::GetAPIFn>(
        ::GetProcAddress(module, SPS::API::kGetAPIFunctionNameV1));
    const auto* api = requestAPI ? requestAPI() : nullptr;
    return api && api->version == SPS::API::kVersion &&
        api->size >= sizeof(SPS::API::InterfaceV1) ? api : nullptr;
}
```

An addon that requires the V2 read-only values should resolve
`SchlongPhysicsSwapper_GetAPI_V2`, require `version == kVersionV2`, validate
`size >= sizeof(InterfaceV2)`, and check the `LiveArousalReadback` and
`ErectThresholdReadback` capabilities. Keep using the V1 export when only V1
features are required; that export and structure remain unchanged.

## Reading live arousal and the erect threshold (V2)

```cpp
const SPS::API::InterfaceV2* GetSPSReadAPI()
{
    const auto module = ::GetModuleHandleW(SPS::API::kPluginDLL);
    const auto getAPI = module ? reinterpret_cast<SPS::API::GetAPIV2Fn>(
        ::GetProcAddress(module, SPS::API::kGetAPIFunctionNameV2)) : nullptr;
    const auto* api = getAPI ? getAPI() : nullptr;
    if (!api || api->version != SPS::API::kVersionV2 ||
        api->size < sizeof(SPS::API::InterfaceV2) || !api->GetCapabilities ||
        !api->GetArousal || !api->GetErectThreshold) return nullptr;
    const auto required = static_cast<std::uint32_t>(SPS::API::Capability::LiveArousalReadback) |
        static_cast<std::uint32_t>(SPS::API::Capability::ErectThresholdReadback);
    return (api->GetCapabilities() & required) == required ? api : nullptr;
}

const auto* apiV2 = GetSPSReadAPI();
float arousal = 0.0F;
float threshold = 0.0F;
if (apiV2 && apiV2->GetArousal(SPS::API::kPlayerFormID, &arousal) ==
        SPS::API::Result::Success &&
    apiV2->GetErectThreshold(&threshold) == SPS::API::Result::Success) {
    // arousal and threshold are both on SPS's 0-100 scale.
}
```

`GetArousal(actorFormID, &arousal)` returns the latest accepted provider reading
that SPS currently holds for its decisions. It returns `NotReady` when SPS has
no valid reading, including after a save reset. A genuine zero is `Success`.
It can succeed before physics ownership is confirmed, independently of
`GetState`. During provider delays SPS may retain its last good value; paused,
disabled or forced modes do not make this API resample the provider. There is
no extra query, enrollment, evaluation, ownership request or settings write.

`GetErectThreshold(&threshold)` reads the current `General/ArousalThreshold`
setting under the settings lock. It returns `NotReady` before settings load,
then succeeds regardless of arousal availability or physics readiness. It
reports the configured threshold even in forced modes, scenes or while SPS is
disabled. It is not the lower soft-return threshold (which includes hysteresis)
or `ErectionStartArousal`. Scene/external overrides can choose physics separately,
so comparing the two numbers does not replace `GetState`.

Both methods return `InvalidArgument` for a null output pointer and leave the
output unchanged on every failure. `GetArousal` returns `UnsupportedActor` for
all IDs except the player (`0x14`), including zero; there is no implicit player
fallback. Basic NPC tracking does not yet extend this public API's actor scope.
Use `IsActorSupported` to check scope and handle `NotReady` separately.

The reads are safe from other threads: arousal value and validity are captured
together, while threshold reads share the existing settings lock. The two API
calls are separate observations, not an atomic combined snapshot.

V1's export, table layout, capabilities and behavior remain unchanged. V2 adds
the two capability bits and function pointers. The request, snapshot and event
payloads retain `apiVersion=1` even when passed through V2; use their defaults.

## NPC arousal and scene reads (V3)

`SchlongPhysicsSwapper_GetAPI_V3` returns `InterfaceV3`: the V2 layout unchanged,
plus two members. Check `version == kVersionV3`, `size >= sizeof(InterfaceV3)` and
the capability bits `ActorArousalReadback` and `ActorSceneReadback`.

| Call | Input | Output | Notes |
|---|---|---|---|
| `GetActorArousal(formID, float*)` | any loaded actor | 0-100 | A real 0 is `Success`. `NotReady` if the provider has no reading. Works for any loaded NPC, not only ones SPS tracks. The player (`0x14`) behaves like `GetArousal`. |
| `IsActorInScene(formID, bool*)` | player or loaded NPC | true/false | SexLab and OStim (and, for the player, Accurate Penetration), plus SPS's scene-end delay afterwards (the same hold the player gets). |

Rules:

- **Game thread only** (for example from an SKSE task). No Papyrus stack is
  created per call.
- Outputs are written only on `Success`. A null output is `InvalidArgument`. An
  actor that is not loaded and enabled is `UnsupportedActor`.
- `IsActorInScene` for an NPC answers from a cache SPS refreshes about once a
  second through its existing scene bridge. The first call for an actor returns
  `NotReady`; keep asking. An answer older than 5 seconds is `NotReady` again, and
  actors not asked about for 30 seconds are dropped (up to 32 are cached).
- `GetActorArousal` reads OSL Aroused (`GetArousalExt`) or SLO Aroused NG directly,
  and SexLab Aroused Redux through its arousal faction. With no arousal provider it
  is `NotReady`.
- **`IsActorSupported` is unchanged and still means the player only.** `GetState`,
  `RequestPhysics` and the listeners still accept only the player, so other mods
  cannot request physics on NPCs through these reads.

## Scene role, rise hold and notifications (V4)

`SchlongPhysicsSwapper_GetAPI_V4` returns `InterfaceV4`: the V3 layout unchanged,
plus five members. Check `version == kVersionV4`, `size >= sizeof(InterfaceV4)` and
the capability bits `kCapabilitySceneRoleReadback`, `kCapabilityRiseHold` and
`kCapabilityActorEvents`. Every V4 call is player-only (`0x14`) and runs on the game
thread. Nothing in V1-V3 changed, except that `IsActorInScene` for the player is also
true while Accurate Penetration is reporting a scene.

**Scene role.** `GetActorSceneRole(formID, SceneRole*)` gives `Unknown`, `Receiving`
or `Penetrating`. It works without Accurate Penetration: OStim and SexLab roles are
used first, then PPA's. `Unknown` is a successful answer (no scene, role not reported
yet, or not identified). Asking keeps SPS's SexLab/OStim role queries running for 30
seconds even if the user has role switching off. The role needs SPS's SexLab/OStim
scene override enabled, as scene detection does. NPCs return `UnsupportedActor`.

**In-scene flag.** `IsActorInScene` (V3) reads scene state only. A rise hold, a
`RequestPhysics` hold or an SPS handoff never changes it, so it stays true for the
whole scene plus SPS's scene-end delay. `StateSnapshot.source` is still
`ExternalAPI` while your `RequestPhysics` is active; use `IsActorInScene` for the
scene, not `source`.

**Wait before going erect.** `RequestRiseHold(RiseHoldRequest*)` returns a handle (0
if rejected) and `ReleaseRiseHold(handle)` ends it. While any hold is active and
SMP (or an unknown owner) has the bones, SPS does not hand them to CBPC because of
arousal. It does not delay a scene's own request and does not stop a rise already
under way. `durationMilliseconds` must be 1 to `kMaxRiseHoldMilliseconds` (10000);
a longer value is clamped, zero is rejected, so a hold always ends. Holds also end
when a game loads or SPS is disabled. The handle is separate from `RequestPhysics`
handles: pass it to `ReleaseRiseHold`, not `ReleasePhysics`. Releasing lets a waiting
rise start straight away.

**Notifications.** `RegisterActorEventListener(callback, userData)` /
`UnregisterActorEventListener(handle)`. The callback receives an `ActorEvent` with
`kind`, the current `arousal` (`arousalValid` says whether SPS has one), `inScene` and
`role`:

- `ArousalChanged`: the accepted arousal moved by 0.5 or more (or became valid). It
  runs before SPS evaluates the new value, in the same game-thread task, so a hold
  you take inside the callback is in place when SPS decides. This closes the race on
  sudden arousal jumps: take `RequestRiseHold` and then your SMP request inside the
  callback.
- `SceneChanged`: `inScene` or `role` changed (checked on SPS's poll, about every
  tick). The baseline is taken when you register, so read `IsActorInScene` once at
  registration.

Callbacks run with no SPS lock held and may call any V4 function. Keep them short
and do not throw; an exception is caught and logged.

Not verified in game yet: the V4 calls are built and unit-checked for layout only.

## NPC state, scene role and events (V5)

`SchlongPhysicsSwapper_GetAPI_V5` returns `InterfaceV5`: the V4 layout unchanged,
plus four members. Check `version == kVersionV5`, `size >= sizeof(InterfaceV5)` and
the capability bits `kCapabilityActorState`, `kCapabilityActorSceneRole` and
`kCapabilityNpcEvents`. **Everything in V5 is read-only.** It never controls NPC
physics: `RequestPhysics`, `GetState`, the V4 rise hold and the V4 listeners still
accept only the player. Call on the game thread.

| Call | Notes |
|---|---|
| `GetActorState(formID, ActorStateSnapshot*)` | The player or any loaded actor. Pass a snapshot with `apiVersion = kVersionV5` and `size = sizeof(ActorStateSnapshot)`. `validFlags` says which fields are real (`kStatePhysicsValid`, `kStateArousalValid`, `kStateSceneValid`, `kStateRoleValid`); ignore a field whose bit is clear. Physics (`state`, `source`) is only valid for the player and managed NPCs; `managed` is 1 for those. An actor that is not loaded returns `UnsupportedActor`. |
| `GetAnyActorSceneRole(formID, SceneRole*)` | Like V4 `GetActorSceneRole` but also for loaded NPCs. The first NPC call returns `NotReady` while SPS asks SexLab/OStim (about a second); answers are cached and refresh about once a second, like `IsActorInScene`. `Unknown` is a successful answer. |
| `RegisterNpcEventListener` / `UnregisterNpcEventListener` | `ActorEvent` for **managed NPCs only**, with `actorFormID` set to the NPC: `ArousalChanged` (arousal moved by 0.5 or more) and `SceneChanged` (`inScene` or `role` changed). |

NPC events come from SPS's NPC service tick (about once a second), on the game thread,
so they are not instant. The role comes from OStim first, then SexLab's directional
interactions (the same test the player role uses). A scene that only OSL Aroused
reports has no role, so `role` stays `Unknown`. NPC reads need the matching SPS Papyrus
bridges (`SPS_PositionBridge` v7; `SPS_SexLabBridge` v6 or newer for SexLab; the optional
`SPS_OStimBridge` v4 for OStim). With an
older bridge, NPC scene and role reads stay `NotReady`.

Not verified in game yet: layout-checked and built only.

## Requesting physics

Keep the returned handle and release it when your scene or feature ends:

```cpp
SPS::API::RequestHandle requestHandle = 0;

void BeginMyScene(const SPS::API::InterfaceV1* api)
{
    if (!api || !api->IsActorSupported(SPS::API::kPlayerFormID)) {
        return;
    }

    SPS::API::PhysicsRequest request;
    request.state = SPS::API::PhysicsState::CBPC;
    request.requesterName = "My SKSE plugin";
    requestHandle = api->RequestPhysics(&request);
}

void EndMyScene(const SPS::API::InterfaceV1* api)
{
    if (api && requestHandle != 0) {
        api->ReleasePhysics(requestHandle);
        requestHandle = 0;
    }
}
```

Set `durationMilliseconds` for a request that should expire automatically.
Zero keeps it active until released. SPS also clears every request when a save
is loaded or a new game begins, preventing a forgotten request from becoming
stuck between game sessions.

If several plugins request control, the newest active request wins. Releasing
or expiring it reveals the previous active request. Do not submit a new request
every frame or every update; keep and reuse your handle.

`RequestPhysics` returns zero when the request is invalid, the actor is not yet
supported, or SPS has been disabled by the player.

## Reading state

```cpp
SPS::API::StateSnapshot state;
const auto result = api->GetState(SPS::API::kPlayerFormID, &state);
if (result == SPS::API::Result::Success) {
    const bool erectPhysics = state.state == SPS::API::PhysicsState::CBPC;
}
```

`NotReady` means SPS has loaded but has not confirmed the player's first
physics state yet. The snapshot still reports `Unknown` in that case.

## State-change callbacks

Register once after SKSE's data-loaded message and unregister during your own
shutdown path if needed:

```cpp
void __cdecl OnSPSStateChanged(
    const SPS::API::StateChangedEvent* event,
    void* userData)
{
    if (!event || event->apiVersion != SPS::API::kVersion ||
        event->size < sizeof(SPS::API::StateChangedEvent)) {
        return;
    }
    // The callback runs on Skyrim's game thread.
}

const auto listener = api->RegisterStateListener(OnSPSStateChanged, nullptr);
```

The callback is sent only after SPS successfully changes ownership between SMP
and CBPC. Query `GetState` whenever you need a complete snapshot.

## Cooperating with physics resets

If your plugin resets or rebuilds the player's physics, notify SPS after
starting that reset:

```cpp
if (api && api->IsActorSupported(SPS::API::kPlayerFormID)) {
    api->NotifyPhysicsReset(SPS::API::kPlayerFormID);
}
```

SPS waits briefly for the reset to settle, then re-confirms its already-chosen
SMP or CBPC owner once. It does not change arousal, scene role, settings, or
erect angle. Check the `ResetNotification` capability before using this member
when supporting future API versions.

## Compatibility rules

- Treat SPS as optional and obtain the API with `GetProcAddress`.
- Check `version`, `size`, capabilities and actor support.
- Ask SPS for a physics state instead of directly toggling the same genital
  bones while SPS is active.
- Call `NotifyPhysicsReset` after an unavoidable actor physics rebuild.
- Release requests promptly when your scene or temporary feature ends.
- Do not use this API to control the live erection angle. SPS coordinates that
  separately with SOS/TNG and PPA.

V2 consumer behavior still needs an in-game check with a consuming plugin.
Compilation and ABI/package validation do not establish a live integration pass.
