#include "controllers/PoseTargets.h"

#include <iostream>
#include <string_view>

int RunPoseTargetsTests()
{
    using namespace SPS::Core;
    int failures = 0;
    const auto expect = [&](bool condition, std::string_view message) {
        if (!condition) {
            ++failures;
            std::cerr << "FAIL: " << message << '\n';
        }
    };

    // The reported bug: the angle was written as soon as the handoff committed,
    // which is before CBPC has bound the bones, so the bend went to nodes
    // nothing was simulating. The configured delay must not undercut that.
    expect(PoseTargets::SwitchSettleMs(0) >= PoseTargets::kCbpcBindSettleMs,
        "a zero configured delay still waits for CBPC to bind");
    expect(PoseTargets::SwitchSettleMs(100) == PoseTargets::kCbpcBindSettleMs,
        "a short configured delay is raised to the attach wait");
    expect(PoseTargets::SwitchSettleMs(900) == 900,
        "a longer configured delay is left alone");
    expect(PoseTargets::SwitchSettleMs(-50) == PoseTargets::kCbpcBindSettleMs,
        "a negative configured delay cannot skip the attach wait");
    expect(PoseTargets::SwitchSettleMs(999999) == PoseTargets::kCbpcBindSettleMaxMs,
        "an absurd configured delay is capped so the angle cannot park forever");

    // Even-step ramp: Rise/Fall time is the exact travel time between stages.
    for (const bool tng : { false, true }) {
        for (int index = -9; index <= 9; ++index) {
            expect(PoseTargets::EventIndex(PoseTargets::BendForIndex(index, tng), tng) == index,
                "BendForIndex round-trips every graph index");
        }
        expect(PoseTargets::StepRampBend(0, 20, 0.0F, tng) == 0, "rise starts on the start bend");
        expect(PoseTargets::StepRampBend(0, 20, 1.0F, tng) == 20, "rise lands on the target at t = 1");
        expect(PoseTargets::StepRampBend(20, 0, 1.0F, tng) == 0, "fall lands on the target at t = 1");
        expect(PoseTargets::EventIndex(PoseTargets::StepRampBend(0, 20, 0.999F, tng), tng) == 8,
            "the top stage is not reached before the duration ends");
        expect(PoseTargets::EventIndex(PoseTargets::StepRampBend(20, 0, 0.999F, tng), tng) == -8,
            "the bottom stage (CBPC release) is not reached before the fall time ends");
        // 18 steps from -9 to 9: stage k is shown in the middle of slot k.
        bool evenRise = true;
        bool evenFall = true;
        for (int k = 0; k < 18; ++k) {
            const float mid = (static_cast<float>(k) + 0.5F) / 18.0F;
            evenRise = evenRise &&
                PoseTargets::EventIndex(PoseTargets::StepRampBend(0, 20, mid, tng), tng) == -9 + k;
            evenFall = evenFall &&
                PoseTargets::EventIndex(PoseTargets::StepRampBend(20, 0, mid, tng), tng) == 9 - k;
        }
        expect(evenRise, "every rise stage gets one equal time slot, in order");
        expect(evenFall, "every fall stage gets one equal time slot, in order");
        // A restarted rise from a mid stage still walks one stage at a time.
        int previous = PoseTargets::EventIndex(10, tng);
        bool monotonic = true;
        for (int i = 0; i <= 1000; ++i) {
            const int stage = PoseTargets::EventIndex(
                PoseTargets::StepRampBend(10, 20, static_cast<float>(i) / 1000.0F, tng), tng);
            monotonic = monotonic && stage >= previous && stage - previous <= 1;
            previous = stage;
        }
        expect(monotonic && previous == 9, "a partial rise climbs one stage at a time to the top");
    }

    expect(PoseTargets::CanWrite(true, true, true, false),
        "the angle is written when control is on and CBPC owns the assembly");
    expect(!PoseTargets::CanWrite(false, true, true, false),
        "no write when angle control is off");
    expect(!PoseTargets::CanWrite(true, false, true, false),
        "no write before the owner is known");
    expect(!PoseTargets::CanWrite(true, true, false, false),
        "no write while SMP owns the assembly");
    expect(!PoseTargets::CanWrite(true, true, true, true),
        "no write while a handoff is still in flight");

    // The smoothness rule: one dispatch per visible stage, so the SOSBend clip
    // is allowed to finish its blend instead of being restarted every tick.
    expect(PoseTargets::ShouldDispatchStage(5, 4, false),
        "a new visible stage is dispatched");
    expect(!PoseTargets::ShouldDispatchStage(5, 5, false),
        "an unchanged stage is NOT re-sent, so its blend can complete");
    expect(PoseTargets::ShouldDispatchStage(5, 5, true),
        "a forced write still goes out, for recovery paths");

    expect(PoseTargets::VisibleStage(0) == -9 && PoseTargets::VisibleStage(1) == -9 &&
        PoseTargets::VisibleStage(20) == 9 && PoseTargets::VisibleStage(10) == 0,
        "bend maps onto SOSBend-9 .. SOSBend9, with the bottom clamped to SOSBend-9");
    expect(PoseTargets::VisibleStage(-5) == -9 && PoseTargets::VisibleStage(99) == 9,
        "an out-of-range bend clamps into the stage range");
    expect(PoseTargets::VisibleStage(13) != PoseTargets::VisibleStage(14),
        "bend values above the bottom keep distinct stages");
    expect(PoseTargets::IsCbpcReleaseIndex(PoseTargets::VisibleStage(0)) &&
        PoseTargets::IsCbpcReleaseIndex(PoseTargets::VisibleStage(1)) &&
        !PoseTargets::IsCbpcReleaseIndex(PoseTargets::VisibleStage(2)),
        "CBPC is released at SOSBend-9 and not while the angle is still above it");
    int previous = -10;
    bool monotonic = true;
    for (int bend = 0; bend <= 20; ++bend) {
        const int stage = PoseTargets::EventIndex(bend, false);
        if (stage < previous) monotonic = false;
        previous = stage;
    }
    expect(monotonic, "a rise from 0 to 20 never steps the SOS event backwards");
    expect(std::string_view(PoseTargets::EventName(0, false)) == "SOSBend-9" &&
        std::string_view(PoseTargets::EventName(1, false)) == "SOSBend-9" &&
        std::string_view(PoseTargets::EventName(10, false)) == "SOSBend0" &&
        std::string_view(PoseTargets::EventName(19, false)) == "SOSBend9" &&
        std::string_view(PoseTargets::EventName(20, false)) == "SOSBend9",
        "the player curve bottoms at SOSBend-9 and caps at SOSBend9");

    expect(std::string_view(PoseTargets::StageEventName(5)) == "SOSBend5",
        "a mid stage names the matching SOS event");
    expect(std::string_view(PoseTargets::StageEventName(-9)) == "SOSBend-9",
        "the bottom stage is SOSBend-9, the lowest angle");
    expect(std::string_view(PoseTargets::StageEventName(0)) == "SOSBend0",
        "stage 0 is the middle of the signed range");
    expect(std::string_view(PoseTargets::StageEventName(9)) == "SOSBend9",
        "the top stage is the maximum SOS bend event");
    expect(std::string_view(PoseTargets::StageEventName(50)) == "SOSBend9",
        "an out-of-range stage cannot index past the event table");

    return failures;
}
