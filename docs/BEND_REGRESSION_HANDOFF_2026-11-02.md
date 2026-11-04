# SPS bend regression: handoff brief for external review

Written 2026-11-02 for a code reviewer who has no context on this session.

---

## 1. The goal

Restore the Schlong Physics Swapper (SPS) erection angle behaviour to **exactly
what it did before the Schlongs of Skyrim AE 4.0.0 update**, then keep it
correct.

SPS is an SKSE plugin (C++/CommonLibSSE-NG) that decides whether Faster HDT-SMP
or CBPC simulates the male genital assembly, and separately drives the bend /
erection angle from arousal.

The desired behaviour, stated by the user:

1. At zero arousal the schlong rests at its lowest angle (flaccid).
2. When arousal crosses the threshold, ownership switches to **CBPC**.
3. The angle then rises gradually `0 -> ErectBend` (a 0-20 value).
4. When arousal falls, it descends `ErectBend -> 0` **while CBPC still owns the
   bones**.
5. Only after reaching the bottom does ownership hand back to **SMP**, which
   then holds the low angle.

Step 4-5 ordering matters and is already implemented. The user's complaint is
about the *angle* itself.

---

## 2. What the SOS AE update broke

Schlongs of Skyrim AE 4.0.0 removed the Papyrus native
`SOSAE_SKSE.SetSchlongBend(Actor, Int)`. That was the only supported way to ask
SOS for a specific bend angle. SPS called it through `CallSosAeBend`.

The update did **not** touch the skeleton or behaviour graph. All 23 files under
`meshes\auxbones\SOS` are byte-identical to 3.x, and `SOSMaleBehavior.hkx` still
binds:

- events `SOSBend-9` .. `SOSBend9` (nineteen), plus `SOSFlaccid`,
  `SOSFastErect`, `SOSSlowErect`, `SOSErect`, `SOSBendUp`, `SOSBendDown`
- `hkbStateMachine`, `hkbBlenderGenerator`, `hkbBlendingTransitionEffect`

So the animation-event channel survived; only the native wrapper is gone.

### The reference implementation

SOS's own bend control lives in `src/Input.cpp` of
<https://github.com/SpongeBobHentaiSimulator/Schlongs-of-Skyrim-AE-NG>:

```cpp
// Plays the event of a bend level: 0 = flaccid, 1-20 = bend from -9 to 10
void PlayBend(RE::FormID a_actorID, int a_level)
{
    SKSE::GetTaskInterface()->AddTask([a_actorID, a_level]() {
        auto* actor = RE::TESForm::LookupByID<RE::Actor>(a_actorID);
        if (!actor || !actor->Is3DLoaded())
            return;

        if (a_level <= 0)
            actor->NotifyAnimationGraph("SOSFlaccid");
        else
            actor->NotifyAnimationGraph(
                std::format("SOSBend{}", std::min(a_level, Util::kMaxRank) - 10));
    });
}
```

`Util::kMaxRank == 20`. So the contract is:

| bend level | event sent |
|---|---|
| 0 | `SOSFlaccid` |
| 1-9 | `SOSBend-9` .. `SOSBend-1` |
| 10-19 | `SOSBend0` .. `SOSBend9` |
| 20 | `SOSBend10` (graph binds no such event; resolves to nothing) |

### The disassembled original native

`SetSchlongBend` in the pre-update DLL agrees, confirmed by reading the actual
function body at `0x18007a520`:

```asm
cmp  [rsp+0xe0], 0        ; akBendValue <= 0 ?
jg   .format
lea  rdx, "SOSFlaccid"
call NotifyAnimationGraph
.format:
mov  [rsp+0x24], 0x14     ; clamp high = 20
mov  [rsp+0x28], 0        ; clamp low  = 0
call clamp
sub  eax, 0xa             ; value - 10
cmp  [rsp+0x20], 0xa
mov  [rsp+0x20], 0xa      ; cap (value - 10) at 10
lea  rdx, "SOSBend{}"
call fmt::format
call NotifyAnimationGraph
```

