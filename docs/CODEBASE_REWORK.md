# SPS codebase rework

## Goal

Make SPS safer to change and ready for managed-NPC support without changing
the player physics behaviour that already works.

## Starting point

The native plugin currently contains more than 4,000 lines in one source file.
Settings, state decisions, Papyrus calls, physics ownership, position changes,
recovery, scene integration, diagnostics, MCM rendering and SKSE lifecycle
events all share global state. That coupling makes a small fix capable of
affecting unrelated systems and leaves most behaviour impossible to unit test.

## Rules for the rework

- Preserve existing settings, defaults, INI keys and saved values.
- Preserve player physics, arousal, scene, angle and recovery behaviour.
- Move code in small compiling steps; do not replace the controller wholesale.
- Put game-independent policy in modules that can be unit tested.
- Keep Skyrim, Papyrus, FSMP and CBPC calls behind narrow runtime boundaries.
- Do not begin managed-NPC support until the player controller passes the
  regression checklist after the extraction.

## Target structure

1. **Core** - settings, owner decisions, scene policy and timer/state models.
   This layer has no Skyrim dependency and is covered by unit tests.
2. **Runtime adapters** - Papyrus dispatch, FSMP/CBPC ownership, SOS/TNG angle
   backends and SKSE event registration.
3. **Controllers** - player state, arousal, scenes, recovery and position.
4. **Presentation** - MCM pages, diagnostics and support reports. This reads
   controller state but does not decide physics ownership.
5. **Actor management** - a later layer for player plus managed NPC contexts;
   it will reuse the tested core rather than duplicating player logic.

## Delivery phases

### Phase 1 - protected foundation

- Extract the complete settings model.
- Extract normal SMP/CBPC decision policy.
- Add build-time unit tests for modes, threshold and hysteresis behaviour.
- Record the player regression checklist.

### Phase 2 - runtime boundaries

- Isolate Papyrus readiness and dispatch.
- Isolate the staged FSMP/CBPC ownership transaction.
- Return explicit queued/completed/failed results instead of treating a queued
  Papyrus call as proof that ownership changed.

### Phase 3 - controllers

- Move arousal provider state and polling into one controller.
- Move SexLab/OStim role policy into one scene controller.
- Move angle animation and recovery timers into a position controller.
- Move armour/load recovery into a bounded recovery controller.

### Phase 4 - UI and diagnostics

- Make MCM pages consume snapshots and send commands through controller APIs.
- Centralise error codes, activity records and support-report formatting.
- Remove duplicate UI-side state application.

### Phase 5 - NPC-ready architecture

- Introduce per-actor state contexts.
- Keep the player as the first and always-managed context.
- Add nearby/selected NPC management with strict limits and event-driven work.
- Bridge unmanaged NPCs back to standard SOS arousal behaviour.

## Regression gate

Every phase must build with `/W4`, pass the core unit tests, validate the FOMOD
and preserve these in-game paths: load soft, load erect, 0-to-100 and 100-to-0
arousal changes, armour equip/unequip in both states, manual soft/erect, angle
application, save switching, SexLab, optional OStim and troubleshooting repair.

## Current implementation state - 2026-09-01

The rework has continued from commit `29a8e70`; it was not restarted. The
current development tree now has these boundaries:

- Phase 1 is complete. Settings/storage and owner/scene decision policy are in
  the game-independent core and are covered by the build-time regression suite.
- Phase 2 implementation is complete. Papyrus readiness and SOS/FSMP/CBPC calls
  are behind runtime adapters. The version-2 FSMP bridge returns only after its
  ordered handoff stack completes, and `PhysicsOwnershipController` keeps the
  queued/completed/failed transaction, timeout and stale-callback generation.
  A queued call is no longer published as the selected owner.
  The 2.0.0 boundary hardening also resolves position, SexLab and legacy OSL
  player references inside Papyrus bridges; current OSL uses its native export.
  SPS pauses background dispatch while the Journal/MCM is open.
- Phase 3's player state extraction is complete enough for its regression gate.
  Arousal and scene controllers remain intact; position policy, animation state,
  bounce/failure recovery, due-timer claims and actor-scoped recovery resets now
  have controller APIs and snapshots. The claims are tested as one-shot actions
  and retain queued soft/erect work until ownership and intent agree. Skyrim
  event actions remain in the plugin entry point so the already-working load,
  armour and scene order is not rewritten before in-game validation.
- Phase 4 has a central activity log, diagnostics scanner and support-report
  formatter. Home, appearance, troubleshooting and advanced UI paths consume
  immutable ownership, position and recovery snapshots and send actions through
  existing command functions. Splitting the remaining ImGui page layout into
  separate files is cosmetic and is not a prerequisite for the player
  regression gate.
- Phase 5 has only the reusable per-actor context and tested controllers. The
  player (`0x14`) remains the sole managed context. NPC discovery, persistence
  and runtime management are intentionally not implemented until every player
  regression path below passes in game.

The authoritative in-game checklist is also recorded in
`RELIABILITY_AUDIT_2026-08-25.md`. In particular, the new completion-aware
handoff must be exercised under load/save, arousal, equipment, manual repair,
SexLab and optional OStim timing before managed-NPC work begins.
