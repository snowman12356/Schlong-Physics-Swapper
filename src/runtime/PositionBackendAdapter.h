#pragma once

#include <RE/Skyrim.h>

#include <cstdint>

namespace SPS::Runtime {

struct PositionDispatchResult {
    bool accepted{ false };
    bool nativeAccepted{ false };
    int method{ -1 };  // -1 no backend required, 0 pose, 1 event, 2 both
};

// bendMethod codes, matching the INI values:
//   0 = SOS AE direct control (legacy; the native call was removed from
//       Schlongs of Skyrim AE 4.0.0, so this now falls back to graph events)
//   1 = SOS / TNG animation stages
//   2 = choose automatically
//   3 = SPS pose angle (experimental). Writes GenBase directly. The SOS/TNG
//       animation graph and CBPC rewrite that bone every frame, so this does not
//       produce a visible angle in a live game; kept for diagnosis only.
inline constexpr int kBendMethodNative = 0;
inline constexpr int kBendMethodEvents = 1;
inline constexpr int kBendMethodAutomatic = 2;
inline constexpr int kBendMethodPose = 3;

int AnimationEventBend(int bend, bool tngBackend);

// Graph event for a 0-20 bend. The lowest pose is SOSBend-9. SOSFlaccid is a
// separate clip and is not part of this curve; ApplyPosition sends it only
// when the caller passes the flaccid flag for an SMP-owned rest.
const char* SosBendEventName(int bend, bool tngBackend);

// Blocks only the Papyrus fallback of SendPositionEvent until untilMs (execution
// clock). Thread-safe; later calls can only extend the hold.
void HoldPositionPapyrusFallback(std::int64_t untilMs);

bool SendPositionEvent(
    RE::Actor* actor,
    const RE::BSFixedString& eventName,
    bool tngBackend,
    std::int64_t allowedAfterMs,
    std::int64_t nowMs);
PositionDispatchResult ApplyPosition(
    RE::Actor* actor,
    int bend,
    bool flaccid,
    bool animate,
    int bendMethod,
    bool animateChanges,
    std::int64_t allowedAfterMs,
    std::int64_t nowMs);

}
