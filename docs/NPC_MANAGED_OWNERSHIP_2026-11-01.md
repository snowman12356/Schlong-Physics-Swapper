# Managed NPC ownership bridge, 2026-11-01

Branch: `codex/codebase-rework`. Stage 3 of
[`NPC_SUPPORT_WORKFLOW.md`](NPC_SUPPORT_WORKFLOW.md), first two increments.

Status: source and build verification only. No package and no in-game evidence.
Stage 3 is not complete.

## What blocked Stage 3

`SPS_FSMPBridge.psc` was player-only. Every ownership entry point resolved its
target with `Game.GetPlayer()`, so there was no way to hand a specific NPC to SMP
or CBPC. The workflow forbids the obvious shortcut too: a failed NPC target must
never fall back to the player.

Actor-scoped arousal, scene and position calls already worked through
`SPS_ArousalBridge` and `SPS_PositionBridge`, which use a token that resolves to
one actor and is re-checked before each mutation. Ownership had no equivalent.

## What changed

Bridge version 8 (`SPS_FSMPBridge`, paired DLL required):

- `SetActorOwnerV3(String token, Bool useCBPC, Int preparation)` returns the same
  `Int[4]` read-back as the player path: validity, dynamic-bone count, bone-array
  length, reserved. The evidence rules are unchanged, so only the disable
  direction proves anything and CBPC attachment stays an accepted command.
- `OwnershipRequestCurrent(String token, Actor)` and
  `ResolveOwnershipRequest(String token)` are the actor-scoped guards. A token
  resolves to exactly one actor and is re-verified on both sides of every engine
  mutation.
- The NPC path omits the player-only steps: no SOS flaccid event, no soft-pose
  bend, no full `ResetPhysics`. Those follow player settings and would overwrite
  an NPC's own configured pose.

Native (`PhysicsOwnershipAdapter`):

- `SetActorPhysicsOwner` rejects the player, requires a live managed-actor lease
  whose ticket matches the actor, and shares `PapyrusOperationGate` with the
  player and position paths. An NPC handoff therefore cannot overlap a player
  handoff.
- `CancelActorOwnership(ticket)` invalidates the guard and cancels the gate
  token without force-releasing a stack that is still executing.
- Ownership tokens live in their own ledger, so cancelling an NPC's arousal work
  cannot silently release an ownership lease.

`ActorRequestLedger` gained `ActorRequestKind::ownership` and one rule the old
code lacked: position and ownership both mutate physics through the shared
engine gate, so one actor may hold at most one of them at a time. `Cancel` also
now reports ownership tokens as mutations; previously only `position` was
reported, which would have left an ownership token holding the gate.

`BasicNpcRuntime::Remove` now calls `CancelActorOwnership` as well.

## Verification

- Papyrus: all five bridges compile with 0 errors and 0 warnings.
- Contracts: `SPS_FSMPBridge` v8, `SPS_SexLabBridge` v3, `SPS_PositionBridge`
  v3, `SPS_ArousalBridge` v3, `SPS_OStimBridge` v2 all pass `Test-SPSPexContract`.
- Native: clean build, no warnings. Physics-decision, diagnostics, and actor
  lifetime/request/basic-NPC suites all pass.
- New tests cover ownership holding the single mutation slot, ownership not
  authorizing a position dispatch, delivery on completion, and handoff resuming
  after retirement.

## Managed runtime (same increment)

`ManagedNpcRuntime` drives one enrolled NPC through the same decision rule the
player uses, rather than a parallel copy:

- `Core::NormalSettingsWantCBPC` is reused unchanged, so NPC thresholds,
  hysteresis and mode behaviour cannot silently drift from the player's.
- A managed NPC gets its own arousal and scene polling on its own cadence, its
  own `ActorContext` (so its own ownership state and generation), and its own
  `PhysicsOwnershipController` built from that context each tick.
- The pose event comes from `Core::ManagedSosEvent`, which uses that NPC's
  threshold and configured `erectBend`. It is deliberately *not*
  `BasicSosEvent`, the ordinary-SOS fallback for unmanaged actors.
- A scene that is not verified clear suppresses all ownership changes, and an
  unknown or stale arousal never authorizes a command.
- Removal invalidates the actor, cancels bridge and ownership tokens, and drops
  the driver record — the NPC returns to the basic bridge on the next tick.

`BasicNpcRuntime::Eligible` now excludes enrolled actors, so the basic bridge and
full management can never command the same target.

Selection uses `RE::CrosshairPickData::GetSingleton()->GetActiveTarget()`
(verified in the local CommonLibSSE-NG headers). The render callback only queues
an intent; the crosshair is resolved and the registry mutated on the game thread.

Membership is **session-only**. Nothing is written to the save, so loading a save
never inherits a stale managed list. That is a deliberate Stage 3 limit, not
Stage 4 lifecycle support.

## Not verified

None of this has run in game. Command completion is execution evidence, not
proof that an NPC's physics visibly changed.

A real defect was found and fixed before that verdict: an unloaded actor resolves
to no `ActorContext`, so an early version would have *deleted* membership on a
mere 3D unload. Unloaded actors now keep membership and are simply not serviced.

## Installed test candidate

`2.2.0.0` — built with the paired workflow (`Build-Release.ps1`), so the DLL and
all five bridges were compiled together and hashed.

| Item | Value |
| --- | --- |
| ZIP | `out\release\Schlong-Physics-Swapper-2.2.0.0.zip` |
| ZIP SHA-256 | `6B747DE93B82526D1057E919A78FF401699EFA1BBF97C4CFFB296996846C592C` |
| DLL SHA-256 | `1569B55C31794B1F2891FAF0825B77AC5DDE0E2FEC8C1363CC38115E0E5DB0F7` |
| Previous DLL | `1F02B2BB…93BCF2` (2.1.0.3) |
| Backup | `out\backups\Schlong Physics Swapper-20261101-120920` (25 files) |

Preserved, verified by hash after install: `SchlongPhysicsSwapper.ini`,
`meta.ini`, and all three `hdtSkinnedMeshConfigs` XMLs. The installed
`SPS_FSMPBridge.pex` passes the v8 contract check, so `SetActorOwnerV3` is
present alongside the paired DLL.

Rollback: copy the backed-up `SKSE\Plugins\SchlongPhysicsSwapper.dll` and
`Scripts\*.pex` back over the installed ones. Both sides must match — the DLL
rejects a bridge whose version differs.

## First in-game checks

1. Enable **managed NPC** on the NPCs page; look at an NPC and **Add**.
2. Confirm the NPC leaves the basic list (it must not appear in both).
3. Raise that NPC's arousal with its own OSL slider and confirm it switches and
   takes an angle while the player stays unchanged.
4. **Remove** and confirm it returns to basic behaviour.
5. Walk out of range and back; confirm it suspends and reconciles without a jump.
6. Disable managed NPC in the menu; confirm work stops and the pose is left alone.

## Next

1. In-game verification: one NPC with its own arousal switching independently of
   the player, with visible angles.
2. Removal back to basic behaviour, unload/reload, and an unmanaged comparison NPC.
3. Then a separate test package; installation needs its own authorization.

## Capability boundary

This makes an NPC handoff *possible and safe to attempt*. It does not prove the
NPC's mesh is SMP-capable: six found bones are still a positioning prerequisite,
not proof of an equipped SMP mesh. Stage 3 keeps its gate requiring visible
independent switching and angles before it counts as passed.
