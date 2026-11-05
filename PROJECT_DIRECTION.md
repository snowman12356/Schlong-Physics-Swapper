# Schlong Physics Swapper - project direction

This file is the permanent high-level direction for SPS. Read it before planning
or implementing project changes. Detailed implementation plans belong in the
`docs` directory and must remain consistent with this direction.

## Permanent update rule

Update this file after every change to the mod. Changes to native or Papyrus
code, configuration, compatibility components, tests, build/package tooling or
user-visible behaviour must update the development status and next gate below
in the same working change whenever practical. Git history and the detailed
documents in `docs` remain the implementation record; this file must always
provide an accurate high-level view of where SPS stands and what comes next.

## Purpose

SPS is becoming a dependable physics-management system for the player and,
later, selected NPCs. It should consistently coordinate SMP and CBPC without
requiring users to repair the state manually.

## Priorities

1. **Player reliability first.** Loading a game, changing arousal, equipping or
   removing armour, entering or leaving scenes, and external physics resets must
   leave the player in the correct state.
2. **Maintainable architecture.** Keep settings, decisions, runtime adapters,
   arousal, scenes, position, recovery, diagnostics and MCM presentation
   separated. New fixes should not destabilise unrelated systems.
3. **Runtime compatibility.** Preserve Skyrim SE, AE and VR support through
   CommonLibSSE-NG. The pinned dependency is `alandtse/CommonLibSSE-NG`
   v15.4.0 or newer; see `docs/DEVELOPMENT.md`.
4. **Managed NPC support after the player is stable.** Users will be able to
   select NPCs and add or remove them from SPS management. Managed NPCs should
   receive SPS switching, transitions, scene handling, positioning and bounded
   recovery. Unmanaged NPCs must retain standard SOS/arousal-mod behaviour.
5. **Low overhead.** Prefer events, bounded queues and strict managed-actor
   limits over constant world scanning or heavy polling.
6. **Cooperative compatibility.** SPS should take ownership only when needed and
   should coexist with supported arousal, scene and physics mods. HDT-SMP Flex
   support is not currently planned.

## Non-negotiable compatibility rules

- Preserve existing settings, INI keys, defaults and saved values.
- Preserve save compatibility and existing working player behaviour.
- Do not enable NPC support until the player regression checklist passes.
- Do not reintroduce the abandoned experimental semi-state.
- Keep DLL and Papyrus bridge files version-matched in every test package.
- Do not modify an installed Skyrim mod, publish a release, or push public
  changes unless the user explicitly requests it.

## Current development status

**Menu framework check and Method combo fix (built and installed 2026-11-05,
not checked in game):** installed with Skyrim closed. 13 files were verified and
8 changed; INI, `meta.ini`, XMLs and CBPC files were unchanged. Rollback:
`out/backups/testmod-before-methodfix-20261105-173705`. at the user's request, SPS's menu calls were checked against the
SKSE Menu Framework 3 source now in `codex-references/ui`. All 22 framework
functions SPS uses exist in the installed Menu Framework 3.23.0, and the newer
framework header binds them identically. Begin/EndDisabled calls are balanced
and there are no duplicate widget IDs. Bug found and fixed: on
Appearance → Advanced, "Choose automatically (recommended)" wrote `Method=3`, the
experimental pose angle that the animation graph overwrites, so touching the combo
stopped the angle moving. It now writes `2`, automatic; the pose angle is a
separate, labelled third choice, and the hint text is corrected (19 positions).
README angle-method and page text corrected. Open notes: `CMakeLists.txt`
fetches the menu example from a moving `master` (pinned locally by the
`.sps-deps` override), and the framework's wrappers call exports without null
checks, with no minimum-version check in SPS. ZIP SHA-256
`DD8EB30B16FC22CD8E21432C934332131F029BB23EC471080B1E6D0C88D2EEFF`, DLL SHA-256
`783683349DACDB4595122D51839B8EAE17A0BD1B1725DA1B565766FF0E08079D`. Native
only; the existing INI value 2 is unchanged.

**SOS schlong / SexLab futa (built and installed 2026-11-04, not checked in
game):** installed with Skyrim closed. 13 files were verified and 8 changed;
INI, `meta.ini`, XMLs and CBPC files were unchanged. Rollback:
`out/backups/testmod-before-futa-20261104-222458`. requested by the user, with a per-NPC button (not automatic), shown
for the player and managed NPCs, and remembered per save. The NPCs page
"Schlong and SexLab" section shows each actor's SOS AE addon and size (via
read-only `SOSAE_SKSE.GetActorAddonName/GetActorRank`) and SexLab P+ sex (via
`SexLabRegistry.GetSex`, with and without overrides). Females with a schlong
get **Make futa in SexLab**, which calls P+'s own `TreatAsFuta`; forced ones
get **Undo futa**, which calls `ClearForcedSex`. The user reported "SexLab
forgets between saves". P+ stores the override only as a faction rank on the
reference (`sslActorLibrary.psc:203-212`), so it is lost when the reference is
rebuilt. SPS therefore adds its **first SKSE co-save data**: unique ID `SPSW`,
record `FUTA` v1, a list of up to 256 form IDs, remapped with `ResolveFormID`
on load and cleared on revert. It re-applies `TreatAsFuta` (at most every 10 s
per actor) to loaded actors in the list whose SexLab sex no longer reads futa.
`SPS_SexLabBridge` advances to v4 (`DescribeActor`, `SetActorFuta`; the
existing functions are unchanged); the DLL checks v4 before using it. New
build-only stub `SexLabRegistry.psc`; `SexLabFramework.psc` stub extended. Save
impact: new co-save data is ignored by older SPS and dropped if SPS is
removed. The SexLab faction override itself is SexLab's own data and stays on
the actor until cleared (use Undo before uninstalling if desired). Validation:
the native build had 0 warnings, the three suites passed, all six scripts
compiled 0/0, and package validation and guards passed. ZIP SHA-256
`BF5E3873CA7CE04B8B71CE850E9BFFF7022E6DBBDB1F7DF6999F2DBBCCD56B74`, DLL SHA-256
`35B57113935218C282478ABC024754CF45D6170F17FF1BE352F6F55E1593A2C4`.
Unverified: `Game.GetForm` with runtime (FF) form IDs and P+ native `GetSex`
results were not exercised in game.

**Managed NPC logging (built 2026-11-04, not installed):** requested by the
user. `SchlongPhysicsSwapper.log` now records:
- runtime state changes (`Managed NPC runtime: …`);
- add, remove and refusal with the reason;
- load/unload;
- scene changes;
- each physics request, with arousal and target;
- handoff completion, the SMP proof or "did not complete";
- each angle event sent or deferred;
- why a handoff or angle was not sent.

Changes are always logged; repeats of the same request or blocked reason at
most every 10 s, so the 250 ms NPC tick cannot flood the log. Native only. ZIP
SHA-256 `7664F1B0028D036CC8C8BE4191EA105484A33AD745A7A9AAE9165539086BC24E`, DLL
SHA-256 `6C3D1322A12AB792ABC44A1C39368DE021072C5030819D7E560DA5D2FFE71085`
(includes the handoff fix). The native build had 0 warnings, the suites passed,
and package validation passed.

