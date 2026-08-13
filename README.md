# Schlong Physics Swapper 1.6.2

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

Please link to the original GitHub or Nexus page where practical. Files derived
from other projects remain under their respective licences; see
[THIRD_PARTY.md](THIRD_PARTY.md). In particular, the included OSL Aroused
compatibility files are distributed under OSL Aroused's Unlicense.

The author may participate in Nexus Mods' Donation Points programme. This does
not restrict these permissions or place any feature, update, support, or file
behind payment. The mod remains freely available under the licences included
with it.

## Requirements

- Skyrim SE 1.5.97 or AE 1.6.x, with the matching SKSE64
- Address Library for SKSE Plugins
- SKSE Menu Framework 3
- Faster HDT-SMP
- CBPC 
- OSL Aroused, SLO Aroused NG, or classic SexLab Aroused Redux
- A compatible SOS addon with SMP physics 

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

OSL normally sends `SOSFlaccid` and `SOSBend` events whenever player arousal
changes, which can compete with this plugin's position control. The package
includes a player-only OSL compatibility override based on OSL Aroused's
published 2.9.0 source and validated against the installed 2.9.2 script. It
only skips OSL's player SOS-position event; OSL arousal, integrations, UI, and
NPC behavior remain unchanged. OSL Aroused is distributed under the Unlicense;
the upstream license, patched source, and attribution are included.

SLO Aroused NG is also supported through its built-in `OSLArousedNative`
compatibility API. When using SLO, leave its **Use SOS** option disabled so it
does not send competing `SOSFlaccid`/`SOSBend` events. The packaged OSL script
override is only used by OSL Aroused and does not replace SLO's native DLL.

Classic SexLab Aroused Redux is supported as a fallback by reading the
player's public `sla_Arousal` faction rank. OSL and SLO remain higher-priority
providers when installed. Leave classic SLA's **Enable SOS** option disabled so
its position events do not compete with this plugin. No SLA script is replaced.

SexLab P+ is optional. When present, the plugin verifies the player through
`SexLabUtil.IsActorActive` and listens for SexLab start/end events. A player
scene temporarily takes priority over manual and OSL control, then normal
control resumes after a configurable delay.

## Settings

Open SKSE Menu Framework in game, then select **Schlong Physics Swapper**.
The streamlined **Main settings** page provides only everyday controls:

- live arousal, current physics engine, and overall health
- plain-language Automatic, Always Soft, and Always Erect modes
- immediate erect vertical-position control from 0 to 20
- optional gradual erection with an adjustable transition time
- advanced hysteresis, polling, cooldown, position safety, and SexLab P+ options
- selectable native, animation-event, or compatibility position method
- bounce guard, recovery limit, settle delay, and suspend-position-control mode
- optional separate SexLab erect angle and a one-click position test
- requested/applied angle, last method, and recovery status diagnostics

Technical controls now live on their own **Advanced** page. A separate
**Troubleshooting** page gives one overall health result, plain-language status
for each dependency/configuration, quick soft/erect tests, one-click recovery,
suggested fixes with stable error codes, and support-report controls.
- refresh and restore-default buttons

Defaults are threshold 60, hysteresis 5, bend 14, and a 1000 ms polling
interval. Settings are saved to
`Data/SKSE/Plugins/SchlongPhysicsSwapper.ini`. The packaged default
automatically imports an existing `UBEPhysicsSwitch.ini` once, preserving the
old settings even when both files are initially present.

## Behavior

- Soft/below threshold: CBPC is stopped first, then SMP is enabled.
- Above threshold: SMP is disabled first, then CBPC is started.
- With gradual erection enabled, the native SOS AE position eases from 0 to the
  selected erect bend after CBPC settles. Only changed integer bend values are
  sent, avoiding per-frame Papyrus traffic and animation-event bouncing.
- A 5-point default hysteresis keeps ownership stable around the threshold.
- Handoffs are only marked successful when both external Papyrus APIs accept
  every request. Failed handoffs are retried and the last confirmed state is
  restored on a best-effort basis.
- A switch cooldown prevents rapid ownership changes.
- While CBPC owns an erect schlong, a lightweight idempotent guard re-disables
  SMP on Gen01-Gen06 after an external `smp reset`. It does not restart CBPC or
  replay the SOS position.
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
2. Install the GitHub release archive with MO2 or Vortex.
3. Ensure **Schlong Physics Swapper** wins conflicts for its two `ZZZ` CBPC
   files. OSL Aroused users should also let its `Scripts/OSLAroused_Main.pex`
   win; SLO Aroused NG users should leave SLO's **Use SOS** option disabled.
4. Start Skyrim through SKSE and open the Schlong Physics Swapper section in
   SKSE Menu Framework.

The public archive contains the SKSE DLL, INI template, dedicated CBPC files,
and attributed OSL compatibility override. It does not contain an ESP, body
meshes, SMP XML, or any other OSL files.

## Troubleshooting page

The troubleshooting page shows loaded and live connection state for:

- SKSE Menu Framework
- Arousal provider (OSL, SLO NG, or classic SexLab Aroused)
- Faster HDT-SMP
- CBPC
- SOS AE bend support
- The New Gentleman position support
- the supported schlong addon and six live Gen01-Gen06 skeleton nodes
- SexLab P+
- Procedural Penis Animations

Its health check scans the active MO2 virtual `Data` directory for SMP XMLs
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
Saved health reports are written to
`Data/SKSE/Plugins/SchlongPhysicsSwapper_Diagnostics.txt`.
Debug captures are written to
`Data/SKSE/Plugins/SchlongPhysicsSwapper_DebugCapture.txt`. Reports include
versions, settings, connection state, relevant configuration filenames and
recent events. They do not include the Windows username, save name, or full
computer paths.

When reporting a problem, attach the diagnostic report or 30-second capture and
include the schlong addon, Skyrim runtime, mod-manager name, expected result,
actual result, and short reproduction steps. See [SUPPORT.md](docs/SUPPORT.md).

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
headers are fetched during configuration.

The OSL compatibility override is stored in `compat/OSL Aroused` with its
patched Papyrus source and upstream Unlicense. It is not compiled by CMake.
