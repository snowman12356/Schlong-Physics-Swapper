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
   CommonLibSSE-NG where possible.
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
correctly selected SMP. The latest development DLL SHA-256 is
`A41A180C4CF10CE9299450F97CFC3F321575F1102DEF0A20BEE38D4C8E8BCCC6` and the
latest package SHA-256 is
`CA1CDDFC69591FDBF96DAAD0F11CF7BBA9906347AE9AF8F35858FE9121063001`.

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
unchanged. The user's `MaleGenitals` compatibility exclusions are also merged
into the private large-mesh profiles. This separate large-mesh tuning remains a
local override, while the generally tuned combined profiles are packaged as the
public opt-in FOMOD component. Goutou is credited under SOFTBODY's published
modification and asset-use permissions; the component must remain free and out
of paid compilations.

The next gate is the documented in-game player regression pass. It must first
confirm that a compatible six-bone schlong is recognised automatically after
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
collisions in all three SOFTBODY profiles. Do not begin managed NPC support
until the player gate has passed.
