# Implementation and validation

## Why the switch works

FSMP's `TogglePhysics(actor, bones, on)` changes the selected Bullet rigid
bodies between dynamic and kinematic state. With `on = false`, FSMP stops
simulating those bodies and follows the skeleton pose, allowing CBPC to supply
that pose. With `on = true`, FSMP resumes simulation and clears stale velocity
to avoid an explosive transition.

The controller explicitly calls CBPC's per-node `StopPhysics` before enabling
SMP and `StartPhysics` after disabling SMP. The order prevents a transition
frame in which both engines own the same dynamic chain.

The bundled late-loading CBPC map assigns aliases `UBEPS01` through `UBEPS06`
to:

1. `NPC Genitals01 [Gen01]`
2. `NPC Genitals02 [Gen02]`
3. `NPC Genitals03 [Gen03]`
4. `NPC Genitals04 [Gen04]`
5. `NPC Genitals05 [Gen05]`
6. `NPC Genitals06 [Gen06]`

`NPC GenitalsBase [GenBase]` is mass-zero in the installed SMP XML and is not
toggled. Scrotum nodes are also excluded because the active CBPC SOS mapping
does not drive them.

## Mesh/XML requirements

A compatible schlong mesh must have one consistent bone chain shared by its SMP
XML and CBPC config. The native controller toggles Gen01 through Gen06. Parent
collision-only bodies should not be added unless both physics engines drive
them.

Do not install a generic XML over the user's addon by default. Mass,
constraints, bone names and collision groups must match the exact mesh. The
FOMOD therefore defaults to keeping the user's own physics. Its personal SPS
profile is an explicit opt-in for compatible SOS, TNG and UBE six-bone meshes.
The separate SOFTBODY 3.37.2 option combines those dynamics with SOFTBODY's
normal, soft and anal collision profiles. Its merged `MaleGenitals` shape must
remain unique and retain both SOFTBODY's weight thresholds and SPS's collision
exclusions.

## Test procedure

When SexLab provides a valid role result, that result is authoritative for the
physics decision. This includes its temporary `Not identified` result, which
uses the configured unknown-role behaviour instead of accepting a contradictory
PPA record. PPA continues to own live penetration alignment, but its role record
is a physics fallback only when the SexLab role query is unavailable or invalid.
This prevents a paired PPA interaction record from changing a receiving player
to CBPC before SexLab finishes identifying the stage.

All equipment recovery, player-only SMP resets and delayed bend replays wait for
the current ownership transaction to finish. After a PPA/Accurate Penetration or
SOFTBODY scene ends while the player is still using SMP, one delayed
player-only reset clears scene-stretched transforms before SPS reconfirms the
soft owner. A real CBPC-to-SMP handoff uses the same refresh path.

A failed or timed-out Papyrus ownership transaction marks the cached owner as
unknown. SPS then retries the current policy decision with a new ordered
transaction; it never guesses that the pre-transaction owner still controls the
live bones.

Papyrus workload pressure (`overstressed`) is diagnostic information, not a
dispatch veto. The September 5 test continued running other scripts and reading
native OSL arousal after suspended-stack warnings, while SPS rejected all scene
queries, ownership calls and manual repair attempts. The gateway now permits
its existing bounded requests when the VM is initialized and unfrozen, even
under workload pressure. Post-load delay, game activity, Loading/Journal menus,
player 3D and VM policy checks still apply. Captures and reports show both the
actual dispatch wait reason and the pressure flag. This does not clear the
game's workload flag or alter another mod's stacks.

## Audit repairs, 2026-09-06

`SPS_FSMPBridge.SetPlayerOwnerV3` owns preparation and restoration in one
callback-tracked operation. Its process/session token is checked at entry,
after waits and before engine mutations. Native cancellation invalidates the
token but retains an executing stack's lease until it returns. A token from a
previous save/process cannot enter a new operation. A queued SKSE barrier
follows FSMP's reset task; only then does the 750 ms settling delay begin.
Equipment release, settling and reacquisition use the same operation. Older
global bridge entry points remain for compatibility; SPS no longer calls them.
The bridge version must return 3 before this DLL dispatches ownership work.

An execution clock excludes game inactivity, frozen-VM periods and paused
menus. It is used consistently by queries, ownership, recovery and timed
position control. Periodic game tasks are coalesced to prevent a backlog during
long frames. Equipment follow-up has a 12-second budget; asynchronous
confirmation failures retain their original episode deadline. Core policy can
still recover an unknown owner, including after a failed soft relaxation.

