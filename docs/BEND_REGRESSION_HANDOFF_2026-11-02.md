# SOS AE 4.0.0 bend removal: evidence

Kept from the 2026-11-02 bend-regression handoff (the rest is in git history):
what SOS AE 4.0.0 removed, SOS's own `PlayBend` contract, and the disassembly
of the removed native.

## What the SOS AE update broke

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
