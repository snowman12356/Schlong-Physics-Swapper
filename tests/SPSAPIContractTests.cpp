#include "SPSAPI.h"
#include <cstddef>
#include <type_traits>

// Freeze the published x64 ABI, not just the relative layout of two structs
// which could otherwise change together and still pass a prefix comparison.
using namespace SPS::API;
static_assert(sizeof(void*) == 8);
static_assert(kVersion == 1 && kVersionV1 == 1 && kVersionV2 == 2);
static_assert(sizeof(PhysicsRequest) == 32 && offsetof(PhysicsRequest, requesterName) == 24);
static_assert(sizeof(StateSnapshot) == 40 && offsetof(StateSnapshot, arousal) == 20 &&
    offsetof(StateSnapshot, activeRequest) == 24);
static_assert(sizeof(StateChangedEvent) == 32 && offsetof(StateChangedEvent, activeRequest) == 24);
static_assert(PhysicsRequest{}.apiVersion == 1 && StateSnapshot{}.apiVersion == 1 &&
    StateChangedEvent{}.apiVersion == 1);  // V2 still uses the original payloads.
static_assert(std::is_standard_layout_v<InterfaceV1> && std::is_standard_layout_v<InterfaceV2>);
static_assert(sizeof(InterfaceV1) == 72 && InterfaceV1{}.version == 1);
static_assert(sizeof(InterfaceV2) == 88 && InterfaceV2{}.version == 2);

#define SPS_CHECK_API_OFFSET(member, offset) \
    static_assert(offsetof(InterfaceV1, member) == offset); \
    static_assert(offsetof(InterfaceV2, member) == offset)
SPS_CHECK_API_OFFSET(version, 0);
SPS_CHECK_API_OFFSET(size, 4);
SPS_CHECK_API_OFFSET(GetCapabilities, 8);
SPS_CHECK_API_OFFSET(IsActorSupported, 16);
SPS_CHECK_API_OFFSET(GetState, 24);
SPS_CHECK_API_OFFSET(RequestPhysics, 32);
SPS_CHECK_API_OFFSET(ReleasePhysics, 40);
SPS_CHECK_API_OFFSET(NotifyPhysicsReset, 48);
SPS_CHECK_API_OFFSET(RegisterStateListener, 56);
SPS_CHECK_API_OFFSET(UnregisterStateListener, 64);
#undef SPS_CHECK_API_OFFSET

static_assert(offsetof(InterfaceV2, GetArousal) == 72 && offsetof(InterfaceV2, GetErectThreshold) == 80);
// V3 keeps the whole V2 layout and appends exactly two function pointers.
static_assert(kVersionV3 == 3 && sizeof(InterfaceV3) == 104 && InterfaceV3{}.version == 3);
static_assert(std::is_standard_layout_v<InterfaceV3>);
static_assert(offsetof(InterfaceV3, GetArousal) == 72 && offsetof(InterfaceV3, GetErectThreshold) == 80 &&
    offsetof(InterfaceV3, GetActorArousal) == 88 && offsetof(InterfaceV3, IsActorInScene) == 96);
static_assert(static_cast<std::uint32_t>(Capability::ActorArousalReadback) == (1u << 7) &&
    static_cast<std::uint32_t>(Capability::ActorSceneReadback) == (1u << 8));
static_assert(std::is_same_v<GetActorArousalFn, Result(__cdecl*)(std::uint32_t, float*)>);
static_assert(std::is_same_v<IsActorInSceneFn, Result(__cdecl*)(std::uint32_t, bool*)>);
// V4 keeps the whole V3 layout and appends exactly five function pointers.
static_assert(kVersionV4 == 4 && sizeof(InterfaceV4) == 144 && InterfaceV4{}.version == 4);
static_assert(std::is_standard_layout_v<InterfaceV4>);
static_assert(offsetof(InterfaceV4, IsActorInScene) == 96 &&
    offsetof(InterfaceV4, GetActorSceneRole) == 104 && offsetof(InterfaceV4, RequestRiseHold) == 112 &&
    offsetof(InterfaceV4, ReleaseRiseHold) == 120 && offsetof(InterfaceV4, RegisterActorEventListener) == 128 &&
    offsetof(InterfaceV4, UnregisterActorEventListener) == 136);
