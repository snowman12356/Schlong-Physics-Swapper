#include "SosPoseAngle.h"

#include "controllers/PoseAngle.h"

#include <SKSE/SKSE.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <unordered_map>

namespace SPS::Runtime::SosPose {
namespace {

constexpr float kDegreesToRadians = 0.017453292F;
// Sentinel for "use the INI value", matching SPS_PoseAngle.SetActorPoseAngle.
constexpr float kUseConfiguredDegrees = -1.0F;

struct BoneState {
    RE::NiMatrix3 baseRotation;  // captured before SPS wrote anything
    RE::NiPoint3 baseTranslate;  // captured to prove SPS never moved the bone
    bool captured{ false };
    bool posed{ false };
    int bend{ 0 };
    float degrees{ 0.0F };
};

// Keyed by form so an unload can drop the entry, and holding the node so a
// rebuilt skeleton (different address) is treated as a fresh capture.
struct Entry {
    RE::NiAVObject* bone{ nullptr };
    BoneState state;
};

// Reached from the game thread (position dispatch, ramp tasks, managed NPCs)
// and, through the SPS_PoseAngle natives, from Papyrus threads. The map is
// guarded by this lock; bone writes themselves only happen on the game thread
// (the natives queue them as SKSE tasks).
std::mutex entriesLock;
std::unordered_map<std::uint32_t, Entry> entries;

// Degrees at full bend. Set from settings so a Papyrus caller can pass -1.
std::atomic<float> configuredDegreesAtMaxBend{ kDegreesAtMaxBend };

RE::NiAVObject* FindBone(RE::Actor* actor)
{
    if (!actor) {
        return nullptr;
    }
    auto* root = actor->Get3D();
    if (!root) {
        return nullptr;
    }
    return root->GetObjectByName(RE::BSFixedString(kRootBoneName));
}

float ResolveConfiguredDegrees(float requested)
{
    if (requested == kUseConfiguredDegrees) {
        return configuredDegreesAtMaxBend.load();
    }
    return Core::PoseAngle::ValidDegrees(requested) ? requested : configuredDegreesAtMaxBend.load();
}

// Caller holds entriesLock.
bool ApplyLocked(RE::Actor* actor, int bend, float degreesAtMaxBend);

}  // namespace

Tilt ResolveTilt(int bend, float degreesAtMaxBend)
{
    Tilt tilt{};
    // The clamp/scale/NaN handling lives in Core::PoseAngle so it is covered by
    // tests that do not need a running game.
    tilt.degrees = Core::PoseAngle::Degrees(bend, degreesAtMaxBend);
    // Pitch axis of the humanoid body frame. Applied in the bone's own frame, so
    // this is the axis the assembly hangs from rather than a world direction.
    tilt.axis = RE::NiPoint3{ 1.0F, 0.0F, 0.0F };
    return tilt;
}

bool Apply(RE::Actor* actor, int bend, float degreesAtMaxBend)
{
    std::scoped_lock lock(entriesLock);
    return ApplyLocked(actor, bend, degreesAtMaxBend);
}

namespace {

bool ApplyLocked(RE::Actor* actor, int bend, float degreesAtMaxBend)
{
    auto* bone = FindBone(actor);
    if (!bone) {
        return false;
    }
    // Looked up before insertion so a stale bone address can be detected and
    // re-captured rather than restored into the wrong object.
    auto& entry = entries[actor->GetFormID()];
    if (entry.bone != bone) {
        entry.bone = bone;
        entry.state = BoneState{};
    }
    auto& state = entry.state;
    if (!state.captured) {
        // Captured once, before any SPS write. If another mod had already posed
        // the assembly this is that settled pose, which is the intended
        // reference: our offset is relative to it, not to the rest pose.
        state.baseRotation = bone->local.rotate;
        state.baseTranslate = bone->local.translate;
        state.captured = true;
    }

    bend = std::clamp(bend, 0, kMaxBend);
    const auto tilt = ResolveTilt(bend, degreesAtMaxBend);

    // Composed from the captured base every time, so the write is a pure
    // function of bend. Repeating it cannot accumulate the way a queued graph
    // event can, which is what allows this mode to run from a polling tick.
    RE::NiMatrix3 offset;
    offset.MakeRotation(tilt.degrees * kDegreesToRadians,
        tilt.axis.x, tilt.axis.y, tilt.axis.z);

    RE::NiTransform next = bone->local;
    next.rotate = offset * state.baseRotation;
    // Translation and scale are left exactly as the skeleton set them. GenBase
    // owns the position of the assembly, so the angle must not move it.
    next.translate = state.baseTranslate;
    bone->local = next;

    state.posed = true;
    state.bend = bend;
    state.degrees = tilt.degrees;
    return true;
}

}  // namespace

bool Clear(RE::Actor* actor)
{
    if (!actor) {
        return false;
    }
    std::scoped_lock lock(entriesLock);
    const auto found = entries.find(actor->GetFormID());
    if (found == entries.end()) {
        return false;
    }
    auto& entry = found->second;
    auto* bone = FindBone(actor);
    if (bone && bone == entry.bone && entry.state.posed) {
        RE::NiTransform next = bone->local;
        next.rotate = entry.state.baseRotation;
        next.translate = entry.state.baseTranslate;
        bone->local = next;
    }
    entry.state.posed = false;
    entry.state.bend = 0;
    entry.state.degrees = 0.0F;
    return true;
}

bool Recapture(RE::Actor* actor)
{
    if (!actor) {
        return false;
    }
    std::scoped_lock lock(entriesLock);
    const auto found = entries.find(actor->GetFormID());
    if (found != entries.end()) {
        found->second.state.captured = false;
        found->second.state.posed = false;
    }
    return ApplyLocked(actor, 0, kDegreesAtMaxBend);
}

void Forget(std::uint32_t formID)
{
    std::scoped_lock lock(entriesLock);
    entries.erase(formID);
}

void ForgetAll()
{
    std::scoped_lock lock(entriesLock);
    entries.clear();
}

PoseState ReadState(RE::Actor* actor)
{
    PoseState result{};
    auto* bone = FindBone(actor);
    if (!bone) {
        return result;
    }
    result.boneFound = true;
    if (!actor) {
        return result;
    }
    std::scoped_lock lock(entriesLock);
    const auto found = entries.find(actor->GetFormID());
    if (found == entries.end() || found->second.bone != bone) {
        return result;
    }
    result.captured = found->second.state.captured;
    result.posed = found->second.state.posed;
    result.bend = found->second.state.bend;
    result.degrees = found->second.state.degrees;
    return result;
}

void LogBoneFrame(RE::Actor* actor)
{
    auto* bone = FindBone(actor);
    if (!bone) {
        SKSE::log::info("SPS pose angle: '{}' not present on this skeleton", kRootBoneName);
        return;
    }
    const auto& m = bone->local.rotate;
    SKSE::log::info(
        "SPS pose angle: '{}' X=({:.3f},{:.3f},{:.3f}) Y=({:.3f},{:.3f},{:.3f}) Z=({:.3f},{:.3f},{:.3f}) T=({:.2f},{:.2f},{:.2f})",
        kRootBoneName,
        m.entry[0][0], m.entry[0][1], m.entry[0][2],
        m.entry[1][0], m.entry[1][1], m.entry[1][2],
        m.entry[2][0], m.entry[2][1], m.entry[2][2],
        bone->local.translate.x, bone->local.translate.y, bone->local.translate.z);
}

void SetConfiguredDegrees(float degrees)
{
    configuredDegreesAtMaxBend = Core::PoseAngle::ValidDegrees(degrees)
        ? std::abs(degrees)
        : Core::PoseAngle::kDefaultDegreesAtMaxBend;
}

float ConfiguredDegrees()
{
    return configuredDegreesAtMaxBend.load();
}

namespace {

// Papyrus natives run on script threads. Bone transforms are only written on
// the game thread, so these check the bone now (the documented false-when-
// missing result) and queue the write. The actor is re-resolved from its handle
// inside the task, so an actor that unloaded in between is simply skipped.
template <class Work>
bool QueueOnGameThread(RE::Actor* actor, Work work)
{
    if (!actor || !FindBone(actor)) {
        return false;
    }
    auto* tasks = SKSE::GetTaskInterface();
    if (!tasks) {
        return false;
    }
    const auto handle = actor->GetHandle();
    tasks->AddTask([handle, work] {
        if (const auto live = handle.get()) {
            work(live.get());
        }
    });
    return true;
}

bool NativeSetActorPoseAngle(
    RE::StaticFunctionTag*, RE::Actor* actor, std::int32_t bend, float degreesAtMaxBend)
{
    const float degrees = ResolveConfiguredDegrees(degreesAtMaxBend);
    return QueueOnGameThread(actor, [bend, degrees](RE::Actor* live) {
        Apply(live, bend, degrees);
    });
}

bool NativeClearActorPoseAngle(RE::StaticFunctionTag*, RE::Actor* actor)
{
    return QueueOnGameThread(actor, [](RE::Actor* live) { Clear(live); });
}

bool NativeRecaptureActorPoseAngle(RE::StaticFunctionTag*, RE::Actor* actor)
{
    return QueueOnGameThread(actor, [](RE::Actor* live) { Recapture(live); });
}

// 0 = base bone missing, 1 = bone present but no SPS pose applied, 2 = posed.
// A single int avoids depending on Papyrus array marshalling for a diagnostic.
std::int32_t NativeReadActorPoseAngle(RE::StaticFunctionTag*, RE::Actor* actor)
{
    const auto state = ReadState(actor);
    if (!state.boneFound) {
        return 0;
    }
    return state.posed ? 2 : 1;
}

void NativeLogActorPoseFrame(RE::StaticFunctionTag*, RE::Actor* actor)
{
    LogBoneFrame(actor);
}

}  // namespace

bool RegisterPoseAngleNatives(RE::BSScript::IVirtualMachine* vm)
{
    if (!vm) {
        return false;
    }
    vm->RegisterFunction("SetActorPoseAngle", "SPS_PoseAngle", NativeSetActorPoseAngle);
    vm->RegisterFunction("ClearActorPoseAngle", "SPS_PoseAngle", NativeClearActorPoseAngle);
    vm->RegisterFunction("RecaptureActorPoseAngle", "SPS_PoseAngle", NativeRecaptureActorPoseAngle);
    vm->RegisterFunction("ReadActorPoseAngle", "SPS_PoseAngle", NativeReadActorPoseAngle);
    vm->RegisterFunction("LogActorPoseFrame", "SPS_PoseAngle", NativeLogActorPoseFrame);
    return true;
}

}