So the old native did exactly `PlayBend`: flaccid at 0, otherwise
`clamp(bend, 0, 20) - 10` capped at 10.

---

## 3. What SPS did before the update

Pre-update source is commit `d19733d` in this repository. `ApplyBend` fired
**two channels**:

```cpp
const auto legacyBend = std::clamp(
    static_cast<int>(std::lround(bend * 9.0 / 20.0)), 0, 9);

graphOK  = flaccid
    ? actor->NotifyAnimationGraph("SOSFlaccid")
    : actor->NotifyAnimationGraph(fmt::format("SOSBend{}", legacyBend));

nativeOK = useNative && CallSosAeBend(actor, bend);   // -> SetSchlongBend(bend)
```

The graph channel used a **midpoint guess** (`bend * 9 / 20` -> `0..9`). The
native channel used the real value. The native landed second and won, so the
guess was never visible and was never validated.

The update removed the native, promoting the guess to the primary path. The
guess was wrong, and that is the entire regression.

---

## 4. What was attempted in this session, and what was wasted

Recorded so the reviewer does not repeat it.

1. **CBPC handoff timing** - assumed the bend was dispatched before CBPC bound
   `Gen01`-`Gen06`. Added `PoseTargets::SwitchSettleMs` with a 400 ms floor,
   capped at 2000 ms. This is a *real* but separate concern; it did not cause
   the observed failure.
2. **`NPC GenitalsBase [GenBase]` bone writes** - assumed a continuous angle
   could be written directly. **Impossible**: every SOS/TNG bend clip binds
   `GenBase`, so the animation graph rewrites it every frame. This dead end is
   already documented in `docs/SOSAE_4_POSE_ANGLE_2026-11-01.md`.
3. **Disassembly of the old DLL** - recovered the implementation, but this was
   done before reading the public source that was available all along.

Total: roughly a full session spent on 1-3 before reading `Input.cpp`, at which
point the fix was one line.

---

## 5. Current state of the code

Build `49775D49B8B981579944BDCEE1C862EA4FB9B7DB1C5024CB63BFD9E87D0FB881`,
built but **not yet deployed**. All unit tests pass.

### 5.1 The curve (believed correct now)

`src/runtime/PositionBackendAdapter.cpp`:

```cpp
int AnimationEventBend(int bend, bool tngBackend)
{
    bend = std::clamp(bend, 0, 20);
    if (tngBackend) {
        return std::clamp(
            static_cast<int>(std::lround(bend * 18.0 / 20.0)) - 9, -9, 9);
    }
    return std::clamp(bend - 10, -10, 9);
}

const char* SosBendEventName(int bend, bool tngBackend)
{
    bend = std::clamp(bend, 0, 20);
    static thread_local char name[32];
    if (bend <= 0) {
        return "SOSFlaccid";
    }
    std::snprintf(name, sizeof(name), "SOSBend%d",
        std::clamp(bend - 10, -9, 9));
    return name;
}
```

This is a transcription of `PlayBend`, with one deliberate deviation: bend 20
caps to `SOSBend9` instead of sending the unbound `SOSBend10`, so the queued key
and the sent event always agree.

**Every call site must go through `SosBendEventName`.** Two sites previously
formatted `"SOSBend{}"` themselves and emitted `SOSBend-10` at bend 0.

### 5.2 Ramp mechanics (`src/plugin.cpp`, `StartBendAnimation`)

The ramp thread samples the eased 0-20 curve every 50 ms and dispatches only on
a visible-stage change:

```cpp
const int queueKey = useGraphEvents
    ? std::clamp(AnimationEventBend(bend, tngBackend), -10, 9) + 10
    : bend;
const int previousQueueKey = playerContext.position.animationLastQueuedBend.exchange(queueKey);
if (queueKey != previousQueueKey || t >= 1.0F) { ... dispatch ... }
```

Two details that were broken and restored:

