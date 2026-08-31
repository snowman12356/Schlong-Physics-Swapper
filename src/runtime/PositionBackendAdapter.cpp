#include "PositionBackendAdapter.h"

#include "Compatibility.h"
#include "PapyrusGateway.h"

#include <fmt/format.h>

#include <algorithm>
#include <cmath>

namespace SPS::Runtime {

int AnimationEventBend(int bend, bool tngBackend)
{
    bend = std::clamp(bend, 0, 20);
    if (tngBackend) {
        return std::clamp(
            static_cast<int>(std::lround(bend * 18.0 / 20.0)) - 9, -9, 9);
    }
    return std::clamp(static_cast<int>(std::lround(bend * 9.0 / 20.0)), 0, 9);
}

bool SendPositionEvent(
    RE::Actor* actor,
    const RE::BSFixedString& eventName,
    bool tngBackend,
    std::int64_t allowedAfterMs,
    std::int64_t nowMs)
{
    if (!actor) {
        return false;
    }
    if (tngBackend) {
        return DispatchStatic(
            "Debug", "SendAnimationEvent", allowedAfterMs, nowMs,
            actor, eventName);
    }
    return actor->NotifyAnimationGraph(eventName);
}

bool SetNativeBend(
    RE::Actor* actor,
    int bend,
    std::int64_t allowedAfterMs,
    std::int64_t nowMs)
{
    return actor && SosAeNativeLoaded() &&
        DispatchStatic(
            "SOSAE_SKSE", "SetSchlongBend", allowedAfterMs, nowMs,
            actor, std::clamp(bend, 0, 20));
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
    const bool tngBackend = TngLoaded();
    const bool nativeBackend = SosAeNativeLoaded();
    const bool eventBackend = tngBackend || LegacySosLoaded() || nativeBackend;
    const bool useGraph = eventBackend && (!nativeBackend ||
        (bendMethod != 0 && animate && animateChanges));
    const bool useNative = nativeBackend &&
        (bendMethod != 1 || (bendMethod == 1 && !animateChanges));

    if (!useGraph && !useNative) {
        return { true, false, -1 };
    }

    const int eventBend = AnimationEventBend(bend, tngBackend);
    const bool graphAccepted = useGraph && SendPositionEvent(
        actor,
        flaccid ? RE::BSFixedString("SOSFlaccid") :
                  RE::BSFixedString(fmt::format("SOSBend{}", eventBend)),
        tngBackend, allowedAfterMs, nowMs);
    const bool nativeAccepted = useNative &&
        SetNativeBend(actor, bend, allowedAfterMs, nowMs);
    const int method = useGraph && useNative ? 2 : (useGraph ? 1 : 0);
    return { graphAccepted || nativeAccepted, nativeAccepted, method };
}

}