SexLab's extended `Hook*` events are Papyrus events. Its native SKSE callback
uses the bare event name and a string thread ID. SPS matches that ID against the
player bridge result, so unrelated NPC stages do not schedule player resets.
New player scenes are also detected by the existing bounded query. Cached
scene identity and queued query results are invalidated across load boundaries;
scene and arousal callback results are committed on the game thread.

A successful transaction acknowledges executed calls, not live engine read-back.
FSMP's Boolean array describes previous states and does not uniquely identify a
missing bone; CBPC's per-node calls return no ownership result. Visible movement
and first-/third-person skeleton attachment still require in-game tests. SPS
does not guess engine internals or change another mod to hide this limitation.

## SMP drift detection, bridge version 7

SPS previously discarded FSMP's `TogglePhysics` return value. Verified against
Faster HDT-SMP `src/dhdtPapyrusFunctions.cpp` (`TogglePhysicsImpl`), the array
holds, per requested bone, whether that bone **was dynamic at the moment of the
call**. The sample is taken before any state change, is recorded once per bone
even when several skinned meshes carry the same name, and FSMP early-outs when a
bone already matches the requested state, so sampling does not alter a bone that
already agrees.

That makes the existing SMP-off maintenance call a passive drift probe: it runs
only while policy still wants CBPC and the cached owner is already CBPC, so any
bone reported dynamic there proves SMP had resumed ownership unseen. Drift is
recorded only under that precondition; an ordinary SMP -> CBPC switch legitimately
reports dynamic bones and must never raise an alarm.

The inverse direction remains unprovable. FSMP reports "kinematic or not found"
identically, so a false entry never confirms that SMP released a bone, and a
CBPC -> SMP transition supplies no evidence at all. SPS therefore never
re-asserts SMP purely to sample it. `smpConnected` and `cbpcConnected` are now
set separately from the selected owner instead of from one shared boolean, which
previously reported SMP as ready after an SMP-off handoff.

Bridge version 7 returns `Int[4]` (validity, dynamic-bone count, bone-array
length, reserved) and requires the paired DLL. CBPC exposes no state at all:
`CBPCPluginScript.StartPhysics`/`StopPhysics` are void native calls, so "CBPC
attached" remains an accepted command, never a verified condition.

## Actor-scoped ownership, bridge version 8

Version 8 adds `SetActorOwnerV3(String token, Bool useCBPC, Int preparation)` for
explicitly managed NPCs. It returns the same `Int[4]` read-back as the player
path, so NPC switching has the same evidence and the same limits: only the
disable direction proves anything, and CBPC attachment stays an accepted command.

It deliberately omits the player-only steps in `SetPlayerOwnerV3` -- the SOS
flaccid event, the soft-pose bend and the full `ResetPhysics`. Those follow player
settings and would overwrite an NPC's own configured pose.

`OwnershipRequestCurrent` and `ResolveOwnershipRequest` are actor-scoped guards.
A token resolves to exactly one actor and is re-checked after every engine
mutation, so a removed, unloaded or superseded NPC cannot fall back to the player.
This is the rule from the NPC workflow: never resolve a failed NPC target to the
player. Ownership shares `PapyrusOperationGate` with the player and position
paths, so an NPC handoff can never overlap a player handoff; the caller's ledger
slot is released only when the real stack returns.

The audit regression list and save compatibility assessment are in
[`AUDIT_REPAIRS_2026-09-06.md`](AUDIT_REPAIRS_2026-09-06.md).

1. Disable CBPC handling of the chain temporarily and verify visible SMP motion
   below the threshold.
2. Re-enable CBPC. Set manual mode to `Force CBPC` and verify that movement is
   controlled only by CBPC.
3. Set manual mode to `Force SMP` and verify SMP resumes without a jump.
4. Return to `Automatic`; set arousal just below and above the threshold.
5. Stand near the boundary for at least 30 seconds and confirm hysteresis
   prevents repeated switching.
6. Save in each mode, reload, and confirm the first update reapplies the mode.

If only part of the chain moves, a rigid-body name is missing. If the chain
vibrates or stretches violently, both engines still own at least one dynamic
bone or the two configs use different parent transforms.