**Managed NPC handoff bug fixed (built 2026-11-04, not installed):** the user
reported that NPC switching kept failing. The 21:33-21:40 log has no NPC lines,
because the managed runtime did not log. Cause, confirmed in source:
`SetActorPhysicsOwner` erased the NPC's ownership request from the ledger
(`ownershipRequests.Finish`) immediately after queuing `SetActorOwnerV3`. The
script's first step, `ResolveOwnershipRequest(token)`, therefore found nothing
and returned None, so every NPC handoff returned "not done" and was retried
indefinitely. The working arousal, scene and position bridges release their
request only in the completion callback. The ownership callback now does the
same: it releases the request together with the gate lease when the stack
returns or is disposed. Also fixed: the NPC ownership completion touched the
game-thread-only registry from the Papyrus callback thread, and now runs as an
SKSE task like the player path. NPC handoffs are now logged ("Managed NPC …
switched to …", the SMP proof, or "did not complete"). ZIP SHA-256
`CFDA9B7B7FCF68F68175C276E569B91D18845BAD35852E31FCB388EAC4CEFD15`, DLL SHA-256
`D824FD988B5FE93559D52D6C67ABF932154F9579D1A1917E3843F791CC07527F`. The native
build had 0 warnings, the suites passed, and package validation passed.
Unverified in game.

**Managed NPC startup bug fixed (built and installed 2026-11-04, not checked in
game):** installed with Skyrim closed. 13 package files were verified and 7
changed; INI, `meta.ini`, XMLs and CBPC files were unchanged. Rollback:
`out/backups/testmod-before-npcfix-20261104-212537`. the user
reported an NPC at arousal 100 not erect. The 21:01-21:15 log shows four
`Could not queue managed NPC FF000B9C`. Cause, confirmed in source:
`ConfigureManagedNpcCommands` was called only when the menu checkbox changed,
never at settings load or reload, so a saved `ManagedNPCEnabled=true` left the
runtime "Off". The first Add stayed queued forever and later clicks were
rejected as duplicates. Both load paths now configure it, matching the basic
path. Basic (unmanaged) NPCs only receive SOS events, never a physics switch;
with SMP genital XMLs (the installed SPS SOFTBODY profiles apply to every actor
using those paths) an SMP-simulated shaft can stay hanging at SOSBend9. That is
inferred from the design, not observed; full management (CBPC) is the path that
makes an NPC visibly erect. The same log also shows repeated `SMP resumed
ownership of 6/6 managed bones while CBPC was selected` for the player between
20:56 and 21:14, each reasserted automatically; the trigger is not yet
established. ZIP SHA-256
`9521B407BEEFF1AD405A6934F879423F00221FC869819F7DC10E5144DFFDE388`, DLL SHA-256
`EFE66590258A4D8440F199770B422B0E91D151BC08E996E6216825E8F73AE034` (includes
the managed NPC menu and the OSL no-repeat change). The native build had 0
warnings, the suites passed, and package validation passed.

**Basic NPCs follow OSL 2.9.3's SOS behaviour (built 2026-11-04, not installed,
not checked in game):** the user asked that ordinary NPCs copy what OSL does with
Enable SOS on. Read from OSL's public source at commit `b1f85b6` ("2.9.3",
matching the installed 2.9.3.0), `ActorStateManager::UpdateSOSAnimation`:
`int(arousal)/4 - 14`, below -9 sends `SOSFlaccid`, capped at `SOSBend9`; skipped
in scenes, with the cache cleared so the pose is re-asserted afterwards; an event
is sent only when the bucket differs from the last applied one, and is never
repeated; dead actors are sent `SOSFlaccid`. SPS's mapping was already
identical. The one change: SPS no longer re-sends an unchanged event every 10 s
(`kRefreshMs` removed). Remaining deliberate differences:
- SPS waits 1.5 s after a scene clears, because its scene check is an async
  query.
- SPS re-sends after an NPC's 3D reloads, which OSL's formID cache does not.
- SPS skips dead actors.
- SPS still tracks at most 8 NPCs with a Gen01 node within 3000 units, where OSL
  covers all humanoids in nearby cells.

Native only, safe for existing saves. The native build had 0 warnings, the
three suites passed with the updated no-repeat tests, the scripts compiled 0/0,
and package validation and guards passed. ZIP SHA-256
`229591758AC4995F30F77EC7A53CECF10EDDB5D3F447B341A417CE9E3C46D0D1`, DLL SHA-256
`4771A0C6F399053CCA1698F48E61415FF202A736CCBFB2F4C6671F1AC4E1B400` (includes
the managed NPC selection UI).

**Managed NPC selection UI (built 2026-11-04, not installed, not checked in
game):** requested by the user. The NPCs page now lists up to eight loaded NPCs
within about 58 m (4096 units), closest first, each with a **Manage** button
or the reason it cannot be managed. "Managed now" shows each managed NPC's name,
physics command state (Soft (SMP) / Erect (CBPC) / Switching), state and
arousal, with a per-NPC **Remove** button. In game, SPS shows a notification
on add, remove or refusal, and when the crosshair lands on a managed NPC (at
most every 8 s for the same NPC), using vanilla `Debug.Notification`, verified
in `vanilla-papyrus/Debug.psc`. CommonLib's build has no native notification
helper, so none was invented. The nearby scan runs once a second over the
high-process handles with cheap checks, and only does the node check for the
closest eight; it runs only while managed NPC is enabled. The crosshair-Add
button and the one-NPC limit are unchanged. Native only: no settings, scripts,
bridge versions or saved data changed, so it is safe for existing saves. The
native build had 0 warnings, the three suites passed, all six scripts compiled
0/0, and package validation and guards passed. ZIP SHA-256
`3BA955891309A67C8E5AE94407366F78F0E814B68989ADEE7691667C32A7C6AF`, DLL SHA-256
`4138A8EAB089E65FAA145D0FCE80D2FFFC5446CED8638BAC376354048479706C`. Managed NPC
physics itself is still unverified in game (Stage 3 gate).

**User-reported pass (2026-11-04, 20:33-20:41 session):** after installing the
audit-fix build the user reported "it works". The matching log was written to
the normal SKSE log path by the new wide-path logger (2.3.0.0 loaded at
20:33:41). It records 5 `Rise primed on SOSBend-9` lines, 2 completed gradual
rises, 2 descents released at SOSBend-9, every SMP proof at 6/6, and no SPS
error codes. That covers the rise snap, even-step rise/fall and SOSFlaccid
removal on this setup. Per-check observations (addon on/off, changed Rise/Fall
times, soft resting look) were not itemised, and the non-ASCII log path case
remains untested.

**Audit fixes 1-3 (built and installed 2026-11-04):**
installed into the dedicated test mod with Skyrim closed. 13 package files
were verified and 7 changed (DLL, manifest, five PEX); INI, `meta.ini`, XMLs and
CBPC files were unchanged. Rollback: `out/backups/testmod-before-auditfix-20261104-203115`. a
read-only audit of SPS found three code issues, now fixed natively. (1) Startup
crash risk: CommonLib's `SKSE::Init` log setup and SPS's own spdlog file sink
both converted the Documents log path with `path::string()`, which throws for
characters outside the system code page, inside plugin load. CommonLib's
logger is now disabled (`InitInfo{ .log = false }`), and SPS opens its log
through a wide-path `std::ofstream` with an `ostream_sink` (null sink if the
file cannot open). The file name and location are unchanged. (2) The experimental pose-angle
state map was unlocked while reachable from Papyrus threads, and its natives
wrote bone transforms from script threads. The map is now locked, and the natives
queue the write on the game thread through the actor handle (they still return
false when the base bone is missing). (3) FSMP's DLL version was re-read from
disk on every call, and legacy SOS/SOFTBODY/Aroused-Redux plugin and script
file checks hit the filesystem per call. Module versions are now cached per
loaded module, and those checks are cached after `kDataLoaded`. No settings,
scripts, bridge versions or saved data changed: safe for existing saves. The
native build had 0 warnings, all three suites passed, all six scripts compiled
0/0, and package validation and guards passed. ZIP SHA-256
`BBD44F2201F3DF9F0730692F48B179BAAEECA0ABC23A701011CC5AB5A01C3715`, DLL SHA-256
`1532B213412652049B16BDF7E00F673ED228C3B37FC59E2D19227B9CA27CBF71`. Includes all
round-2 angle changes. Unverified: the non-ASCII path case was not reproduced;
the fix follows the source.

