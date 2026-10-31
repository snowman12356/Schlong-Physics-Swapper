# Basic NPC runtime test candidate

Continues `codex/codebase-rework` at baseline `103e402`, preserving the existing
uncommitted NPC work. The user authorized implementation and confirmed the
remaining player baseline checks. This is Stage 2 of the
[workflow](NPC_SUPPORT_WORKFLOW.md), before full managed NPC physics.

## Candidate scope and choices

Version **2.1.0.1 - NPC basic test** has its own package identity. Published
2.0.1 must retain SHA-256
`B5056B31BDF2ECB1D85D9067FBEB856266DAC5C54320E7E9522DAFC6C10CEB4A`.

- Basic behavior is off by default. The NPCs page requires confirmation that
  the user disabled OSL's own SOS control, then a separate enable checkbox.
  Global SPS disable also stops it. These settings are shared across saves;
  the transient actor list is not saved.
- The first live scope uses OSL numeric arousal and the source-backed legacy
  SOS event mapping already implemented in `BasicNpcPolicy`. Other providers
  and the independent SOS-state fallback remain pending. The legacy OSL
  installer option and its version restriction are unchanged.
- At most eight loaded, living, enabled adult humanoid actors within 3000
  game units are tracked, excluding the player. The race must have the
  `ActorTypeNPC` keyword and the 3D must contain `NPC Genitals01 [Gen01]`.
  These are positioning prerequisites, not proof of an equipped addon or SMP
  mesh. Actors with a compatible skeleton but no addon can occupy test slots.
  An event completing does not demonstrate visible motion.
- Discovery checks at most 32 high-process handles per second until the eight
  slots are filled. Existing entries are checked for availability each tick.
  One actor receives a turn per worker wake (250 ms); a 25 ms frame tolerance
  avoids losing turns to task/frame jitter. Queries have a one-second retry
  floor, position writes two seconds. The round-robin cap makes query cadence
  up to two seconds per actor. No world-wide form scan, persistent quest or additional worker exists.
  Eight is a development cap, not a measured final performance limit. An actor
  outside the cap receives no basic behavior from SPS in this test.
- Basic actors have cancellable lifetime state, separate from the dormant
  full-management registry and `ActorContext`. Both owners now share a process
  lifetime generation allocator, preventing the same FormID/generation from
  aliasing across different owners or reconstructed registries.
- The existing update worker keeps the player's configured polling interval
  and services NPCs between player ticks. Player handoffs, position transitions
  and manual actions block basic writes; all writes retain the shared operation
  gate. The basic path never starts, stops or resets a physics engine.
- Scene/arousal freshness and settling are unchanged. Timestamps refer to
  request start, not callback arrival. Scenes are checked again immediately
  before output. Missing bindings, stale data and unknown membership block
  changes. Optional OStim requires its matching helper when OStim is present.
- Request slots expire locally after ten seconds of active time and back off
  without releasing the real bridge stack or creating fresh observations.
  Bridge records remain bounded until the actual callback retires. Root 3D,
  handle, FormID, lifetime and request deadline are checked before accepting
  work/results. Object loaded/unloaded events revoke the actor lease without
  adding event tasks to an unbounded queue.
- Disable revokes permission immediately, including from the menu thread.
  Re-enable creates new permission and actor lifetimes. Load/new-game reset
  invalidates basic work before the existing bridge/gate reset. Removing a
  basic entry does not force-release an executing stack.
- Disabling stops further commands and leaves the last pose. It does not claim
  to restore an unreadable prior angle or change OSL settings. The user can
  restore OSL's SOS control if desired.

The menu reports current actor count, unavailable information, scenes, pending
work and command results. Counters describe commands and current tracked actors;
they are not engine ownership read-back or motion verification. Full managed
selection, SMP/CBPC handoff, cleanup, saved membership and NPC scene roles remain
later work. Before full management is enabled, enrollment must remove an actor
from the basic path and wait for outstanding shared work to retire.

## Verified local interfaces

Configured dependency: `D:\Modding\codex\.sps-deps\CommonLibSSE-NG`.

- `include/RE/P/ProcessLists.h`, `highActorHandles`; `src/RE/P/ProcessLists.cpp`,
  `ForEachHighActor` confirms traversal via handles. The new caller performs
  bounded indexing on the game thread, without retaining iterators.
