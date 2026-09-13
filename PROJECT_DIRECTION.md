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
remains SPS-only. The user has now explicitly authorized uploading 2.0.0 to
GitHub and requested the validated ZIP and a simple changelog for their own
Nexus upload. Publication is being prepared from the existing tested build;
no rebuild or physics change is needed. Release notes are recorded in
`docs/RELEASE_NOTES_2.0.0.md`. The validated release package remains
`out/release/Schlong-Physics-Swapper-2.0.0.zip`, with the hashes above unchanged.

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
