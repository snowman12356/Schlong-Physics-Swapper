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
