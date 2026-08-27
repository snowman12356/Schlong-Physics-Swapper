# Schlong Physics Swapper reliability and cleanup audit

Date: 25 August 2026; updated 27 August 2026

## Result

The complete tracked mod was reviewed as one system: the SKSE DLL, automatic and
manual switching, soft/erect position handling, armour and mesh recovery,
SexLab/OStim/arousal bridges, the MCM diagnostics, FOMOD, documentation and
release scripts.

The final `1.9.2` DLL and FOMOD package have been built and validated. The
preceding test build was installed in the local MO2 mod and its latest targeted
in-game test passed. The source, DLL and staged package also pass the available
build and static checks.

## Main problems found

1. Some physics confirmation paths could retry indefinitely when an external
   Papyrus/API call never became available. This could create repeated work and
   misleading activity after a failed handoff.
2. While CBPC already owned the schlong, the ordinary one-second arousal poll
   still sent another FSMP disable call. This was redundant and added Papyrus
   traffic without proving which engine actually owned the bones.
3. Not every delayed bend, confirmation and repair timer was cleared between
   saves or new games. A delayed action from the previous session could
   therefore run during the next load.
4. A player-only SMP reset could be followed by another complete ownership
   switch. That duplicated work and could make soft-state recovery less
   predictable.
5. The quick-fix buttons could appear to do nothing when used before the player
   and Papyrus were ready.
6. A successful recovery could leave an older red handoff/reset error visible
   on the Troubleshooting page.
7. The MCM and installer treated an arousal mod as mandatory even when the user
   selected manual Keep soft or Keep erect mode.
8. The package validator had the old release number hard-coded, making future
   version changes easier to get wrong.
9. Startup could choose soft physics before the first real arousal result, then
   switch to erect and later run a now-obsolete SMP reset. On a busy Papyrus VM
   that sequence could take nearly a minute to settle into the correct state.
10. Some armour managers replace the player's genital mesh without sending the
    NiNode update SPS previously relied upon. While erect, CBPC could therefore
    remain attached to the old bones and the replacement mesh would use SMP.

## Changes made

### Physics ownership and recovery

- Removed the redundant FSMP call from the normal same-state CBPC poll.
- Kept deliberate ownership checks after a real switch, mesh change, external
  reset or repair request.
- Added firm time limits to post-switch, external-reset, SMP, CBPC and quick-fix
  confirmation windows. Failed external calls now stop cleanly instead of
  becoming background retry loops.
- Reasserted both halves of an erect handoff in the correct order: FSMP off,
  then CBPC on.
- Reasserted both halves of a soft handoff in the correct order: CBPC off, then
  FSMP on.
- Replaced duplicate complete switches after player SMP refreshes with a
  targeted confirmation of the already-selected owner.
- Centralised all temporary timer cleanup and run it before and after loading a
  save or starting a new game. Bend confirmations, mesh repairs, queued quick
  actions and ownership retries can no longer leak into the next session.
- Kept the startup reconciliation which waits for a usable player, reads the
  current mode/arousal and then restores the correct owner.
- Successful ownership or SMP refresh recovery now clears the matching old red
  error instead of leaving a resolved problem on screen.
- Startup automatic mode now waits for the first usable arousal result instead
  of temporarily forcing soft physics. If the first selected state is erect,
  the obsolete delayed SMP reset is cancelled.
- Reduced the post-load dispatch gate from three seconds to one second and
  removed the unconditional startup soft handoff. This gets the initial state
  decision started sooner while still allowing the new Papyrus VM to bind.
- Added a direct player armour equip/unequip listener as a fallback for armour
  managers that do not emit an NiNode update.
- An erect mesh replacement now releases bindings to the old bones and performs
  a bounded CBPC reacquire against the rebuilt mesh before replaying its angle.
  Soft mesh recovery continues to use the player-only SMP reset path.
- Cancels every delayed erect-angle settle, confirmation and retry as soon as
  the active control source wants soft. This prevents a failed or delayed SMP
  handoff from raising 0 back toward the erect angle at zero arousal.
- The soft-handoff refresh now freezes only the six genital SMP bones before
  the player-only full reset, then restores SMP after the reset settles. Hair,
  clothing and other actors are not affected.

### Buttons and reports

- Soft, erect and Repair current physics actions now queue briefly while the game
  is still loading, then run once the player and Papyrus are ready.
- Queued actions have a 15-second limit and report `SPS-020` if loading never
  finishes.
- The diagnostics now label the owner counter as confirmed owner restorations,
  which describes what it actually measures.
- Added support descriptions for `SPS-018`, `SPS-019` and `SPS-020`.

### Menu flow and wording

- Renamed the five pages to Home, Appearance, Scenes, Advanced and
  Troubleshooting so their purpose is obvious.
- Reworked Home around current status, main controls, the existing soft/erect
  position controls and quick actions. Manual mode no longer shows an empty
  arousal bar or a disabled automatic threshold.
- Kept Appearance focused on transitions and moved random/morning erection
  options into a single optional expandable section.
- Put normal scene controls before compatibility details and moved scene status
  into an expandable section.
- Grouped automatic recovery and angle control on Advanced, with low-level timing
  controls collapsed by default.
