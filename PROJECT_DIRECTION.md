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

## Current status (2026-11-26)

Per-build histories, hashes and install logs before this date were moved out of
this file on 2026-11-26 (backup `out/backups/PROJECT_DIRECTION-before-compact-20261126.md`;
also in git history and the vault dev logs).

- **Branch:** `codex/codebase-rework`. The 2.3.0.5 work below was committed on 2026-11-26 as
  one checkpoint on top of `558a7ef` (2.3.0.4 reverted to 2.3.0.3); see `git log`. Not pushed.
- **Soft recovery coordinator moved to a controller (2026-11-26, built, not installed):**
  `src/controllers/SoftRecoveryCoordinator.{h,cpp}` holds the round state and decisions
  (merge, quiet period, scene settle, max and long waits, dispatch-failure bound) and the
  dependency-quirk follow-ups (SMP proof re-checks, clean-bake rule and retries, CBPC release
  once per load). `plugin.cpp` only supplies inputs and runs the transaction. Behaviour is
  unchanged. New `SPSSoftRecoveryTests` (61 checks, run by every build). DLL `EE3AAC7C...9973`,
  ZIP `E950DA04...78DE`. The installed test mod still has the audit-fix build (`B3B22AE0`).
- **Published:** GitHub releases v2.0.0, v2.0.1, v2.3.0.2, v2.3.0.3 and v2.3.0.4 (2.3.0.4
  was reverted in code; whether to withdraw its release is not decided). Nexus mod 188174
  (uploads are left to the owner).
- **Local build 2.3.0.5 (audit fixes, built 2026-11-26, installed 2026-11-26 17:11, not checked in game):**
  new `SPS_IdentityBridge` v1 holds the identity reads (SOS addon, rank; SexLab sex through
  the new `SPS_SexLabBridge.GetActorSex`) so `SPS_SexLabBridge`, which declares SexLab types,
  is no longer called on games without SexLab (2.3.0.3 rule). Bridges: FSMP v12 (unused
  player preparations 4 and 5, the unused softBend 100 branch and the four unguarded legacy
  entry points removed; preparation 2 no longer overwrites its SMP-off sample), SexLab v9
  (`DescribeActor` moved out, unused `IsPlayerActive` removed), position v7 (unused
  `SetPlayerPoseAngle` removed). The player path refuses `resetSoft` (NPC-only). Shipped
  SOFTBODY SPS XMLs: 54 duplicate no-collide tags per file and a nonexistent bone entry
  removed (tag sets and weight thresholds unchanged). Seven bridges, fifteen paired hashes.
  A missing identity bridge logs SPS-013 once. Build 0 warnings, tests and package guards
  pass; DLL `B3B22AE0...562A`, ZIP `C158FE29...822C`.
- **Installed in the test mod** (`D:\Modding\mods\Schlong Physics Swapper`): the audit-fix
  build, 2026-11-26 17:11 with Skyrim closed, 17 files hash-verified (DLL, seven PEX/PSC pairs,
  manifest, `SPSAPI.h`), DLL `B3B22AE0...562A`, rollback
  `out/backups/testmod-before-auditfixes-20261126-171120` (52 files). INI, `meta.ini`, XMLs and
  local CBPC overrides untouched. Not checked in game. The disabled
  MO2 mod `Schlong` holds old bridges and its own DLL and must stay disabled.
- **2.3.0.5 content so far** (all uncommitted, none checked in game unless stated): menu
  redesigned into six pages (Overview, Erection, Angles, Scenes, NPCs, Help) with a draggable
  arousal slider; managed NPC goes erect when penetrating in a scene (scene and role checks
  alternate); OStim role read from actions (OStim bridge v4); player node updates ignored
  while an ownership transaction is pending (reset-chain fix); shipped shaft XMLs evened out;
  soft recovery coordinator with clean rest bake (preparation 6); bridge v11 reliability fixes
  (proof-driven follow-up, CBPC release after load, safe timeouts, armour-slot filter, NPC
  equipment re-check); the audit fixes above.
- **Rules from recent fixes:** never declare an optional mod's script type in a bridge that is
  called without that mod (2.3.0.3 crash). The TNG and SOS angle go native first
  (`NotifyAnimationGraph`), Papyrus only as fallback (2.3.0.2).