static_assert(sizeof(RiseHoldRequest) == 24 && offsetof(RiseHoldRequest, requesterName) == 16 &&
    RiseHoldRequest{}.apiVersion == 4);
static_assert(sizeof(ActorEvent) == 32 && offsetof(ActorEvent, arousal) == 16 &&
    offsetof(ActorEvent, inScene) == 20 && offsetof(ActorEvent, role) == 24 &&
    offsetof(ActorEvent, arousalValid) == 28 && ActorEvent{}.apiVersion == 4);
static_assert(static_cast<std::uint32_t>(SceneRole::Unknown) == 0 &&
    static_cast<std::uint32_t>(SceneRole::Receiving) == 1 &&
    static_cast<std::uint32_t>(SceneRole::Penetrating) == 2);
static_assert(static_cast<std::uint32_t>(ActorEventKind::ArousalChanged) == 1 &&
    static_cast<std::uint32_t>(ActorEventKind::SceneChanged) == 2);
static_assert(kCapabilitySceneRoleReadback == (1u << 9) && kCapabilityRiseHold == (1u << 10) &&
    kCapabilityActorEvents == (1u << 11) && kMaxRiseHoldMilliseconds == 10000);
static_assert(std::is_same_v<GetActorSceneRoleFn, Result(__cdecl*)(std::uint32_t, SceneRole*)>);
static_assert(std::is_same_v<RequestRiseHoldFn, RequestHandle(__cdecl*)(const RiseHoldRequest*)>);
static_assert(std::is_same_v<ReleaseRiseHoldFn, Result(__cdecl*)(RequestHandle)>);
// V5 keeps the whole V4 layout and appends exactly four function pointers.
static_assert(kVersionV5 == 5 && sizeof(InterfaceV5) == 176 && InterfaceV5{}.version == 5);
static_assert(std::is_standard_layout_v<InterfaceV5>);
static_assert(offsetof(InterfaceV5, UnregisterActorEventListener) == 136 &&
    offsetof(InterfaceV5, GetActorState) == 144 && offsetof(InterfaceV5, GetAnyActorSceneRole) == 152 &&
    offsetof(InterfaceV5, RegisterNpcEventListener) == 160 &&
    offsetof(InterfaceV5, UnregisterNpcEventListener) == 168);
static_assert(sizeof(ActorStateSnapshot) == 40 && offsetof(ActorStateSnapshot, validFlags) == 12 &&
    offsetof(ActorStateSnapshot, state) == 16 && offsetof(ActorStateSnapshot, source) == 20 &&
    offsetof(ActorStateSnapshot, arousal) == 24 && offsetof(ActorStateSnapshot, inScene) == 28 &&
    offsetof(ActorStateSnapshot, role) == 32 && offsetof(ActorStateSnapshot, managed) == 36 &&
    ActorStateSnapshot{}.apiVersion == 5);
static_assert(kCapabilityActorState == (1u << 12) && kCapabilityActorSceneRole == (1u << 13) &&
    kCapabilityNpcEvents == (1u << 14));
static_assert(kStatePhysicsValid == 1 && kStateArousalValid == 2 && kStateSceneValid == 4 &&
    kStateRoleValid == 8);
static_assert(std::is_same_v<GetActorStateFn, Result(__cdecl*)(std::uint32_t, ActorStateSnapshot*)>);
static_assert(std::is_same_v<GetAnyActorSceneRoleFn, Result(__cdecl*)(std::uint32_t, SceneRole*)>);
static_assert(std::is_same_v<GetAPIFn, GetAPIV1Fn>);
static_assert(std::is_same_v<GetArousalFn, Result(__cdecl*)(std::uint32_t, float*)>);
static_assert(std::is_same_v<GetErectThresholdFn, Result(__cdecl*)(float*)>);
