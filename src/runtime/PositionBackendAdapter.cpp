#include "PositionBackendAdapter.h"

#include "Compatibility.h"
#include "PapyrusGateway.h"
#include "SosPoseAngle.h"
#include "controllers/PoseTargets.h"

#include <atomic>
#include <filesystem>

namespace SPS::Runtime {
namespace {

// Schlongs of Skyrim AE 4.0.0 removed SOSAE_SKSE.SetSchlongBend. The script that
// ships with SPS still calls it, so a native bend request can no longer be
// dispatched and must not be attempted: a failed Papyrus stack would be reported
// as a failed angle instead of an unsupported one.
//
// The graph events and AnimObjects that the same update uses for its own keys
// are unaffected, which is what the event fallback below relies on.
constexpr bool kSosAeNativeBendRemoved = true;

// The Papyrus fallback below is held back for a while after a load or a player
// mesh rebuild. A crash report (2.3.0, SendPlayerAnimationEvent from this
// fallback a few seconds after loading) shows the engine dispatch is not safe
// while the player is being rebuilt. The native route is not affected.
std::atomic<std::int64_t> fallbackHoldUntilMs{ 0 };

bool HasPositionBridge()
{
    return PositionBridgeScriptPresent();
}

// TNG and legacy SOS expose bend purely as graph events. SOS AE 4.0.0 still
// binds SOSBend0-9 in its behaviour graph, so the event contract remains valid
// for it even though the native call is gone.
bool EventChannelAvailable(bool tngBackend)
{
    if (tngBackend) {
        return HasPositionBridge();
    }
    return TngLoaded() || LegacySosLoaded() || SosAeNativeLoaded();
}

}

void HoldPositionPapyrusFallback(std::int64_t untilMs)
{
    auto current = fallbackHoldUntilMs.load();
    while (untilMs > current && !fallbackHoldUntilMs.compare_exchange_weak(current, untilMs)) {}
}

int AnimationEventBend(int bend, bool tngBackend)
{
    // SOSBend-9 .. SOSBend9. Bend 0 clamps to SOSBend-9, the lowest angle.
    // SOSFlaccid is not this index: playing it on a rise snaps through the
    // middle of the blend and then falls before the climb.
    return SPS::Core::PoseTargets::EventIndex(bend, tngBackend);
}

const char* SosBendEventName(int bend, bool tngBackend)
{
    return SPS::Core::PoseTargets::EventName(bend, tngBackend);
}

bool SendPositionEvent(
    RE::Actor* actor,
    const RE::BSFixedString& eventName,
    bool tngBackend,
    std::int64_t allowedAfterMs,
    std::int64_t nowMs)
{
    if (!actor || actor != RE::PlayerCharacter::GetSingleton()) {
        return false;
    }
    if (tngBackend) {
        if (!PapyrusReady(allowedAfterMs, nowMs) || !HasPositionBridge()) {
            return false;
        }
        return DispatchStatic(
            "SPS_PositionBridge", "SendPlayerAnimationEvent",
            allowedAfterMs, nowMs, eventName);
    }
    // Native graph dispatch for SOS: no Papyrus round trip, so the angle can be
    // re-asserted from a polling tick without consuming the dispatch budget.
    //
    // NotifyAnimationGraph is the primary route because it is free. It is not
    // guaranteed to accept an event on every SOS AE build, though, so the proven
    // Papyrus route is used when it refuses. Without this fallback a refused
    // native call was indistinguishable from "no backend at all", and the angle
    // silently never moved while the physics swap kept working.
    if (actor->NotifyAnimationGraph(eventName)) {
        return true;
    }
    if (nowMs < fallbackHoldUntilMs.load()) {
        return false;
    }
    if (!PapyrusReady(allowedAfterMs, nowMs) || !HasPositionBridge()) {
        return false;
    }
    return DispatchStatic(
        "SPS_PositionBridge", "SendPlayerAnimationEvent",
        allowedAfterMs, nowMs, eventName);
}

PositionDispatchResult ApplyPosition(
    RE::Actor* actor,
    int bend,
    bool flaccid,
    bool animate,
    int bendMethod,
    bool animateChanges,
    std::int64_t allowedAfterMs,
    std::int64_t nowMs)
{
    // animate/animateChanges smooth graph transitions between stages. The pose
    // angle is continuous and written per tick, so it needs no staging.
    (void)animate;
    (void)animateChanges;
    const bool tngBackend = TngLoaded();
    const bool eventChannel = EventChannelAvailable(tngBackend);

    // The pose angle is opt-in only. It used to be the automatic choice whenever
    // SOS AE was present, but every SOS and TNG bend clip binds GenBase, so the
    // animation graph rewrites that bone every frame and a direct write from a
    // task is overwritten before it is ever drawn. That is why the automatic
    // angle visibly did nothing.
    //
    // The channel that does move the assembly is the graph event, because CBPC
    // owns Gen01-Gen06 and follows the event target. That is the default for
    // every backend, including SOS AE 4.0.0, whose graph still binds SOSBend0-9
    // even though its native bend call was removed.
    const bool poseWanted = bendMethod == kBendMethodPose;
    const bool posed = poseWanted && actor &&
        SosPose::Apply(actor, bend, SosPose::ConfiguredDegrees());
    if (posed) {
        return { true, true, 0 };
    }
    // Reached when the pose was wanted but could not be applied: no actor, no 3D
    // yet, or the base bone is absent because SOS has not attached. The caller
    // is never told a pose was applied; fall through to the graph instead.

    if (!eventChannel) {
        return poseWanted ? PositionDispatchResult{ false, false, -1 }
                          : PositionDispatchResult{ true, false, -1 };
    }

    // The soft pose is sent as a bend stage too. SOSFlaccid is a separate clip,
    // not the bottom of the curve: under CBPC (or kinematic bones) it shows a
    // raised pose, which is the snap at the start of a rise. Bend 0 resolves to
    // SOSBend-9, the real lowest position; a configured FlaccidBend resolves to
    // its own stage.
    //
    // Exception: the final resting pose. Once SMP owns the bones (flaccid is only
    // set by the soft-state path, after the handoff) SOSBend-9 still leaves the
    // shaft tilted out of the hanging pose, so an unconfigured soft pose sends
    // SOSFlaccid, the clip OSL itself sends for a soft actor. Never during a rise
    // or fall, and not for TNG, whose graph is not verified to bind it.
    const bool restingPose = flaccid && bend == 0 && !tngBackend;
    const bool accepted = SendPositionEvent(
        actor,
        RE::BSFixedString(restingPose ? "SOSFlaccid" : SosBendEventName(bend, tngBackend)),
        tngBackend, allowedAfterMs, nowMs);
    return { accepted, false, 1 };
}

}
