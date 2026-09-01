#include "PhysicsOwnershipAdapter.h"

#include "PapyrusGateway.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

namespace SPS::Runtime {

namespace {

constexpr auto kBridgePath = "Data/Scripts/SPS_FSMPBridge.pex";

bool IsPlayer(RE::Actor* actor)
{
    const auto* player = RE::PlayerCharacter::GetSingleton();
    return actor && player && actor == static_cast<const RE::Actor*>(player);
}

class OwnerCompletionCallback final : public RE::BSScript::IStackCallbackFunctor {
public:
    explicit OwnerCompletionCallback(OwnershipCompletion completion) :
        completion_(std::move(completion)) {}

    void operator()(RE::BSScript::Variable result) override
    {
        if (completion_) {
            completion_(result.IsBool() && result.GetBool());
        }
    }

    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

private:
    OwnershipCompletion completion_;
};

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
        return bytes.find("GetSPSBridgeVersion") != std::string::npos &&
            bytes.find("SetPlayerOwnerV2") != std::string::npos &&
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

OwnershipDispatchResult SetPlayerPhysicsOwner(
    RE::Actor* actor,
    bool useCBPC,
    bool fsmpActorApiAvailable,
    std::int64_t allowedAfterMs,
    std::int64_t nowMs,
    OwnershipCompletion completion)
{
    if (!IsPlayer(actor) || !fsmpActorApiAvailable ||
        !OrderedFsmpBridgeAvailable() || !completion) {
        return OwnershipDispatchResult::failed;
    }
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{
        new OwnerCompletionCallback(std::move(completion))
    };
    return DispatchStaticWithCallback(
        "SPS_FSMPBridge", "SetPlayerOwnerV2", allowedAfterMs, nowMs,
        std::move(callback), useCBPC) ?
        OwnershipDispatchResult::queued : OwnershipDispatchResult::failed;
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
