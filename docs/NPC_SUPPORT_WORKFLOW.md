# Managed NPC support workflow

Created: 2026-09-21. Branch: `codex/codebase-rework`.

Status: started with user authorization on 2026-09-21. Initial API/baseline
investigation and the first tested Stage 1 registry are recorded in
[`NPC_SUPPORT_FOUNDATION_2026-09-21.md`](NPC_SUPPORT_FOUNDATION_2026-09-21.md).
The actor-specific arousal/position increment and paired validation completed
on 2026-09-22; see
[`NPC_ACTOR_BRIDGES_2026-09-22.md`](NPC_ACTOR_BRIDGES_2026-09-22.md).
The subsequent scene-deferral and basic SOS policy increment also passed its
build, test and package checks on 2026-09-22; see
[`NPC_BASIC_SOS_POLICY_2026-09-22.md`](NPC_BASIC_SOS_POLICY_2026-09-22.md).
NPC runtime remains inactive; no installation or publication has occurred.
Next: connect the bounded basic NPC update loop, eligibility and opt-in/provider
coexistence controls. Managed physics handoff/controller wiring remains outstanding.

## Outcome and scope

Extend the existing SPS player system to explicitly selected NPCs. Each managed
actor should receive independent arousal-based switching, transitions, angles,
scene handling and bounded recovery. Users must be able to add and remove NPCs;
ordinary unmanaged actors receive basic arousal-to-SOS behaviour from an SPS
compatibility bridge when the arousal mod's own SOS control is disabled.

The user supplied a prior design screenshot on 2026-09-21 clarifying this
direction. It is a design reference, not evidence that any proposed API or
fallback already works. There are two distinct responsibilities:

| Actors | SPS responsibility |
| --- | --- |
| Ordinary unmanaged NPCs | Read supported arousal information and provide standard SOS soft/erect behaviour; no full SPS physics ownership, advanced angle policy or recovery enrollment |
| Explicitly managed NPCs | Use actor-specific SPS decisions for SOS state, positioning, SMP/CBPC switching, transitions, scenes and recovery |

For unmanaged NPCs the conceptual flow is arousal to basic SOS state. For
managed NPCs it is arousal to SPS policy to the appropriate SOS state, angle
and physics owner. This describes responsibilities, not a new execution order:
the existing guarded physics handoff order remains authoritative.

Read numeric arousal when a verified provider API exposes it. When it does not,
use a verified general SOS-state fallback where available. Unknown input must
remain unknown, rather than becoming zero arousal or an invented numeric value.
Retain older OSL support through the existing version-gated legacy option.
Implement this within SPS; do not edit or replace another installed mod.

Continue the existing controllers and guarded bridges in small changes. Preserve
the player's settings, saved values and behaviour. The player remains the first
managed context while SPS is enabled. Do not restart the rework, introduce the
abandoned semi-state, or add automatic world-wide NPC management.
The basic SOS compatibility bridge is distinct from enrolling an NPC in full
physics management. Its eligible loaded actors, update budget and scene rules
must also be bounded and documented.

Only SPS and its dedicated test mod are in scope. FSMP, other mods, the main
Skyrim installation and `D:\Modding\mods\SOFTBODY True Soft Shaft - SPS` are
outside the change scope. No subagents unless the user requests them.

## Starting evidence

- Baseline HEAD: `103e402`; release commit:
  `7aa9f80267fcea662cdd0d270396e65d499bc366` (`v2.0.1`).
- The published 2.0.1 ZIP and installed DLL matched their recorded hashes on
  2026-09-21. All nine installed core DLL/PEX/PSC files matched the ZIP.
- `src/controllers/ActorContext.h` already holds actor identity and physics,
  position, recovery and spontaneous-event state. Arousal and scene state also
  need independent ownership; an ActorContext alone does not make NPCs work.
- The existing controllers are reusable. Runtime orchestration and bridge
  entry points still contain player assumptions, including `Game.GetPlayer()`
  in `scripts/Source/SPS_FSMPBridge.psc`.
- `src/SPSAPI.h` already carries actor FormIDs, but the implemented public API
  supports only the player. An actor-aware structure is not NPC support.
- Ordinary player switching, resting length and scene/equipment recovery have
  recorded passes on the tested setup. On 2026-09-21 the user also confirmed
  disable/re-enable, manual Repair and state changes/save loading during a
  pending handoff all passed. Broader runtime/provider/mesh coverage remains
  separate; use `PROJECT_DIRECTION.md` for the current evidence boundaries.

