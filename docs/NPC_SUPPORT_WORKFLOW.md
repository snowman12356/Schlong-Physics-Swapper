# Managed NPC support workflow

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

## Design decisions

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

Current NPC status and evidence are in `PROJECT_DIRECTION.md`; the staged plan this
document used to hold is in git history.
