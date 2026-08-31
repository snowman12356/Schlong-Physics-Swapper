#pragma once

#include <RE/Skyrim.h>

#include <cstdint>

namespace SPS::Runtime {

struct PositionDispatchResult {
    bool accepted{ false };
    bool nativeAccepted{ false };
    int method{ -1 };  // -1 no backend required, 0 native, 1 event, 2 both
};

int AnimationEventBend(int bend, bool tngBackend);
bool SendPositionEvent(
    RE::Actor* actor,
    const RE::BSFixedString& eventName,
    bool tngBackend,
    std::int64_t allowedAfterMs,
    std::int64_t nowMs);
bool SetNativeBend(
    RE::Actor* actor,
    int bend,
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
