#pragma once

#include <cstdint>
#include <RE/Skyrim.h>

// Minimal declarations for Accurate Penetration's documented V1 listener ABI.
// PPA remains optional and is discovered through GetProcAddress at runtime.
namespace SPS::PPA {
inline constexpr auto kPluginDLL = L"AccuratePenetration.dll";
inline constexpr auto kGetAPIFunctionNameV1 = "AccuratePenetration_GetAPI_V1";
inline constexpr std::uint32_t kVersion = 1;

enum class SceneContext : std::uint32_t {
    None = 0,
    Vaginal = 1 << 0,
    Anal = 1 << 1,
    Oral = 1 << 2,
    Aggressive = 1 << 3,
    FemDom = 1 << 4,
    Loving = 1 << 5,
    Dirty = 1 << 6,
    Boobjob = 1 << 7,
    Handjob = 1 << 8,
    Footjob = 1 << 9,
    Masturbation = 1 << 10
};

enum class PenetrationSite : std::uint8_t {
    None = 0,
    Mouth,
    Anus,
    Vagina,
    Both,
    HandL,
    HandR,
    Hands
};

struct InteractionPartner {
    RE::ActorHandle actor;
    PenetrationSite site{ PenetrationSite::None };
    std::uint8_t position{ 0 };
    float penetrationDepth{ 0.0F };
    float penisSize{ 0.0F };
    float penisGirth{ 0.0F };
};

struct AnimationUpdateEvent {
    std::uint32_t apiVersion{ kVersion };
    std::uint32_t size{ sizeof(AnimationUpdateEvent) };
    RE::ActorHandle receiver;
    std::uint8_t position{ 1 };
    SceneContext context{ SceneContext::None };
    const InteractionPartner* selfInteraction{ nullptr };
    const InteractionPartner* actors{ nullptr };
    std::uint32_t actorCount{ 0 };
    float anusOpening{ 0.0F };
    float vaginalOpening{ 0.0F };
    bool ending{ false };
};

using ListenerHandle = std::uint64_t;
using AnimationUpdateCallback = void(__cdecl*)(const AnimationUpdateEvent*, void*);
using RegisterAnimationUpdateListenerFn = ListenerHandle(__cdecl*)(AnimationUpdateCallback, void*);
using UnregisterAnimationUpdateListenerFn = bool(__cdecl*)(ListenerHandle);

struct InterfaceV1 {
    std::uint32_t version{ kVersion };
    std::uint32_t size{ sizeof(InterfaceV1) };
    RegisterAnimationUpdateListenerFn RegisterAnimationUpdateListener{ nullptr };
    UnregisterAnimationUpdateListenerFn UnregisterAnimationUpdateListener{ nullptr };
};

using GetAPIFn = const InterfaceV1*(__cdecl*)();
}
