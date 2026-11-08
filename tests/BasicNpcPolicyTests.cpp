#include "controllers/BasicNpcPolicy.h"

#include <iostream>
#include <limits>
#include <string_view>

int RunBasicNpcPolicyTests()
{
    using namespace SPS::Controllers;
    int failures = 0;
    const auto expect = [&](bool condition, std::string_view message) {
        if (!condition) {
            ++failures;
            std::cerr << "FAIL: " << message << '\n';
        }
    };
    expect(BasicSosEvent(0) == "SOSFlaccid" && BasicSosEvent(19.99F) == "SOSFlaccid" &&
        BasicSosEvent(20) == "SOSBend-9" && BasicSosEvent(23.99F) == "SOSBend-9" &&
        BasicSosEvent(24) == "SOSBend-8" && BasicSosEvent(56) == "SOSBend0" &&
        BasicSosEvent(92) == "SOSBend9" && BasicSosEvent(100) == "SOSBend9" &&
        BasicSosEvent(1000) == "SOSBend9", "legacy OSL boundaries retain signed SOS events and capped output");
    expect(!BasicSosEvent(-1) && !BasicSosEvent(std::numeric_limits<float>::quiet_NaN()) &&
        !BasicSosEvent(std::numeric_limits<float>::infinity()), "unknown arousal never becomes a soft command");

    // Managed NPC: its own threshold and configured bend drive the pose, rather
    // than the ordinary-SOS fallback mapping above.
    ManagedPoseProfile managed;
    managed.threshold = 60.0F;
    managed.erectBend = 14;
    expect(ManagedSosEvent(0, managed, false) == "SOSBend-9" &&
        ManagedSosEvent(59.9F, managed, false) == "SOSBend-9",
        "managed NPC below threshold uses SOSBend-9, the lowest angle");
    expect(ManagedSosEvent(60, managed, false) == "SOSBend4" &&
        ManagedSosEvent(100, managed, false) == "SOSBend4",
        "managed NPC at or above threshold uses its configured erect bend");
    managed.threshold = 30.0F;
    expect(ManagedSosEvent(35, managed, false) == "SOSBend4",
        "managed threshold is independent of the player's arousal mapping");
    managed.erectBend = 0;
    expect(ManagedSosEvent(100, managed, false) == "SOSBend-9",
        "a zero erect bend keeps SOSBend-9 instead of inventing an angle");
    expect(!ManagedSosEvent(-1, managed, false) &&
        !ManagedSosEvent(std::numeric_limits<float>::quiet_NaN(), managed, false),
        "unknown managed arousal never becomes a pose command");

    // The pose channel shares the threshold rule but returns the bend itself, so
    // it must agree with the event path about when the NPC is erect.
    managed.erectBend = 14;
    managed.threshold = 60.0F;
    expect(ManagedPoseBend(0, managed) == 0 && ManagedPoseBend(59.9F, managed) == 0,
        "managed NPC below threshold holds the neutral pose reference");
    expect(ManagedPoseBend(60, managed) == 14 && ManagedPoseBend(100, managed) == 14,
        "managed NPC at or above threshold uses its configured erect bend");
    expect(ManagedPoseBend(-1, managed) == -1 &&
        ManagedPoseBend(std::numeric_limits<float>::quiet_NaN(), managed) == -1,
        "unknown managed arousal never becomes a pose angle");
    managed.erectBend = 0;
    expect(ManagedPoseBend(100, managed) == 0,
        "a zero erect bend holds neutral instead of inventing an angle");
    managed.erectBend = 40;
    expect(ManagedPoseBend(100, managed) == 20,
        "an out-of-range erect bend is clamped to the SOS range");
    // The two channels must not disagree about being erect.
    managed.erectBend = 14;
    managed.threshold = 45.0F;
    expect((ManagedPoseBend(44, managed) == 0) ==
            (ManagedSosEvent(44, managed, false) == "SOSBend-9") &&
        (ManagedPoseBend(46, managed) > 0) ==
            (ManagedSosEvent(46, managed, false) != "SOSBend-9"),
        "pose and event channels agree on the managed threshold");

    // Rise gradually with arousal: the angle follows the value between the start
    // arousal and the threshold, like the player, and both channels agree.
    ManagedPoseProfile rising;
    rising.threshold = 80.0F;
    rising.erectBend = 18;
    rising.arousalBased = true;
    rising.startArousal = 40.0F;
    expect(ManagedPoseBend(0, rising) == 0 && ManagedPoseBend(40, rising) == 0,
        "gradual managed NPC holds neutral at and below the start arousal");
    expect(ManagedPoseBend(60, rising) == 9,
        "gradual managed NPC is half way between start and threshold");
    expect(ManagedPoseBend(80, rising) == 18 && ManagedPoseBend(100, rising) == 18,
        "gradual managed NPC reaches the erect bend at the threshold");
    expect(ManagedPoseBend(61, rising) > ManagedPoseBend(60, rising) ||
            ManagedPoseBend(70, rising) > ManagedPoseBend(60, rising),
        "gradual managed NPC angle changes as arousal changes");
    expect(ManagedSosEvent(60, rising, false) == "SOSBend-1" &&
        ManagedSosEvent(80, rising, false) == "SOSBend8" &&
        ManagedSosEvent(20, rising, false) == "SOSBend-9",
        "gradual managed NPC event follows the same bend");
    expect(ManagedPoseBend(-1, rising) == -1,
        "unknown arousal still gives no gradual angle");
    rising.arousalBased = false;
    expect(ManagedPoseBend(60, rising) == 0 && ManagedPoseBend(80, rising) == 18,
        "gradual off keeps the soft-or-erect behaviour");

    BasicNpcPolicy policy;
    const auto decide = [&](std::int64_t now, float value = 60) {
        return policy.Decide(value, now, now, true, false, true);
    };
    expect(!decide(0), "unobserved scene state grants no permission");
    policy.ObserveScene(ActorSceneStatus::clear, 100);
    expect(!decide(1599) && decide(1600) == "SOSBend1", "first clear observation requires the full settling delay");
    expect(!policy.Decide(60, 1600, 1600, false, false, true) &&
        !policy.Decide(60, 1600, 1600, true, true, true) &&
        !policy.Decide(60, 1600, 1600, true, false, false),
        "disabled feature, managed enrollment and competing/unknown provider control suppress basic output");
    expect(!policy.Decide(60, 1700, 1600, true, false, true) &&
        !policy.Decide(60, -1, 1600, true, false, true) && !decide(99),
        "future or missing samples and clock reversal cannot authorize writes");
    policy.Complete("SOSBend1", BasicPositionResult::applied, 1600);
    expect(!decide(1601) && decide(1601, 0) == "SOSFlaccid", "successful unchanged output is coalesced but target changes can proceed");
    expect(!decide(2601), "stale clear scene status stops even a changed command");
    policy.ObserveScene(ActorSceneStatus::clear, 2000);
    expect(!policy.Decide(60, 0, 2600, true, false, true), "fresh scene data cannot make stale arousal fresh");
    policy.ObserveScene(ActorSceneStatus::active, 2200);
    expect(!decide(2300, 0), "active scenes suppress both soft and erect commands");
    policy.ObserveScene(ActorSceneStatus::clear, 2100);
    expect(!decide(2300), "late clear result cannot overwrite newer active scene evidence");
    policy.ObserveScene(ActorSceneStatus::unknown, 2400);
    expect(!decide(2400), "failed scene query remains unknown");
    policy.ObserveScene(ActorSceneStatus::clear, 3000);
    expect(!decide(4499) && decide(4500) == "SOSBend1", "scene exit waits again and reapplies even the same previous target");
    policy.Complete("SOSBend1", BasicPositionResult::deferred, 4500);
    expect(!decide(4501), "scene starting between query and write revokes cached clear permission");
    policy.ObserveScene(ActorSceneStatus::clear, 4600);
    expect(!decide(6099) && decide(6100), "late scene deferral needs a new full clear interval");
    policy.Complete("SOSBend1", BasicPositionResult::failed, 6100);
    expect(!decide(6101), "failed command does not establish an applied state");

    policy.Reset();
    expect(!decide(7000), "save/unload reset drops scene and output caches");
    policy.ObserveScene(ActorSceneStatus::clear, 7000);
    policy.ObserveScene(ActorSceneStatus::clear, 10000);
    expect(!decide(10000) && decide(11500), "gap in clear observations restarts settling");
    policy.Complete("SOSBend1", BasicPositionResult::applied, 11500);
    // OSL 2.9.3 never repeats an unchanged bucket; neither does SPS.
    bool repeated = false;
    for (std::int64_t time = 12000; time <= 120000; time += 2000) {
        policy.ObserveScene(ActorSceneStatus::clear, time);
        repeated = repeated || decide(time).has_value();
    }
    expect(!repeated, "an unchanged bucket is never re-sent while the scene stays clear");
    expect(decide(120000, 100) == "SOSBend9", "a changed bucket is sent straight away");
    policy.Complete("SOSBend9", BasicPositionResult::applied, 120000);
    policy.ObserveScene(ActorSceneStatus::active, 121000);
    policy.ObserveScene(ActorSceneStatus::clear, 122000);
    expect(!decide(123499, 100) && decide(123500, 100) == "SOSBend9",
        "after a scene the same bucket is re-asserted, as OSL clears its cache on scenes");

    BasicNpcPolicy other;
    expect(!other.Decide(0, 22000, 22000, true, false, true), "one actor's clear scene cannot authorize another actor");
    return failures;
}
