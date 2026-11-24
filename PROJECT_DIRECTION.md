# Schlong Physics Swapper - project direction

This file is the permanent high-level direction for SPS. Read it before planning
or implementing project changes. Detailed implementation plans belong in the
`docs` directory and must remain consistent with this direction.

## Permanent update rule

Update this file after every change to the mod. Changes to native or Papyrus
code, configuration, compatibility components, tests, build/package tooling or
user-visible behaviour must update the status and open items below in the same
working change whenever practical. Git history, `docs/` and the Obsidian vault
(`D:\SecondBrain`, project note `Projects/Schlong Physics Swapper`) are the
implementation record; this file is the accurate high-level view of where SPS
stands and what comes next. Keep it compact: record the current state and the
reasons behind decisions, not build hashes of superseded builds. The full
pre-compaction history is `out/backups/PROJECT_DIRECTION-before-compact-20261116.md`.

## Purpose

SPS is a dependable physics-management system for the player and selected NPCs.
It coordinates SMP (flaccid) and CBPC (erect) for the six shaft bones without
requiring users to repair the state manually.

## Priorities

1. **Player reliability first.** Loading a game, changing arousal, equipping or
   removing armour, entering or leaving scenes, and external physics resets must
   leave the player in the correct state.
2. **Maintainable architecture.** Keep settings, decisions, runtime adapters,
   arousal, scenes, position, recovery, diagnostics and MCM presentation
   separated. New fixes should not destabilise unrelated systems.
3. **Runtime compatibility.** Preserve Skyrim SE, AE and VR support through
   CommonLibSSE-NG (pinned `alandtse/CommonLibSSE-NG` v15.4.0 or newer; see
   `docs/DEVELOPMENT.md`).
4. **Managed NPC support after the player is stable.** Users select NPCs and add
   or remove them from SPS management (capacity 1; 8 with the "Allow up to 8" toggle). Managed NPCs get SPS
   switching, transitions, scene handling, positioning and bounded recovery.
   Unmanaged NPCs keep standard SOS/arousal-mod behaviour.
5. **Low overhead.** Prefer events, bounded queues and strict managed-actor
   limits over world scanning or heavy polling.
6. **Cooperative compatibility.** Take ownership only when needed; coexist with
   supported arousal, scene and physics mods. HDT-SMP Flex is not planned.

## Non-negotiable compatibility rules

- Preserve existing settings, INI keys, defaults and saved values.
- Preserve save compatibility and existing working player behaviour.
- Do not reintroduce the abandoned experimental semi-state.
- Keep DLL and Papyrus bridge files version-matched in every test package.
- Do not modify an installed Skyrim mod, publish a release, or push public
  changes unless the user explicitly requests it. The user rejected changes to
  other mods (including an FSMP patch); all work is SPS and its test mod only.
- The user's private `D:\Modding\mods\SOFTBODY True Soft Shaft - SPS` XMLs are
  theirs; edit only when asked, always with a backup under `out/backups`.

## Current status (2026-11-22)

- **Branch:** `codex/codebase-rework`, last commit `254fcad` (per-NPC settings,
  NPC ramp and soft pose, SOSFlaccid rest, API V3). Nothing pushed.
- **Version:** 2.3.0.0 is the local test build. Published: 2.0.0 (2026-09-13,
  commit `738b2ee`, tag `v2.0.0`) and 2.0.1 (2026-09-15, commit `7aa9f80`, tag
  `v2.0.1`, Unicode-filename startup crash hotfix). Nexus page (mod 188174) was
  updated for 2.0.0. 2.3.0.0 is unpublished.
- **2.3.0.4 reverted (2026-11-24):** 2.3.0.4 (`64792e7`, GitHub release v2.3.0.4) let a PPA
  `Penetrating` role win a SexLab `Receiving` one for handjobs/blowjobs. The owner tested it
  (log 20:54-21:19, 2.3.0.4 loaded) and it did not fix the soft shaft; the code was reverted
  to 2.3.0.3 and the test mod restored to 2.3.0.3 (DLL `92112FEE...DE56`; the 2.3.0.4 mod state
  is in `out/backups/testmod-before-revert2304-20261124-213447`). Its diagnostic line gave the real
  evidence: in both handjob scenes PPA reported the player `Penetrating / top`, SexLab reported
  `Receiving / bottom`, and **P+ reported the player's interaction as `aHandJob`**. P+'s comment
  says `aHandJob` is the hands side, but P+ flagged it on the actor whose penis was handled, so
  `ReadSexLabRole`'s side for handjob (and possibly oral types) does not match P+'s detection.
  Why the PPA rule did not fire is not established (probably the 3 s PPA recency and
  `sexLabRoleQuerySettled` window). The GitHub release v2.3.0.4 still exists; not yet decided.