Keep the existing support issues separate: the 2.0.1 crash reporter's live
confirmation, Barbadoza's mesh/configuration evidence and stale SPS-010
suggestion, and pheonex2077's working XMLs remain outstanding. Do not treat
them as resolved or bundle speculative fixes into the NPC work.

## Decisions to record in Stage 0

These are proposed starting choices, not previously agreed requirements.
Resolve routine implementation details from evidence; ask the user only where
a product choice materially affects how they will use NPC support.

| Decision | Proposed starting point | Evidence or choice still needed |
| --- | --- | --- |
| Selecting an NPC | Explicit selection with Add/Remove controls and a managed list | Choose the selection interaction supported by the existing menu/game APIs |
| Initial capacity | One fully managed NPC alongside the player; basic test capped at eight nearby actors | Basic cap chosen for the 2026-10-03 test, not a measured release limit; see runtime notes |
| NPC settings | A separate shared NPC settings group, initially using familiar SPS defaults | Confirm whether individual overrides are needed in the first release; never make NPC edits change player settings |
| Remembering NPCs | Save-specific managed membership | Verify reference resolution and define temporary/spawned actor eligibility before choosing the storage format |
| Activation | Explicit opt-in; no NPCs enrolled on update | Basic test uses separate enable/OSL SOS-disabled confirmation, both false by default; disable stops commands and leaves the last pose |
| Feature coverage | Core switching, transitions, positioning, scenes and recovery | Decide separately whether random/morning/refractory features should apply to NPCs; do not copy player sleep assumptions |
| Unmanaged actors | SPS supplies basic arousal-to-SOS behaviour, as clarified by the supplied design | Verify numeric APIs, SOS-state fallback, eligible loaded actors, scene cooperation and provider-specific coexistence |

Record final choices here with their reasons before implementing the dependent
part. Do not silently turn proposals into shipped defaults.

## Stage 0 - establish the baseline and feasibility

1. Recheck branch and working tree. Preserve existing work and leave the
   user-owned `docs/USEFUL_WEB_LINKS.txt` untracked and untouched.
2. Reconcile the player checklist against recorded evidence. Carry forward
   documented passes; collect missing player regression results before enabling
   NPC runtime support, as required by `PROJECT_DIRECTION.md`. Keep optional
   integration/runtime results explicit rather than assuming they passed.
3. Map player assumptions in ownership, arousal, position, scenes, recovery,
   equipment events, diagnostics and external API routing. Identify the smallest
   actor-specific extensions to the existing boundaries.
4. Verify required actor targeting, reference lifetime, save resolution and
   event/query APIs against the relevant installed sources and local references.
   Record exact source locations and versions; do not invent API signatures.
5. Verify the arousal-to-SOS bridge design. OSL 2.9.3+ uses a global
   Enable SOS switch: disabling it also disables ordinary NPC angles. Simply
   excluding an actor from SPS does not restore those angles; SPS is intended
   to supply basic SOS behaviour itself. Record each supported provider's
   numeric arousal query, fallback state source, basic SOS output and version
   restrictions. Verify that fallback input is independent of SPS's own last
   output; never treat an echoed SPS state as fresh external arousal evidence.
   Do not blindly enable competing controls, edit another mod, or silently
   enroll all NPCs in full physics management. If an API cannot deliver the
   intended behaviour, record that specific limitation before choosing a fix.
6. Record the product decisions above, initial test setup, baseline package
   hashes and outstanding test cases. Keep the release version undecided until
   the candidate scope is known.

Deliverable: a source-backed actor/API map and a settled first-milestone scope.
Gate: the required player baseline and compatibility prerequisites are resolved
before NPC runtime is enabled. Documentation and read-only investigation can
proceed while live results are outstanding.

## Stage 1 - introduce actor isolation through existing boundaries

1. Extend actor management around the existing context/controllers. Give each
   actor its own arousal, scene, ownership, position and recovery state without
   duplicating the player decision rules.
2. Carry actor identity and operation/session generation through every queued
   request and callback. Reject results from a removed/re-added actor, an old
   save, a superseded request or an invalid reference.
3. Add actor-specific runtime/bridge entry points where required, preserving
   existing player/public signatures and the safe native-to-Papyrus boundary.
   Never resolve a failed NPC target to the player as a fallback.
