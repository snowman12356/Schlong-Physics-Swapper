# When a dependency removes an API (2026-11-02)

Written after the Schlongs of Skyrim AE 4.0.0 bend regression cost a full
session. The fix was one line. Read this before repeating it.

## What happened

SOS AE 4.0.0 removed the Papyrus native `SOSAE_SKSE.SetSchlongBend`. The angle
stopped working. The investigation went:

1. CBPC handoff timing — assumed the bend was dispatched before CBPC bound the
   bones. Plausible, and a real but unrelated bug. Added a settle floor.
2. `NPC GenitalsBase [GenBase]` bone writes — assumed a continuous angle could be
   written directly. This could never work: every SOS and TNG bend clip binds
   `GenBase`, so the animation graph rewrites it every frame.
3. Disassembled the old DLL to recover the removed implementation.
4. **Finally** opened the upstream source, which was public the whole time.

Step 4 should have been step 1. The actual defect was in our own code:

```cpp
// WRONG - a midpoint guess, 0..9, only the positive half of the graph
return std::clamp(static_cast<int>(std::lround(bend * 9.0 / 20.0)), 0, 9);
```

```cpp
// RIGHT - SOS's own arithmetic from Input.cpp PlayBend
return std::clamp(bend - 10, -9, 9);
```

## Rule 1: read the upstream source before anything else

Before disassembling, before inferring, before adding a shim: search for the
project's source. Check upstream repositories, GitHub, the author's other
repos, and any source shipped in the download.

Most Skyrim SKSE plugins are open source. A closed DLL is the exception, not the
default. Disassembly is a last resort, not a first step.

Verify the exact upstream commit or release the user has installed still matches
what is being read.

## Rule 2: a fallback that never mattered was never correct

When a dependency removes a feature, do not assume the surviving code path is
equivalent. Ask:

- Was this path ever the primary one?
- Was it ever validated against the real implementation?
- Or was it a rough approximation that only existed to satisfy a fallback?

Here, `bend * 9 / 20` existed solely because the native did the real work
afterwards. It was never required to be right, so it wasn't. Removing the native
promoted a placeholder to the main path.

An approximation that was correct enough while shadowed is not correct enough
alone.

## Rule 3: before writing a replacement, establish what the original did

State the original behaviour in concrete terms first:

- exact arithmetic, not "roughly proportional"
- exact input range and output range
- exact event/record/function names
- what it clamped, what it ignored, what it sent at the boundaries

If the answer is not available from source, say so explicitly and treat the
replacement as unverified. Do not present an inference as a restoration.

In this case the boundary mattered: SOS sends `SOSFlaccid` at level 0, and
`level - 10` for 1-20, which reaches `SOSBend10` even though the shipped
behaviour graph only binds `SOSBend-9`..`SOSBend9`. Both details are only
visible in the source.

## Rule 4: one channel, one owner

The pre-4.0.0 code fired two channels at once:

```cpp
graphOK  = actor->NotifyAnimationGraph("SOSBend{legacyBend}");  // coarse guess
nativeOK = CallSosAeBend(actor, bend);                          // real value
```

The native landed second and won, masking the wrong guess underneath. If two
paths send the same thing, find out which one actually takes effect before
editing either. "It worked before" often means "one of the two was never being
believed."

## Rule 5: when the user names a reference, use it immediately

"The user said: copy what SOS does." That is a direct instruction to go read
SOS. Do not substitute a hypothesis for the reference that was handed to you,
and do not treat the suggestion as a guess to be independently validated.

If the reference is genuinely unavailable, say so at that moment and explain
what was used instead.

## Rule 6: separate the timing bug from the value bug

Two independent defects were tangled here, and conflating them extended the
session:

- **Value:** wrong event for a given bend (fixed by `bend - 10`).
- **Timing:** event dispatched before CBPC had bound the nodes (fixed by the
  settle floor in `PoseTargets::SwitchSettleMs`).

Both were real. Neither explained the other. When a symptom has more than one
plausible cause, fix them one at a time and state which is being tested.

## Checklist for this class of problem

Before editing code after a dependency changes:

- [ ] Read the upstream source for the removed function.
- [ ] Confirm the installed version matches the source being read.
- [ ] Write down the original's exact arithmetic and boundary behaviour.
- [ ] Identify whether the surviving path was ever primary or always a fallback.
- [ ] Check whether two channels were both firing, and which one won.
- [ ] Fix value correctness and timing separately, in that order (a correct
      value sent at the wrong moment looks identical to a wrong value).
- [ ] State plainly what remains unverified.

## Reference

- The fix: `src/runtime/PositionBackendAdapter.cpp`, `AnimationEventBend`.
- Upstream: <https://github.com/SpongeBobHentaiSimulator/Schlongs-of-Skyrim-AE-NG>
  — `src/Input.cpp`, `PlayBend`.
- Prior notes: `docs/SOSAE_4_POSE_ANGLE_2026-11-01.md`. That document concludes
  "the premise was wrong" about `GenBase`, which remains correct; it does not
  record the `bend - 10` mapping, which is what actually mattered.
