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
    for (std::int64_t time = 12000; time <= 22000; time += 2000) {
        policy.ObserveScene(ActorSceneStatus::clear, time);
        if (time < 21500) expect(!decide(time), "unchanged command stays suppressed inside refresh period");
    }
    expect(decide(22000) == "SOSBend1", "occasional refresh is allowed after the bounded repeat interval");

    BasicNpcPolicy other;
    expect(!other.Decide(0, 22000, 22000, true, false, true), "one actor's clear scene cannot authorize another actor");
    return failures;
}