4. Preserve ordered FSMP/CBPC handoff, SMP-off maintenance, soft-pose recovery,
   cancellation, running-stack leases, reset task barriers and pause-aware
   bounded timers. Audit which resources are global before allowing overlap:
   retain the shared serialization boundary until independent execution is
   proven safe. A timed-out stack must not be overtaken by releasing its lease.
5. Add focused tests for actor isolation, callback routing, stale completion,
   target reversal and cancellation. Keep player-only behaviour covered.

Deliverable: actor-aware plumbing with NPC runtime still disabled by default.
Gate: native/Papyrus builds and relevant regression tests pass; all changed
bridge contracts have a matching version and paired DLL/scripts.

## Stage 2 - arousal-to-SOS compatibility bridge

1. Read eligible NPC arousal from the verified supported providers. Prefer
   numeric values; use the verified SOS-state fallback when numeric input is
   unavailable. Record input validity and source. A binary fallback must not
   pretend to support numeric thresholds or gradual arousal-based angles.
2. For ordinary unmanaged NPCs, provide standard SOS soft/erect behaviour
   while the provider's own SOS control is disabled. Use verified provider/SOS
   behaviour for the mapping rather than assuming player SPS thresholds are
   the ordinary SOS defaults. Do not claim this alone gives floppy SMP dynamics.
3. Keep basic compatibility state separate from full managed-actor state.
   Enrollment transfers responsibility to full SPS control; removal returns
   eligible actors to the basic bridge after outstanding work retires safely.
   Never let both paths issue competing commands to the same actor.
4. Preserve the existing legacy OSL option and its version restrictions. Check
   its NPC behaviour for competition with both paths; do not install it for
   newer OSL versions or modify an external OSL installation as a workaround.
5. Bound loaded-actor discovery and updates separately from the full-management
   cap. Prefer events and cached eligible actors; no continuous world-wide
   scans. Stop dispatch to unavailable actors and discard stale results.
6. Establish verified scene cooperation before sending background SOS changes:
   yield when a scene owns positioning and resume only after appropriate
   cleanup. Define basic-bridge behaviour when SPS is disabled without
   silently changing another mod's settings.

Deliverable: a basic SPS-owned SOS bridge, with documented numeric/fallback and
legacy-provider coverage, distinct from full NPC physics management.
Gate: ordinary eligible NPC behaviour works with external SOS control disabled;
no competing scene commands, self-fed fallback or effects on player control.
Unverified provider paths remain explicitly unsupported/pending, not passed.

## Stage 3 - one selected NPC alongside the player

1. Add minimal selection, Add/Remove and status controls. Reject duplicate,
   invalid and unsupported targets with a useful reason. Use actual loaded
   mesh/3D checks; six skeleton bones alone do not prove SMP mesh compatibility.
2. Exercise one compatible loaded NPC using its own arousal and settings:
   soft/erect decisions, gradual transitions, angle application and manual tests.
3. Define and implement clean removal before calling enrollment complete.
   Stop new work, invalidate queued requests, let running work retire safely,
   and perform only the verified release/normal-control handoff. Do not claim
   to restore a pre-SPS engine state that cannot actually be read back.
4. Handle temporary 3D unload/reload without forcing the actor to stay loaded.
   Suspend unsafe work, invalidate stale results, and reconcile when usable
   3D returns. Define deferred cleanup if removal happens while unavailable.
5. Until Stage 5 validates NPC scenes, detect/defer unsupported scene activity
   through a verified safe policy and disclose that limitation in test notes.
   A first milestone must not apply ordinary arousal rules blindly in scenes.
6. Report the selected actor, requested state, completed commands, pending
   work and failures. Keep live motion unverified until observed in-game.

Deliverable: a separate test candidate for one NPC and the player.
Gate: visible independent switching and angles, clean removal back to basic SOS
behaviour, unload/reload, an unmanaged comparison NPC and no player regression.
This is the first full-management live milestone, not release readiness.

## Stage 4 - lifecycle, save membership and multiple NPCs

1. Persist only the intended membership/settings using verified save-aware
   reference resolution and versioned records. Do not persist raw pointers,
   in-flight operations, timers or cached engine ownership as live truth.
2. Define old-save/new-game defaults, save switching, missing/removed forms,
   actor death/deletion, temporary references and capacity changes. A save
   without NPC records must retain the current player behaviour and an empty
   NPC list. Never transfer a managed list accidentally between saves.