**Rise snap and even-step ramp (built and installed 2026-11-04, not checked in
game):** installed into the dedicated test mod with Skyrim closed. 13 package
files were verified and 8 changed (DLL, manifest, README, five PEX); INI,
`meta.ini`, XMLs and CBPC files were unchanged. Rollback:
`out/backups/testmod-before-round2-20261104-194435`. the user's test of the paired redeploy showed, with SPS Retractable
Addon disabled, "snaps to 9, snaps to -9, then smoothly -9 to 9", and with it
enabled, "snaps to 0, smoothly to -9, then -9 to 9". The log shows CBPC holding
the bones for about 2.2 s before the ramp's first event, while the graph was
still on `SOSFlaccid` from the soft path. The extra CBPC confirmation
transaction (queued at +750 ms) blocked the 400 ms settle. Changes, native only:
(1) `PrimeLowestStageForRise` sends `SOSBend-9` before any SMP-to-CBPC switch
when a timed rise will follow; (2) the CBPC confirmation is deferred until after
the ramp, and the SMP-off maintenance probe pauses while a ramp runs; (3) graph
backends now walk the 19 bound positions in 18 even steps
(`PoseTargets::StepRampBend`), so Rise time / Fall time are the exact -9-to-9
travel times; (4) the ramp's 15 s cap now matches the 30 s sliders/INI clamp.
(5) At the user's request SPS no longer sends `SOSFlaccid` to the player:
the soft pose is sent as a bend stage (bend 0 = `SOSBend-9`, or the configured
FlaccidBend stage), and angle resets no longer ask `SPS_FSMPBridge` for its
`SOSFlaccid` step (only the experimental pose method still uses it). Basic and
managed NPC events are unchanged (the basic path keeps the verified legacy OSL
mapping). No INI keys, defaults, scripts, bridge versions or saved data changed:
safe for existing saves. The native build had 0 warnings, the three suites passed with new
even-step/round-trip tests, all six scripts compiled 0/0, and package validation
and guards passed. ZIP SHA-256
`4BDA1A679DCD84A340320EA2945FC338E3A9DFFE51130005ED9E7A73617F45EC`, DLL SHA-256
`7D6DFDA633F61D057EFE42ED6B9F4C29A4E879C3C47F3200291388AA60A839B9`. Unverified:
whether `SOSBend-9` sent while SMP still owns the bones is visible as a small
twitch, and whether the snap is gone with and without the addon.

**Handover review and paired redeploy (2026-11-04, not checked in game):** a new
agent took over. `Build-Release.ps1` rebuilt 2.3.0.0 as one paired set: six
Papyrus scripts with 0 errors/warnings, the native build, all three test suites,
package validation and package guards passed. ZIP SHA-256
`9BA148F5DB1ABDD20FD716DB9F280EA2987C20542EA23E7D178629DE7BB83A83`, DLL SHA-256
`AAC4B0D5A3497E633B473244B2F5ED4499A582C709A633DA0E2472D3C6168DC2` (unchanged
from the 2026-11-02 deploy). Before this, four installed PEX files did not match
the repository build; the test mod now holds the paired DLL, five core PEX/PSC
pairs, `SPS_PoseAngle` and the manifest. INI, `meta.ini`, XMLs and CBPC files
were untouched. Rollback: `out/backups/testmod-before-paired-2.3.0.0-20261104-184119`;
the earlier 2.3.0.0 ZIP is in `out/backups/release-2.3.0.0-before-paired-20261104-184032`.
The 2026-11-02 log from the reported "rise snaps halfway, drops, then climbs"
session shows that on every 0-to-100 jump SPS started a CBPC handoff, and
16 ms later SPS Retractable Addon requested SMP through the API. The superseded
transaction had already disabled SMP, so the bones followed the current
animation pose (kinematic, no CBPC yet) until the SMP handoff completed about
2.7 s later. After the addon released, the ramp climbed from SOSBend-9. This
matches the symptom but is not confirmed in game; the 2026-11-02 ramp fixes
A/B/C do not address this race. Next gate: the in-game rise test with and
without the addon enabled.

**SMP handoff proof (deployed 2026-11-02, not checked in game):** bridge version 9.
After an SMP enable, the bridge calls Faster HDT-SMP `TogglePhysics(..., true)`
a second time and stores that count in `result[3]` (`-1` when the path did not
sample). FSMP records the live state and leaves a bone that is already dynamic
unchanged, so a true entry is proof that bone is dynamic. The log says
`SMP proof: N/N managed bones were dynamic after the handoff` only when N
equals the managed bone count. A smaller count is `SMP not proven`, and a
missing sample is not proof. CBPC `StopPhysics` still returns nothing, so the
log never says CBPC was observed to release. The SOSBend-9 line only says the
handoff was requested. DLL and `SPS_FSMPBridge.pex` must stay paired; a version
8 script is rejected. Deployed to the MO2 test mod with the bend-curve DLL.
The existing INI, `meta.ini` and physics XML were left in place. Not checked in game.

**Bend curve (deployed 2026-11-02, not checked in game):** SOSBend-9 is the lowest
angle. A rise starts on that event and walks upward. A fall keeps CBPC until
SOSBend-9 is sent, then hands the bones to SMP. Release build of the plugin
compiled, and the actor-registry tests passed, including the new curve checks.
Deployed to `D:\Modding\mods\Schlong Physics Swapper` with the version 9 bridge.
Not checked in game.

**SMP drift read-back (implemented 2026-10-31, not installed):** the ownership
bridge advances to version 7 and returns `Int[4]` instead of a bare boolean
acknowledgement. SPS previously discarded Faster HDT-SMP's `TogglePhysics`
return array. Reading FSMP's own `src/dhdtPapyrusFunctions.cpp`
(`TogglePhysicsImpl`) established that each entry reports whether that bone was
**dynamic at the moment of the call**, sampled before any state change, recorded
once per bone even when several skinned meshes carry the same name, and skipped
when the bone already matches the requested state. That makes the existing
SMP-off maintenance call a passive drift probe: it runs only while policy wants
CBPC and the cached owner is already CBPC, so any bone reported dynamic there
proves SMP had resumed ownership unnoticed. Drift is claimed only under that
precondition, because an ordinary SMP to CBPC switch legitimately reports
dynamic bones. The opposite direction remains unprovable: "kinematic or not
found" are indistinguishable, so a false entry never confirms that SMP released
a bone, and a CBPC to SMP transition yields no evidence. SPS therefore never
re-asserts SMP merely to sample it. `smpConnected` and `cbpcConnected` are now
recorded separately from the selected owner rather than from one shared boolean,
which had reported SMP as ready after an SMP-off handoff, and the Troubleshooting
page now says "commands accepted" instead of "ready" because CBPC's
`StartPhysics`/`StopPhysics` return nothing and cannot be verified at all.
Drift recoveries and the last dynamic-bone count are reported in the menu.
Validation: the FSMP bridge recompiled with zero errors/warnings and reports
version 7 with the `Int[](String,Bool,Int,Int)` signature; the other four bridges
recompiled unchanged; the native SE/AE/VR build passed with no compiler or linker
warnings and all three test executables passed, including new coverage for
legitimate-switch non-drift, real drift, clean pass, unsampled paths and
SMP-target rejection. No installation, package build, commit or publication was
performed; the paired-DLL/scripts candidate requires its own package validation
and a live session before any claim of working drift recovery. DLL SHA-256:
`5F35626040B0730C18589905432786D4A263BDCD3ED8CC40389E4AD26E146872`.

