# Schlong Physics Swapper 1.3.1

Native SKSE plugin for compatible SOS six-bone schlongs. Faster HDT-SMP owns
Gen01-Gen06 while arousal is below a configurable threshold; CBPC owns them
above it. Settings are available through SKSE Menu Framework.

## Requirements

- Skyrim SE 1.5.97 or AE 1.6.x, with the matching SKSE64
- Address Library for SKSE Plugins
- SKSE Menu Framework 3
- Faster HDT-SMP with `DynamicHDT.TogglePhysics`
- CBPC with `CBPCPluginScript.StartPhysics` and `StopPhysics`
- OSL Aroused - Arousal Reborn (Nexus mod 65454)
- A compatible SOS addon and SMP XML using these shaft bones (tested with UBE
  SOS and DW 3BA Futanari):
  - `NPC Genitals01 [Gen01]`
  - `NPC Genitals02 [Gen02]`
  - `NPC Genitals03 [Gen03]`
  - `NPC Genitals04 [Gen04]`
  - `NPC Genitals05 [Gen05]`
  - `NPC Genitals06 [Gen06]`

Schlongs of Skyrim AE is supported through `SOSAE_SKSE.SetSchlongBend`.
Legacy SOS is supported through its `SOSFlaccid`/`SOSBend0`-`SOSBend9`
animation events.

The package includes a dedicated CBPC map and parameter file for all six shaft
bones. It does not depend on another SOS CBPC preset being active.

OSL normally sends `SOSFlaccid` and `SOSBend` events whenever player arousal
changes, which can compete with this plugin's position control. The author's
private test setup uses a player-only OSL 2.9.2 compatibility override. That
modified third-party script is deliberately **not distributed** in this public
repository or its release archive. Physics ownership switching still works
without it, but OSL may occasionally replay a position event. Do not publish an
OSL override unless you have permission from the OSL Aroused author.

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
- advanced hysteresis, polling, cooldown, position safety, and SexLab P+ options
- selectable native, animation-event, or compatibility position method
- bounce guard, recovery limit, settle delay, and suspend-position-control mode
- optional separate SexLab erect angle and a one-click position test
- requested/applied angle, last method, and recovery status diagnostics

Technical controls now live on their own **Advanced** page. A separate
**Troubleshooting** page gives one overall health result, plain-language status
for each dependency/configuration, a one-click recommended-settings repair, and
copy/save diagnostic-report buttons.
- refresh and restore-default buttons

Defaults are threshold 60, hysteresis 5, bend 14, and a 1000 ms polling
interval. Settings are saved to
`Data/SKSE/Plugins/SchlongPhysicsSwapper.ini`. The packaged default
automatically imports an existing `UBEPhysicsSwitch.ini` once, preserving the
old settings even when both files are initially present.

## Behavior

- Soft/below threshold: CBPC is stopped first, then SMP is enabled.
- Above threshold: SMP is disabled first, then CBPC is started.
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
- OSL queries stop while the plugin is disabled or a physics engine is forced.
- No ESP, quests, save-game records, or SkyUI dependency beyond Menu Framework.

## Installation

1. Install the requirements listed above.
2. Install the GitHub release archive with MO2 or Vortex.
3. Ensure **Schlong Physics Swapper** wins conflicts for its two `ZZZ` CBPC
   configuration files.
4. Start Skyrim through SKSE and open the Schlong Physics Swapper section in
   SKSE Menu Framework.

The public archive contains the SKSE DLL, INI template, and dedicated CBPC
files. It does not contain an ESP, body meshes, SMP XML, or the private OSL
compatibility override.

## Troubleshooting page

The troubleshooting page shows loaded and live connection state for:

- SKSE Menu Framework
- OSL Aroused
- Faster HDT-SMP
- CBPC
- SOS AE bend support
- the supported schlong addon and six live Gen01-Gen06 skeleton nodes
- SexLab P+
- Procedural Penis Animations

Its health check scans the active MO2 virtual `Data` directory for SMP XMLs
with a complete `<system>` and all six genital bones. It also checks CBPC
master maps for Gen01-Gen06 and CBPC parameter files for UBEPS01-UBEPS06.
The page shows recent handoffs/errors and can copy or save a diagnostic report.

## Log and report

The log is written to the normal SKSE log directory as
`SchlongPhysicsSwapper.log`. The plugin is compiled for Address Library based SE/AE
runtime independence. External APIs are called dynamically through Papyrus,
so missing optional SOS bend APIs fall back safely without a hard DLL link.
Saved health reports are written to
`Data/SKSE/Plugins/SchlongPhysicsSwapper_Diagnostics.txt`.

## 1.3.1 changes

- Renamed the bundled CBPC files with a `ZZZ` suffix so they load after other
  SOS CBPC maps. This restores the Gen01-Gen06 precedence the pre-rename UBE
  filename had.
- Added one delayed soft-state confirmation after each CBPC-to-SMP handoff and
  after a soft-state skeleton rebuild. It idempotently stops CBPC, re-enables
  SMP, and reapplies the soft SOS pose once.
- Reused one cached bone list for both CBPC and FSMP calls.

## 1.3.0 changes

- Renamed the plugin, menu, files, log, and report to Schlong Physics Swapper.
- Preserves and migrates existing settings from the old INI name.
- Recognizes both UBE and DW 3BA Futanari addons; a live six-node skeleton can
  satisfy the compatibility check even when an addon ESP has an unfamiliar name.
- Removed full XML/CBPC disk rescans from skeleton-repair events. Health scans
  now run only at appropriate lifecycle points or when requested.
- Reuses the FSMP bone list during the erect-state reset guard, reducing work in
  the regular polling path without weakening reset protection.

## Building

The project requires Visual Studio 2022, CMake 3.24 or newer, vcpkg, and a
CommonLibSSE-NG checkout. Set `COMMONLIB_SSE_FOLDER` to that checkout, configure
with the vcpkg toolchain, then build the `SchlongPhysicsSwapper` target. The
Menu Framework SDK headers are fetched during configuration.

The third-party OSL compatibility override is not part of the source tree or
build process.