3. On load, invalidate the previous session's work, resolve membership safely
   and reconcile valid loaded actors. Retain unavailable membership only under
   the documented policy; avoid repeated requests to unloaded actors.
4. Expand from one NPC to two, then to the chosen hard limit. Bound total
   requests per tick, queue size and retries; coalesce duplicate events and
   use fair scheduling so NPC work cannot starve the player or another NPC.
5. Define global SPS disable/re-enable and per-NPC removal while work is pending.
   Never force-unlock an executing bridge stack to make cleanup appear complete.

Deliverable: predictable saved membership and bounded multi-actor operation.
Gate: automated lifecycle/routing tests and live save/load, removal, disable,
unload/reload and simultaneous-transition tests pass. Document save compatibility
and rollback implications; do not assume uninstalling a DLL cleans saved state.

## Stage 5 - scenes and recovery for managed actors

1. Route SexLab and optional OStim events/queries by the actual participant and
   scene/thread identity. Do not generalize the player's special thread IDs.
   Preserve existing role precedence and the treatment of unknown/unavailable
   roles; verify the NPC provider APIs before using them.
2. Test separate simultaneous scenes, a player/NPC shared scene, two managed
   NPCs with different roles, and mixed managed/unmanaged participants. Never
   apply one actor's role, scene exit or angle to another actor.
3. Extend equipment, external reset and SOFTBODY reload recovery to the affected
   managed actor. Preserve settling, cancellation and retry limits. Do not
   change XML profiles or run a global physics reset to repair one actor.
4. Verify normal resting length, scene cleanup, equipment changes and manual
   Repair. Include removal, unload and save loading during pending scene work.
5. If external API NPC support is included, route requests/listeners/reset
   notifications per actor without changing V1 layouts or player semantics.
   External requests must not silently enroll unmanaged actors. Otherwise keep
   NPC API support explicitly unavailable and document that scope.

Deliverable: NPC core feature coverage with integration-specific evidence.
Gate: player and NPC scene/recovery tests pass for every advertised integration.
OStim remains optional and is not claimed tested from SexLab results.

## Stage 6 - controls, diagnostics and performance

1. Finish the managed list with clear active, unloaded, unsupported, pending
   removal and failed states. Provide actor-specific tests and repair controls;
   make it clear which actor an action affects.
2. Ensure support captures identify actors and separate historical failures
   from currently unresolved problems. The known cumulative SPS-010 suggestion
   is a separate evidenced repair; do not copy it into NPC health reporting or
   claim it fixed by actor labels alone.
3. Measure the same scenes/setup with NPC support off, the basic bridge alone,
   one managed NPC, two managed NPCs and the proposed limit. Include crowded
   loaded areas with unmanaged NPCs. Record tick cost, queue depth/wait,
   dispatch rates and recovery volume for both responsibilities separately.
   Choose explicit budgets/caps before accepting the result.
4. Test busy scenes, equipment bursts, paused menus and unavailable actors.
   No unbounded scans, retry loops, queue growth or player starvation is acceptable.
5. Complete the compatibility explanation, mesh requirements, defaults and
   save/removal instructions in user documentation.

Deliverable: understandable controls, useful reports and a measured hard limit.
Gate: no unexplained cross-actor effects or growing backlog; every supported
feature has recorded automated and/or live evidence appropriate to its claim.

## Stage 7 - package, install and release gate

Use this validation/install sequence for each live candidate, not only at the end:

1. Update `PROJECT_DIRECTION.md` and this workflow's progress/evidence record
   with the actual changes, limitations and next gate.
2. Run relevant tests and native builds with existing SE/AE/VR targets, compile
   the affected bridges (all five for a full release build), and inspect warnings.
   A successful multi-runtime build is not an in-game pass on every runtime.
3. Produce a separately named/versioned test package through the existing paired
   build workflow. Never rebuild or overwrite the published 2.0.1 package.
   Validate bridge ABI/version, paired hashes, FOMOD/XML, expanded ZIP and package
   rejection checks appropriate to the changed contracts.
4. When installation is authorized, confirm Skyrim is closed, back up the
   dedicated test mod, and install only the selected candidate files into
   `D:\Modding\mods\Schlong Physics Swapper`. Preserve its INI and `meta.ini`
   and existing physics selection. Verify installed hashes and record rollback.
5. Run the live matrix below and record observations separately from command
   completion. Fix only evidenced failures and repeat the affected checks.
