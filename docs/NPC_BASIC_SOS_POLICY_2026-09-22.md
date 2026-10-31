# Basic NPC SOS policy and scene deferral

Date: 2026-09-22. Branch: `codex/codebase-rework`, baseline HEAD `103e402`.
Continues the user-authorized [NPC workflow](NPC_SUPPORT_WORKFLOW.md) and
[actor bridge increment](NPC_ACTOR_BRIDGES_2026-09-22.md). All existing
uncommitted work is retained.

## Implemented scope

`BasicNpcPolicy` supplies ordinary NPC arousal-to-SOS decisions separately from
the player's settings and full managed-actor physics decisions. It uses the
verified legacy OSL mapping: integer arousal divided by four, minus 14; below
-9 sends `SOSFlaccid`, otherwise a signed `SOSBend` event capped at 9. Invalid
arousal remains unknown; values above 100 follow SPS's existing numeric clamp.
This preserves a known mapping without claiming it matches every current
native provider's implementation or creates SMP dynamics by itself.

The per-actor policy requires the feature enabled, the actor outside full
management and provider SOS control known to be disabled. Those are caller
conditions, not newly implemented settings or automatic detection. Scene and
arousal samples must be no older than 2.5 seconds of active time. Clear scene
state must settle for 1.5 seconds before output can resume. Unknown/active
observations, a gap in observations, reset or a deferred/failed write require
fresh clear evidence. Late samples cannot overwrite newer scene evidence.
Successful unchanged output is suppressed for 10 seconds; changing the target
is not suppressed. These are conservative development timing constants, not
measured release performance guarantees.

`ActorBridgeAdapter` now provides a guarded scene query and a separate basic
position operation. Queries have their own bounded request kind; they do not
acquire the physics mutation gate. Basic position shares the existing gate
and position request slot, checks scenes again inside Papyrus before its one
animation event, and reports applied, deferred or failed. It does not start,
stop or reset either physics engine. The caller must use the policy first,
capture request-start timestamps and resolve completion tickets before updating
actor state. Callback arrival must not make an old observation appear fresh.

Scene queries return `0=unknown`, `1=clear`, `2=active`, deliberately keeping
a default/missing integer result distinct from permission to write. Basic
position returns `-1=failed`, `0=deferred`, `1=applied`; only 1 records success.
All outgoing native-to-Papyrus arguments remain primitive values. Actor target
resolution, stale-work rejection, pause handling and retained engine leases
continue through the existing guarded actor adapter.

## Source evidence and provider boundaries

