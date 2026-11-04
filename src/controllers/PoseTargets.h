#pragma once

#include <algorithm>
#include <cmath>

namespace SPS::Core {

// Rules for how the SPS angle is handed to a bend backend.
//
// Pure and game-independent so the two defects that made the angle fail can be
// regression-tested without a running game:
//
//   1. ORDERING. The angle was written the moment the handoff committed, which
//      is before CBPC has bound Gen01-Gen06. See SwitchSettleMs below.
//
//   2. SMOOTHNESS. The angle was re-sent on every tick. Re-sent absolute stops
//      restart SOS's own transition each time instead of letting it play, which
//      is what made the motion stutter. SOS itself (SOS_PlayerAliasScript
//      OnKeyDown) sends ONE "SOSBend<pos>" event per keypress and lets the
//      SOSBend* clip blend; the clip is where the smoothness lives. See
//      ShouldDispatchStage and StageEventName.
struct PoseTargets {
    // CBPC binds Gen01-Gen06 on its own update, some frames after
    // CBPCPluginScript.StartPhysics returns. SPS_FSMPBridge.SetPlayerOwnerV3
    // returns as soon as it has issued those calls, so the committed owner is
    // not the same thing as CBPC being attached. A bend written inside that gap
    // lands on nodes nothing is simulating, and the assembly falls back to its
    // flaccid pose.
    static constexpr std::int64_t kCbpcBindSettleMs = 400;

    // Upper bound, so a slow or stalled CBPC cannot park the angle forever.
    static constexpr std::int64_t kCbpcBindSettleMaxMs = 2000;

    // Delay before the first angle write after a switch to CBPC. The configured
    // settle delay still applies but can no longer undercut the attach wait.
    static std::int64_t SwitchSettleMs(int configuredSettleMs)
    {
        const std::int64_t configured =
            std::clamp<std::int64_t>(configuredSettleMs, 0, kCbpcBindSettleMaxMs);
        return std::max(configured, kCbpcBindSettleMs);
    }

    // True when the angle may be written for this frame.
    //
    // The bend needs a confirmed CBPC owner: the angle is where CBPC's own
    // simulation starts from, so writing it while SMP still owns the assembly
    // either fights FSMP or is simply discarded. A pending handoff is excluded
    // because it has not settled the owner yet.
    static bool CanWrite(bool angleControl, bool known, bool usingCBPC, bool handoffPending)
    {
        return angleControl && known && usingCBPC && !handoffPending;
    }

    // A stage event is dispatched only when the visible stage changes.
    //
    // This is the smoothness rule. The blend to the new target is the clip's
    // job; repeating the same event restarts that blend from wherever it had
    // reached, so a stage that is re-sent every tick never finishes. Returning
    // false for an unchanged stage is what lets one event play out.
    static bool ShouldDispatchStage(int stage, int lastDispatchedStage, bool force)
    {
        return force || stage != lastDispatchedStage;
    }

    // Signed graph index for a 0-20 bend. SOS's PlayBend sends level-10, and the
    // behaviour graph binds SOSBend-9 .. SOSBend9. SOSBend-9 is the lowest
    // angle (flaccid). Bend 0 would otherwise be the SOSFlaccid clip, which is
    // not a lower pose: that clip enters through the middle of the blend.
    // CBPC stays the owner until this index is reached, then SMP takes over.
    static int EventIndex(int bend, bool tngBackend)
    {
        bend = std::clamp(bend, 0, 20);
        if (tngBackend) {
            return std::clamp(
                static_cast<int>(std::lround(bend * 18.0 / 20.0)) - 9, -9, 9);
        }
        return std::clamp(bend - 10, -9, 9);
    }

    // True for SOSBend-9, the bottom of the curve. A descent releases CBPC here.
    static bool IsCbpcReleaseIndex(int eventIndex)
    {
        return eventIndex <= -9;
    }

    // Event for a signed stage. The bottom stage is SOSBend-9, not SOSFlaccid.
    static const char* StageEventName(int stage)
    {
        static constexpr const char* kNames[] = {
            "SOSBend-9", "SOSBend-8", "SOSBend-7", "SOSBend-6", "SOSBend-5",
            "SOSBend-4", "SOSBend-3", "SOSBend-2", "SOSBend-1",
            "SOSBend0", "SOSBend1", "SOSBend2", "SOSBend3", "SOSBend4",
            "SOSBend5", "SOSBend6", "SOSBend7", "SOSBend8", "SOSBend9"
        };
        return kNames[std::clamp(stage, -9, 9) + 9];
    }

    static const char* EventName(int bend, bool tngBackend)
    {
        return StageEventName(EventIndex(bend, tngBackend));
    }

    // Bend in the plugin's 0-20 units rounded to the visible SOS stage.
    static int VisibleStage(int bend)
    {
        return EventIndex(bend, false);
    }

    // A 0-20 bend that resolves to the given graph index. EventIndex of the
    // result is always the requested index, for both backends.
    static int BendForIndex(int eventIndex, bool tngBackend)
    {
        eventIndex = std::clamp(eventIndex, -9, 9);
        if (tngBackend) {
            return std::clamp(
                static_cast<int>(std::lround((eventIndex + 9) * 20.0 / 18.0)), 0, 20);
        }
        return std::clamp(eventIndex + 10, 1, 19);
    }

    // Even-step ramp for the graph-event backends. The visible stages between
    // the start and target stage are spread evenly over the duration: stage k
    // is shown from k/N to (k+1)/N, and the target stage lands exactly at t = 1.
    // So "Rise time" / "Fall time" is the real travel time from the first stage
    // to the last, rather than an eased 0-20 curve whose end stages only cover
    // part of it.
    static int StepRampBend(int startBend, int targetBend, float t, bool tngBackend)
    {
        startBend = std::clamp(startBend, 0, 20);
        targetBend = std::clamp(targetBend, 0, 20);
        const int from = EventIndex(startBend, tngBackend);
        const int to = EventIndex(targetBend, tngBackend);
        if (t >= 1.0F || from == to) {
            return targetBend;
        }
        if (t <= 0.0F) {
            return startBend;
        }
        const int steps = to > from ? to - from : from - to;
        const int k = std::min(steps, static_cast<int>(std::floor(t * static_cast<float>(steps))));
        if (k <= 0) {
            return startBend;
        }
        if (k >= steps) {
            return targetBend;
        }
        return BendForIndex(from + (to > from ? k : -k), tngBackend);
    }
};

}
