#pragma once

#include <RE/Skyrim.h>

#include <cstdint>

namespace SPS::Runtime {

// SPS-owned schlong angle.
//
// Schlongs of Skyrim AE 4.0.0 removed SOSAE_SKSE.SetSchlongBend, so the native
// angle channel SPS used to rely on no longer exists. This module writes
// 'NPC GenitalsBase [GenBase]' directly as a replacement.
//
// VERIFIED IN GAME 2026-11-01: the write does not produce a visible angle. The
// premise below was wrong. GenBase is kinematic with respect to Faster HDT-SMP,
// but it is still animated: every SOS and TNG bend clip (SOSFlaccid, SOSErect,
// SOSBendUp/Down, SOSFast/SlowErect) binds GenBase, so the behaviour graph
// rewrites the bone every frame and a task-side write is overwritten before it
// is ever drawn. CBPC separately owns Gen01-Gen06.
//
// The channel that actually moves the assembly is the graph event (SOSBend0-9),
// because the clips set GenBase and CBPC follows the resulting target. Method 3
// is therefore opt-in and diagnostic only; automatic mode no longer selects it.
//
// Only GenBase is written. Gen01-Gen06 are never touched: CBPC drives them
// directly and FSMP absorbs direct writes to dynamic bones.
namespace SosPose {

inline constexpr const char* kRootBoneName = "NPC GenitalsBase [GenBase]";
inline constexpr int kMaxBend = 20;
// Degrees of tilt at bend 20, applied as an offset on top of the captured base
// pose rather than as an absolute orientation, so the skeleton's own rest pose
// and the SOS clip contribution are both preserved.
inline constexpr float kDegreesAtMaxBend = 45.0F;

struct Tilt {
    float degrees{ 0.0F };
    RE::NiPoint3 axis{ 1.0F, 0.0F, 0.0F };
};

Tilt ResolveTilt(int bend, float degreesAtMaxBend = kDegreesAtMaxBend);

struct PoseState {
    bool boneFound{ false };
    bool posed{ false };
    bool captured{ false };
    int bend{ 0 };
    float degrees{ 0.0F };
};

// Applies the tilt to GenBase. The base orientation is captured the first time
// the bone is seen and reused afterwards, which makes this idempotent: the
// result depends on bend alone and repeated calls cannot accumulate. Returns
// false when the actor has no 3D or the bone is absent, so a missing skeleton is
// never reported as an applied pose.
bool Apply(RE::Actor* actor, int bend, float degreesAtMaxBend = kDegreesAtMaxBend);

// Restores the captured base orientation. Safe to call when no pose is recorded.
bool Clear(RE::Actor* actor);

// Drops the captured base so the next Apply re-reads the current pose. Needed
// after a deliberate clip change or while tuning, never during normal ticks.
bool Recapture(RE::Actor* actor);

// Releases bookkeeping for an actor whose 3D is gone. Must be called on unload
// so a freed node address cannot be mistaken for a live bone.
void Forget(std::uint32_t formID);

void ForgetAll();

PoseState ReadState(RE::Actor* actor);

// Diagnostic: logs the resolved bone frame once per skeleton so the pitch axis
// and sign can be confirmed in game before the mode is trusted.
void LogBoneFrame(RE::Actor* actor);

// Sets the degrees-at-max-bend used when a Papyrus caller passes -1, so the
// native path follows Position/PoseAngleDegrees instead of a compiled constant.
void SetConfiguredDegrees(float degrees);

// The current degrees-at-max-bend, for native call sites outside this module.
float ConfiguredDegrees();

// Registers the SPS_PoseAngle natives.
bool RegisterPoseAngleNatives(RE::BSScript::IVirtualMachine* vm);

}

}