| Provider / requirement | Evidence and implemented behavior |
| --- | --- |
| Ordinary SOS mapping | `compat/OSL Aroused/Scripts/Source/OSLAroused_Main.psc`, `UpdateSOSPosition` at line 291; the existing legacy 2.9.0-2.9.2 restriction is unchanged |
| OSL scene membership | Installed `D:\Modding\mods\OSL Aroused\Scripts\Source\OSLArousedNative.psc:128`, `IsInScene(Actor)`; only the declaration was added to SPS's build-only stub |
| SexLab scene membership | Installed P+ `Source/Scripts/SexLabFramework.psc:220`, `IsActorActive(Actor)`; query uses the actual NPC and returns unknown if the framework quest cannot resolve |
| Optional OStim scene membership | The local installation has no OStim source package, so the primary [OActor source](https://github.com/VersuchDrei/OStimNG/blob/main/data/Scripts/Source/OActor.psc#L397-L414) was consulted. `IsInOStim(Actor)` checks the participant rather than assuming player thread 0. A build-only declaration was added; NPC roles/thread control remain outside this increment |
| Verify native availability | Configured CommonLib dependency: `include/RE/I/IVirtualMachine.h`, `GetScriptObjectType`; `include/RE/O/ObjectTypeInfo.h`, valid linkage/global functions; `IFunction` signature access; `NF_util::NativeFunctionBase::HasStub`. Script functions report `GetIsNative=false`; the native callable check rejects missing/unbound or incompatible functions |

Native OSL/OStim scene checks require a linked global native function returning
Bool with one Actor parameter and a bound callable. Papyrus names are compared
case-insensitively through `BSFixedString`. A DLL/file name alone is insufficient.
Present providers with unavailable required components block dispatch. Optional
OStim also requires the matching SPS helper; omitting that helper while OStim
is installed must not silently mean no scene. No external files or settings
were changed. The OSL native config source exposes a SOS integration setter
but no matching getter, so this increment does not claim to verify the user's
SOS toggle automatically.

These are compiled, source-backed integrations. Live binding, OSL/SexLab/OStim
scene entry/exit and busy-VM timing have not been tested for NPCs. The API checks
are conservative failure behavior, not proof that every historical version is
supported. OStim scene roles and simultaneous scene control remain later work.
The general SOS-state fallback still lacks an independently verified input;
this change does not manufacture one from SPS's own previous output.

## Bridge and save compatibility

Position bridge version is now 3; optional OStim is 2. Arousal stays 2, SexLab
stays 3 and FSMP stays 6. Existing player function signatures and implementations
remain intact. The package contract includes both scene entry points, the
basic-position entry point and native scene guards. Rejection checks cover the
previous position version and optional OStim version as well as wrong signatures,
native flags and mismatched build files.
The new build-only `OActor.psc` declaration is included in the repository's
explicit stub allow-list; it is not copied into mod packages.

No saved variables, properties, aliases, quests, attached scripts or persistent
event registrations were added. The policy and request records are transient
native state. Existing-save structure is expected to remain compatible, but
this is not a live save/load pass for the new bridge functions.

## Validation

- Final native Release build passed with SE/AE/VR enabled and no compiler/linker
  errors or warnings in the saved log.
- All five Papyrus bridges compiled with zero errors and warnings.
- All three test executables passed. The expanded actor suite covers the exact
  legacy mapping boundaries, invalid arousal, unknown/active/stale scenes,
  settling and freshness boundaries, out-of-order observations, disabled or
  competing control, managed-actor exclusion, command coalescing/refresh,
  late scene deferral, failure/reset and actor isolation. Scene request slots
  also reject duplicate work, mutation misuse and cancelled results.
- Staged and freshly expanded packages passed bridge contracts, 11 paired-file
  hashes, 25 FOMOD source entries and four bundled XML checks.
- Package rejection tests passed. Actual published arousal/position version-1
  PEX files and the previous development position-2/OStim-1 files were rejected
  against their new contracts. Previous artifacts were read only.
- Published 2.0.1 ZIP bytes and all nine installed core DLL/PEX/PSC files still
  match the released baseline. No installation, commit, push or publication was
  performed. Private overrides and the user-owned links file remain untouched.

Evidence: `out/diagnostics/npc-basic-scene-20260922/`, including the DLL,
paired manifest, three test executables, native/Papyrus logs, internal ZIP,
expanded package and `validation.json`. Package log:
`out/npc-basic-scene-package.log`.

| Artifact | SHA-256 |
| --- | --- |
| Development DLL | `8F2A5913EE8D046736D7394E384BFF7C19C80A5E8525691ED1B93A80F527EAE7` |
| Internal validation ZIP | `6AE78EF6E974F5B52C44BEE4C66EEDB24159BE8434A4BEB8A965048A6D4C95F4` |
| Preserved published ZIP | `B5056B31BDF2ECB1D85D9067FBEB856266DAC5C54320E7E9522DAFC6C10CEB4A` |
| Preserved installed DLL | `4A0C1AA600B5C8FF2E97B2ECE7278C5573175A4793C92615C1A3A1AD3D7A21A6` |

The internal package retains baseline metadata and is kept outside `out/release`.
It is inactive development evidence, not a replacement 2.0.1 release or a live
candidate. Assign a distinct candidate version before offering/installing it.

## Remaining work and next gate

Neither the registry nor basic policy is instantiated by the live plugin. No
NPC receives commands. Stage 2 is in progress, not complete: connect a bounded
loaded-NPC update loop and separate basic actor lifetimes, define eligibility,
give the player priority, and expose explicit opt-in/provider coexistence
controls. Do not treat a registry entry as proof of mesh compatibility or as
permission to write without the scene policy. Basic actors must stay distinct
from full management and transfer responsibility without overlapping commands.

After that integration, create a separately versioned live candidate, follow
the dedicated-mod installation gate and test an ordinary compatible NPC plus
the player, scene entry/exit, paused menus, unload/load and disable/re-enable.
The full managed-NPC handoff, enrollment/removal, saved membership, scene roles,
recovery and diagnostics remain subsequent workflow work. The unrelated support
issues remain unchanged.