- **2.3.0.3 bridge crash fix (built 2026-11-24, installed 2026-11-24 19:49 with Skyrim closed, 15 files hash-verified, rollback `out/backups/testmod-before-bridgev6-20261124-194938`; committed `4f90c69`, pushed, `main` fast-forwarded, tag and GitHub release `v2.3.0.3` published 2026-11-24 (Nexus upload left to the owner); not checked in game):**
  the same reporter on 2.3.0.2: no load crash, but the same engine crash (ID 104802+0xAE inside
  `DispatchStaticCall`, called directly from SPS `DispatchStatic`) when arousal was raised to 80,
  on the first rise step (`SOSBend-9`): TNG's graph refused `NotifyAnimationGraph`, so the Papyrus
  fallback ran. The full Crash Logger log shows the reporter has **no SexLab** (no `SexLab.esm`,
  no `SexLabUtil.dll`). Since `993d8ec` (after 2.0.1) `SPS_PositionBridge` declared
  `SexLabFramework` locals (and an `sslThreadController` expression); 2.0.1's bridge had none,
  and the SexLab bridge itself is only ever dispatched when SexLab is present. Every crash was the
  first call into `SPS_PositionBridge`. Leading cause (Papyrus log not yet seen): the script cannot
  link without SexLab and the engine faults dispatching into it. Fix: `SPS_PositionBridge` v6 holds
  no optional-mod types; its SexLab reads call new `SPS_SexLabBridge` v6 globals
  `ReadActorSceneState(Actor)` / `ReadActorRole(Actor)` (same return values). C++ expects position
  v6 and SexLab v6; contracts updated. Rule: never declare an optional mod's script type in a
  bridge that is called without that mod. 2.3.0.2's native-first angle stays. Build 0 warnings,
  6 scripts compile, tests and package guards pass. After the Papyrus and save-safety reviews, `SPS_PositionBridge` also returns 0 (state) or -1 (role) when `SPS_SexLabBridge` is missing or older than v6, so a stale SexLab bridge cannot read as "no role"; API doc updated. Reviews: probably safe for existing saves (global-only Hidden scripts, no co-save/INI change), test one load. DLL `92112FEE...DE56`, ZIP `02A4AE86...F8AD`.
  The reporter also says idle bones look worse than 2.0.1 (their log: SMP proof 0/6 for ~20 s
  after load, then 6/6 with repeated `Player mesh changed` refreshes; open item 2); not addressed.
- **2.3.0.2 TNG load crash fix (built 2026-11-24, installed 2026-11-24 19:06 with Skyrim closed,
  not checked in game; pushed, `main` fast-forwarded to `4d2bacf`, tag and GitHub release
  `v2.3.0.2` published 2026-11-24 (Nexus upload left to the owner); 15 files hash-verified, INI/`meta.ini`/XMLs/CBPC configs
  untouched; rollback `out/backups/testmod-before-tngnative-20261124-190621`, 52 files):**
  a Nexus report (UBE + TNG, 1.6.1170) crashes on 2.3.0 and 2.3.0.1, not 2.0.1, a few seconds
  after loading. The disassembled 2.3.0.1 DLL puts the fault inside the engine's
  `DispatchStaticCall` (Address Library ID 104802, BSScript range) for
  `SPS_PositionBridge.SendPlayerAnimationEvent`, called from the SKSE task that 2.3.0.1 added,
  so the 2.3.0.1 theory (VM callback re-entrancy) is refuted, and its fallback hold never
  applied to the TNG branch. The exact engine null is unconfirmed (the report's log was an AI
  summary without engine frames); leading suspect is the 2.3 bridge script's much larger
  dependency set (SexLab, OSL, OStim/SexLab bridges) versus 2.0.1. Fix: `SendPositionEvent`
  now tries native `NotifyAnimationGraph` first for TNG as well as SOS, and the Papyrus
  fallback (and its hold) applies to both. DLL only, safe for existing saves. Unverified: that
  TNG's graph accepts the native event (if refused, the old Papyrus route runs). Build 0
  warnings, tests and package validation pass. DLL `45A8826E...5C96`, ZIP `C0F6C850...1540`.
  Still wanted from the reporter: the full crash log and `Papyrus.0.log`.
- **Installed in the test mod** (`D:\Modding\mods\Schlong Physics Swapper`), latest first:
  the 2026-11-23 evening build, DLL `00FB6947B27B938411C06601532EF239FD273FF706AD42C25B9B8A34438EA0B1`
  (rollback `out/backups/testmod-before-riseprime-*`), adding the equipment-change second soft SMP
  reset (equip 2 s, unequip 2.5 s debounce) and managed NPC rise priming on SOSBend-9 before the
  soft to CBPC switch (3 s cap). The user reported on 2026-11-23 that everything they tested
  works (user report; no per-item log verification). Physics XML: `scrotumcollision` 0.03,
  `penis`/`peniscollision` 0.15 in the `SOFTBODY True Soft Shaft - SPS` override.
  Before that, the 2026-11-23 13:01 build (DLL
  `2283B5189771C0F5CD54B4ACF0F2A15018755C16B7D908C66D4FA20485BB2B5E`, 9 files hash-verified,
  INI and `meta.ini` untouched; rollback `out/backups/testmod-before-managedsave-20261123-130157`)
  with per-save managed NPCs, the scene-check fix (earlier DLL `43C542E3...20FE00`) and the futa work below, on top of the 2026-11-19
  audit fixes, SexLab role repair and NPC futa fix (earlier DLL
  `9283C8FE...6470`, installed 2026-11-19 21:55). Rollback for the latest install:
  `out/backups/testmod-before-npcscenekeep-20261122-143722`; earlier:
  `testmod-before-forcefuta-20261121-173934`, `testmod-before-auditfutafix-20261119-215427`.
  Not checked in game. The user is testing on throwaway saves. INI,
  `meta.ini` and the physics XMLs in the test mod are never overwritten by
  installs.
- **Uncommitted:** FOMOD natural-hang option, pex section in
  `docs/DEVELOPMENT.md`, the logging code, README/.gitignore/compat changes,
  and the 2026-11-19 audit fixes below.