- `include/RE/A/Actor.h`: `LookupByHandle`, `GetHandle`, `GetRace`.
- `include/RE/T/TESObjectREFR.h`: `Get3D`, `GetPosition`, availability checks.
- `include/RE/T/TESRace.h`: `BGSKeywordForm` inheritance;
  `include/RE/B/BGSKeywordForm.h`: `HasKeywordString`.
- Existing `Diagnostics.cpp` establishes the node lookup used by SPS.
- `include/RE/T/TESObjectLoadedEvent.h`: `formID` and loaded flag. Registration
  uses the existing `ScriptEventSourceHolder` event-sink pattern.
- Provider/scene contracts and event mapping evidence remain in
  [the preceding increment](NPC_BASIC_SOS_POLICY_2026-09-22.md).

## Save and rollback assessment

Probably compatible with existing saves, requiring live verification: only
transient native state and two optional INI keys were added. No ESP records,
serialization records, saved Papyrus variables/properties, aliases, quests or
attached scripts changed. The DLL and all installed bridges must remain paired.
Both new keys default false when absent from the preserved test INI.

Rollback requires Skyrim closed and restoration of the paired DLL/bridge files
from the dedicated-mod backup. Do not remove scripts or clean the save as a
generic remedy. Turning NPC mode off does not restore another mod's SOS toggle.

## Validation and next gate

The initial native build and all three test executables passed. Compiler
conversion warnings in new test/discovery code were corrected, followed by
the complete paired build. The final native build completed with zero warnings
or errors, all five Papyrus compiles and all three test executables passed.
Staged and freshly expanded ZIP packages passed 25 FOMOD source checks, four
bundled XML checks, bridge signatures/versions, all eleven paired-file hashes
and package rejection tests. Live results remain pending.

| Artifact | SHA-256 |
| --- | --- |
| 2.1.0.1 test ZIP | `72D9DA657D73800D500B12295A1D6E410BA152A1FF3D26C4E6057BE3F47A75E2` |
| 2.1.0.1 DLL | `C9C1254F702F1785CB92430D375F8F7372365458CF107D45D440FFA2F9545621` |

Package: `out/release/Schlong-Physics-Swapper-2.1.0.1.zip`.
Paired build log: `out/npc-basic-runtime-paired.log`.
Evidence directory: `out/diagnostics/npc-basic-runtime-20261003/`.

Installation completed with Skyrim closed into
`D:\Modding\mods\Schlong Physics Swapper`. Ten files were updated: DLL, four
core PEX/PSC pairs and the candidate README. All installed hashes match the
validated package. All thirteen other existing files retained their original
hashes, including the INI, meta.ini, CBPC configuration and selected SMP XMLs.
Optional OStim was not selected in the existing installation and remains absent;
the matching optional bridge is available in the test package. The unchanged
INI has no NPC keys, so the new mode remains off on first launch. Preserving
meta.ini also preserves MO2's older displayed version; use the SPS version or
installed DLL hash to identify this candidate.

Full rollback backup:
`out/backups/testmod-before-npc-2.1.0.1-20261003-173315`.
Per-file before/after evidence: `pre-install-hashes.json` and `installation.json`
in the evidence directory. Only the dedicated test mod was written. Published
2.0.1, private SOFTBODY overrides, FSMP, other mods and the main game installation
were not changed. No commit, push, publication or external post was made.

New focused tests cover the hard cap, invalid/player identity rejection,
round-robin service, changed handle/3D rejection, globally distinct lifetimes,
immediate permission/unload revocation, re-enable/remove behavior, stale callback
rejection, dropped callbacks/backoff and old-INI defaults/settings round trips.
The cadence test covers every supported population from one through eight with
frame jitter; sampling must stay within the existing 2.5-second freshness budget.
Existing policy, request ledger, operation gate and player tests remain included.

Before Stage 3, observe ordinary NPC poses with OSL SOS disabled, player behavior,
scene entry/exit, paused menus, unload/reload, save/load and disable/re-enable.
Test opposite arousal values on two NPCs and crowded-area behavior at the cap.
Record runtime, provider versions, actors/addons, mesh setup and visible results.
No SE/AE/VR, OSL/SexLab/OStim or motion pass is inferred from compilation.
Unrelated support issues remain outstanding.