**OSL test controls (requested, built and installed 2026-10-10):** the separate
2.1.0.3 candidate extends the prior 2.1.0.2 API/NPC build. The NPC page adds a
player slider and an individual slider for every actor currently held by the
bounded basic NPC pool. Releasing a slider writes the actor's real OSL Aroused
value through the installed, source-verified
`OSLArousedNative.SetArousal(Actor, Float)` contract; it does not introduce an
SPS-only override or edit OSL files. NPC writes retain the actor handle, root,
lifetime and save-session guards. Player writes use a deliberately
player-enabled test lifetime, and all writes wait until the Journal/MCM closes.
The arousal bridge is version 3 and must stay paired with the new DLL. The
SE/AE/VR native build, three native test executables, all five Papyrus compiles,
compiled bridge ABI/version checks, DLL V1/V2 export check, paired-package
validation, negative package guards and deterministic ZIP check passed. The
2.1.0.3 candidate is now installed in the dedicated MO2 test mod. Installation
ran with Skyrim/SKSE closed after a full verified backup. The ten selected files
(DLL, four core PSC/PEX pairs and README) match the candidate; the other thirteen
installed files retain their previous hashes, including the INI, `meta.ini`,
CBPC configuration and selected SMP XMLs. Optional OStim remains unselected.
Live validation was reported by the user as passing on 2026-10-31: the player
slider below and above the SPS threshold, two tracked NPCs on opposite OSL
values posing independently, scene entry/exit deferral, leaving and returning
to the area (unload/reload), save/load, and disabling/re-enabling basic NPC
behaviour. These are user-observed results; no matching log or capture was
supplied, and command counts or OSL write success were not treated as motion
evidence on their own. Unmanaged basic OSL behaviour remains the only live
Stage 2 path: the generic SOS-state fallback, other arousal providers, and SE/AE/
VR-specific runs are still unverified, as are NPC scene roles that belong to
Stage 5. The next gate is Stage 3, one explicitly selected NPC with full SPS
management, and requires explicit user authorization before implementation.
Candidate ZIP SHA-256:
`DAC86495951825411CDC1E773695C4553BBC2FC743D157DF5B0EE9BA8EC5A27B`.
Candidate DLL SHA-256:
`F45436EE3626E2AA02BB8E29A32123CD4D35D92EDEDECE87F14BBFAAA4130B1B`.
Rollback: `out/backups/testmod-before-osl-controls-2.1.0.3-20261010-194143`.
Evidence: `out/diagnostics/osl-arousal-controls-20261010/`.
See
[`docs/OSL_AROUSAL_CONTROLS_2026-10-10.md`](docs/OSL_AROUSAL_CONTROLS_2026-10-10.md).

**API increment (requested 2026-10-09, installed 2026-10-10):** the user requested
read-only arousal and erect threshold access. The V2 API work is complete in
the separate 2.1.0.2 test candidate. `GetArousal(actorFormID, float*)` reads the
player controller's accepted value/validity together, with no provider query;
`GetErectThreshold(float*)` reads the loaded setting under its lock. Missing
values return NotReady and outputs remain untouched on failure. V1's table,
payloads and capabilities remain unchanged. V2 retains player-only API scope;
basic NPC tracking does not grant public NPC API support. The SE/AE/VR build,
all five Papyrus compiles, three test executables, V1/V2 layout checks, both DLL
exports and paired-package validation passed with no native warnings/errors.
The dedicated test mod now has the paired 2.1.0.2 DLL/bridges. Installation ran
with Skyrim closed after a full backup; INI, meta.ini and physics files retain
their previous hashes. The earlier approval-review usage-limit failure ran no
installation and was resolved by the approved retry. Live API/NPC checks remain
outstanding. The 2.1.0.1 test ZIP and published 2.0.1 ZIP remain unchanged.
Current ZIP SHA-256: `BE04AA864244E744DB7AE7349155951F1E5835E3CC65FA9CC0B3D6B0A7E48BE9`.
Current DLL SHA-256: `17AD898F6806EBD4ED670D448C94D283F17C2F429B09513E22C5AB8CA4045120`.
Rollback: `out/backups/testmod-before-api-2.1.0.2-20261010-131042`.
Evidence: `out/diagnostics/api-readback-20261009/validation.json`. See
[`docs/API_READBACK_2026-10-09.md`](docs/API_READBACK_2026-10-09.md).

**NPC milestone (2026-10-03):** the user authorized starting the NPC
workflow and confirmed user-observed passes for disable/re-enable, manual
Repair and state changes/save loading during a pending handoff. Follow
[`docs/NPC_SUPPORT_WORKFLOW.md`](docs/NPC_SUPPORT_WORKFLOW.md). Initial API
findings and the tested first Stage 1 increment are recorded in
[`docs/NPC_SUPPORT_FOUNDATION_2026-09-21.md`](docs/NPC_SUPPORT_FOUNDATION_2026-09-21.md).
The user supplied a prior design screenshot to
clarify the intended solution: SPS will supply basic arousal-to-SOS soft/erect
behaviour for ordinary unmanaged NPCs while external SOS control is disabled;
explicitly managed NPCs receive full SPS positioning, physics switching,
transitions and recovery. Prefer numeric arousal APIs, with a verified general
SOS-state fallback when unavailable, and retain the existing version-gated
legacy OSL option. This basic bridge is separate from full physics enrollment
and must respect scenes and bounded loaded-actor work. The OSL global switch
is a known constraint; the screenshot specifies intent, not verified APIs or
an already working integration. No other installed mod is to be changed.
After the basic bridge, the first full-management milestone is one explicitly
selected NPC alongside the player, followed by saved membership, bounded
multiple-NPC support and scenes.
A bounded native NPC registry now reuses `ActorContext`, isolates actor work
with lifetime generations, and rejects old callbacks across unload, removal
and save changes. The actor bridge increment adds actor-specific arousal and
position bridge entry points with verified handle resolution, bounded requests
and game-thread completion. Actor position calls share the existing physics
operation gate; scoped cancellation cannot cancel a different actor's command
or release a running stack early. The latest increment adds per-actor scene
queries and a basic SOS policy: unknown/stale inputs block output, clear scenes
settle before resumption, and the guarded basic write rechecks scene membership.
The legacy OSL event mapping is independent of player SPS thresholds. OSL and
OStim native scene queries require verified bound functions; optional OStim
membership uses the actor rather than player thread 0. Position is now bridge
version 3, optional OStim version 2 and arousal version 3; the player entry
points and FSMP version 6 are preserved. No saved variables
or properties were added. Full managed enrollment remains inactive. The native SE/AE/VR build,
all five Papyrus compiles, all three test executables and isolated paired-package
checks passed. See
[`docs/NPC_BASIC_SOS_POLICY_2026-09-22.md`](docs/NPC_BASIC_SOS_POLICY_2026-09-22.md)
for the latest code, source evidence and validation; the preceding actor bridge
increment remains documented separately. The current working increment connects
a separate basic NPC loop: at most eight nearby loaded adult humanoid actors,
32 high-process handles inspected per second and one actor serviced per 250 ms.
It uses OSL numeric arousal and the verified legacy event mapping, defaults off,
and requires explicit confirmation that OSL SOS control is disabled. Basic state
does not instantiate full physics contexts. Shared lifetime generations, 3D
identity checks, timeouts and immediate permission revocation protect queued
work; player actions retain priority through the existing shared gate. A new
NPC menu exposes opt-in and basic status. Managed physics handoff/controller
wiring remains part of Stage 1/3 work. The complete paired 2.1.0.1 build now
passes: all five Papyrus compiles, three test executables, stage/expanded ZIP,
bridge ABI/version/hash checks and package rejection tests. Final native build
has zero compiler/linker warnings or errors. One-through-eight actor cadence
tests include frame jitter. The candidate was installed into the dedicated test
mod with Skyrim closed, after a full backup; its INI, meta.ini, optional OStim
selection and physics files were preserved. Live NPC checks are the next gate;
see [`docs/NPC_BASIC_RUNTIME_2026-10-03.md`](docs/NPC_BASIC_RUNTIME_2026-10-03.md).
No independent generic SOS-state getter has yet been established from the
inspected local sources; do not claim that fallback is implemented. That NPC
candidate's metadata is 2.1.0.1 (NPC basic test), separate from published 2.0.1;
its dedicated installation has since been superseded by the API increment above.
The test ZIP is `out/release/Schlong-Physics-Swapper-2.1.0.1.zip`, SHA-256
`72D9DA657D73800D500B12295A1D6E410BA152A1FF3D26C4E6057BE3F47A75E2`;
DLL SHA-256 `C9C1254F702F1785CB92430D375F8F7372365458CF107D45D440FFA2F9545621`.
Evidence is under `out/diagnostics/npc-basic-runtime-20261003`; rollback backup
is `out/backups/testmod-before-npc-2.1.0.1-20261003-173315`.
The earlier inactive packages retain baseline version metadata and must not
be offered as live candidates. Published 2.0.1 bytes and other installed mods
remain unchanged. No push, publication or external posting was performed.
Broader compatibility checks and separate publication authorization still apply;
no NPC live-motion result is claimed.