- **2026-11-19 audit fixes (installed, not checked in game):** SexLab
  role detection broke when SexLab P+ 2.19.0 (installed 2026-11-13) removed
  `HasInteractionType` and renumbered the interaction types (28,032 "Method not
  found" log errors, SKSE log always `Role=Not identified`). `SPS_SexLabBridge.ReadSexLabRole` now
  uses P+ `HasActiveInteractionAny` with the `SexLabThread.psc` type IDs, shared
  by the player role and `SPS_PositionBridge.ReadActorSceneRole` (which now
  returns -1, not 0, when SexLab is requested but unresolvable). The side of
  each type follows the P+ property comments, except grinding, which follows P+'s own
  code (aGrinding is the penis side); not yet seen in game.
  `CountDynamicBones` no longer compares an array to None (the "Cannot cast
  from None to Bool[]" log noise). Removed uncalled `TogglePlayerPhysics` and
  unused stub declarations; OStim `GetPlayerRole` reuses `GetActorRole`. Bridge
  versions unchanged except SexLab (below). Release scripts now default `-Version` from `CMakeLists.txt`.
  Historical docs removed; their durable facts moved to `docs/IMPLEMENTATION.md`.
  Save safety: safe (global-only Hidden bridges, no stored state). Build, tests,
  package checks and `pex audit` pass.
- **2026-11-19 NPC futa fix (installed, not checked in game):** the
  NPCs page stayed on "reading..." for managed NPC Cicero (`FF001FA9`), so no
  futa button appeared. `SPS_SexLabBridge.DescribeActor`/`SetActorFuta` used
  `Game.GetForm`, which returns None for FormIDs >= 0x80000000 (every FF
  runtime-created and FE light-plugin actor). New native
  `SPS_SexLabBridge.ResolveIdentityActor(Int)` (DLL `LookupByID<Actor>`); SexLab
  bridge v4 to v5 in PSC, `ActorIdentity.cpp` and the contracts. The co-save
  already keeps FF IDs (SKSE `ResolveFormId` passes them through). Safe for
  existing saves; DLL and PEX must be installed together (an old PEX with the
  new DLL logs SPS-013 and identities stay unread). Build 0 warnings, tests,
  package checks and `pex audit` pass. DLL
  `9283C8FE7EA61922A1C3615B623D26BD9308F20FCEB41D7E441D52A07B796470`, ZIP
  `C23BFD9BDD99251E7EAB976BE57DBFA580B6934528DB710D49FB88132A5A161F`.
- **2026-11-21 futa hint and Force futa (installed, not checked in game):** the
  "Schlong and SexLab" section now shows SexLab's base sex next to the current
  sex ("(base: X)" or "(forced)"), and a one-line `Identity <FormID>: schlong
  '<addon>' size N, SexLab base sex B, current sex S` log entry whenever an
  actor's identity changes. A character whose base sex is not male or female
  (for example a creature base sex) gets a **Force futa in SexLab** button with a
  warning that creature animations may stop matching; `SetActorFuta` itself is no
  longer gated on base sex. A forced futa is remembered per save in the same
  `SPSW` / `FUTA` co-save record. The reapply path now requires only that the
  identity was read, not `FutaEligible`, so a forced actor is re-applied too.
  Co-save format unchanged: safe for existing saves.
- **2026-11-23 managed NPCs remembered per save (installed 2026-11-23 13:01, not checked in game):**
  the managed list was dropped on every game load because `ResetTransientTimers()`
  (kPostLoadGame/kNewGame) calls `ResetManagedNpcs()`, and nothing stored it; only
  futa had a co-save record. Now a second `SPSW` record `MNPC` v1 (count, up to 8 form
  IDs, resolved on load; FF runtime IDs pass through) is written by the same callbacks
  in `ActorIdentity.cpp`. `ManagedNpcRuntime` keeps `memberIds` (added on a successful
  Add, erased on an explicit Remove or a save revert; `ResetManagedNpcs` leaves it) and
  re-adds the members on the next enabled tick without the 3D/SOS checks, because the
  NPC may be in another cell (the tick keeps membership for an unloaded actor). An actor
  that no longer exists is dropped with a log line; the limit is honoured and a refused
  member stays in the save. Log lines: `Managed NPC list loaded from this save: N`,
  `... restored from this save`. A side effect: changing the managed-NPC settings no
  longer forgets the group. Safe for existing saves (no record means an empty list; an
  older or removed SPS ignores the record). Per-NPC options already persisted by base
  record. Build 0 warnings, tests pass, package validated; the co-save path itself has no
  automated test. A save-safety review found no blockers and led to one fix (a member dropped by the tick is also erased from the saved list). DLL `2283B5189771C0F5CD54B4ACF0F2A15018755C16B7D908C66D4FA20485BB2B5E`.
- **2026-11-22 managed NPC scene-check fix (installed, not checked in game):**
  a scene query that cannot be dispatched (`QueueActorScene` returns false while
  the script engine is busy) used to mark the NPC's scene unknown, which the pose
  logic treats as "in a scene": it cleared `sentIndex`/`curBend` and replayed the
  whole angle rise. `ManagedNpcRuntime.cpp` now finishes the slot, counts it as
  blocked and keeps the last known scene, logging `scene check not sent (script
  engine busy); keeping the last known scene` at most once per `kRepeatLogMs`.
  Transient driver state only: safe for existing saves. Motivated by the Cicero
  (`FF001FA9`) loop, `SOSBend-9` then `SOSBend4` every 3-6 s with no scene.
  The 2026-11-22 14:10-14:36 log that showed the loop predates this build (the
  rollback backup was taken after the log ended), so it neither confirms nor
  refutes the fix: it has 70 angle sends but only two ownership switches (SMP,
  then CBPC) and three logged scene changes, and no `scene check not sent` line
  because the old build did not log it. The old failure path set the scene to
  unknown without logging, so the pattern fits but is not proof. Confirm with a
  new play session (open item 3).
- **Verified in game by the user:** player switching, soft resting length,
  scene/equipment recovery (2.0.0 gate, 2026-09-13); rise/fall even-step ramp
  and "it works" on the audit-fix build (2026-11-04); basic OSL NPC posing,
  OSL test sliders (2026-10-31).
- **Built/installed but NOT verified in game:** managed NPC support (limit 1, up to 8 by toggle,
  per-NPC settings, rise/fall ramp, soft pose hold, angle hysteresis, NPCs page
  redesign), male futa, API V3, SOSFlaccid resting pose after the SMP handoff,
  full SMP reset after a fall, reset-only preparation 5, no resting-pose replay.

## Open items

1. **Shaft regrowth after full flaccid (reported fixed by the user, 2026-11-22;
   cause and fix not recorded, so the notes below are history).** The shaft grows a few seconds
   after full flaccid and needs a manual FSMP reset. Ruled out: SPS code and
   Papyrus (silent after the reset), SOS Grower (uninstalled), mesh rebuild
   events. Shaft-bone logging showed Gen01 settling from the animated hanging
   pose (about -38 deg) to a fixed +20.1 deg over about 6 s after every reset;
   that is SMP settling to the XML's own rest and is **by design** (Gen01
   limits x -0.92/-0.36, equilibrium -0.68 is the anti-clip fix against the
   balls; do not change it for regrowth. An attempt on 2026-11-16 removed the
   anti-clip and was reverted; backups `out/backups/gen01-*-20261116`). Leading
   unconfirmed suspect: SPS Arousal Morphs length morph retriggering while
   arousal hovers 17-24 after a scene (`StartGrowingArousal=20` in
   `SPSArousalMorphs.ini`, source `D:\Modding\codex\SPS_RetractableAnatomy`).
   The user skipped the addon-off test and declined both fixes (raise the
   threshold, hysteresis in code). Do nothing on this unless asked. Vault note:
   `Debugging/SPS Shaft Regrowth After Reset`.
2. **Player owner handoff re-triggering.** Logs show `Player mesh changed;
   guarded erect-physics reconnect queued` every 7-10 s with no equipment change,
   plus `SMP resumed ownership of 6/6 ... reasserted`, and "NPC work is waiting
   for the player: player owner handoff pending" about every 11 s. Probably
   the arousal-morph addon's NiNode updates being read as mesh rebuilds
   (unproven). A held-angle memory (20 s) stops the angle restarting from the
   bottom; the trigger itself is not fixed. Also noted: SMP reset about every
   12 s in one 13:54-13:55 window on 2026-11-16, not investigated.
3. **NPC verification.** Test one and two managed NPCs in game: handoff, ramp,
   scene angle, soft pose hold, hysteresis. The Cicero `SOSBend-9`/`SOSBend4`
   loop was reported sorted by the user on 2026-11-23 on the 2026-11-22 scene
   check build (user report; the cause was not confirmed from the log). Split
   player and NPC physics gates is deferred until the gate-waiting cause is
   known.
4. **Gate checks still unrecorded.** Fresh-load automatic six-bone detection
   without Repair, OSL 2.9.3 query without the access violation, OSL MCM, TNG
   `Debug.SendAnimationEvent` errors, SOS AE bend crash, save/load boundaries,
   SOFTBODY scene transitions in each owner, the non-ASCII log path case, and
   other arousal providers and runtimes (SE/AE/VR).
5. **Predator SMP Head / HIMBO lip-throat report (unresolved).** Reporter's
   setup: 1.6.1170, 3BA, SOFTBODY, OStim, HIMBO, TNG. The zero shaft
   collision-margin multipliers in the SPS profiles plausibly reject some
   contacts. An XML-only margin test exists (`out/compatibility/predator-head-test-1`,
   ZIP `CA3F3CB8...`) and gave only partial improvement; the reporter's own
   working XMLs are still outstanding. The SOFTBODY PPA patch v3.0 supplies no
   dynamic shaft chain, so it must not replace SPS profiles. See
   `docs/PREDATOR_HIMBO_COMPATIBILITY_2026-09-14.md`.
6. **FOMOD natural-hang option** ("Use my SOFTBODY physics (natural hang)",
   `compat/SOFTBODY SPS Natural/`, unpublished): tuned for one large mesh,
   check on a normal mesh before releasing. The shaft-thigh collision
   experiment is not shipped. `Test-PackageGuards.ps1` did not finish in two
   minutes, so the guards are not verified for that option.
7. **Menu framework hygiene.** `CMakeLists.txt` fetches the menu example from a
   moving `master` (pinned locally by the `.sps-deps` override); the
   framework's wrappers have no null checks and SPS has no minimum-version check.

**Settle gate before the automatic soft reset (built 2026-11-16, NOT installed, not checked in game):**
idea 1 from the "needs a manual FSMP reset" discussion: SMP rebuilds its bodies from the skeleton as it is at
reset time, so the automatic reset (2.5 s after the handoff) may bake a pose that is still blending, while a manual
reset later does not. `RunSoftHandoffSMPReset` now waits until the six shaft bones' local rotations move less than
0.5 deg between two samples at least 400 ms apart, twice in a row (`ShaftPoseStable`), then resets; capped at 6 s,
after which it resets anyway. Log: `Soft SMP reset: shaft pose stable after N ms` or `still moving after 6 s,
resetting anyway`. Native only, transient statics, no INI/script/save change: safe for existing saves. Compare the
`Shaft sample` and stability lines against a manual reset. If the log shows the bones were already stable at +2.5 s
the cause is elsewhere (scale/morph baked at rebuild, or the global-vs-actor reset difference). DLL
`02E2007C795F62C1BAB13703277C10AF21A89471F37C99A05A3998D0B49A992D`, ZIP
`8F7279C25F63075E4108CBC7FD7CB39E26D8D0F55C22214A75E7404B535196EA`. Includes the logging sampler.
**Installed 2026-11-16 18:09 (Skyrim closed):** DLL, five PEX, five PSC, manifest and `SPSAPI.h` copied into the test
mod and hash-verified against the package (the PEX bytes changed only because the build recompiled the unchanged
PSC). INI, `meta.ini`, XMLs and the local CBPC overrides untouched. Rollback:
`out/backups/testmod-before-settlegate-20261116-180904` (48 files, count matches the mod). Not checked in game.
User reported the player settle gate "looks like it works" (2026-11-16).

**Managed NPC soft reset with the same settle gate (built 2026-11-16, NOT installed, not checked in game):** NPCs
had no automatic reset after a CBPC to SMP handoff, only the soft pose. Now, after a real CBPC to SMP handoff
(previous owner known and CBPC), `ManagedNpcRuntime` schedules one reset at +2.5 s (window 12 s). It waits until the
soft pose has been sent (`sentIndex == SOSBend-9`, or the pose channel) and the NPC's six shaft bones are stable
(`ShaftPoseTracker`, now shared with the player's `ShaftPoseStable`), capped at 6 s, then queues
`OwnershipPurpose::resetSoft`, which sends `SPS_FSMPBridge.SetActorOwnerV3` preparation 5 (new: SMP only,
`DynamicHDT.ResetPhysics(actor, true)` + barrier + 0.75 s + proof sample, nothing else). Ownership is not changed by
it; a reset that cannot dispatch uses `ResetPending()` so it never marks the owner unknown, and it is retried until
the window ends (then logged as skipped). Not scheduled when the owner was unknown (first manage) or when going
to CBPC. Logs: `Managed NPC ... soft SMP reset: shaft pose stable after N ms` / `still moving after 6 s`,
`requesting a soft SMP reset`, `soft SMP reset done; SMP proof n/n`, or `skipped: it could not run in time`.
Bridge version stays 9 like the player's preparation 5 (an old PEX would treat 5 as a plain SMP handoff; DLL and PEX
are paired by the manifest). Transient driver state only; no co-save, INI, API or script-property change: safe for
existing saves. Build: 0 errors, 0 warnings, tests pass, package validated. The `papyrus-reviewer` and
`save-safety-reviewer` subagents were not loadable in this session, so both reviews were done inline.
**Installed 2026-11-16 19:55 (Skyrim closed):** the same 13 files as the settle-gate install copied into the test mod
and hash-verified against the package (DLL `8EE4BBD4...3984`). INI, `meta.ini`, XMLs and local CBPC overrides
untouched. Rollback: `out/backups/testmod-before-npcreset-20261116-195554` (48 files, matches). Not checked in game.

**Public API V4 (built 2026-11-16, NOT installed, not checked in game):** requested by an addon author. New export
`SchlongPhysicsSwapper_GetAPI_V4` returns `InterfaceV4` (V3 layout unchanged + 5 members, 144 bytes; V1-V3 exports and
payloads untouched). Player only. (1) `GetActorSceneRole` gives Unknown/Receiving/Penetrating from OStim, SexLab, then
PPA, so it works without PPA; asking keeps the SexLab/OStim role queries running for 30 s even when the user's role
switching is off (decisions are still gated by the user's settings). (2) The in-scene flag already existed as V3
`IsActorInScene` and never depended on `ControlSource`; V4 extends it for the player to include an active PPA scene and
documents that `GetState.source` stays ExternalAPI during an addon hold. (3) `RequestRiseHold`/`ReleaseRiseHold`:
while a hold is active and SMP or an unknown owner has the bones, `Evaluate` returns before the arousal-driven
CBPC rise; scenes and a rise already under way are not delayed; duration 1-10000 ms (zero rejected, longer clamped),
cleared on game load. (4) `RegisterActorEventListener`: `ArousalChanged` fires from `ArousalController::QueueEvaluation`
in the same game-thread task and just before `Evaluate`, so a hold taken in the callback is in place when SPS decides;
`SceneChanged` fires from the poll when inScene or role changes. Capability bits 9-11. New tests: ABI offsets/sizes in
`SPSAPIContractTests.cpp`. Docs: `docs/MOD_AUTHOR_API.md`. Build 0 errors/0 warnings, tests pass, package validated.
No co-save, INI, Papyrus or V1-V3 behaviour
change (the V3 `IsActorInScene` result is only ever true in more cases): safe for existing saves. Consumers
(from the vault): SPS Arousal Morphs calls V3 `IsActorInScene` and
`GetActorArousal`, V4 `GetActorSceneRole`, the V4 rise hold and the `ArousalChanged`
listener, and V5 `GetAnyActorSceneRole` for NPCs. Its 2026-11-21 log showed V3/V4/V5
detected and role switching in SexLab scenes. Cumshot SPS uses V4 scene roles; its
log lines are not on record. Not seen in game: V5 `GetActorState`, the V5 NPC event
listeners, OStim roles, and NPC roles (the NPCs were never loaded nearby in the
logged runs).
**Installed 2026-11-16 20:02 (Skyrim closed):** the same 13 files copied into the test mod and hash-verified against
the package (DLL `2737AFC9...58E4`). INI, `meta.ini`, XMLs and local CBPC overrides untouched. Rollback:
`out/backups/testmod-before-apiv4-20261116-200228` (48 files, matches). Not checked in game.

**Public API V5, read-only NPC support (built 2026-11-16, NOT installed, not checked in game):** new export
`SchlongPhysicsSwapper_GetAPI_V5` (`InterfaceV5` = V4 + 4 members, 176 bytes). `GetActorState` (player or any loaded
actor; `validFlags` per field; physics only for the player and managed NPCs), `GetAnyActorSceneRole` (NPC role, cached
like `IsActorInScene`, first call NotReady), `Register/UnregisterNpcEventListener` (managed NPCs only; ArousalChanged and
SceneChanged from the NPC service tick via `SetManagedNpcTickSink`). No NPC physics control: user chose read-only.
NPC role needs new Papyrus: `SPS_PositionBridge` v4 to v5 (`GetActorSceneRole`, `ReadActorSceneRole`; SexLab P+ directional
test with the actor, OStim first) and `SPS_OStimBridge` v2 to v3 (`GetActorRole` using `OActor.GetThreadID`,
`OThread.GetActorPosition`, `OThread.GetScene`; signatures taken from the upstream OStimNG source via a fetch and
added to the build-only `OActor.psc` stub). The C++ expects position v5 and ostim v3, so a new DLL with old PEX files
makes NPC scene queries fail closed (logged version mismatch); the test mod keeps the OStim bridge only under
`Optional\OStim`, which the install does not touch. Save safety: global-function-only scripts, transient C++ state, no
co-save/INI change: safe for existing saves, but DLL and PEX must be installed together. Build 0 errors/0 warnings, tests
pass, contracts validated. The reviewer subagents were not loadable, so the
Papyrus and save-safety review was done inline.
**Installed 2026-11-16 20:48 (Skyrim closed):** the same 13 files copied into the test mod and hash-verified (DLL
`415093BF...DCCC`, PositionBridge v5 PEX included). INI, `meta.ini`, XMLs, CBPC overrides and `Optional\OStim`
untouched. Rollback: `out/backups/testmod-before-apiv5-20261116-204826` (48 files, matches). Not checked in game.
Then the test mod's `Optional\OStim` bridge (PEX and PSC) was updated to v3 to match the DLL and hash-verified; its
previous v2 files are in `out/backups/testmod-ostim-optional-before-v3-*`.

**Managed NPC limit back to 1, with an "Allow up to 8" toggle (built 2026-11-16, NOT installed, not checked in game):**
`kManagedNpcCapacity` is 1 again; new INI key `[NPCs] ManagedNPCUpToEight=false` (`Settings::managedNpcUpToEight`,
menu checkbox under Enable managed NPCs, with an amber performance warning when on, untested above two NPCs).
`ManagedActorRegistry` gained `SetCapacity`/`Capacity`; `TickManagedNpcs` applies the limit every tick. Lowering the limit
keeps NPCs already managed and only refuses new ones; the menu says how many to remove. The ownership ledger (8) and
request ledger (64) already fit eight NPCs; the public API text now says one by default. Registry test added. Build clean,
tests pass. Safe for existing saves: the new INI key defaults to
off and nothing else is stored.
**Installed 2026-11-16 (Skyrim closed):** the paired DLL, PEX, PSC, manifest and `SPSAPI.h` (plus the optional OStim bridge
files) copied into the test mod and hash-verified (DLL `66BFDC69...D777`). The test mod's INI was not touched, so the new
key is absent and defaults to off (limit 1). Rollback: `out/backups/testmod-before-npccap-*` (file count matched).
Not checked in game.

## Architecture and behaviour (current)

- **Ownership:** `SPS_FSMPBridge.SetPlayerOwnerV3(token, useCBPC, preparation,
  softBend)` runs one guarded transaction (reset, equipment release/reconnect,
  final ownership). Preparation: 0 ordinary, 1 full reset (`ResetPhysics(full)`
  plus barrier, then SMP on), 2 equipment reconnect, 3 SMP-off only, 4 reset plus
  soft-pose alignment (kinematic settle), 5 reset-only (`ResetPhysics` + barrier +
  0.75 s wait + proof sample, no CBPC stop or SMP enable). Bridge version 9. The
  DLL and PEX must stay paired (package guards pair the PEX hash).
- **Supersession:** a new opposite-owner decision marks the in-flight
  transaction superseded, invalidates uncertain ownership and cancels its token;
  the slot and lease are kept until the stack returns (or times out). A stale
  callback cannot commit ownership or run recovery. Queued manual tests keep
  priority over arousal polling and are withdrawn by scene/API/disable control.
- **Proof:** after an SMP enable the bridge calls `TogglePhysics(..., true)` a
  second time; FSMP reports each bone's dynamic state sampled before the call and
  leaves dynamic bones unchanged, so `SMP proof: N/N` is real proof of SMP. The
  opposite direction is unprovable (CBPC `StopPhysics` returns nothing, false
  entries are ambiguous), so SPS never claims CBPC release. The SMP-off
  maintenance probe runs only while policy wants CBPC and the owner is CBPC; a
  dynamic bone there is real drift.
- **Soft handoff after a fall:** SPS runs one full SMP reset (preparation 1,
  or 5 reset-only for the settle) 2.5 s after the handoff so no manual FSMP
  reset is needed; the resting pose is not replayed after that reset.
- **Angle:** timed ramp, one SOS event per visible stage, 18 even steps over the
  19 bound positions so Rise/Fall time are exact -9 to 9 travel times (15 s cap
  matches the 30 s setting clamp). `SOSBend-9` is the lowest stage; it primes
  every SMP-to-CBPC switch before a rise, and a fall holds CBPC until it is
  sent. The soft resting pose is `SOSFlaccid`, sent only when SMP owns the
  bones (it shows raised under CBPC or kinematic bones); a configured
  FlaccidBend is unchanged. If a snap returns at the end of a fall or the start
  of a rise, revert only the `flaccid=true` condition in `ApplyPosition`.
  Method combo: "Choose automatically" writes 2; the experimental pose angle
  (3) is a separate choice because the graph overwrites it.
- **Managed NPCs** (`src/runtime/ManagedNpcRuntime.cpp`): registry capacity 1 (8 with `ManagedNPCUpToEight`),
  one driver per actor, the shared Papyrus gate allows one handoff at a time.
  Per-NPC options saved by base record in
  `Data/SKSE/Plugins/SchlongPhysicsSwapper_NPCs.ini` (Erect at 60, Erect angle
  14, Rise with arousal off, Rise 3 s, Fall 5 s; fixed hysteresis 5, start
  arousal 20). Angle latches erect at `>= threshold` and drops at `< threshold - 5`.
  Rise/fall ramp follows the player's design, an NPC fall holds CBPC until the
  lowest stage (15 s cap), soft pose is held with `SOSFlaccid` after each SMP
  ownership change or finished scene, angle state is cleared while a scene is
  active. Ownership requests are released in the completion callback and
  completion runs as an SKSE task. Not replicated for NPCs: the
  `FlaccidAngleControl`, random erections, SexLab/OStim role angles, pose-angle
  method. Managed NPC state changes are logged (throttled to 10 s for repeats).
- **Basic (unmanaged) NPCs** follow OSL 2.9.3's SOS mapping (commit `b1f85b6`):
  `int(arousal)/4 - 14`, below -9 `SOSFlaccid`, cap `SOSBend9`, skipped in scenes,
  never repeated. Up to 8 nearby adult humanoids with a Gen01 node, 32 handles
  inspected per second, one actor serviced per 250 ms, default off, requires
  confirmation that OSL's own SOS control is disabled. Basic NPCs only receive SOS
  events, never a physics switch.
- **NPC gate:** `playerBusy` for NPC work excludes animating/relaxing (they do not
  hold the Papyrus gate); the reason is logged as `NPC work is waiting for the
  player: <reason>`.
- **Public API:** V1 ownership requests, V2 arousal and threshold read-back,
  V3 `GetActorArousal` and `IsActorInScene` (NPC scene state from a probe cache,
  up to 32 actors, about 1 s refresh). `IsActorSupported` stays player-only.
  ABI frozen in `tests/SPSAPIContractTests.cpp` (V3 sizeof 104, V4 144, V5 176). Docs
  `docs/MOD_AUTHOR_API.md`.
- **SexLab futa:** per-actor "Make futa in SexLab" / "Undo futa" using P+
  `TreatAsFuta` / `ClearForcedSex` for the player and managed NPCs (any actor
  SexLab lists as sex 0 or 1 with an SOS schlong). SPS's first co-save record
  (`SPSW` / `FUTA` v1, up to 256 form IDs, remapped on load, cleared on revert)
  re-applies it at most every 10 s per actor. `SPS_SexLabBridge` v5 resolves the
  actor through the native `ResolveIdentityActor` (FE/FF FormIDs work). Ignored
  by older SPS and dropped if SPS is removed.
- **OSL arousal test sliders** (player and each basic/managed NPC) write real OSL
  values through `OSLArousedNative.SetArousal(Actor, Float)` and wait until the
  Journal/MCM closes. OSL `GetArousal` decays a set value toward a baseline
  unless `SetActorArousalLocked(actor, true)` is applied after setting it; the
  revert is not fixed (epsilon and the installed commit unverified).
- **Scene roles:** every valid SexLab result is honoured (including a temporary
  unknown); PPA is used only when the SexLab role query is unavailable. SOFTBODY
  3.37.2 reload events are recovered automatically by reasserting the already
  selected owner when `HDT SMP Object - Simple.esp` is loaded. Disabling SPS
  waits for the in-flight transaction and restores plain SMP.
- **Logging:** `SchlongPhysicsSwapper.log` is opened with a wide-path stream and
  CommonLib's logger is disabled (non-ASCII Documents paths crashed startup).
  The experimental pose-angle map is locked and its natives queue bone writes on
  the game thread. FSMP versions and legacy file checks are cached.
- **Release gating:** CMake and tools: `pwsh ./tools/Build-Release.ps1` (output
  redirected to a file; PowerShell 7 only; piping to `Select-Object -First N`
  kills the build), `tools/Test-ReleasePackage.ps1`, `tools/New-ReleasePackage.ps1`.
  Optional pex audit/diff/info with `D:\Modding\codex-references\tools\skqe-pex\pex.exe`
  on copies only (documented in `docs/DEVELOPMENT.md`; compiled PEX headers carry
  the user/computer name `joel`). `pex audit` on the six bridge PEX copies: 0
  findings, 55 functions.

## Physics profiles and local tuning

- **FOMOD physics choices:** keep the user's own XML, the personal SPS six-bone
  profile, or the combined SPS + SOFTBODY profile (Goutou credited; must remain
  free and out of paid compilations), plus the unpublished natural-hang option.
  The personal profile's collision name was corrected from `scotumcollision` to
  `scrotumcollision`; the test mod uses the combined option, which never had the
  typo, so do not replace it with the personal one. SOFTBODY's stock genital XML
  gives Gen02-Gen06 zero gravity, so the shaft can look rigid even when SMP is
  correctly selected.
- **Private SOFTBODY override** (`D:\Modding\mods\SOFTBODY True Soft Shaft - SPS`,
  above `SOFTBODY Custom Physics - Joel` and base SOFTBODY; it wins
  `MaleGenitals*.xml` over the SPS test mod). It is tuned for the user's large
  shaft mesh. Current intent: balls free-hang (gravity 1.0, angular stiffness
  near 0, linear stiffness GenScrot 290, L/R 250); shaft Gen01 at its original
  anti-clip values; Gen02-06 graduated, looser toward the tip with pitch lower
  limits -0.30..-0.38; the experimental shaft-thigh collision exclusion removal
  is UNVERIFIED (FSMP source copy lacks the pairing code; both shapes are
  per-triangle and may not collide at all). Backups for every step are in
  `out/backups/*-20261114` and `*-20261116`.
- **Balls under SMP:** `CBPCMasterConfig_SOSScroum.txt` in the test mod has the
  `NPC GenitalsScrotum [GenScrot]` line commented out so only SMP drives GenScrot;
  CBPC keeps `GenitalsScrotumLag`. `CBPConfig_ZZZ_SchlongPhysicsSwapper_Scrotum.txt`
  tunes only that lag node. Both are local to the test mod, not shipped; delete to
  revert. SPS owns only the six shaft bones.
- **Gen01 and balls** are coupled: anything that loosens the Gen01 pitch limits
  makes the shaft clip into the balls (no shaft/ball collision exists in the
  mesh). If it clips, raise ball hang or Gen01 stiffness before loosening.

## Compatibility facts

- **FSMP 4.1.1 first-person skeleton:** actor toggle/reset calls can stop at the
  first-person skeleton before reaching the body. An isolated candidate patch
  exists (`out/diagnostics/fsmp-actor-api-20260907`, `docs/FSMP_ACTOR_SELECTION_2026-09-08.md`)
  but the user rejected modifying other mods; it remains uninstalled.
- **FSMP global reset vs actor reset:** the global `smp reset` also resets the
  physics world; the actor API reloads that actor's meshes only. Never assume
  equivalence. The kinematic-settle path (preparation 4) and preparation 5 are
  the SPS attempts to match it.
- **Menu framework 3.23.0:** all 22 functions SPS uses exist; Begin/EndDisabled
  are balanced; no duplicate widget IDs.
- **SKSE Menu / OSL / OStim:** OSL `GetArousalExt` needs a real `RE::Actor*`
  (FormID per docs but pointer in the ABI; an earlier crash). Optional OStim
  stays an experimental, deliberately installed bridge.

## Test and deployment procedure

- Install only with Skyrim closed. Back up the whole test mod to
  `out/backups/testmod-before-<name>-<timestamp>`, copy the DLL, the six paired
  PEX files, `Source\Scripts` PSC files, `SPSBuildManifest.json` and
  `Mod Author API\SPSAPI.h`, then hash-verify against the package. Do not touch
  INI, `meta.ini` or the physics XMLs.
- Never claim in-game success from build, package or log counters alone;
  distinguish "command executed" from "observed motion".

## Documents

`docs/DEVELOPMENT.md`, `docs/IMPLEMENTATION.md` (including engine and dependency
facts), `docs/NPC_SUPPORT_WORKFLOW.md`, `docs/MOD_AUTHOR_API.md`, `docs/SUPPORT.md`,
`docs/FSMP_ACTOR_SELECTION_2026-09-08.md`, `docs/PREDATOR_HIMBO_COMPATIBILITY_2026-09-14.md`,
`docs/SOSAE_4_POSE_ANGLE_2026-11-01.md`, `docs/BEND_REGRESSION_HANDOFF_2026-11-02.md`
(SOS AE 4.0 evidence only), `docs/DEPENDENCY_API_REMOVAL_PLAYBOOK.md`,
`docs/WORKSPACE.md`, `docs/CBPC_AUTHOR_REQUEST.md` (unsent). Historical increment
records, audits, handoffs and release notes were removed on 2026-11-19; they
remain in git history and the GitHub releases.
The user-owned `docs/USEFUL_WEB_LINKS.txt` is tracked and must stay unmodified.
