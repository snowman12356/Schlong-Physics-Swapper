#include "controllers/SoftRecoveryCoordinator.h"

#include <iostream>
#include <string_view>

namespace {
int failures = 0;
void Expect(bool condition, std::string_view name)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << name << '\n';
    }
}

using Coordinator = SPS::Controllers::SoftRecoveryCoordinator;
using Action = Coordinator::Action;
using FollowUp = Coordinator::ProofFollowUp;

// A soft player with a known SMP owner and nothing busy.
Coordinator::Inputs Soft(std::int64_t now, const char* scene = nullptr)
{
    Coordinator::Inputs in;
    in.now = now;
    in.sceneBusy = scene;
    in.ownerKnown = true;
    in.ownerCBPC = false;
    return in;
}

void PlainCheckWaitsForQuiet()
{
    Coordinator c;
    Expect(c.Evaluate(Soft(0)).action == Action::none, "nothing requested: no action");
    c.Request("the soft-state confirmation", false, false, 1000);
    Expect(c.Evaluate(Soft(1000)).action == Action::wait, "a fresh request waits for the quiet period");
    Expect(c.Evaluate(Soft(2400)).action == Action::wait, "still waiting just before 1.5 s of quiet");
    const auto plan = c.Evaluate(Soft(2600));
    Expect(plan.action == Action::confirm && plan.merged == 1, "plain check runs after 1.5 s of quiet");
    c.OnDispatched(plan, false);
    Expect(c.Evaluate(Soft(4000)).action == Action::none, "a dispatched check clears the round");
}

void TriggersMergeIntoOneRound()
{
    Coordinator c;
    c.Request("a player mesh change", true, false, 0);
    c.Request("the completed equipment change", false, false, 500);
    c.Request("a player mesh change", true, false, 1000);
    Expect(c.Evaluate(Soft(1000)).action == Action::wait, "scene-over clock starts");
    const auto plan = c.Evaluate(Soft(3100));
    Expect(plan.action == Action::bake, "a bake request wins over a plain check");
    Expect(plan.merged == 3, "three requests merged");
    Expect(plan.reasons == "a player mesh change, the completed equipment change",
        "reasons listed once each");
    c.OnDispatched(plan, true);
    Expect(c.Evaluate(Soft(5000)).action == Action::none, "one bake covers the whole round");
}

void BakeWaitsForTheSceneButCheckDoesNot()
{
    Coordinator c;
    c.Request("an external physics reset", false, false, 0);
    c.Request("the soft rest refresh", true, false, 0);
    // A plain check may run mid-scene once quiet, even with a bake pending.
    auto plan = c.Evaluate(Soft(5000, "a SexLab scene"));
    Expect(plan.action == Action::confirm, "plain check may run during a scene");
    c.OnDispatched(plan, false);
    plan = c.Evaluate(Soft(6000, "a SexLab scene"));
    Expect(plan.action == Action::wait && plan.waitingFor != nullptr &&
        std::string_view(plan.waitingFor) == "a SexLab scene", "the bake waits for the scene");
    Expect(c.Evaluate(Soft(7000)).action == Action::wait, "scene just ended: settle time");
    Expect(c.Evaluate(Soft(8500)).action == Action::wait, "settle time not over at 1.5 s");
    Expect(c.Evaluate(Soft(9100)).action == Action::bake, "bake after the scene has been over 2 s");
}

void ManualRepairSkipsTheSceneWait()
{
    Coordinator c;
    c.Request("a manual repair", true, true, 0);
    Expect(c.Evaluate(Soft(1600, "an OStim scene")).action == Action::bake,
        "forced bake runs during a scene");
}

void MaxWaitStopsEndlessTriggers()
{
    Coordinator c;
    (void)c.Evaluate(Soft(1000));
    for (std::int64_t t = 0; t < 10000; t += 1000) {
        c.Request("a player node update", false, false, t);
        Expect(c.Evaluate(Soft(t)).action == Action::wait, "steady triggers keep it waiting");
    }
    c.Request("a player node update", false, false, 10000);
    Expect(c.Evaluate(Soft(10000)).action == Action::confirm, "runs after the 10 s maximum wait");
}