- `|| t >= 1.0F` guarantees the final step is sent. Without it the last value
  can be swallowed by deduplication.
- The `+ 10` offset exists because the sentinel for "nothing queued" is `-1`,
  which is also a legitimate event name (`SOSBend-1`). Without the offset,
  bends 1-9 were indistinguishable from "no event".

### 5.3 Reporting symptom from the user (current build under review)

After the previous build, with the curve fixed:

> "100 - 0 arousal seems correct but 0-100 it snaps halfway then goes down again
> before it goes back up."

Descending is correct. Ascending **snaps up, drops, then continues**. Three
candidate causes were identified and patched in the current build; none is
confirmed in game.

#### Fix A - repeated ramp start from flaccid

`StartGradualErection` was hard-coded to start at bend 0:

```cpp
void StartGradualErection(int targetBend, int durationMs) {
    StartBendAnimation(0, targetBend, durationMs, false);
}
```

`StartGradualErection` is invoked from `Evaluate` when `ClaimSettle` succeeds,
and `ClaimSettle` fires whenever ownership is re-confirmed - not only on a
rising edge. A completed rise could therefore be restarted from flaccid,
producing a snap down and a fresh climb.

Changed to start from the currently applied bend:

```cpp
const int current = playerContext.position.appliedBend.load();
const int from = std::clamp(current < 0 ? 0 : current, 0, targetBend);
StartBendAnimation(from, targetBend, durationMs, false);
```

#### Fix B - mid-ramp confirmation truncating the rise

`Evaluate` has a confirmation pass that applies the final value outright. It was
guarded only on `animating`, so a ramp that failed and was waiting to retry
(`animating == false`, `retryDueMs != 0`) could be confirmed mid-rise.
`ApplyRequestedBend` also writes `animationTargetBend`, so the ramp then aimed at
whatever height it had already reached.

Now guarded on both:

```cpp
const bool rampOwnsAngle = playerContext.position.animating.load() ||
    playerContext.position.retryDueMs.load() != 0;
if (!rampOwnsAngle && !ownershipController.Read().pending &&
    positionController.ClaimConfirmation(...)) {
    ApplyRequestedBend(true, true, true);
}
```

#### Fix C - retry sawtooth

On a failed step the ramp called `FailAnimationStep(now + 1500)`, which cancels
and leaves `animationTargetBend` at the original target. The retry then resumed
from the observed value towards the full height and dipped again on the next
failure.

Added `PositionController::SetAnimationTargetBend(bend)` and call it after
`FailAnimationStep`, so the retry continues from where the angle actually peaked.
`BeginAnimation` now also sets `animationCompleted = false`, and
`CompleteAnimation` sets it true; the completion path early-returns if the ramp
is already complete, so a retry landing on an already-dispatched stage cannot
re-run completion (which rewrites `appliedBend` and reissues the SMP handoff).

---

## 6. What a reviewer should check

1. **Is Fix A safe?** `StartGradualErection` is called on every successful
   `ClaimSettle`. If `appliedBend` is stale or wrong, the new start point is
   wrong. Is `appliedBend` reliable at that moment, or should the start be the
   last *dispatched* stage instead?
2. **Does Fix B open a hole?** If a ramp permanently fails, `retryDueMs` can
   stay non-zero and the confirmation pass is blocked indefinitely. Is there a
   path that clears `retryDueMs` other than a successful step or
   `CancelAnimation`? If not, the angle may never be confirmed after a hard
   failure.
3. **Is Fix C's `animationCompleted` correctly scoped?** It is reset in
   `BeginAnimation` and set in `CompleteAnimation`. `CancelAnimation` does not
   reset it. Is there a sequence where a cancelled ramp leaves it true and the
   next ramp inherits it?
4. **Does anything still format `"SOSBend{}"` directly?** Every site must use
   `SosBendEventName`, or the `SOSBend-10` / negative-event bug returns.
