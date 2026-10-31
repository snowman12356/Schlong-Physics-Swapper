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
inline constexpr std::uint32_t kVersionV1 = 1;
inline constexpr std::uint32_t kVersionV2 = 2;
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
    ErectThresholdReadback = 1u << 6
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
}
