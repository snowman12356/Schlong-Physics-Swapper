# SPS compatibility API (V1)

The SPS API lets another SKSE plugin cooperate with Schlong Physics Swapper
without linking against it or making it a required dependency.

V1 can:

- check whether SPS supports an actor;
- read the player's current SMP/CBPC state;
- temporarily ask SPS to hold SMP or CBPC;
- notify SPS after rebuilding/resetting player physics;
- receive a callback after SPS changes the physics state.

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

V1 is being introduced for SPS 1.8 and should be treated as an experimental API
until that release is marked stable.
