# Schlong Physics Swapper 2.3.0.0

Local development build, not a published release; the latest release is 2.0.1.
Optional NPC support is on the **NPCs** page and is off by default.

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
released under the [GNU General Public License v3.0 or later](LICENSE). You may
use, copy, modify, and redistribute them, including in your own mods, under the
terms of that licence. In practice that means:

- If you distribute SPS, or a mod built from it, you must also make the
  corresponding source available under GPL-3.0-or-later.
- You must preserve the licence notice and give credit as:

> Schlong Physics Swapper by snowman12356

- Because the DLL statically links CommonLibSSE-NG v15+, which is
  GPL-3.0-or-later, the distributed binary is a GPL-3.0-or-later combined work.
  See [THIRD_PARTY.md](THIRD_PARTY.md) for that dependency's exact terms.

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

Schlongs of Skyrim AE is supported. SPS calls only SOS AE's read-only addon and rank
getters. Schlongs of Skyrim AE 4.0.0 removed `SOSAE_SKSE.SetSchlongBend`, so SPS sets the angle through
the animation events SOS still ships, `SOSBend-9` (lowest) to `SOSBend9`
(highest), using the same mapping as SOS's own code. Legacy SOS and TNG use the
same 19 events.

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

SOFTBODY is detected automatically. When its scene scripts dynamically reload a
player's genital SMP XML, SPS waits for the reload to settle and reapplies the
physics owner it had already selected. By default SPS does not replace
SOFTBODY's XML choices or collision equipment. The FOMOD offers three physics
choices: keep an existing compatible XML, install the SPS author's personal
six-bone physics, or install the combined **SPS SOFTBODY physics**. Both SPS
profiles support compatible SOS, TNG and UBE six-bone meshes. The SOFTBODY
choice merges the SPS shaft physics and collision exclusions with SOFTBODY's
normal, soft and anal collision profiles. When selected, make SPS win conflicts
for all three `MaleGenitals` XML files.

**Predator SMP Head compatibility is under investigation.** A user reports
that its lips and throat stop responding to genital collisions with SPS
installed. Both SPS physics options include the `MaleGenitals` shape and tag
filters from `TNG_Himbo_SMP_XMLs`, but that does not preserve every collision
property: SPS's shaft bones use zero collision-margin multipliers. The
incoming standalone genital XML has no active shaft dynamics, so replacing
SPS's profile with it also loses the supplied floppy simulation. Keep the
separate HIMBO and Predator files; no tested compatibility fix is available
yet. See [the investigation](docs/PREDATOR_HIMBO_COMPATIBILITY_2026-09-14.md)
for the evidence and remaining checks.

When PPA/Accurate Penetration or SOFTBODY may have changed a soft shaft during
a scene, SPS performs one delayed player-only SMP pose refresh after scene
cleanup. Equipment recovery, ownership handoffs, and bend replay are serialized
so they cannot race each other on replacement genital nodes.

## Mod-author compatibility API

SPS exposes an optional native API (V1-V5) for other SKSE plugins: read
physics, arousal and scene state, temporarily hold SMP or CBPC, receive
state-change notifications and report physics resets. Later versions add
NPC reads. No new requirement is added for players. See
[SPSAPI.h](src/SPSAPI.h) and the [mod-author guide](docs/MOD_AUTHOR_API.md).

## Settings

Open SKSE Menu Framework in game, then select **Schlong Physics Swapper**.
The streamlined **Home** page provides the core controls and the two positions
SPS switches between:

- live arousal, current physics engine, and overall health
- plain-language Automatic, Always Soft, and Always Erect modes
- soft and erect angle controls in one place
- automatic soft-angle refresh, without a manual SMP reset
- a simple recommended-settings button

The **Advanced** section of the Appearance page lets you choose how SPS sets the angle:

- `Choose automatically` (recommended): uses the SOS/TNG animation stages.
- `SOS / TNG animation stages`: sends the 19 `SOSBend-9`-`SOSBend9` graph events.
- `SPS pose angle` (experimental): writes `NPC GenitalsBase [GenBase]` directly.
  The SOS/TNG animation rewrites that bone every frame, so this usually does not
  move the angle; it remains only for diagnosis.

The **Appearance** page controls the transition between those positions:

- an optional erection that rises gradually with live arousal
- **Rise time** and **Fall time** sliders (0.5-30 seconds). Each is the exact
  time to travel between the lowest (`SOSBend-9`) and highest (`SOSBend9`)
  positions, in 18 even steps
- separate rise and softening times, so an erection can fade naturally instead
  of dropping immediately
- optional random erections with separate shortest and longest intervals
- an optional normal-gameplay guard that waits during combat, dialogue, loading
  and paused menus
- optional recovery time between spontaneous erections and an optional erection
  after a long sleep or wait
- a test button so random behaviour can be checked immediately

SexLab and OStim controls have their own **Scenes** page, and NPC controls
have the **NPCs** page. Rare timing, recovery and compatibility controls live
in collapsed **Advanced** sections and are collapsed
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
The physics page offers **I have my own compatible physics**, **Use my personal
physics**, **Use my SOFTBODY physics**, and **Use my SOFTBODY physics (natural hang)**
(looser balls and shaft, same SOFTBODY collisions). The supplied profiles work with
compatible SOS, TNG and UBE six-bone meshes; only choose the SOFTBODY profile
when GT SOFTBODY 3.37.2 is installed.
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
- With optional soft-angle control enabled, SPS sends the selected soft stage
  while SMP still owns the six physics bones.
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
- With gradual erection enabled, the SOS animation stage eases from the soft
  stage to the selected erect bend after CBPC settles. Only changed integer bend values are
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
- No ESP, quests or SkyUI dependency beyond Menu Framework. Per-save NPC settings
  and futa choices are stored in the SKSE co-save.

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

The public archive contains the SKSE DLL, INI template, dedicated CBPC files,
an optional OSL 2.9.0-to-2.9.2 compatibility script, the SPS author's optional
six-bone SMP XML, and an opt-in combined SOFTBODY 3.37.2 SMP XML set. It does
not contain an ESP, body meshes or any other OSL files.

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
runtime independence. Optional APIs are resolved at runtime, and player-facing
Papyrus calls use SPS bridge scripts so native object pointers never cross the
Papyrus boundary directly.
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

## Unreleased 2.2.0 changes

- Added the `SPS pose angle` angle method. SPS rotates `NPC GenitalsBase
  [GenBase]` itself, giving a continuous angle instead of the 19 animation
  stages, and needs no Schlongs of Skyrim native. Experimental: the SOS/TNG
  animation graph rewrites that bone every frame, so the angle does not persist.
- Fixed angle control with Schlongs of Skyrim AE 4.0.0, which removed
  `SOSAE_SKSE.SetSchlongBend`. SPS no longer calls it anywhere; the position
  bridge, physics bridge and build stubs were updated accordingly.
- Managed NPCs use the pose angle when that method is selected, so their angle
  no longer depends on the SOS event contract.
- Fixed the position-dispatch fallback so a pose that cannot be applied (no 3D
  yet, or no base bone) falls through to the animation graph instead of being
  reported as applied.
- Added pose diagnostics: a bone-frame log line and a base-pose recapture
  control, so the rotation axis can be confirmed in game.

## Earlier changes

See the [GitHub releases](https://github.com/snowman12356/Schlong-Physics-Swapper/releases).

## Building

See [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md).