- Moved setup problems to the top of Troubleshooting, renamed the physics test
  buttons to state their ten-second duration, and kept file/API/activity details
  out of the default path.

### Manual mode and installer

- Manual Keep soft and Keep erect modes no longer require an arousal mod for
  the core-ready check.
- The Home and Help pages explicitly say that arousal is not used in manual
  mode.
- Added a FOMOD choice for `No arousal mod (manual modes only)`.
- Updated the README so automatic mode and manual mode requirements are clear.

### Packaging and maintenance

- Made the release-package checker read or accept the requested version instead
  of being locked to `1.9.1`.
- Updated the package builder to pass its version into the validator.
- Reviewed every tracked bridge, script, configuration and public API file.
  The SexLab bridge, experimental OStim bridge, OSL compatibility script,
  Papyrus build stubs and author API are all actively used or required for a
  supported build, so none were deleted.
- Removed redundant runtime operations rather than deleting files merely
  because they are optional. Generated build and staging folders remain outside
  the release contents.

## Compatibility reviewed

- The core DLL still builds as one SE, AE and VR multi-runtime binary.
- SOS AE native/event handling, legacy SOS fallback and TNG/event handling
  remain separate paths; the audit did not merge them into one unsafe call.
- OSL Aroused, SLO Aroused NG and classic Aroused Redux provider paths remain.
- SexLab scene-role handling remains part of the normal package.
- OStim remains an optional, experimental bridge.
- Current OSL Aroused 2.9.3+ is used through its native arousal interface with
  OSL's **Enable SOS** option disabled. The player-only compatibility script is
  retained only as an optional legacy choice for OSL 2.9.0 through 2.9.2.

## Validation performed

- Release x64 DLL compiled successfully with the project's strict `/W4` warning
  settings.
- DLL exports verified:
  - `SKSEPlugin_Load`
  - `SKSEPlugin_Query`
  - `SKSEPlugin_Version`
  - `SchlongPhysicsSwapper_GetAPI_V1`
- DLL dependency inspection found only the expected Windows and Microsoft
  runtime libraries.
- Git whitespace/error check passed. The only messages were existing Windows
  line-ending notices.
- Both PowerShell release scripts parsed successfully.
- FOMOD XML parsed successfully and all 15 installer source entries resolved.
- The staged package contained every required DLL, INI, CBPC file, PEX bridge,
  API header and document.
- All six CBPC genital bones and all six SPS CBPC parameter groups were found.
- No SMP XML is bundled, as designed; the compatible schlong addon supplies it.
- Compiled PEX symbols for the SexLab, OStim and OSL compatibility bridges were
  checked.
- A dead-code/reference pass found no tracked function or bridge that could be
  safely removed. The apparent single-reference functions are callbacks passed
  to SKSE or the menu framework.

Latest built DLL SHA-256:

`92CEF553738597625C99A29D6FE6654396DDF40D9F4F08C6D255C452CFED1F61`

## Local installation

Installed DLL:

`D:\Modding\mods\Schlong Physics Swapper\SKSE\Plugins\SchlongPhysicsSwapper.dll`

Backup of the DLL that was installed before this audit:

`D:\Modding\mods\Schlong Physics Swapper\SKSE\Plugins\SchlongPhysicsSwapper.pre-audit-20260825.bak`

Backup of the reliability-only build from immediately before the menu redesign:

`D:\Modding\mods\Schlong Physics Swapper\SKSE\Plugins\SchlongPhysicsSwapper.pre-ui-20260825.bak`

Backup made immediately before the startup and armour-event recovery build:

`D:\Modding\mods\Schlong Physics Swapper\SKSE\Plugins\SchlongPhysicsSwapper.pre-startup-armour-20260826-0025.bak`

The tested pre-release DLL contained the same behaviour as the final build; the
final compile changes the embedded version label from `1.9.2-test` to `1.9.2`.
It cancels stale erect-angle work as soon as soft is requested and freezes only
the six genital bones during their player-only SMP reset.

## Recommended regression checks

The latest targeted in-game check passed. Future regression testing should still
cover the following paths because an SKSE DLL is loaded only when the game
process starts and static tests cannot reproduce every load-order timing case:

1. Load a save below the threshold and confirm the first settled state is SMP
   without a long delayed correction.
2. Load a save already above the threshold and confirm SPS selects CBPC directly
   without first switching to soft or running a later SMP reset.
3. Raise arousal above the threshold and confirm CBPC and the selected erect
   angle are applied.
4. Lower arousal and confirm the angle lowers gradually and SMP takes ownership
   as soon as the selected soft angle is reached.
5. Equip and remove armour while soft, then while erect. While erect, confirm
   CBPC remains the final owner of the replacement mesh.
6. Switch schlong/addon while soft, then while erect.
7. Use Test soft, Test erect and Repair current physics on the Troubleshooting
   page and confirm each produces a visible temporary result or a clear
   loading-time message.
8. Load another save or start another new game and confirm no state from the
   previous session is replayed.
9. Test manual Keep soft/Keep erect without an arousal mod if that installer
   option is going to be advertised.
10. Test an OStim scene separately and keep it labelled experimental until that
   path has wider user testing.

If those checks pass, this is suitable to turn into the next release candidate.
Until then it should remain a local test build and should not replace the public
archive.
