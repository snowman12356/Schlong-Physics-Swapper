#include "PhysicsOwnershipAdapter.h"

#include "PapyrusGateway.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace SPS::Runtime {

namespace {

constexpr auto kBridgePath = "Data/Scripts/SPS_FSMPBridge.pex";

bool IsPlayer(RE::Actor* actor)
{
    const auto* player = RE::PlayerCharacter::GetSingleton();
    return actor && player && actor == static_cast<const RE::Actor*>(player);
}

}

bool OrderedFsmpBridgeAvailable()
{
    // A DLL/PEX mismatch can otherwise dispatch a missing static function.
    // Cache the result because the installed bridge cannot change safely while
    // Skyrim is running.
    static const bool available = [] {
        std::ifstream stream(kBridgePath, std::ios::binary);
        if (!stream) {
            return false;
        }
        const std::string bytes(std::istreambuf_iterator<char>(stream), {});
        return bytes.find("SetPlayerOwner") != std::string::npos &&
            bytes.find("ReleasePlayerPhysics") != std::string::npos;
    }();
    return available;
}

bool ResetPlayerPhysics(
    RE::Actor* actor,
    bool full,
    bool fsmpActorApiAvailable,
    std::int64_t allowedAfterMs,
    std::int64_t nowMs)
{
    return IsPlayer(actor) && fsmpActorApiAvailable &&
        std::filesystem::exists(kBridgePath) &&
        DispatchStatic("SPS_FSMPBridge", "ResetPlayerPhysics", allowedAfterMs, nowMs, full);
}

bool SetPlayerPhysicsOwner(
    RE::Actor* actor,
    bool useCBPC,
    bool fsmpActorApiAvailable,
    std::int64_t allowedAfterMs,
    std::int64_t nowMs)
{
    return IsPlayer(actor) && fsmpActorApiAvailable && OrderedFsmpBridgeAvailable() &&
        DispatchStatic("SPS_FSMPBridge", "SetPlayerOwner", allowedAfterMs, nowMs, useCBPC);
}

bool ReleasePlayerPhysics(
    RE::Actor* actor,
    bool fsmpActorApiAvailable,
    std::int64_t allowedAfterMs,
    std::int64_t nowMs)
{
    return IsPlayer(actor) && fsmpActorApiAvailable && OrderedFsmpBridgeAvailable() &&
        DispatchStatic("SPS_FSMPBridge", "ReleasePlayerPhysics", allowedAfterMs, nowMs);
}

}
