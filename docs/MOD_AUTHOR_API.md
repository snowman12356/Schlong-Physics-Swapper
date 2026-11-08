# SPS compatibility API (V1, V2 and V3)

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

V2 is first available in the local **2.1.0.2 test candidate**. Published 2.0.1
and the earlier 2.1.0.1 NPC candidate do not export it.

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
| `IsActorInScene(formID, bool*)` | player or loaded NPC | true/false | SexLab and OStim, plus SPS's scene-end delay afterwards (the same hold the player gets). |

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