5. **Ownership ordering.** Confirm the descent still keeps CBPC as owner until
   the bottom step is sent, and only then calls `SetOwner(false, true)`.
6. **TNG path - fixed while writing this, please verify.** `SosBendEventName` now
   branches on `tngBackend` and uses `AnimationEventBend(bend, true)` for TNG
   instead of the SOS subtraction. Before that it applied `bend - 10` to TNG as
   well, which would name events TNG does not bind. Confirm the TNG branch is
   correct and that the TNG range really is `-9..9`.
7. **The `-10` vs `-9` lower bound.** `AnimationEventBend` returns in
   `[-10, 9]` while `SosBendEventName` clamps the formatted value to `[-9, 9]`.
   Two different ranges for the same quantity is a smell; confirm the queue key
   never disagrees with the sent event. (The `-10` value is unreachable through
   `SosBendEventName` because bend 0 returns `SOSFlaccid` first, but the range
   mismatch should be made explicit or removed.)

---

## 7. Files touched in this session

| File | Change |
|---|---|
| `src/runtime/PositionBackendAdapter.cpp` | `AnimationEventBend` curve; new `SosBendEventName`; `ApplyPosition` uses it |
| `src/runtime/PositionBackendAdapter.h` | declares `SosBendEventName` |
| `src/plugin.cpp` | ramp uses `SosBendEventName`; queue-key offset; `t >= 1.0F` restored; `StartGradualErection` starts from current bend; confirmation guard; `SetAnimationTargetBend` on failure; `animationCompleted` guard |
| `src/controllers/PositionController.{h,cpp}` | `SetAnimationTargetBend`; `animationCompleted` set in `BeginAnimation`/`CompleteAnimation` |
| `src/controllers/ActorContext.h` | `animationCompleted` field |
| `src/controllers/PoseTargets.h` | `VisibleStage` uses the signed SOS curve |
| `src/controllers/BasicNpcPolicy.cpp` | managed NPC event uses `bend - 10` |
| `tests/PoseTargetsTests.cpp`, `tests/BasicNpcPolicyTests.cpp` | expectations updated to the signed curve |
| `docs/DEPENDENCY_API_REMOVAL_PLAYBOOK.md` | new: process notes on what went wrong |

---

## 8. Reference material

- Upstream SOS AE-NG source: <https://github.com/SpongeBobHentaiSimulator/Schlongs-of-Skyrim-AE-NG>
  - `src/Input.cpp` - `PlayBend`, the authoritative bend contract
  - `src/SchlongLogic.cpp` - bone nodes, `ScaleSchlongBones`
  - `src/API.cpp`, `src/SOSAE_API.h` - the exported API; **no bend function**
- Old native disassembly: `out/diagnostics/sos_dis_linear.py`, function at
  `0x18007a520` in `D:\Modding\usefulmods\Schlongs of Skyrim AE old\SKSE\Plugins\SchlongsOfSkyrim.dll`
- Pre-update SPS source: `git show d19733d:src/plugin.cpp`, `ApplyBend`
- Prior analysis: `docs/SOSAE_4_POSE_ANGLE_2026-11-01.md` (the `GenBase` dead end)
- Process notes: `docs/DEPENDENCY_API_REMOVAL_PLAYBOOK.md`

---

## 9. Build and deploy (this machine)

```powershell
cd "D:\Modding\codex\schlong smp"
pwsh -NoProfile -ExecutionPolicy Bypass -File tools\Build-Local.ps1
```

Outputs `out\build\SchlongPhysicsSwapper.dll`. Deploy to
`D:\Modding\mods\Schlong Physics Swapper\SKSE\Plugins\SchlongPhysicsSwapper.dll`.
The game must be closed or the copy fails with a file-in-use error.

Tests are run by the same script: `SPSCoreTests`, `SPSDiagnosticsTests`,
`SPSActorRegistryTests`. There is **no test** covering
`AnimationEventBend` / `SosBendEventName` directly - adding one is the cheapest
guard against this regressing again.