Development continues on `codex/codebase-rework`. The workspace migration and
cleanup are complete, the reproducible native/Papyrus/package workflow passes,
and the player implementation now has modular decision, runtime, ownership,
arousal, scene, position, recovery, compatibility and diagnostics boundaries.
The obsolete OneDrive workspace and reproducible build/output caches have been
removed, and the reduced external dependency set passes the environment check.
With explicit approval, the temporary migration archive, pre-install recovery
copy, migration stashes and unreachable Git objects were also permanently
removed after verification. Intentional branches, tags, shared references,
required dependencies and the active test mod remain.

The 2.0.0 development package now passes the reproducible build, all five
Papyrus bridge compiles, core tests, FOMOD validation and expanded-ZIP
validation. It removes the remaining unsafe native Actor-to-Papyrus calls,
uses OSL's native arousal export and update value, and pauses SPS background
work while the Journal/MCM is open. A crash found in the first 2.0.0 test build
was traced to OSL's documentation describing `GetArousalExt` as FormID-based
even though its runtime ABI requires `RE::Actor*`; SPS now passes the actual
player pointer and rejects null actors. The live six-bone result is now
refreshed cheaply after player mesh events settle, fixing the stale initial
post-load check without repeating the full filesystem compatibility scan.
SOFTBODY 3.37.2 compatibility is automatic when
`HDT SMP Object - Simple.esp` is loaded: after its relevant SexLab or OStim
scene reload events settle, SPS reasserts only the physics owner it already
selected. No FOMOD option is required for ownership recovery and SPS leaves the
addon's XML untouched by default. The FOMOD now offers three explicit physics
choices: keep the user's own compatible XML, install the author's personal SPS
six-bone profile, or install combined SPS + SOFTBODY physics. Both supplied SPS
profiles support compatible SOS, TNG and UBE six-bone meshes. The SOFTBODY
choice combines the SPS dynamics/collision exclusions with SOFTBODY 3.37.2's
normal, soft and anal collision profiles, so users do not need competing
genital XMLs. The three installer choices use concise first-person wording so
the decision reads naturally to users. A live log captured PPA reporting the
player as penetrating while SexLab authoritatively reported receiving; the
decision now keeps every valid SexLab result, including its temporary unknown
result, and uses PPA only when the SexLab role query is unavailable or invalid.
This prevents a flaccid receiving player from being forced briefly to CBPC or
held in that wrong state after the scene. Physics maintenance is serialized behind
the active ownership transaction so equipment recovery and bend replay cannot
race the FSMP/CBPC handoff. A soft player also receives one delayed player-only
SMP pose rebuild after scene cleanup when PPA/Accurate Penetration or SOFTBODY
may have left the chain stretched; brief P+ role-query delays avoid querying its
native thread while it is still registering. Disabling SPS now waits for an
in-flight transaction, restores plain SMP ownership, and discards stale recovery
timers before a later re-enable. Failed or timed-out transactions invalidate the
cached owner so a fresh ordered handoff is required. Testing established that
SOFTBODY's stock genital XML gives the Gen02-Gen06 shaft chain zero gravity
while its scrotum uses normal gravity, so it can look rigid even when SPS has
correctly selected SMP. The preceding workload-pressure development DLL SHA-256 was
`A5A796A3D983BB0BA70A19463F2E5337EE6BF570900C044DA02EF7BAE5B6FB1C` and the
latest package SHA-256 is
`AFE28F503127E753F3B46D0A3F887F035E11D885A2E3424D77DE0D1E6757478A`.

On 2026-09-02 the validated 2.0.0 files were installed only in the dedicated
MO2 test mod at `D:\Modding\mods\Schlong Physics Swapper`; they have not been
released. The clean test install uses OSL Aroused 2.9.3 support, omits the
optional OStim and legacy OSL bridges, and preserves the user's existing SPS
INI and MO2 metadata. The corrected OSL ABI build replaced the initial test
DLL on 2026-09-02. The newer automatic post-load bone-detection build was then
installed after Skyrim closed; all 18 deployed files match the validated
package. On 2026-09-03 the automatic SOFTBODY recovery build was also installed
after Skyrim closed; all 18 deployed files match the validated package, while
the existing INI and MO2 metadata remain unchanged. On 2026-09-04 the package
was extended with the three physics choices and revalidated with 25 FOMOD source
entries and four bundled XMLs. Its 18 core files plus the selected three-file
SPS SOFTBODY profile were installed for testing after Skyrim closed. The
existing INI and MO2 metadata were again preserved, and the private large-mesh
override received the same merged SPS compatibility collision shape.
Later on 2026-09-04 the validated scene-role and serialized recovery build was
installed after Skyrim closed. All 18 core files and the selected three-file
SPS SOFTBODY profile match the package; the existing INI and MO2 metadata remain
unchanged.

For the user's private SOFTBODY 3.37.2 setup, a separate local test override is
enabled at `D:\Modding\mods\SOFTBODY True Soft Shaft - SPS`, above both
`SOFTBODY Custom Physics - Joel` and the base SOFTBODY mod. It preserves the
existing normal, soft and anal collision profiles. The initial gravity-only
test still appeared rigid because SOFTBODY kept Gen01 animation-driven and
tethered Gen02-Gen06 to its stiff lag-bone chain. The installed test override
now uses the known-working UBE six-bone shaft/scrotum dynamics block, including
per-bone gravity, while retaining each SOFTBODY collision tail unchanged. This
was subsequently tuned for the user's unusually large shaft mesh with higher
base-to-tip mass and inertia, moderately stronger gravity response, and firmer
damped joints; the mesh-following SOFTBODY collision definitions remain
unchanged. The private normal, soft and anal profiles also include the UBE
`MaleGenitalsCollision`, `VirtualCrotch` and `VirtualGround` definitions so the
shaft can contact the mesh's virtual ground while the scrotum remains excluded.
The user's `MaleGenitals` compatibility exclusions are also merged into the
private large-mesh profiles. This separate large-mesh tuning remains a
local override, while the generally tuned combined profiles are packaged as the
public opt-in FOMOD component. Goutou is credited under SOFTBODY's published
modification and asset-use permissions; the component must remain free and out
of paid compilations.

The next gate remains the documented in-game player regression pass, starting
with the post-scene lockout reported on 2026-09-05. The 2026-09-06 audit repairs
continue the existing ownership controller and preserve the selected physics
profiles, role policy, INI keys and defaults. They remove the unknown-owner
relaxation deadlock and the workload-pressure dispatch veto. Resets, equipment
release/reconnection and final ownership now run as one guarded Papyrus V3
transaction. Expired work cannot acquire a new transaction; a running cancelled
stack must return before replacement work can start. Player resets wait for
FSMP's queued game task before settling and restoring ownership. Recovery
budgets stop in paused menus and remain bounded across completion failures.

SexLab recovery now listens to the native unprefixed events and checks the
player's thread ID. Loading another save clears cached scene identity and
invalidates queued scene/arousal callbacks. Manual erect tests retain their
position updates at low arousal, and invalid legacy arousal readings do not
become zero. Diagnostics distinguish commands that executed from observed live
motion. Packaging validates compiled bridge signatures, native flags and version
return values, plus a paired-build hash manifest for the DLL, five PEX files
and their sources. See `docs/AUDIT_REPAIRS_2026-09-06.md` for findings, risks and
the focused regression checklist.