- **Private physics override:** `SOFTBODY True Soft Shaft - SPS` (the owner's, wins
  `MaleGenitals*.xml`) has had several test-only passes (realism retune, collision margins)
  and further edits by other sessions on 2026-11-25 (backups `trueshaft-before-*` in
  `out/backups`). The shipped presets do not include those test passes.
- **Verified in game by the user:** player switching, soft resting length,
  scene/equipment recovery (2.0.0 gate, 2026-09-13); rise/fall even-step ramp
  (2026-11-04); basic OSL NPC posing, OSL test sliders (2026-10-31); the 2026-11-23
  equipment reset and rise priming build ("everything I tested works", user report).
- **Not verified in game:** managed NPC support beyond the Cicero loop report, male futa,
  API V3-V5 NPC reads, the soft recovery coordinator, bridge v11 and the audit fixes.

## Open items

1. **Shaft regrowth after full flaccid (back, owner report 2026-11-25).** In the
   2026-11-25 19:27-20:27 log every rest rebuild that started from the hanging
   pose (19:50:09, 19:58:37, 20:10:53, 20:19:50) sampled Gen01 15.2, Gen02 4.8,
   Gen06 5.1 deg at +0.8 s and settled at Gen01 20.1, Gen02 9.7, Gen06 0.2 deg by
   +9.8 s (scale 1.00); rebuilds that started already settled did not move. Shaft
   joints have zero linear limits, so the bones cannot stretch: the "longer" look
   is the shaft swinging out and straightening, a mesh morph, or both. Two
   candidates, neither proven: (a) the SOSFlaccid resting clip that SPS sends right
   after the rebuild (built from SOSBend-9) moves the kinematic GenBase the chain
   hangs from; (b) SPS Arousal Morphs started "Progressive growth (arousal 22.2,
   target 4%)" in the same second as the 19:58 fall. The override XML was changed
   again after that log (backups `trueshaft-before-ballsmp-20261125-215858`,
   `trueshaft-before-shaftgravity-20261125-220739`), so the current file is untested
   for this. The 2026-11-25 diagnostics build logs `Player resting pose sent:
   SOSFlaccid`, a `span=` (Gen01-Gen06 distance) on every shaft sample, and samples
   for 15 s after the console closes (to compare a manual `smp reset`). Next test:
   one fall with Arousal Morphs off. Earlier history (reported fixed 2026-11-22,
   cause not recorded): The shaft grows a few seconds
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
2. **Player owner handoff re-triggering.** *Update 2026-11-24 (owner log 20:54-21:19, 2.3.0.4):*
   10 `Player mesh changed; refreshing its SMP physics` and 4 erect reconnects in 25 min, in
   chains such as 21:11:04 -> 05.5 -> 12.5 -> 23.5 and 21:13:21 -> 28.7 -> 32.7: each SPS reset
   transaction takes 4-6 s, longer than its own 3 s `ignoreNodeEventsUntilMs`, so the NiNode
   update the reset itself causes lands after the window, waits for the transaction
   (`nodeRefreshDueMs` retried every 500 ms while pending) and fires another reset as soon as it
   completes. Each reset re-bakes SMP's rest frame from the pose at that moment, a likely source
   of the owner's "bones sometimes look broken". Resting samples on the shipped SOFTBODY SPS
   preset: Gen02-06 local x about 9.7, 5.3, 4.7, 5.2, 0.2 deg with Gen02 y -4.0 deg, i.e. most
   joints near their +0.12 rad (6.9 deg) upper stop, an uneven (segmented) resting curve. Not
   *Fixed in 2.3.0.5 (built, not installed, not checked in game):* `NiNodeSink` ignores player
   node updates while the player's ownership transaction is pending (completion already extends
   the ignore window; armour changes are still caught by `EquipEventSink`). Shaft XML evening-out
   in all six shipped SOFTBODY SPS / No Body Collision XMLs and, with the owner's approval, the
   private `SOFTBODY True Soft Shaft - SPS` override (backup
   `out/backups/softbody-truesoft-before-smooth-20261124-215250`; now identical to the shipped
   preset): Gen02 joint upper x 0.12 -> 0.16, equilibrium x -0.018 -> -0.08 (off its stop) and
   y 0 -> +0.072 (cancels the -4.1 deg sideways twist baked from the idle pose; FSMP builds joint
   zero from the pose at build time, `calcFrame` with `m_currentTransform`); Gen03-05 upper x
   0.12 -> 0.18; Gen06 upper x 0.12 -> 0.20, equilibrium x -0.018 -> +0.08 (tip was straight).
   Values derived from the logged rest samples, not simulated; verify with the `Shaft sample`
   lines (aim: similar x per joint, Gen02 y near 0). 2.3.0.5 DLL `7415C467...6D03`, ZIP
   `54FCB067...6708`. *Tip follow-up (owner: "make bone 5 and 6 follow each other better"):*
   the Gen06 joint was the loosest in the chain, so the tip moved on its own. Gen06-Gen05:
   stiffness 30/55/55 -> 50/90/90, damping 24/28/28 -> 36/40/40, limits x -0.38/0.20 ->
   -0.28/0.16 and y/z +-0.26 -> +-0.16, equilibrium x 0.08 -> 0.04; Gen05-Gen04: stiffness
   38/70/70 -> 48/85/85, damping 26/30/30 -> 32/36/36, y/z limits +-0.24 -> +-0.20. Same six
   shipped XMLs and the private override (backup `out/backups/softbody-truesoft-before-tip-20261124-230512`);
   active at the next load or SMP reset; not checked in game. *Realism retune (owner: "realistic
   and plausible rotation, weight movement and gravity"), TEST ONLY in the private override
   (backup `out/backups/softbody-truesoft-before-realism-20261124-230903`); the shipped presets
   are NOT changed until the owner confirms it in game:* shaft bones had half gravity
   (gravity-factor 0.50-0.60, scrotum 1.0), a 4x mass taper (1.04 to 0.24, very light tip),
   inertia far above mass (2.5-1.5, so rotation lagged), and heavy body drag (0.70/0.78). Now
   gravity 1.0; near-uniform mass with a slightly heavier glans (0.75, 0.65, 0.60, 0.55, 0.52,
   0.58); inertia about 1.4x mass (1.05 to 0.74); body drag 0.32/0.40 (Gen01 0.35/0.42, like the
   scrotum's 0.40/0.45); joint stiffness raised and smoothed for the extra weight (x / sideways:
   Gen01 190/260, Gen02 120/180, Gen03 96/150, Gen04 80/130, Gen05 68/115, Gen06 60/105) with
   damping about 0.65x stiffness for settling without bounce; up-stops x 0.20-0.22 so gravity
   sets the curve, not the stops. Gen01 limits and anti-clip equilibrium, the Gen02 sideways fix
   and the other equilibria are unchanged. Expect more hang and a weighted swing with
   follow-through. *Second pass (owner: does not sit off the balls; tip must follow Gen05
   again), backup `out/backups/softbody-truesoft-before-lift-20261124-231146`:* full gravity
   pushed Gen01 onto its droop stop. Gen01 joint: x droop stop -0.36 -> -0.52 (gravity is +x),
   equilibrium -0.68 -> -0.74, stiffness 190/260 -> 280/300, damping 125/150 -> 175/170.
   Tip: Gen06 joint 60/105 -> 92/150 (damping 40/68 -> 60/96), limits x -0.24..0.16, sideways
   +-0.12; Gen05 joint 68/115 -> 78/125 (damping 44/74 -> 50/80); glans mass 0.58 -> 0.50,
   inertia 0.80 -> 0.72. Still test-only in the private override. *Third pass (owner: the balls
   now move wrong), backup `out/backups/softbody-truesoft-before-calm-20261124-231439`:* the
   scrotum values were verified unchanged, so the cause is the shaft hitting them (full gravity
   and low drag kept it moving against ball bones of 0.28). Shaft mass, inertia, joint stiffness
   and joint damping all scaled by 0.75 together (same hang shape, 25% less momentum; masses now
   0.56/0.49/0.45/0.41/0.39/0.38), and body drag raised from 0.32/0.40 to 0.50/0.58 (Gen01
   0.52/0.60; the original preset had 0.70/0.78). Gravity stays 1.0. *Fourth pass (owner: balls
   do not swing like before; reduce shaft-ball collision), backup
   `out/backups/softbody-truesoft-before-collision-20261124-231646`:* the shaft collision shape
   `MaleGenitalsCollision` (tag penis) had margin 4 and penetration 0, an inflated hard surface
   that shoved and pinned the balls (`scrotumcollision`). Now margin 1.5, penetration 0.6; ball
   shape penetration 0 -> 0.4 (margin stays 0.03). Collision kept (not excluded) so the shaft still
   cannot sink through the balls. *Fifth pass (owner: reduce the shaft reacting with the balls
   further), backup `out/backups/softbody-truesoft-before-collision2-*`:* shaft collision margin
   1.5 -> 0.8, penetration 0.6 -> 1.0; ball shape penetration 0.4 -> 0.6. *Sixth pass
   (owner, 2026-11-25: "reduce it to 0.05"):* shaft collision margin 0.8 -> 0.05 (penetration
   1.0 and the ball shape unchanged), backup `out/backups/softbody-truesoft-before-collision3-20261125-165200`.
   Earlier note: Logs show `Player mesh changed;
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
   `compat/SOFTBODY SPS Natural/`, unpublished). *2026-11-25 audit:* neither that folder
   nor a natural-hang entry in `fomod/ModuleConfig.xml` exists in the working tree, so
   this item is stale until the option is found or dropped. Originally: tuned for one large mesh,
   check on a normal mesh before releasing. The shaft-thigh collision
   experiment is not shipped. `Test-PackageGuards.ps1` did not finish in two
   minutes, so the guards are not verified for that option.
7. **Menu framework hygiene.** `CMakeLists.txt` fetches the menu example from a
   moving `master` (pinned locally by the `.sps-deps` override); the
   framework's wrappers have no null checks and SPS has no minimum-version check.

## Architecture and behaviour (current)

- **Ownership:** `SPS_FSMPBridge.SetPlayerOwnerV3(token, useCBPC, preparation,
  softBend)` runs one guarded transaction (reset, equipment release/reconnect,
  final ownership). Preparation: 0 ordinary, 1 full reset (`ResetPhysics(full)`
  plus barrier, then SMP on), 2 equipment reconnect, 3 SMP-off only, 6 clean soft rest
  bake (SMP off on the six bones, resting clip, 0.9 s, `ResetPhysics` + barrier, SMP on),
  7 CBPC release only (after a load). Preparations 4 and the player's 5 were removed in
  v12; `SetActorOwnerV3` keeps preparation 5 (reset only) for managed NPCs. Every
  reset-barrier wait is bounded (10 s). Bridge version 12. The
  DLL and PEX must stay paired (package guards pair the PEX hash).
- **Timeouts (2026-11-25):** a stack that was queued but never entered is retired after
  5 s (player) or 10 s (NPC) and cannot enter afterwards; a stack that has entered is left
  to finish up to 20 s, because a cancel between engine calls could leave the shaft half
  handed over. NPC ownership tokens stay valid for those 20 s and are revoked on expiry.
- **Proof follow-up (2026-11-25):** an SMP handoff with proof below 6/6 gets at most two
  plain re-checks through the soft coordinator (no reset), then SPS-022; a clean rest
  rebuild needs 6/6 dynamic at its SMP-off step (unless the previous bake aborted) and
  6/6 after, else at most two more rebuilds. Reason: FSMP's actor calls can land on the
  first-person skeleton (see Compatibility facts).
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
- **Soft recovery coordinator (2.3.0.5, built 2026-11-25; installed 2026-11-25 17:51 with Skyrim closed, 15 files hash-verified, DLL `FF8AD380...FE52`, rollback `out/backups/testmod-before-coordinator-20261125-175100`; the disabled MO2 mod `Schlong` still holds an old v9 bridge, so it must stay disabled; not checked in game):**
  while the player is soft, every recovery trigger (fall to soft, scene end, soft confirmation,
  external SMP reset, mesh or outfit change, game load, post-switch check, manual repair or soft
  test) posts a request to `RunSoftRecoveryCoordinator`, which runs last in each tick. A rebuild
  (clean rest bake, `OwnershipPurpose::bakeSoft`, bridge preparation 6) waits until no SexLab
  (override on, state valid) or OStim scene is active and PPA has let go for 2 s, and no trigger
  arrived for 1.5 s (at most 10 s after the scene was over); manual repair and the soft test skip
  the scene wait. A plain ownership check (confirmSoft) may run during a scene after the quiet
  period. One transaction per round; a bake also covers pending checks. Preparation 6: CBPC
  StopPhysics on the six bones, SMP off (kinematic), the soft stage clip (always a bend stage,
  SOSBend-9 for an unconfigured rest, because SOSFlaccid shows a raised pose on kinematic bones),
  0.9 s, full `ResetPhysics` + barrier (FSMP rebuilds new dynamic bodies from that pose), 0.4 s,
  proof sample. An unconfigured SOS rest then gets SOSFlaccid natively once SMP owns the bones.
  Failed bakes retry at most twice (count decays after 2 min); three synchronous dispatch
  failures drop the round; a wait over 60 s logs SPS-019. Logs: `Soft physics recovery: ...
  (N requests merged)`, `Soft SMP rest rebuilt from the resting pose; SMP proof n/6`. Removed:
  `RunSoftHandoffSMPReset`, `RefreshSMPAfterPlayerMeshChange`, `ShaftPoseStable` (player). CBPC
  recovery and NPCs unchanged. Bridge v9 -> v10 (DLL and PEX must ship together). Papyrus and
  save-safety reviews: safe for existing saves; their logic findings (scene time counted in the
  max wait, double bake on outfit changes, ungated SexLab state, scene deferral of checks and
  manual repair, unbounded synchronous failures, missing CBPC stop, SOSFlaccid on kinematic
  bones, pose recorded without a clip) are all addressed. DLL `FF8AD380...FE52`, ZIP
  `373DCBDD...31C5`.
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
  re-applies it at most every 10 s per actor. `SPS_IdentityBridge` (identity reads)
  and `SPS_SexLabBridge` (futa writes) resolve the actor through the native
  `ResolveIdentityActor` (FE/FF FormIDs work). Ignored
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
  first-person skeleton before reaching the body. *2026-11-25:* the order is not stable:
  FSMP re-sorts skeletons every frame, both player skeletons get distance 0 and tie
  (`ActorManager.cpp` 1056-1058, 475-477), and the sort is not stable, so a call can
  reach either one; upstream master is unchanged. Draft upstream issue (unsent):
  `docs/FSMP_UPSTREAM_REPORT_DRAFT.md`. A manual `smp reset` reloads every skeleton
  and resets every system, so it always reaches the body. An isolated candidate patch
  exists (`out/diagnostics/fsmp-actor-api-20260907`, `docs/FSMP_ACTOR_SELECTION_2026-09-08.md`)
  but the user rejected modifying other mods; it remains uninstalled.
- **CBPC 1.7.3 Start/StopPhysics** only set a flag in a map keyed by actor FormID and
  node name (`config.cpp` 3421-3435); they never rebind nodes. CBPC clears the whole map
  on every PostLoadGame and when the Main Menu opens, so CBPC runs on the shaft after
  each load until stopped again (SPS now releases it while the first decision waits).
  SPS's `ZZZ` CBPC map has no actor condition and CBPC simulates nearby actors, so
  unmanaged NPCs with an SMP schlong probably get both engines on Gen01-06 (not seen in
  game; not changed).
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
  `out/backups/testmod-before-<name>-<timestamp>`, copy the DLL, the seven paired
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
`docs/WORKSPACE.md`, `docs/CBPC_AUTHOR_REQUEST.md` (unsent),
`docs/FSMP_UPSTREAM_REPORT_DRAFT.md` (unsent). Historical increment
records, audits, handoffs and release notes were removed on 2026-11-19; they
remain in git history and the GitHub releases.
The user-owned `docs/USEFUL_WEB_LINKS.txt` is tracked and must stay unmodified.