void ErectOrDisabledClearsTheRound()
{
    Coordinator c;
    c.Request("the game load", true, false, 0);
    auto erect = Soft(5000);
    erect.ownerCBPC = true;
    Expect(c.Evaluate(erect).action == Action::none, "CBPC owner: soft recovery dropped");
    Expect(c.Evaluate(Soft(9000)).action == Action::none, "and it stays dropped");
    c.Request("the game load", true, false, 0);
    auto off = Soft(5000);
    off.enabled = false;
    Expect(c.Evaluate(off).action == Action::none, "SPS disabled: soft recovery dropped");
}

void WaitReasonsAndLongWaitLogOnce()
{
    Coordinator c;
    c.Request("the game load", false, false, 0);
    auto in = Soft(3000);
    in.ownerKnown = false;
    auto plan = c.Evaluate(in);
    Expect(plan.action == Action::wait && std::string_view(plan.waitingFor) == "the physics owner",
        "unknown owner is waited for");
    in = Soft(3000);
    in.handoffPending = true;
    plan = c.Evaluate(in);
    Expect(std::string_view(plan.waitingFor) == "the current physics handoff", "pending handoff is waited for");
    in.handoffPending = false;
    in.papyrusReady = false;
    in.now = 61000;
    plan = c.Evaluate(in);
    Expect(std::string_view(plan.waitingFor) == "Papyrus" && plan.logLongWait, "long wait logged after 60 s");
    in.now = 62000;
    Expect(!c.Evaluate(in).logLongWait, "long wait logged only once per round");
}

void NewerRequestsSurviveADispatch()
{
    Coordinator c;
    c.Request("the soft rest refresh", true, false, 0);
    (void)c.Evaluate(Soft(1000));
    const auto plan = c.Evaluate(Soft(3000));
    Expect(plan.action == Action::bake, "bake planned");
    c.Request("a late outfit rebuild", true, false, 3100);
    c.OnDispatched(plan, true);
    Expect(c.Evaluate(Soft(3200)).action == Action::wait, "a request after the plan keeps its own round");
    Expect(c.Evaluate(Soft(4700)).action == Action::bake, "and runs once quiet");
}

void PlannedBakeThatRanAsCheckKeepsTheBake()
{
    Coordinator c;
    c.Request("the game load", true, false, 0);
    (void)c.Evaluate(Soft(1000));
    const auto plan = c.Evaluate(Soft(3000));
    c.OnDispatched(plan, false);  // FSMP actor API unavailable: a plain check ran
    Expect(c.Evaluate(Soft(3000)).action == Action::bake, "the bake stays requested");
}

void DispatchFailuresAreBounded()
{
    Coordinator c;
    c.Request("the post-switch check", false, false, 0);
    Expect(!c.OnDispatchFailed(true), "a busy refusal with a known owner is not counted");
    Expect(!c.OnDispatchFailed(false) && !c.OnDispatchFailed(false), "two failures are retried");
    Expect(c.OnDispatchFailed(false), "the third failure drops the round");
    Expect(c.Evaluate(Soft(5000)).action == Action::none, "dropped round does nothing");
}

void SmpProofFollowUpIsBounded()
{
    // FSMP can answer for the first-person skeleton: at most two re-checks.
    Coordinator c;
    Expect(c.OnSmpProof(false, 1000) == FollowUp::recheck, "first unproven proof: re-check");
    Expect(c.OnSmpProof(false, 3000) == FollowUp::recheck, "second unproven proof: re-check");
    Expect(c.OnSmpProof(false, 5000) == FollowUp::giveUp, "third: give up and log");
    Expect(c.OnSmpProof(false, 7000) == FollowUp::none, "after giving up: stay quiet");
    Expect(c.OnSmpProof(false, 7000 + Coordinator::kRetryDecayMs + 1) == FollowUp::recheck,
        "budget returns after two quiet minutes");
    Expect(c.OnSmpProof(true, 200000) == FollowUp::none, "a certain proof needs nothing");
    Expect(c.OnSmpProof(false, 200100) == FollowUp::recheck, "and resets the budget");
}