These changes are probably safe for existing saves, subject to testing: no
stored script variables/properties, quests, aliases, records or serialization
layout change. Existing bridge entry points remain available. A saved stack
from the older unguarded bridge cannot gain the new cancellation checks;
perform the update with Skyrim closed and begin testing from a save outside an
active scene. No save cleaning or new save is required by the implementation.
On 2026-09-06 the final audit repair build passed native compilation, the core
regression tests, all five Papyrus compiles without warnings/errors, compiled
bridge ABI/version checks, the eleven paired-build hashes, 25 FOMOD references,
four bundled XMLs, expanded-ZIP validation and package rejection tests. With
Skyrim confirmed closed, 18 core files plus three combined SOFTBODY XMLs were
installed and verified in the dedicated MO2 test mod. Its INI/meta.ini, all
private SOFTBODY XMLs and `docs/USEFUL_WEB_LINKS.txt` remained unchanged. The
previous installed files and pre-update logs are retained under
`out/diagnostics/audit-repairs-20260906`. No release was published.
2026-09-06 DLL SHA-256:
`4B6288444AD32DCE356A60B0CEEAFEF1FC63B5DE19A7A8F971AEE970650477D5`.
2026-09-06 ZIP SHA-256:
`6EB5F3DDBFB32DF390D4B7872A05F5FED369783C1076815FE38D69159FC51F5F`.
On 2026-09-08 investigation continued after the user reported that ordinary
swapping works only with SMP disabled globally, while viewing the player in
third person; scenes with the audit repair build remain untested. The installed
paired SPS build is unchanged. FSMP 4.1.1 source contains a confirmed selection
defect: actor toggle/reset calls can stop at the first-person skeleton before
reaching the body. An isolated candidate reuses FSMP's existing skeleton
classification in those two functions. Extracted-function regression tests
reproduce the original failure and pass with the patch. The full AVX Release
candidate builds with SE/AE/VR targets and passes binary/version/dependency
checks; its DLL and PDB identifiers match. It is packaged separately from SPS
under `out/diagnostics/fsmp-actor-api-20260907`. Its relevance to the user's live
failure still needs an in-game test; no FSMP installation or activation has
been changed. See `docs/FSMP_ACTOR_SELECTION_2026-09-08.md` for evidence, scope,
build provenance and the reversible test plan. The user subsequently rejected
changes to other mods; the FSMP candidate is not an active repair plan and
remains uninstalled. All resumed implementation and installation work is
restricted to SPS and its dedicated MO2 test mod.

On 2026-09-09 the user authorized restoring only the missing SMP-off safeguard,
keeping the existing bridge and modular rework. The supplied archive named
1.8.3 is byte-identical to the published 1.8.2 release candidate. That source
reasserts SMP-off for the six managed bones on ordinary erect-state polls;
1.9.2 removed this before the bridge and formal rework were introduced. This
is a confirmed behavioural difference and a regression candidate, not proof
of the cause of the user's current failure. The old release is a comparison
baseline, not a fresh successful test in the current setup.

SPS now restores that narrow command at the end of its existing tick, only
while current policy still requests CBPC, the cached owner is known CBPC,
relaxation is inactive and no physics transaction is pending. It waits at least
one active second after the last successful transaction. Reset/reconnection
and scene decisions retain priority. The guarded bridge adds an SMP-off-only
operation; it does not restart CBPC, reset SMP, change bend timers or publish
another owner change. Failure invalidates ownership through the existing
recovery path. Bridge version 4 is required to prevent an older bridge treating
the new operation as a full handoff; previous function signatures remain.
No persistent script state, settings, save layout, XML or external mod changes
are required. Existing saves are probably safe, subject to the player gate.

The safeguard build passed native compilation with SE/AE/VR enabled, the core
regression tests, all five Papyrus compiles without warnings/errors, compiled
bridge ABI/version checks, all eleven paired-build hashes, FOMOD/XML/expanded-ZIP
validation and package rejection tests. The actual previously installed V3
bridge was also confirmed incompatible with the new version-4 requirement.
After confirming Skyrim was closed, six changed SPS DLL/PEX/PSC files were
installed into the dedicated test mod and all 21 selected package files were
hash-verified. Its INI, MO2 metadata and selected XMLs are unchanged, as are the
private SOFTBODY override, installed FSMP DLL and user-owned links file.
The backup, build log and installation receipt are retained under
`out/diagnostics/smp-off-safeguard-20260909`. No other mod was edited or installed
and no release was published. Safeguard-build DLL SHA-256:
`9F4D346DA171A208F9D2DF7E9AD44E53D86C703B0CA258760CB57FCF0AF8C136`.
2026-09-09 ZIP SHA-256:
`726698CEEEA6DD20989F9C518055078A271F740E482FB6BBBAFADD1829D34EE3`.

On 2026-09-13 the personal physics option's collision-shape name was corrected
from `scotumcollision` to `scrotumcollision`, matching the local UBE SOS/TNG
collision meshes and UBE's supplied XML. FSMP looks up mesh shapes by this name;
the misspelling can omit the intended scrotum collider. Only that identifier
changes; dynamics, tags and collision exclusions remain intact. This XML-only
fix preserves the existing paired DLL/bridges and does not change save data.
The dedicated test mod uses the combined SOFTBODY option, which does not contain
this typo, so its profile must not be replaced with the personal option. The
separate private SOFTBODY override remains outside the authorized SPS-only
change. Validation confirmed that only the intended name changed, the XML is
well formed and its collision shape names are unique. The package was rebuilt
using the unchanged paired DLL/bridges; FOMOD/XML, compiled bridge contracts,
eleven build hashes, expanded-ZIP validation and package rejection checks pass.
With Skyrim closed, all 21 files selected by the dedicated test mod were
verified against the rebuilt package. They already match, so no installation
writes or profile change were needed. The INI, metadata, private XMLs, FSMP DLL
and user-owned links are unchanged. Evidence is retained under
`out/diagnostics/scrotum-name-20260913`. No release was published. XML-only ZIP
SHA-256: `FF1918EA3B18094498214F40893664AE698C9D9D9295947EDF34C048EAD0FC41`.
This is independent of the still-unverified SMP/CBPC switching regression.

Later on 2026-09-13 the user reported that swapping seems to work, but the
floppy shaft remains too long until FSMP's `smp reset` control is used. Scenes
have not been tested. The matching safeguard-build log shows three ordinary
erect-to-soft transitions, each followed by a queued player pose reset and
acknowledged bridge completion. Thus the automatic reset trigger is present;
acknowledged completion has not established restoration of the resting length.
The inspected FSMP source shows that the global reset also resets the physics
world after reloading meshes, whereas the actor API used by SPS reloads that
actor's meshes. Their equivalence must not be assumed. This difference is not
yet a proven cause, and the earlier actor-selection risk is also unconfirmed
in this session. The user then confirmed that SPS's manual Repair current
physics does not shorten it; only FSMP's reset does.
The logs are retained under `out/diagnostics/soft-length-20260913`; nine installed
SPS code files match the safeguard build. No runtime code, installed files or
other-mod settings were changed during that investigation.

A targeted SPS soft-reset candidate now briefly makes the six managed bones
kinematic after the actor mesh reload and CBPC stop calls, uses the existing
0.25-second settle wait, then resumes SMP. The local FSMP implementation makes
kinematic bodies follow the skeleton during simulation; enabling an already
dynamic body, as the old reset sequence did, exits without that alignment.
This is a source-supported correction to test, not proof of the live cause.
Only the existing soft-pose recovery operation uses it, including Test soft,
Repair and post-scene soft recovery. Ordinary owner changes, mesh reconnects,
the erect safeguard and other actors are unchanged. The operation retains the
existing cancellation lease, reset task barrier, pause clock and timeout.
Bridge version 5 adds the new preparation mode while preserving all existing
function signatures; install it only with its paired DLL. No saved script
state, XML, settings or external-mod changes are required.

