# Schlong Physics Swapper 1.9.6

Native SKSE plugin for compatible SOS six-bone schlongs. Faster HDT-SMP owns
Gen01-Gen06 while arousal is below a configurable threshold; CBPC owns them
above it. Settings are available through SKSE Menu Framework.

## Development disclosure

This mod was created through user-directed AI-assisted development (sometimes
called **vibe coding**), followed by repeated compilation, in-game testing, and
debugging. The complete source is public so users and experienced developers
can inspect it, report issues, or contribute improvements. AI-assisted code can
still contain mistakes, so useful bug reports and diagnostic logs are welcome.

## Permissions and credit

All original Schlong Physics Swapper code, configuration, and documentation are
released under the [MIT License](LICENSE). You may freely use, copy, modify,
redistribute, include, or build upon them, including in your own mods, provided
you preserve the licence notice and give credit as:

> Schlong Physics Swapper by snowman12356

Please link to the original GitHub or Nexus page where practical. Third-party
projects used through public compatibility interfaces remain under their
respective licences; see [THIRD_PARTY.md](THIRD_PARTY.md). The optional OSL
Aroused 2.9.0-to-2.9.2 compatibility file is distributed under OSL Aroused's
Unlicense.

The author may participate in Nexus Mods' Donation Points programme. This does
not restrict these permissions or place any feature, update, support, or file
behind payment. The mod remains freely available under the licences included
with it.

## Requirements

- Skyrim SE 1.5.97, AE 1.6.x, or Skyrim VR 1.4.15, with the matching SKSE
- Address Library for SKSE Plugins
- SKSE Menu Framework 3
- Faster HDT-SMP 4.0.1 or newer; 4.1.1 or newer is recommended (select the build matching your Skyrim runtime)
- CBPC 
- OSL Aroused, SLO Aroused NG, or classic SexLab Aroused Redux for Automatic
  mode. Manual Keep soft and Keep erect modes work without an arousal mod.
- A compatible SOS addon with SMP physics 

Crash Logger is optional and only needed when reporting a Skyrim crash. Physics
Editor may stay installed; disable only its schlong controls if it starts
changing the same physics as SPS. Auto Physics Reset is optional; disable its
overlapping load, cell or scene triggers if it changes SPS's chosen state.

Schlongs of Skyrim AE is supported through `SOSAE_SKSE.SetSchlongBend`.
Legacy SOS is supported through its `SOSFlaccid`/`SOSBend0`-`SOSBend9`
animation events.

The New Gentleman is also supported. TNG uses the same Gen01-Gen06 skeleton
nodes and SOS-style animation events, so physics switching and position control
do not require TNG files to be patched or redistributed. TNG does not bundle a
physics configuration; the active genital mesh must still point to a valid
`MaleGenitals.xml` or another complete Gen01-Gen06 SMP XML.

The package includes a dedicated CBPC map and parameter file for all six shaft
bones. It does not depend on another SOS CBPC preset being active.

OSL Aroused is supported through its native arousal interface. Leave OSL's
**Enable SOS** option disabled so its arousal-based position changes do not
compete with SPS for the player. This also disables OSL's automatic SOS angles
for NPCs outside scenes; SexLab and OStim scene animations still control their
participants normally. Users who remain on OSL 2.9.0 through 2.9.2 can instead
select the clearly labelled legacy FOMOD option. That optional script
excludes only the player and retains the old OSL NPC angle behavior. It must not
be installed with OSL 2.9.3 or newer, where angle control moved into OSL's DLL.

SLO Aroused NG is supported by reading its standard cached `sla_Arousal`
value. This avoids adding a repeating Papyrus request to busy load orders. When
using SLO, leave its **Use SOS** option disabled so it does not send competing
`SOSFlaccid`/`SOSBend` events. SPS does not replace SLO's native DLL.

Classic SexLab Aroused Redux is supported as a fallback by reading the
player's public `sla_Arousal` faction rank. OSL and SLO remain higher-priority
providers when installed. Leave classic SLA's **Enable SOS** option disabled so
its position events do not compete with this plugin. No SLA script is replaced.