void BakeRulesAndRetries()
{
    using C = Coordinator;
    Expect(C::BakeClean(6, 6, 6, false), "6/6 before and after is clean");
    Expect(!C::BakeClean(0, 6, 6, false), "SMP-off found nothing: the off call may have missed the body");
    Expect(C::BakeClean(0, 6, 6, true), "after an aborted bake the shaft may be kinematic");
    Expect(!C::BakeClean(6, 5, 6, false), "not every bone dynamic after the rebuild");
    Expect(!C::BakeClean(-1, -1, 0, false), "no sample is never clean");

    Coordinator c;
    c.NoteBakeAborted();
    Expect(c.CheckBake(0, 6, 6), "aborted flag allows a kinematic before-sample");
    Expect(!c.CheckBake(0, 6, 6), "and is consumed by one check");

    Expect(c.AllowBakeRetry(1000) && c.AllowBakeRetry(2000), "two bake retries allowed");
    Expect(!c.AllowBakeRetry(3000), "the third is refused");
    c.OnBakeSucceeded();
    Expect(c.AllowBakeRetry(4000), "a good bake restores the budget");
    Coordinator d;
    Expect(d.AllowBakeRetry(0) && d.AllowBakeRetry(1) && !d.AllowBakeRetry(2), "budget spent");
    Expect(d.AllowBakeRetry(2 + Coordinator::kRetryDecayMs + 1), "budget returns after two minutes");
}

void CbpcReleaseOncePerLoad()
{
    // CBPC clears its stop flags on every load: release once, bounded.
    Coordinator c;
    Expect(c.CbpcReleaseWanted(), "release wanted after a load");
    c.OnCbpcReleaseQueued();
    c.OnCbpcReleaseQueued();
    Expect(c.CbpcReleaseWanted(), "still wanted after two failed attempts");
    c.OnCbpcReleaseQueued();
    Expect(!c.CbpcReleaseWanted(), "stops after three attempts");
    c.ResetSession();
    Expect(c.CbpcReleaseWanted(), "the next load wants it again");
    c.OnCbpcReleaseQueued();
    c.OnCbpcReleased();
    Expect(!c.CbpcReleaseWanted(), "not wanted once released");
}

void ResetSessionForgetsTheRound()
{
    Coordinator c;
    c.Request("the game load", true, false, 0);
    (void)c.AllowBakeRetry(0);
    (void)c.AllowBakeRetry(1);
    (void)c.OnSmpProof(false, 0);
    (void)c.OnSmpProof(false, 1);
    c.ResetSession();
    Expect(c.Evaluate(Soft(10000)).action == Action::none, "no round after a load");
    Expect(c.AllowBakeRetry(5), "bake budget reset");
    Expect(c.OnSmpProof(false, 5) == FollowUp::recheck, "proof budget reset");
}
}

int main()
{
    PlainCheckWaitsForQuiet();
    TriggersMergeIntoOneRound();
    BakeWaitsForTheSceneButCheckDoesNot();
    ManualRepairSkipsTheSceneWait();
    MaxWaitStopsEndlessTriggers();
    ErectOrDisabledClearsTheRound();
    WaitReasonsAndLongWaitLogOnce();
    NewerRequestsSurviveADispatch();
    PlannedBakeThatRanAsCheckKeepsTheBake();
    DispatchFailuresAreBounded();
    SmpProofFollowUpIsBounded();
    BakeRulesAndRetries();
    CbpcReleaseOncePerLoad();
    ResetSessionForgetsTheRound();
    if (failures != 0) {
        std::cerr << failures << " soft recovery test(s) failed\n";
        return 1;
    }
    std::cout << "All soft recovery coordinator tests passed\n";
    return 0;
}