The soft-reset candidate passed native compilation with SE/AE/VR enabled, the
core regression tests, all five Papyrus compiles without warnings/errors,
compiled bridge ABI/version checks, all eleven paired-build hashes,
FOMOD/XML/expanded-ZIP validation and package rejection tests. The previously
installed V4 bridge was also confirmed incompatible with the V5 requirement.
With Skyrim closed, six changed SPS DLL/PEX/PSC files were installed into the
dedicated test mod at 12:52 BST on 2026-09-13; all 21 selected package files were
hash-verified. The installed INI, metadata and selected XMLs, private SOFTBODY
override, FSMP DLL and user-owned links file are unchanged. The verified backup,
build log and installation receipt are under
`out/diagnostics/soft-length-20260913`. No other mod was changed and no release
was published. Soft-reset candidate DLL SHA-256:
`5AEF4DD2F0C1C6FB5D3C93A9BEFD5B6E3F4789AB8C2F966CFF91AB9245E62A9C`.
Soft-reset candidate ZIP SHA-256:
`19C6538631982905EF2AE0F847BAB01781B9B31E9623DA8F13F6E1E6D05BD937`.
These checks do not validate simulated motion or resting length in-game. The
next gate is repeated ordinary erect-to-soft changes with normal resting length
returning without FSMP reset, before scene testing resumes.

The user subsequently requested further SPS-only switching reliability work.
Inspection found that `SetOwner` waited for an opposite in-flight transaction
to finish rather than invalidating its remaining calls. A scene/arousal/disable
decision could therefore be followed by the obsolete owner's final commands.
The controller now marks such work superseded, invalidates uncertain ownership
and cancels its existing execution token. It retains the transaction slot until
completion/timeout and the adapter retains any running stack's lease until it
actually retires. A superseded callback cannot commit ownership, run recovery
side effects or revive its target if policy reverses again. The latest policy
then requests a fresh ordered handoff; unchanged requests retain their original
transaction. Waiting for a retired stack's lease no longer creates repeated
failed dispatches or increments the physics failure counter.

Queued manual tests now retain priority over ordinary arousal polling while
waiting for their handoff, and are withdrawn when scene/API/disabled control
takes priority. The existing visible ten-second test period is unchanged.
The bridge also checks that the player's 3D remains loaded before subsequent
physics steps and before acknowledging completion. Bridge version 6 is required;
all existing entry-point signatures and the version-5 soft-pose recovery remain.
No new Skyrim API, native hook, global reset, polling thread, saved script state,
INI setting, XML profile or external-mod change is introduced. Save compatibility
is probably preserved but still needs live testing; the new controller flag is
transient native state and clears on load.

Focused regression coverage includes opposite-target supersession for every
ownership purpose, queued and running stacks, rapid reversals, late successful
callbacks, timeout while a lease remains held, load invalidation and queued-test
priority. Startup reconciliation also respects queued manual tests. The final
native build with SE/AE/VR enabled, all core regression tests and all five
Papyrus compiles passed without compiler warnings/errors. Staged and expanded
ZIP checks passed, including bridge ABI/version validation, eleven paired-build
hashes, 25 FOMOD references, four XMLs and deliberate package rejection cases.
The previously installed V5 bridge was explicitly rejected by the V6 contract.
With Skyrim confirmed closed, six changed SPS code files were installed into
the dedicated test mod; all 21 selected package files were hash-verified. Its
INI, metadata and selected XMLs, the private SOFTBODY override, FSMP DLL and
user-owned links file are unchanged. The previous package, verified install
backup, build log and installation receipt are retained under
`out/diagnostics/switch-reliability-20260913`. No other mod was changed and no
release was published. Current DLL SHA-256:
`470575B47C18664007888B3777C6B0A846B252DEC5C116058FE8FE2DA9C5DFBC`.
Current ZIP SHA-256:
`E8A23CF7E2DB6C16FE6A5BA14E2D758540FA3838ECA12F56C11610A94C758F34`.
Additional in-game coverage to record includes a target reversal during
a pending handoff, disable during a handoff, queued tests at conflicting arousal,
scene priority over a queued test and loading while work is pending. These are
specific ordering repairs, not proof that FSMP's actor API reached the live bodies
or that the previously reported length problem is fixed. A VM stack that never
resumes/disposes still cannot safely be overtaken; the existing loading/restart
recovery boundary is retained.

After testing the reliability build, the user reported that it was fine. The
2026-09-13 session from 20:24 to 20:41 records three CBPC handoffs, three SMP
handoffs and three completed automatic soft-pose rebuilds. One startup SPS-010
at 20:27:34 recovered automatically at 20:27:42; one deferred CBPC confirmation
also completed later. There were no later SPS errors, no SPS bridge errors and
no P+ thread-not-found messages in the matching Papyrus log. All 21 selected
installed files match the last validated build; the ZIP hash is unchanged and
the staged package passes validation again. The log snapshots and verification
record are under `out/diagnostics/release-check-20260913`. No runtime code or
installed files changed during this check.

The user subsequently confirmed that the remaining queried checks were fine:
normal soft length without an FSMP reset and scene/equipment recovery. Record
these as user-observed passes, alongside the log-supported ordinary switching
results; the archived log itself does not show active scenes or equipment
recovery. The reported switching, resting-length and scene/equipment regressions
are therefore considered passed on the tested player setup. Together with the
successful build, regression tests and package checks, this supports release
readiness for the current 2.0.0 player build. No further speculative physics
change is warranted by this test result. The recovered startup timeout remains
recorded, rather than being described as an entirely error-free session.

Release readiness is not a claim that every runtime, optional integration or
physics profile below has been tested. Detailed manual Repair, disable/re-enable
and pending-operation load/reversal coverage remain unrecorded. All further work
remains SPS-only. With explicit user authorization, 2.0.0 was published on
GitHub on 2026-09-13 from release commit
`738b2eecea0bb034440003a642f6b254cba8419b` and tag `v2.0.0` on
`codex/codebase-rework`. Release URL:
`https://github.com/snowman12356/Schlong-Physics-Swapper/releases/tag/v2.0.0`.
The public release is neither a draft nor a prerelease. GitHub reports the
uploaded ZIP's SHA-256 as the same validated hash above (470,982 bytes).
No rebuild, physics change or installed-mod update was performed for publication.
Automatic approval review rejected the proposed default-branch update as outside
the upload request, so publication used the project branch and tag; `main`
remains unchanged. The release notes are in `docs/RELEASE_NOTES_2.0.0.md`, with
a plain-text copy at `out/release/Schlong-Physics-Swapper-2.0.0-changelog.txt`.
The ZIP is ready for the user's Nexus upload; Nexus was not changed. The
user-owned `docs/USEFUL_WEB_LINKS.txt` remains untracked and unmodified.
The validated release package remains
`out/release/Schlong-Physics-Swapper-2.0.0.zip`, with the hashes above unchanged.

On 2026-09-13 the Nexus Mods description for mod 188174 was updated in the
existing edit page for version 2.0.0. The wording and structure were retained,
while outdated statements were corrected: the page now describes the optional
SPS and combined SOFTBODY physics profiles, current OSL version choices,
optional OStim support, scene/equipment recovery, the current Scenes and
Troubleshooting menu names, and the current test/repair controls. The broken
compatibility-API link markup was also corrected. The Nexus mod version field
was changed from 1.9.6 to 2.0.0. The page saved successfully; no files or
settings in the local mod installation were changed.

On 2026-09-14 the supplied Predator SMP Head 1.4 and TNG/HIMBO XML archives
were compared with the released SPS profiles. The TNG/HIMBO `MaleGenitals`
collision definition is already retained by both SPS physics options; the
incoming genital XML alone supplies no active shaft dynamics. Predator's
facial bones do not overlap SPS's six managed bones, and the inspected head
and genital collision tags allow interaction. No runtime or XML patch was
applied. A subsequent Nexus screenshot reports missing lip/throat response
with SPS installed, so compatibility is now recorded as an unresolved reported
failure. The tag comparison did not account for SPS's zero shaft collision-margin
multipliers: FSMP scales the collider margin and penetration with these values,
and a scalar contact-plane example confirms that some contacts accepted by the
original collision-only profile are rejected. This is a plausible contributor,
not a confirmed cause of the user's report. Obtain the active genital XML and
soft/erect comparison before a targeted SPS test; a blanket multiplier change
can enlarge other proxy colliders. Combined head collisions during CBPC
ownership and after the player's pose reset remain untested. See
`docs/PREDATOR_HIMBO_COMPATIBILITY_2026-09-14.md`. Installed mods and the
published release remain unchanged.

