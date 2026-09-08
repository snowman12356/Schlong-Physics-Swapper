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
Current DLL SHA-256:
`4B6288444AD32DCE356A60B0CEEAFEF1FC63B5DE19A7A8F971AEE970650477D5`.
Current ZIP SHA-256:
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
build provenance and the reversible test plan. The immediate gate is ordinary
soft/erect switching with SMP globally enabled before resuming scene testing.
The package remains `out/release/Schlong-Physics-Swapper-2.0.0.zip`. The gate must verify receiving scenes stay soft, post-scene resting
length returns, penetrating scenes switch to CBPC, equipment changes reconnect,
Repair remains usable afterward and P+ adds no new thread-not-found errors.
The wider gate must also confirm that a compatible six-bone schlong is recognised automatically after
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