SexLab P+ is optional. When present, the plugin checks P+'s directional stage
interactions throughout player scenes. While receiving or servicing a partner,
the state from immediately before the scene is retained: flaccid stays flaccid
and hard stays hard. The player uses CBPC while penetrating or having their
penis serviced.

The Scenes page can change bottom/receiving behaviour to follow live arousal,
always use SMP, or always use CBPC. Keeping the pre-scene state is the default.
Unknown or unregistered stages keep the current engine by default, preventing
visible pops. Normal arousal control resumes after a configurable delay.

OStim Standalone support is optional and experimental. When its FOMOD option is
installed, SPS listens for player OStim scenes and checks whether the player is
on top or on the bottom. Top/penetrating uses CBPC. Bottom/receiving follows the
same user choice as SexLab: keep the pre-scene state, follow arousal, always use
SMP, or always use CBPC. Normal arousal control resumes after the scene delay.
OStim itself is not included, and this feature does not change OStim scenes or
animation positions.

PPA (Procedural Penis Animations) is optional and compatible. Current PPA
versions are detected through its [documented V1 listener API](https://asdasdduck.github.io/ppa-docs/skse-api.html), with a safe DLL
fallback for older builds. During a PPA scene, PPA owns genital position and
SPS changes only the SMP/CBPC physics owner. SPS restores the selected erect
position after the scene instead of sending competing bend events.

## Mod-author compatibility API

SPS now exposes an optional native V1 API for other SKSE plugins. A mod can
detect SPS at runtime, read the current physics state, temporarily ask SPS to
hold SMP or CBPC, receive state-change notifications, and then release control
back to normal arousal handling. A cooperating mod can also notify SPS after a
physics reset so the selected owner is restored once the rebuild settles. No
new requirement is added for players.

The API uses actor FormIDs from the beginning, but V1 intentionally supports
only the player. That leaves room for selected-NPC support later without
replacing the interface. The public header and examples are in
[SPSAPI.h](src/SPSAPI.h) and the [mod-author guide](docs/MOD_AUTHOR_API.md).

## Settings

Open SKSE Menu Framework in game, then select **Schlong Physics Swapper**.
The streamlined **Home** page provides the core controls and the two positions
SPS switches between:

- live arousal, current physics engine, and overall health
- plain-language Automatic, Always Soft, and Always Erect modes
- soft and erect angle controls in one place
- automatic soft-angle refresh for SOS AE-NG, without a manual SMP reset
- a simple recommended-settings button

The **Appearance** page controls the transition between those positions:

- an optional erection that rises gradually with live arousal
- separate rise and softening times, so an erection can fade naturally instead
  of dropping immediately
- optional random erections with separate shortest and longest intervals
- an optional normal-gameplay guard that waits during combat, dialogue, loading
  and paused menus
- optional recovery time between spontaneous erections and an optional erection
  after a long sleep or wait
- a test button so random behaviour can be checked immediately

SexLab and OStim controls have their own **Scenes** page. Rare timing,
recovery and compatibility controls live under **Advanced** and are collapsed
until needed. **Troubleshooting** shows the important setup results first,
keeps file names and counters inside optional detail sections, and uses normal
language for quick fixes and support reports.

Defaults are threshold 60, hysteresis 5, bend 14, and a 1000 ms polling
interval. Settings are saved to
`Data/SKSE/Plugins/SchlongPhysicsSwapper.ini`. The packaged default
automatically imports an existing `UBEPhysicsSwitch.ini` once, preserving the
old settings even when both files are initially present.

The Nexus archive includes a simple FOMOD. You manually choose OSL Aroused,
SLO Aroused NG, SexLab Aroused Redux, or no arousal mod for manual-only use,
so the installer does not have to guess.
The current OSL, SLO NG and Redux choices install no extra SPS compatibility
file; leave each arousal mod's SOS position option disabled. OSL 2.9.0 through
2.9.2 has a separate legacy option that installs the old player-only override. If
updating to OSL 2.9.3 or newer, replace the old SPS mod rather than merging the
new archive into it so that override is removed. A separate optional choice
installs the experimental OStim role bridge. SOS AE, legacy SOS and TNG all use
the same SPS core DLL. New users can install the recommended settings; updating
users can choose **Keep my existing settings** so their INI is not replaced.
Physics Editor and Auto Physics Reset notices are shown on the in-game
**Troubleshooting** page and do not block installation.

## Behavior

- Soft/below threshold: CBPC is stopped first, then SMP is enabled.
- With optional soft-angle control enabled, SOS AE-NG can hold the selected
  flaccid angle while SMP still owns the six physics bones. Legacy SOS and TNG
  keep their normal floppy animation-event position.
- Changing the soft angle performs one debounced player-only SMP refresh and
  reapplies the selected pose after FSMP settles.
- Above threshold: SMP is disabled first, then CBPC is started.
- With arousal-based rising enabled, CBPC starts at the selected starting
  arousal and the bend rises with arousal until the normal threshold is reached.
- Optional random erections choose a fresh delay between the user's minimum
  and maximum after every event. They pause during scenes and external API
  control, and can also wait during combat, dialogue, loading and paused menus.
  They soften gradually when the event ends, then observe the selected recovery
  time before another can start. Normal arousal control then resumes.
- Optional morning erections can start after sleeping or waiting for at least
  three in-game hours. They use the same safe-context and gradual-softening rules.
- Changing schlongs or equipment while soft triggers one debounced player-only
  SMP refresh, then restores the selected soft state and angle. This prevents
  stale FSMP chains from leaving the tip drifting or hanging in place.
- SexLab P+ bottom/receiving role: retain the pre-scene physics state.
  Top/penetrating role: CBPC.
- Experimental OStim player scenes use the same bottom/top choices when the
  optional bridge is installed.
- PPA controls live scene position when installed; SPS does not send competing
  SOS bend or flaccid commands while PPA is active.
- With gradual erection enabled, the native SOS AE position eases from 0 to the
  selected erect bend after CBPC settles. Only changed integer bend values are
  sent, avoiding per-frame Papyrus traffic and animation-event bouncing.
- A 5-point default hysteresis keeps ownership stable around the threshold.
- About 10 seconds after loading a save or starting a new game, SPS resets the
  player's SMP once and then restores the correct soft/erect state. This can be
  turned off or delayed on the Advanced page.
- Handoffs are only marked successful when both external Papyrus APIs accept
  every request. Failed handoffs are retried and the last confirmed state is
  restored on a best-effort basis.
- A switch cooldown prevents rapid ownership changes.
- After a handoff, mesh change, or notified external physics reset, SPS runs a
  short bounded confirmation that restores the selected owner if needed. It
  does not send an FSMP command on every ordinary arousal poll.
- The erect bend is applied after a CBPC handoff and confirmed once more after
  CBPC has fully settled. Slider changes receive one debounced final
  confirmation. Blind periodic replays are avoided because they cause visible
  bouncing.
  Both the SOS animation event and SOS AE's finer native 0-20 API are used.
  Animated events run only on a genuine state or slider change; automatic OSL
  checks do not replay an already accepted position.
- Position control can be suspended independently while SMP/CBPC switching stays
  enabled, allowing another mod to own the angle.
- A bounce guard pauses overly frequent automatic repairs for five seconds. A
  separate failure limit stops automatic recovery when SOS repeatedly rejects
  requests; the menu can resume it without restarting the game.
- Settings are global in the INI and are shared by all characters and saves.
- Player skeleton updates queue one delayed position repair for outfit/body swaps
  without forcing a complete physics handoff.
- Procedural Penis Animations is detected at runtime. During an active SexLab
  scene it owns the genital position; this mod restores the selected SOS angle
  after the scene instead of fighting over the same bones.
- Arousal queries stop while the plugin is disabled or a physics engine is forced.
- No ESP, quests, save-game records, or SkyUI dependency beyond Menu Framework.

## Installation

1. Install the requirements listed above.
2. Install the GitHub release archive with MO2 or Vortex. When updating, replace
   the old SPS mod rather than merging files into it.
3. Ensure **Schlong Physics Swapper** wins conflicts for its two `ZZZ` CBPC
   files. OSL Aroused users must turn off **Enable SOS**; SLO Aroused NG users
   must turn off **Use SOS**; Redux users must turn off **Enable SOS**. The only
   exception is the legacy OSL 2.9.0-to-2.9.2 installer option, which provides
   its own player exclusion.
4. Start Skyrim through SKSE and open the Schlong Physics Swapper section in
   SKSE Menu Framework.

The public archive contains the SKSE DLL, INI template, dedicated CBPC files and
an optional OSL 2.9.0-to-2.9.2 compatibility script. It does not contain an
ESP, body meshes, SMP XML, or any other OSL files.

## Troubleshooting page

The Troubleshooting page shows loaded and live connection state for:

- SKSE Menu Framework
- Arousal provider (OSL, SLO NG, or classic SexLab Aroused)
- Faster HDT-SMP
- CBPC
- SOS AE bend support
- The New Gentleman position support
- the supported schlong addon and six live Gen01-Gen06 skeleton nodes
- SexLab P+
- OStim Standalone and its optional role bridge
- Procedural Penis Animations

Its setup check scans the active MO2 virtual `Data` directory for SMP XMLs
with a complete `<system>` and all six genital bones. It also checks CBPC
master maps for Gen01-Gen06 and CBPC parameter files for UBEPS01-UBEPS06.
The page shows recent handoffs/errors and can copy or save a privacy-safe
diagnostic report. It also includes temporary verbose logging and a 30-second
capture that records the state once per normal poll while the user reproduces
the problem.

## Log and report

The log is written to the normal SKSE log directory as
`SchlongPhysicsSwapper.log`. The plugin is compiled for Address Library based SE/AE
runtime independence. External APIs are called dynamically through Papyrus,
so missing optional SOS bend APIs fall back safely without a hard DLL link.
Saved reports are written to
`Data/SKSE/Plugins/SchlongPhysicsSwapper_Diagnostics.txt`.
Debug captures are written to
`Data/SKSE/Plugins/SchlongPhysicsSwapper_DebugCapture.txt`. Reports include
versions, settings, connection state, relevant configuration filenames and
recent events. They do not include the Windows username, save name, or full
computer paths.

When reporting a problem, attach the diagnostic report or 30-second capture and
include the schlong addon, Skyrim runtime, mod-manager name, expected result,
actual result, and short reproduction steps. See [SUPPORT.md](docs/SUPPORT.md).

## 1.9.6 changes

- Fixed a save-loading crash on heavily scripted games when SPS tried to
  dispatch a physics request before the player's cell and body were ready.
- Moved FSMP player lookup into the SPS Papyrus bridge, so the native plugin no
  longer passes an Actor through the unsafe startup path.
- Applied the same protection to normal FSMP enable/disable calls.
- Made each FSMP/CBPC ownership change one ordered Papyrus transaction with a
  short settle period between releasing one engine and enabling the other.
- Reduced each normal ownership change from seven Papyrus jobs to one.
- Added a DLL/bridge compatibility check so an incomplete update fails safely
  instead of dispatching a missing Papyrus function.

## 1.9.5 changes

- Moved FSMP's six-bone physics list into a required Papyrus bridge instead of
  allocating the array from the native plugin during startup.
- Fixed the remaining timing-dependent crash when SPS switched FSMP physics
  while a busy game was still completing its load transition.
- Added setup checks and support-report details for a missing SPS FSMP bridge.

## 1.9.4 changes

- Fixed a startup crash that could occur when SPS asked FSMP to switch the
  player's physics state.
- Made SPS safely handle the result returned by FSMP's `TogglePhysics` call.

## 1.9.3 changes

- Prevented a startup crash on Skyrim 1.5.97 when an older FSMP build was
  installed.
- Added a proper FSMP version check before SPS calls its actor physics API.
- Older unsupported FSMP versions now show a clear update warning instead of
  crashing or leaving a half-applied physics state.
- Set FSMP 4.0.1 as the minimum supported version, with 4.1.1 or newer
  recommended.
- Reduced OSL and SLO arousal polling while the value is stable, while still
  reacting immediately when the arousal mod reports a change.
- Avoided unnecessary physics evaluations when the arousal value has not
  changed.

## 1.9.2 changes

- Made physics switching more reliable after loading a save, starting a new
  game, equipping armour, removing armour, changing schlongs and rebuilding the
  player mesh.
- Added bounded verification and recovery so SPS can restore the selected SMP
  or CBPC owner without creating an endless retry loop.
- Fixed erect physics sometimes keeping SMP, losing its angle or visually
  staying soft after an equipment change.
- Fixed soft physics sometimes keeping an incorrect stretched pose until a
  manual FSMP reset.
- Made the troubleshooting test buttons hold their result for 10 seconds and
  queue safely while the game is still loading.
- Prevented disabled plugins such as SOS Physics Manager from being reported as
  active conflicts.
- Reorganised the MCM into Home, Appearance, Scenes, Advanced and
  Troubleshooting pages without changing existing saved settings.
- Added separate OSL installer choices: current OSL 2.9.3+ uses its native
  arousal interface with **Enable SOS** disabled, while OSL 2.9.0 through 2.9.2
  can use the optional legacy player-only compatibility script.
- Added a repeatable local Windows build script and strengthened release-package
  validation.

## 1.9.1 changes

- Fixed SPS sometimes missing OSL Aroused updates at high arousal.
- Fixed the erect angle resetting to 0 after loading or changing equipment.
- Improved recovery after armour, schlong and player mesh changes.
- Fixed a severe slowdown when a soft-position update was rejected.
- Added separate OSL installer choices: current OSL 2.9.3+ uses native arousal
  with **Enable SOS** off, while OSL 2.9.0 through 2.9.2 can still use the
  optional player-only legacy compatibility script.

## 1.9.0 changes

- Added optional experimental OStim Standalone scene support.
- Added an optional flaccid-angle setting for SOS AE-NG that refreshes without
  requiring a manual SMP reset.
- Added optional erections that rise gradually with live arousal.
- Added partial erections between the chosen starting and fully erect arousal.
- Added separate rise and softening times, including gradual recovery when a
  spontaneous erection is interrupted.
- Added optional random erections with user-set minimum and maximum intervals.
- Added an optional normal-gameplay guard for random erections.
- Added an optional recovery delay between spontaneous erections.
- Added optional erections after a long sleep or wait.
- Added automatic soft SMP recovery after changing schlongs or equipment.
- Kept subtle movement and follow-through in the supplied physics values rather
  than making the erect state completely rigid.
- Moved soft and erect position controls to Home and kept transition behaviour
  on the clearer Appearance page.
- Kept one DLL for Skyrim SE, AE and VR.
- Made legacy SOS detection safer and added a clear warning for SOS Physics
  Manager conflicts.
- Kept OStim optional: users who do not select it install no OStim bridge.

## 1.8.2 changes

- Added Skyrim VR support to the universal SKSE plugin build.
- Made OSL Aroused connect sooner after loading a save.
- Fixed SOS AE-NG being mistaken for legacy SOS, while keeping Skyrim 1.5.97
  on the safe legacy route.
- Fixed the help page showing a false erect-angle error or reporting
  `none - ready`.
- Stopped gradual erection from trying to run when no compatible angle
  controller is available.

## 1.8.1 changes

- Fixed the remaining Skyrim 1.5.97 crash when gradual erection was enabled.
- Made every SOS AE bend call use the same safety check.

## 1.8.0 changes

- Added a small compatibility API so other mods can work with SPS safely.
- Added protection and recovery for outside physics resets.
- Improved SexLab scene switching so the physics do not fight each other.
- Improved SLO Aroused NG and SexLab Aroused Redux support.
- Added a manual FOMOD choice for OSL, SLO NG or Aroused Redux.
- Fixed a crash on Skyrim 1.5.97 with legacy SOS.
- Added safer handling while loading saves or when Papyrus is overloaded.

## 1.7.2 changes

- Rebuilt the menu around Home, Appearance, Scenes, Advanced and
  Troubleshooting pages.
- Moved SexLab options out of the general advanced page so scene behaviour is
  easier to understand.
- Reworded controls and status messages in normal language.
- Changed millisecond sliders to seconds where people actually adjust them.
- Hid rare timing, file and activity details inside optional sections.
- Removed duplicate reset buttons, repeated status rows and technical clutter
  from the everyday pages.
- Simplified the troubleshooting flow and made support-report tools clearer.

## 1.7.1 changes

- Added player role-aware SexLab P+ switching: bottom/receiving keeps the state
  from immediately before the scene, while top/penetrating uses CBPC.
- Added selectable bottom behaviour: keep the entry state, follow live arousal,
  always SMP, or always CBPC.
- Added a safe unknown-role fallback that keeps the current physics owner by
  default, plus Advanced-page controls.
- Added current-stage refreshes for P+ start, stage, actor-change, relocation,
  ending and cleanup events.
- Added optional PPA V1 listener integration for live role updates.
- Prevented all SPS bend/flaccid commands while PPA owns scene position, then
  restored the selected position after the scene.
- Added role bridge, PPA API and current role details to diagnostics.
- Added an optional one-time player SMP reset 10 seconds after loading, followed
  by an automatic restore of the correct SPS physics state.
- Fixed bottom-role polling repeatedly sending the soft-position command and
  fighting the active scene animation.

## 1.7.0 changes

- Initial player role-aware SexLab P+ and PPA compatibility test build.

## 1.6.2 changes

- Added The New Gentleman detection, six-bone physics support and SOS-style
  position fallback.
- Added gradual TNG erection using its available animation-event stages.
- Added classic SexLab Aroused Redux as the lowest-priority arousal provider.
- Added provider/body/backend details and conflict guidance to troubleshooting
  reports.

## 1.6.0 changes

- Added official SLO Aroused NG support through its built-in OSL compatibility API.
- Added SLO DLL detection and provider-specific troubleshooting status.
- Added immediate refresh support for SLO's `sla_UpdateComplete` event.
- Added a warning to leave SLO's **Use SOS** option disabled to prevent position conflicts.

## 1.5.0 changes

- Added Skyrim, SKSE, plugin and dependency version details to reports.
- Added stable `SPS-xxx` problem codes with plain-language suggested fixes.
- Added one-click soft/erect tests and a repair-physics action.
- Added optional verbose logging and a guided 30-second debug capture.
- Added privacy wording, support instructions, and a GitHub bug-report form.

## Building

The project requires Visual Studio 2022, CMake 3.24 or newer, vcpkg, and the
[CharmedBaryon/CommonLibSSE-NG](https://github.com/CharmedBaryon/CommonLibSSE-NG)
checkout. Set `COMMONLIB_SSE_FOLDER` to that checkout, configure with the vcpkg
toolchain, then build the `SchlongPhysicsSwapper` target. The Menu Framework SDK
headers are fetched during configuration. For a VR-capable build, initialise
CommonLibSSE-NG's `extern/openvr` submodule before configuring.

The optional OSL 2.9.0-to-2.9.2 compatibility override is stored in
`compat/OSL Aroused` with its patched Papyrus source and upstream Unlicense. It
is not compiled by CMake and is placed only inside the release archive's legacy
optional folder.

### Reliable Windows build command

Long OneDrive paths can make Ninja lock up or leave generated projects tied to
a temporary drive that no longer exists. `tools/Build-Local.ps1` avoids both
problems: it uses the Visual Studio generator, maps the workspace to a short
temporary drive for configuration and compilation, then removes that mapping.

Set `COMMONLIB_SSE_FOLDER` and `VCPKG_ROOT`, or pass their paths directly:

```powershell
.\tools\Build-Local.ps1 `
    -CommonLib 'C:\path\to\CommonLibSSE-NG' `
    -VcpkgRoot 'C:\path\to\vcpkg'
```

By default, the heavy CMake build cache is kept under
`%LOCALAPPDATA%\SPSBuild` instead of OneDrive. The script reuses that cache,
skips a full configure when possible, and builds with four parallel jobs.
The first build still has to compile CommonLib, but later builds should be
much faster.

The finished DLL is copied to `out\build\SchlongPhysicsSwapper.dll`, ready
for testing or packaging. Useful optional switches are:

```powershell
.\tools\Build-Local.ps1 -Jobs 8
.\tools\Build-Local.ps1 -Reconfigure
```

Set `SPS_BUILD_ROOT` if you want the persistent build cache on another fast
local drive. To package this output, use:

```powershell
.\tools\New-ReleasePackage.ps1 -BuildDirectory out\build
```