The reporter's **Use my SOFTBODY physics** selection was subsequently confirmed.
An isolated XML-only contact-margin test was prepared from the hash-verified
2.0.0 release using `tools/New-PredatorHeadCollisionTest.ps1`. Each of the three
SOFTBODY profiles has exactly six zero-to-one shaft margin-multiplier edits;
all remaining bytes, dynamics, filters and weight thresholds are preserved.
The selected component contains no personal-profile margin-4 proxy shapes,
but other shaft contacts and NPCs using the same XML paths can still be affected.
Structure, exact-diff, ZIP-entry hashes and archive integrity checks passed.
The test is `out/compatibility/predator-head-test-1/` and its ZIP SHA-256 is
`CA3F3CB89300309B35B6A1209DB1045CE3692F54A36A4A40F318CB6A00324EB1`.
No DLL/bridge build, installation or release update was performed. The next
gate is the reporter's lip/throat response in soft and erect states, followed
by other collisions and ordinary switching/reset/scene recovery checks.
Do not call this a confirmed fix before those in-game results.

On 2026-09-15 the reporter's follow-up screenshots confirmed only partial,
unsatisfactory improvement from the Predator collision test. Their setup uses
Skyrim 1.6.1170, 3BA, SOFTBODY, OStim, HIMBO and TNG. They offered custom working
XMLs; those remain outstanding. The supplied SOFTBODY PPA patch v3.0 was checked
as a baseline: its two XMLs have the same 58 bone names and 11 collision-shape
names as the corresponding SPS profiles. Ten shape definitions match exactly
apart from formatting; the remaining shape retains all PPA exclusions/weights
but has SPS's extra filters and penetration setting. The patch supplies no
dynamic shaft chain or soft-profile XML, so it must not replace the SPS profiles
wholesale. Await the reporter's custom XMLs to identify the remaining differences
before another targeted test. Comparison details are in the Predator document
and `out/diagnostics/ppa-softbody-v3-20260915`. No runtime/profile/package or
installed-mod changes were made for this comparison; test 1 remains unconfirmed.

The earlier standalone XML packaging request was also completed on 2026-09-14.
`out/release/SPS-SOFTBODY-MaleGenitals-XML-2.0.0.zip` contains the three
combined SOFTBODY XMLs at their game-data paths and the original README with
credits. All four entries were hash-matched to the published 2.0.0 archive,
and the ZIP integrity check passed. This is the existing SOFTBODY option,
not a newly tested Predator patch. No DLL/bridge rebuild or install occurred.
Standalone ZIP SHA-256:
`22847076F062C234E1856F9F16225991A8524FE0630263E1F1A040B4A996A7A6`.

On 2026-09-14 the user chose to wait one week for Nexus bug reports before
considering NPC support. Hold NPC implementation until at least 2026-09-21
and until the user explicitly authorizes that phase. During this observation
period, prioritize evidence-backed player bug reports and the pending Predator
SOFTBODY collision-test result. A quiet week alone does not validate the
remaining compatibility checks or automatically authorize NPC development.

On 2026-09-15 dacama's supplied pre-menu crash report was traced to SPS's
startup diagnostics filename conversion. A symbol-only relink of the retained
2.0.0 objects produced byte-identical executable code and mapped the reported
stack to `OnMessage -> RefreshDiagnostics -> Diagnostics::Scan ->` Windows
wide-to-narrow conversion, throwing error 1113. The unchanged scanner reproduces
that error with an unrelated Unicode TXT filename in the plugin directory.
The 2.0.1 hotfix candidate compares filenames/extensions in their native encoding
and uses UTF-8 for displayed summaries, with a label fallback for malformed
UTF-16. The existing file scan is isolated within diagnostics so regression
tests exercise the actual startup scanner without Skyrim. Detection rules,
scan scope, physics ownership, APIs, Papyrus contracts, saved data, settings
and XML profiles are unchanged. The original scanner fails the Unicode TXT
reproducer with error 1113; the fixed scanner passes. The native build with
SE/AE/VR enabled, both test executables, all five Papyrus compiles, staged and
expanded ZIP validation and package rejection tests passed. With Skyrim closed,
five changed SPS code files were backed up and installed in the dedicated test
mod. Twenty selected package files and all 23 installed files were verified;
the INI, metadata, selected XMLs, private SOFTBODY override, FSMP DLL and user
links file are unchanged. Records are under
`out/diagnostics/unicode-startup-20260915`. The unpublished candidate is
`out/release/Schlong-Physics-Swapper-2.0.1.zip`, SHA-256
`B5056B31BDF2ECB1D85D9067FBEB856266DAC5C54320E7E9522DAFC6C10CEB4A`.
DLL SHA-256:
`4A0C1AA600B5C8FF2E97B2ECE7278C5573175A4793C92615C1A3A1AD3D7A21A6`.
The published 2.0.0 archive is preserved. Nothing has been republished.
The next gate is reaching the main menu and loading a save on the reporter's
unchanged setup, followed by ordinary soft/erect switching. See
`docs/STARTUP_CRASH_2026-09-15.md`. The separate Predator test and NPC hold remain.

With explicit user authorization, 2.0.1 was published on GitHub on 2026-09-15
from commit `7aa9f80267fcea662cdd0d270396e65d499bc366` and tag `v2.0.1` on
`codex/codebase-rework`. Release URL:
`https://github.com/snowman12356/Schlong-Physics-Swapper/releases/tag/v2.0.1`.
The unchanged ZIP (473,306 bytes) passed fresh expanded-archive validation,
and both regression executables passed again. GitHub reports the uploaded
ZIP's SHA-256 as the same validated hash above. The uploaded changelog was
also hash-verified. The release is public, neither draft nor prerelease.
The release notes disclose that the reporter's in-game confirmation remains
pending. The separate Predator experiment is not included in the release
asset. The copy-paste changelog is
`out/release/Schlong-Physics-Swapper-2.0.1-changelog.txt` and the release notes
are in `docs/RELEASE_NOTES_2.0.1.md`. Verification records are under
`out/diagnostics/publish-201-20260915`. No rebuild, installation, default-branch
change or Nexus upload was performed for publication. The user-owned links
file remains untracked and unmodified.

The broader compatibility checklist remains for follow-up testing; do not mark
these cases passed from the user's confirmation of the reported regressions.
Confirm that a compatible six-bone schlong is recognised automatically after
loading without using Repair current physics, and that the corrected OSL
2.9.3 query runs without the `OSLAroused.dll` access violation, then confirm
that OSL Aroused's MCM opens normally, TNG no longer logs
`Debug.SendAnimationEvent` argument errors, and SOS AE bend changes no longer
crash. Load/save boundaries, arousal changes, soft and erect equipment changes,
manual repair and SexLab behaviour still apply. Optional OStim remains a
separate experimental test when its bridge is deliberately installed. With
SOFTBODY enabled, test SexLab scene start, animation/stage changes and scene end
while SPS is set to each owner in turn; SOFTBODY's collision behaviour must
remain active and SPS must retain the selected SMP or CBPC owner after each
reload. After a full Skyrim restart, the local true-soft override must also be
tested in Always soft mode: run Repair current physics once, confirm the shaft
now responds to gravity with stable large-mesh motion rather than folding or
oscillating excessively, then repeat normal, vaginal and anal scene transitions
and confirm the scrotum and collision behaviour remain stable. Separately test
the packaged personal profile with compatible SOS, TNG and UBE six-bone builds.
Test the packaged generic combined option with SOFTBODY and a compatible
six-bone build, confirming that its single merged `MaleGenitals` shape supports
collisions in all three SOFTBODY profiles. Managed NPC support remains outside
this release and requires a separate authorized phase after broader player
coverage is complete.