6. Prepare release notes, final hashes and the stated support scope. Push,
   publication and external posting each require authorization for that action.
   Preserve 2.0.1 bytes and keep experiments outside the release candidate.

## Acceptance matrix

Mark each result as passed, failed, pending or not applicable with a reason.
Record build/hash, runtime, providers, actor/mesh setup and evidence location.

| Area | Required checks |
| --- | --- |
| Player baseline | Load soft/erect; arousal up/down; angles; normal soft length; equipment; scenes; manual tests/Repair; disable/re-enable; pending reversal/load; paused menus |
| Enrollment | Add/remove/re-add; duplicate; unsupported mesh; invalid/deleted target; hard limit; remove during queued/running work; unloaded removal |
| Actor isolation | Player plus NPC in opposite states; two NPCs with different arousal/roles; late callbacks; no reset/angle/role leakage |
| Lifecycle | Save/reload; different save; new game; old save with no NPC records; unload/reload; death/deletion; missing forms; pending work across load |
| Scenes/recovery | SexLab and optional OStim separately; entering/leaving; role/stage changes; mixed participants; independent scenes; SOFTBODY reload; equipment/external reset |
| Coexistence | Numeric arousal and SOS-state fallback separately; unknown input; legacy/current OSL; unmanaged basic behaviour with external SOS control disabled; scene ownership; enrollment/removal transfers responsibility without competition |
| Performance | Support off/basic bridge only/one/two/limit; crowded loaded areas; busy scenes; event bursts; unavailable actors; bounded queues/retries; player responsiveness |
| Packaging | Paired DLL/bridges; ABI/version guards; selected install hashes; preserved INI/metadata/profiles; existing-save and rollback assessment |

## Progress and resumption

| Stage | Status | Evidence / next action |
| --- | --- | --- |
| Workflow | Complete | Created 2026-09-21; user subsequently authorized starting |
| 0 - baseline and feasibility | Initial map recorded; provider checks remain | User confirmed the queried player checks. CommonLib/OSL/SOS API findings recorded; generic SOS fallback and other provider details remain unverified |
| 1 - actor isolation | In progress; basic runtime connected | Guarded endpoints and globally distinct basic/managed lifetimes are tested. Full managed physics/controller wiring remains |
| 2 - basic arousal-to-SOS bridge | Live pass reported by the user on 2026-10-31 | Eight nearby actors, separate lifetimes, scene policy, opt-in/confirmation and menu status connected. Player/per-tracked-NPC sliders use OSL's real SetArousal contract for controlled live testing. User observed the player slider below and above threshold, two tracked NPCs on opposite OSL values posing independently, scene entry/exit deferral, unload/reload, save/load and disable/re-enable. Command counters and successful OSL writes were not accepted as motion evidence by themselves. No log or capture was attached. Native, Papyrus, package and installed-file verification passed. See `NPC_BASIC_RUNTIME_2026-10-03.md` and `OSL_AROUSAL_CONTROLS_2026-10-10.md`. Generic SOS fallback, non-OSL providers and per-runtime SE/AE/VR runs remain pending |
| 3 - one managed NPC | Not started; awaiting explicit user authorization | First full-management live milestone after Stage 2 |
| 4 - lifecycle and multiple NPCs | Not started | Depends on Stage 3 |
| 5 - scenes and recovery | Not started | Depends on Stage 4 |
| 6 - controls and performance | Not started | Depends on Stage 5; basic diagnostics begin with the bridge |
| 7 - release gate | Validated 2.1.0.3 OSL-controls package installed | The new arousal bridge is compiled, paired and installed with the 2.1.0.3 DLL; build, tests, ABI/export, package, negative-guard, deterministic-ZIP and installed-file checks passed. INI, metadata and physics selections were preserved. Published 2.0.1 and prior test ZIPs remain unchanged. User-observed live pass recorded 2026-10-31; no log attached and no publication |

At each stop, record completed work, exact candidate identity, checks run,
outstanding evidence and the next concrete action. Preserve partial changes;
do not restart a stage or rerun unchanged checks without a reason.

References: `PROJECT_DIRECTION.md`, `docs/CODEBASE_REWORK.md`,
`docs/IMPLEMENTATION.md`, `docs/DEVELOPMENT.md`, `docs/MOD_AUTHOR_API.md`,
`docs/RELIABILITY_AUDIT_2026-08-25.md` and `docs/AUDIT_REPAIRS_2026-09-06.md`.
