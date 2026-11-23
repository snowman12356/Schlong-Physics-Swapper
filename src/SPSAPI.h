#pragma once

#include <cstddef>
#include <cstdint>

// Public, dependency-free ABI for Schlong Physics Swapper.
// Consumers should copy this header and discover the interface at runtime with
// GetModuleHandle/GetProcAddress. SPS remains an optional dependency.
namespace SPS::API {
inline constexpr auto kPluginDLL = L"SchlongPhysicsSwapper.dll";
inline constexpr auto kGetAPIFunctionNameV1 = "SchlongPhysicsSwapper_GetAPI_V1";
inline constexpr auto kGetAPIFunctionNameV2 = "SchlongPhysicsSwapper_GetAPI_V2";
inline constexpr auto kGetAPIFunctionNameV3 = "SchlongPhysicsSwapper_GetAPI_V3";
inline constexpr std::uint32_t kVersionV1 = 1;
inline constexpr std::uint32_t kVersionV2 = 2;
inline constexpr std::uint32_t kVersionV3 = 3;
// Source compatibility for existing V1 consumers.
inline constexpr std::uint32_t kVersion = kVersionV1;
inline constexpr std::uint32_t kPlayerFormID = 0x14;

using RequestHandle = std::uint64_t;
using ListenerHandle = std::uint64_t;

enum class Result : std::uint32_t {
    Success = 0,
    InvalidArgument,
    UnsupportedActor,
    NotReady,
    NotFound
};

enum class PhysicsState : std::uint32_t {
    Unknown = 0,
    SMP,
    CBPC
};

enum class ControlSource : std::uint32_t {
    None = 0,
    SPS,
    Scene,
    ExternalAPI
};

enum class Capability : std::uint32_t {
    Player = 1u << 0,
    TimedRequests = 1u << 1,
    StateListeners = 1u << 2,
    ActorAwareABI = 1u << 3,
    ResetNotification = 1u << 4,
    LiveArousalReadback = 1u << 5,
    ErectThresholdReadback = 1u << 6,
    // V3: GetActorArousal works for any loaded actor, not only the player.
    ActorArousalReadback = 1u << 7,
    // V3: IsActorInScene is available for the player and loaded NPCs.
    ActorSceneReadback = 1u << 8
};

struct PhysicsRequest {
    std::uint32_t apiVersion{ kVersion };
    std::uint32_t size{ sizeof(PhysicsRequest) };
    std::uint32_t actorFormID{ kPlayerFormID };
    PhysicsState state{ PhysicsState::Unknown };
    // Zero keeps the request active until ReleasePhysics is called. Requests
    // are always cleared when a save is loaded or a new game starts.
    std::uint32_t durationMilliseconds{ 0 };
    // Optional diagnostic label. SPS copies this string during the call.
    const char* requesterName{ nullptr };
};

struct StateSnapshot {
    std::uint32_t apiVersion{ kVersion };
    std::uint32_t size{ sizeof(StateSnapshot) };
    std::uint32_t actorFormID{ kPlayerFormID };
    PhysicsState state{ PhysicsState::Unknown };
    ControlSource source{ ControlSource::None };
    float arousal{ 0.0F };
    RequestHandle activeRequest{ 0 };
    std::uint32_t activeRequestCount{ 0 };
    std::uint32_t reserved{ 0 };
};

struct StateChangedEvent {
    std::uint32_t apiVersion{ kVersion };
    std::uint32_t size{ sizeof(StateChangedEvent) };
    std::uint32_t actorFormID{ kPlayerFormID };
    PhysicsState previousState{ PhysicsState::Unknown };
    PhysicsState currentState{ PhysicsState::Unknown };
    ControlSource source{ ControlSource::None };
    RequestHandle activeRequest{ 0 };
};

using StateChangedCallback = void(__cdecl*)(const StateChangedEvent*, void* userData);
using GetCapabilitiesFn = std::uint32_t(__cdecl*)();
using IsActorSupportedFn = bool(__cdecl*)(std::uint32_t actorFormID);
using GetStateFn = Result(__cdecl*)(std::uint32_t actorFormID, StateSnapshot* snapshot);
using RequestPhysicsFn = RequestHandle(__cdecl*)(const PhysicsRequest* request);
using ReleasePhysicsFn = Result(__cdecl*)(RequestHandle request);
// Tell SPS that another plugin rebuilt/reset this actor's physics. SPS waits
// briefly for the reset to settle, then re-confirms its current owner once.
using NotifyPhysicsResetFn = Result(__cdecl*)(std::uint32_t actorFormID);
using RegisterStateListenerFn = ListenerHandle(__cdecl*)(StateChangedCallback callback, void* userData);
using UnregisterStateListenerFn = Result(__cdecl*)(ListenerHandle listener);

struct InterfaceV1 {
    std::uint32_t version{ kVersionV1 };
    std::uint32_t size{ sizeof(InterfaceV1) };
    GetCapabilitiesFn GetCapabilities{ nullptr };
    IsActorSupportedFn IsActorSupported{ nullptr };
    GetStateFn GetState{ nullptr };
    RequestPhysicsFn RequestPhysics{ nullptr };
    ReleasePhysicsFn ReleasePhysics{ nullptr };
    NotifyPhysicsResetFn NotifyPhysicsReset{ nullptr };
    RegisterStateListenerFn RegisterStateListener{ nullptr };
    UnregisterStateListenerFn UnregisterStateListener{ nullptr };
};

using GetAPIFn = const InterfaceV1*(__cdecl*)();
using GetAPIV1Fn = GetAPIFn;

// These values are the authoritative values currently used by SPS. GetArousal
// returns NotReady until SPS has accepted a valid reading from its configured
// provider. Neither function changes SPS state or triggers a provider query.
// "Live" means the latest accepted value still used by SPS, including a last
// good value retained during provider delays; it is not a synchronous resample.
// Currently only actorFormID=0x14 (player) is supported. Null output pointers
// return InvalidArgument; other actors return UnsupportedActor. Outputs are
// written only on Success. A valid zero arousal is Success, not NotReady.
// GetErectThreshold returns NotReady before settings load, then reads the
// configured Automatic-mode threshold even when SPS is disabled/forced/in-scene.
using GetArousalFn = Result(__cdecl*)(std::uint32_t actorFormID, float* arousal);
using GetErectThresholdFn = Result(__cdecl*)(float* threshold);

// V2 deliberately repeats the complete V1 prefix before appending members.
// Consumers still resolve the versioned V2 export and validate version/size;
// the matching prefix keeps the ABI straightforward to audit.
struct InterfaceV2 {
    std::uint32_t version{ kVersionV2 };
    std::uint32_t size{ sizeof(InterfaceV2) };
    GetCapabilitiesFn GetCapabilities{ nullptr };
    IsActorSupportedFn IsActorSupported{ nullptr };
    GetStateFn GetState{ nullptr };
    RequestPhysicsFn RequestPhysics{ nullptr };
    ReleasePhysicsFn ReleasePhysics{ nullptr };
    NotifyPhysicsResetFn NotifyPhysicsReset{ nullptr };
    RegisterStateListenerFn RegisterStateListener{ nullptr };
    UnregisterStateListenerFn UnregisterStateListener{ nullptr };
    GetArousalFn GetArousal{ nullptr };
    GetErectThresholdFn GetErectThreshold{ nullptr };
};

static_assert(offsetof(InterfaceV2, UnregisterStateListener) ==
              offsetof(InterfaceV1, UnregisterStateListener));

using GetAPIV2Fn = const InterfaceV2*(__cdecl*)();

// V3 adds two read-only, per-actor answers. Neither changes SPS state, requests
// physics, or makes an actor "supported" for RequestPhysics/GetState: those still
// accept only the player (IsActorSupported is unchanged).
//
// Both must be called on the game thread (for example from an SKSE task). They
// are cheap: no Papyrus stack is created per call. Outputs are written only on
// Success. Null output pointers return InvalidArgument; an actor that is not a
// loaded, enabled actor returns UnsupportedActor.
//
// GetActorArousal returns the current 0-100 value from the configured arousal
// provider for any loaded actor, read directly (OSL Aroused, SLO Aroused NG) or
// from the provider's faction (SexLab Aroused Redux). A real 0 is Success.
// NotReady means the provider has no reading for that actor.
//
// IsActorInScene is true while SexLab or OStim has the actor in a scene and for
// SPS's scene-end delay afterwards, the same hold the player gets. NPCs are
// answered from a cache that SPS refreshes about once a second after the first
// call, so the first call for an actor returns NotReady. Actors not asked about
// for 30 seconds are dropped from that cache.
using GetActorArousalFn = Result(__cdecl*)(std::uint32_t actorFormID, float* arousal);
using IsActorInSceneFn = Result(__cdecl*)(std::uint32_t actorFormID, bool* inScene);

// V3 repeats the complete V2 layout unchanged and appends two members.
struct InterfaceV3 {
    std::uint32_t version{ kVersionV3 };
    std::uint32_t size{ sizeof(InterfaceV3) };
    GetCapabilitiesFn GetCapabilities{ nullptr };
    IsActorSupportedFn IsActorSupported{ nullptr };
    GetStateFn GetState{ nullptr };
    RequestPhysicsFn RequestPhysics{ nullptr };
    ReleasePhysicsFn ReleasePhysics{ nullptr };
    NotifyPhysicsResetFn NotifyPhysicsReset{ nullptr };
    RegisterStateListenerFn RegisterStateListener{ nullptr };
    UnregisterStateListenerFn UnregisterStateListener{ nullptr };
    GetArousalFn GetArousal{ nullptr };
    GetErectThresholdFn GetErectThreshold{ nullptr };
    GetActorArousalFn GetActorArousal{ nullptr };
    IsActorInSceneFn IsActorInScene{ nullptr };
};

static_assert(offsetof(InterfaceV3, GetErectThreshold) == offsetof(InterfaceV2, GetErectThreshold));

using GetAPIV3Fn = const InterfaceV3*(__cdecl*)();

// ---------------------------------------------------------------------------
// V4: scene role, "wait before going erect", and push notifications.
//
// All V4 calls are player-only (actorFormID 0x14); any other actor returns
// UnsupportedActor (RequestRiseHold returns 0). Call them, and expect the actor
// listener callbacks, on the game thread. Nothing here changes how V1-V3 behave
// except that IsActorInScene (V3) now also reports a scene that only Accurate
// Penetration is describing.
// ---------------------------------------------------------------------------
inline constexpr auto kGetAPIFunctionNameV4 = "SchlongPhysicsSwapper_GetAPI_V4";
inline constexpr std::uint32_t kVersionV4 = 4;
// The longest a rise hold lasts. A longer request is clamped to this, so an
// addon that crashes or forgets to release cannot keep the shaft soft for long.
inline constexpr std::uint32_t kMaxRiseHoldMilliseconds = 10000;

enum class SceneRole : std::uint32_t {
    Unknown = 0,
    Receiving = 1,
    Penetrating = 2
};

enum class ActorEventKind : std::uint32_t {
    // The accepted arousal value moved by at least 0.5 (or became valid).
    ArousalChanged = 1,
    // inScene or role changed.
    SceneChanged = 2
};

// V4 capability bits continue the Capability enum (values 1u<<9 and up).
inline constexpr std::uint32_t kCapabilitySceneRoleReadback = 1u << 9;
inline constexpr std::uint32_t kCapabilityRiseHold = 1u << 10;
inline constexpr std::uint32_t kCapabilityActorEvents = 1u << 11;

struct RiseHoldRequest {
    std::uint32_t apiVersion{ kVersionV4 };
    std::uint32_t size{ sizeof(RiseHoldRequest) };
    std::uint32_t actorFormID{ kPlayerFormID };
    // 1 to kMaxRiseHoldMilliseconds. Zero is rejected: a hold must always end.
    std::uint32_t durationMilliseconds{ 0 };
    // Optional diagnostic label. SPS copies this string during the call.
    const char* requesterName{ nullptr };
};

struct ActorEvent {
    std::uint32_t apiVersion{ kVersionV4 };
    std::uint32_t size{ sizeof(ActorEvent) };
    std::uint32_t actorFormID{ kPlayerFormID };
    ActorEventKind kind{ ActorEventKind::ArousalChanged };
    // The accepted arousal at the time of the event; 0 while arousalValid is 0.
    float arousal{ 0.0F };
    // Same meaning as IsActorInScene: a scene, plus SPS's scene-end delay.
    // Independent of who controls physics (a rise hold or RequestPhysics does
    // not hide it).
    std::uint32_t inScene{ 0 };
    SceneRole role{ SceneRole::Unknown };
    std::uint32_t arousalValid{ 0 };
};

// GetActorSceneRole: the player's role in the current scene, or Unknown when no
// scene is active, the scene framework has not reported a role yet, or the role
// could not be identified. Sources, in order: OStim, SexLab, then Accurate
// Penetration. Works without Accurate Penetration. Success is returned whenever
// the call itself worked, including for Unknown.
//
// RequestRiseHold: ask SPS to delay the CBPC rise that arousal alone would
// start. While any hold is active and SMP (or an unknown owner) has the bones,
// SPS does not hand them to CBPC for arousal. It does NOT delay a scene's own
// request, and does not stop a rise already under way. Release with
// ReleaseRiseHold; it also ends on its own after durationMilliseconds, when the
// game is loaded, and when SPS is disabled. Returns 0 if rejected. The handle is
// not valid for ReleasePhysics.
//
// Actor listeners are called on the game thread, in order, with no SPS lock
// held, so a callback may call RequestRiseHold or any read function. On an
// ArousalChanged event the callbacks run before SPS evaluates the new value, so
// a hold taken inside the callback is in place when SPS decides.
using GetActorSceneRoleFn = Result(__cdecl*)(std::uint32_t actorFormID, SceneRole* role);
using RequestRiseHoldFn = RequestHandle(__cdecl*)(const RiseHoldRequest* request);
using ReleaseRiseHoldFn = Result(__cdecl*)(RequestHandle request);
using ActorEventCallback = void(__cdecl*)(const ActorEvent*, void* userData);
using RegisterActorEventListenerFn = ListenerHandle(__cdecl*)(ActorEventCallback callback, void* userData);
using UnregisterActorEventListenerFn = Result(__cdecl*)(ListenerHandle listener);

// V4 repeats the complete V3 layout unchanged and appends five members.
struct InterfaceV4 {
    std::uint32_t version{ kVersionV4 };
    std::uint32_t size{ sizeof(InterfaceV4) };
    GetCapabilitiesFn GetCapabilities{ nullptr };
    IsActorSupportedFn IsActorSupported{ nullptr };
    GetStateFn GetState{ nullptr };
    RequestPhysicsFn RequestPhysics{ nullptr };
    ReleasePhysicsFn ReleasePhysics{ nullptr };
    NotifyPhysicsResetFn NotifyPhysicsReset{ nullptr };
    RegisterStateListenerFn RegisterStateListener{ nullptr };
    UnregisterStateListenerFn UnregisterStateListener{ nullptr };
    GetArousalFn GetArousal{ nullptr };
    GetErectThresholdFn GetErectThreshold{ nullptr };
    GetActorArousalFn GetActorArousal{ nullptr };
    IsActorInSceneFn IsActorInScene{ nullptr };
    GetActorSceneRoleFn GetActorSceneRole{ nullptr };
    RequestRiseHoldFn RequestRiseHold{ nullptr };
    ReleaseRiseHoldFn ReleaseRiseHold{ nullptr };
    RegisterActorEventListenerFn RegisterActorEventListener{ nullptr };
    UnregisterActorEventListenerFn UnregisterActorEventListener{ nullptr };
};

static_assert(offsetof(InterfaceV4, IsActorInScene) == offsetof(InterfaceV3, IsActorInScene));

using GetAPIV4Fn = const InterfaceV4*(__cdecl*)();

// ---------------------------------------------------------------------------
// V5: read-only NPC state, scene role and events.
//
// Everything here is read-only. None of it controls NPC physics: RequestPhysics,
// GetState and the V4 rise hold still accept only the player. Call on the game
// thread. "Managed" means the actor is enrolled in SPS's NPC management (at most
// one at a time by default, up to eight if the user allows it); other loaded actors can still be asked about scene and role.
// ---------------------------------------------------------------------------
inline constexpr auto kGetAPIFunctionNameV5 = "SchlongPhysicsSwapper_GetAPI_V5";
inline constexpr std::uint32_t kVersionV5 = 5;

inline constexpr std::uint32_t kCapabilityActorState = 1u << 12;
inline constexpr std::uint32_t kCapabilityActorSceneRole = 1u << 13;
inline constexpr std::uint32_t kCapabilityNpcEvents = 1u << 14;

// ActorStateSnapshot::validFlags: which fields hold a real reading. A field whose
// bit is clear holds its default and must be ignored.
inline constexpr std::uint32_t kStatePhysicsValid = 1u << 0;
inline constexpr std::uint32_t kStateArousalValid = 1u << 1;
inline constexpr std::uint32_t kStateSceneValid = 1u << 2;
inline constexpr std::uint32_t kStateRoleValid = 1u << 3;

struct ActorStateSnapshot {
    std::uint32_t apiVersion{ kVersionV5 };
    std::uint32_t size{ sizeof(ActorStateSnapshot) };
    std::uint32_t actorFormID{ kPlayerFormID };
    std::uint32_t validFlags{ 0 };
    // The owner SPS selected (command state, not proof of visible motion).
    PhysicsState state{ PhysicsState::Unknown };
    // SPS for a managed NPC, the usual source for the player, None otherwise.
    ControlSource source{ ControlSource::None };
    float arousal{ 0.0F };
    // Same meaning as IsActorInScene.
    std::uint32_t inScene{ 0 };
    SceneRole role{ SceneRole::Unknown };
    // 1 when SPS manages this actor's physics (the player always, a managed NPC).
    std::uint32_t managed{ 0 };
};

// GetActorState: the player or any loaded actor. Success whenever the actor is
// loaded; check validFlags for what could be read. Physics is only valid for the
// player and managed NPCs. An actor that is not loaded returns UnsupportedActor.
//
// GetAnyActorSceneRole: like V4 GetActorSceneRole but also for loaded NPCs. For an
// NPC the first call returns NotReady while SPS asks SexLab/OStim (about a second),
// and the answer is cached like IsActorInScene. Unknown is a successful answer.
//
// NPC listeners receive the same ActorEvent as the V4 listeners, with actorFormID set
// to the NPC, for managed NPCs only: ArousalChanged when the NPC's arousal moves by
// 0.5 or more, SceneChanged when its inScene or role changes. They are raised on the
// game thread from SPS's NPC service tick (about once a second), not instantly. The
// V4 listeners still hear only the player.
using GetActorStateFn = Result(__cdecl*)(std::uint32_t actorFormID, ActorStateSnapshot* snapshot);
using GetAnyActorSceneRoleFn = Result(__cdecl*)(std::uint32_t actorFormID, SceneRole* role);
using RegisterNpcEventListenerFn = ListenerHandle(__cdecl*)(ActorEventCallback callback, void* userData);
using UnregisterNpcEventListenerFn = Result(__cdecl*)(ListenerHandle listener);

// V5 repeats the complete V4 layout unchanged and appends four members.
struct InterfaceV5 {
    std::uint32_t version{ kVersionV5 };
    std::uint32_t size{ sizeof(InterfaceV5) };
    GetCapabilitiesFn GetCapabilities{ nullptr };
    IsActorSupportedFn IsActorSupported{ nullptr };
    GetStateFn GetState{ nullptr };
    RequestPhysicsFn RequestPhysics{ nullptr };
    ReleasePhysicsFn ReleasePhysics{ nullptr };
    NotifyPhysicsResetFn NotifyPhysicsReset{ nullptr };
    RegisterStateListenerFn RegisterStateListener{ nullptr };
    UnregisterStateListenerFn UnregisterStateListener{ nullptr };
    GetArousalFn GetArousal{ nullptr };
    GetErectThresholdFn GetErectThreshold{ nullptr };
    GetActorArousalFn GetActorArousal{ nullptr };
    IsActorInSceneFn IsActorInScene{ nullptr };
    GetActorSceneRoleFn GetActorSceneRole{ nullptr };
    RequestRiseHoldFn RequestRiseHold{ nullptr };
    ReleaseRiseHoldFn ReleaseRiseHold{ nullptr };
    RegisterActorEventListenerFn RegisterActorEventListener{ nullptr };
    UnregisterActorEventListenerFn UnregisterActorEventListener{ nullptr };
    GetActorStateFn GetActorState{ nullptr };
    GetAnyActorSceneRoleFn GetAnyActorSceneRole{ nullptr };
    RegisterNpcEventListenerFn RegisterNpcEventListener{ nullptr };
    UnregisterNpcEventListenerFn UnregisterNpcEventListener{ nullptr };
};

static_assert(offsetof(InterfaceV5, UnregisterActorEventListener) ==
              offsetof(InterfaceV4, UnregisterActorEventListener));

using GetAPIV5Fn = const InterfaceV5*(__cdecl*)();
}
