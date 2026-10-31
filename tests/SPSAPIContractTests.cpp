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
static_assert(std::is_same_v<GetAPIFn, GetAPIV1Fn>);
static_assert(std::is_same_v<GetArousalFn, Result(__cdecl*)(std::uint32_t, float*)>);
static_assert(std::is_same_v<GetErectThresholdFn, Result(__cdecl*)(float*)>);
