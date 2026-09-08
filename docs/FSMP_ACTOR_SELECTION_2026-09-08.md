# FSMP actor-selection candidate — 2026-09-08

## Status and scope

The installed SPS build remains `bfee4c1`, with the artifact hashes recorded in
`PROJECT_DIRECTION.md`. The user reports that ordinary swapping still fails
with SMP enabled globally, but works with SMP disabled globally, while viewing
the player in third person. Scenes have not yet been tested with this build.

This investigation continues audit item 10 in
`AUDIT_REPAIRS_2026-09-06.md`. It prepares a separate FSMP candidate; it does not
change SPS ownership policy, bridges, XML, settings or its release package.
No installed FSMP files or MO2 activation settings have been changed.

## Evidence

- The 2026-09-06 SPS log records completed V3 ownership commands and erect bends
  at high arousal. At 18:53:39 CBPC ownership commands completed, followed by an
  erect bend at 18:53:44. These acknowledgements do not measure live motion.
- The same session's FSMP log identifies 4.1.1 AVX. The installed
  `Faster HDT-SMP/SKSE/Plugins/hdtsmp64.dll` has SHA-256
  `FF9C7F3F78ECA122AC0AA9BD58A0628D7200685F86EF63FBD18BFE7F2C6D667F`.
- Local FSMP references identify upstream revision
  `e52ad960048c14e637d14fcbd65314ca017fc3b8`, also verified as the official
  `DaymareOn/hdtSMP64` tag `v4.1.1`. The downloaded original
  `src/dhdtPapyrusFunctions.cpp` matches the local reference byte for byte:
  SHA-256 `64AB570AB40B81D8583A78B6DF12A4078BB501DD66D1B284A0690CD180E344D5`.

### Confirmed source defect: first matching player skeleton can absorb a call

**Severity: High.** In `src/dhdtPapyrusFunctions.cpp`,
`hdt::papyrus::impl::TogglePhysicsImpl` (line 70) and `ResetPhysicsImpl`
(line 159) stop after the first skeleton whose owner FormID matches the actor.
The toggle loop stops even when that skeleton contains none of the requested
physics bones. Reset likewise stops after invoking that skeleton's reload.

`src/ActorManager.cpp` supports both player skeletons. Its
`isFirstPersonSkeleton` helper (line 94) identifies the first-person root by its
camera node. Physics creation and reload paths (lines 952, 1226 and 1245)
deliberately exclude this root. The public `ActorManager::skeletonNeedsParts`
method (line 706; declaration in `ActorManager.h`, line 240) exposes that same
classification. Neither Papyrus operation currently applies it.

**Realistic failure:** the first-person root precedes the body in the actor
manager's list. Toggle/reset returns normally after visiting that empty root.
SPS receives command completion, but the body stays under SMP or retains its
old pose. Being in third-person view does not itself establish the list order.

**Interpretation:** this is consistent with the user's global-SMP-off result
and the command-completion logs. The actual skeleton order in the user's
running game has not been observed. The source defect is confirmed; its role
in this particular session remains a candidate explanation.

## Candidate fix

In just those two functions, extend the existing null-root skip to exclude
roots for which `ActorManager::skeletonNeedsParts` returns false. This uses
FSMP's existing classification and keeps the owner check, locks, bone updates,
reset queue, public Papyrus signatures and previous-state return values.

The source patch is retained in `docs/patches/fsmp-4.1.1-actor-selection.patch`.
It is an external dependency patch, not part of the SPS FOMOD.

**Fix risk: Medium for a local test.** The change is small, but rebuilding FSMP
replaces the shared physics engine, and other mods can call these same APIs.
Hair, clothing, body physics and the installed SMPFixes hooks need observation.
The existing SMPFixes log already reports a missing `reloadMeshes` signature;
that message alone does not establish the cause of this regression.

**Save compatibility:** probably safe for existing saves, subject to testing.
The patch changes no scripts, records or serialization layout. Test from a
save outside a scene and use a full game restart for installation or rollback.

**Before the current regression gate:** first test ordinary soft/erect swapping
with SMP globally enabled. Do not start the scene gate until that works. A
failed candidate test should prompt inspection of actual physics attachments,
not additional speculative ownership-controller changes.

## Focused validation

An isolated C++ harness compiles the actual two original function bodies and
the actual patched bodies against small actor/skeleton/physics test doubles.
It exercises both player skeleton orders, all six bones switching off and on,
previous-state return values, full and soft reset selection, unrelated NPC
isolation, ordinary NPC toggling, missing actors/bodies and null roots.

The original functions produce five failed assertions; the patched functions
produce zero. Some failures cascade from the same incorrect reset selection;
these are not five independent defects. The harness verifies selection and
control flow, not Skyrim threading, live transforms or Bullet simulation.
MSVC compiled both harnesses without warnings.

Source, harnesses, results, original logs and build inputs are retained under
`out/diagnostics/fsmp-actor-api-20260907`. The full Release DLL built on
2026-09-08 with SE/AE/VR targets enabled. There were no compiler or linker
errors. Upstream build warnings were retained: `/Ob3` overrides Release's
`/Ob2`; vendored MinHook hde64 emits C4701 for variable `c`; nifly's unused
install rules name an absolute destination. No install target was invoked.

Binary inspection confirms x64, Windows version 4.1.1.0 (AVX), the test build
identifier, the same three exported entry-point names and eighteen imported
DLL dependencies as installed FSMP. The entire 0x350-byte SKSE version/runtime
declaration matches the installed DLL. The candidate DLL's CodeView GUID and
age match its PDB. All 214 upstream archive files were compared; only the two
expected files differ (runtime selection and build-version metadata).

Candidate DLL SHA-256:
`C0A52F5D4BF9FE8C9954F8BF3A79A5DB63C5BD6E29B2B89BE40E7B6E7195AB22`.
Matching PDB SHA-256:
`750DD8CD72A0CF3DF082DFB85BB91DBA2869396E8A9F87CD7E1F8A2B46B617E8`.
The separate test package is
`out/diagnostics/fsmp-actor-api-20260907/FSMP-4.1.1-SPS-actor-api-test1.zip`.
Its hash and per-file verification are retained alongside it. This is a local
candidate, not a published FSMP or SPS release. It remains uninstalled and
has not been tested in Skyrim.

## Build provenance

- FSMP source: official `v4.1.1` revision above.
- CommonLibSSE: pinned `alandtse/CommonLibVR`
  `3d81614617910e7f34b33d8750881811b5e36445` (6.7.0), compiled from source.
- nifly: pinned `ousnius/nifly`
  `8e98192ebd1307962b1a9d5df36d1214808a840c`.
- OpenVR: pinned `ValveSoftware/openvr`
  `60eb187801956ad277f1cae6680e3a410ee0873b`.
- MinHook hde64: CommonLib's pinned `TsudaKageyu/minhook` tag `v1.3.4`,
  revision `c3fcafdc10146beb5919319d0683e44e3c30d537`.
- Private vcpkg checkout: baseline
  `60b06921c7c7ac787b23a222dfab5cdd3911712e`, with its pinned 2026-04-08 tool.
- Upstream AVX triplet/ports, including its Bullet 3.25 overlay; Release build
  with SE/AE/VR targets enabled. These are build targets, not runtime test claims.
- The tagged archive's root CMake version is 3.0.0. The isolated build sets it
  to the tag's 4.1.1 so Windows/SKSE version metadata remains correct, and adds
  the existing build-info value `sps-actor-api-test1` for log identification.
- Automatic deployment is disabled (`COPY_OUTPUT=OFF`); all outputs stay in
  the workspace. No shared dependency checkout or installed mod is replaced.

## Prioritised test and rollback plan

1. Obtain explicit approval for a dedicated MO2 FSMP test override, then verify
   Skyrim is closed before installing it. Preserve the original FSMP mod and
   all existing configuration. The override should contain the candidate DLL
   and matching PDB; retain its provenance and license alongside them.
2. With SMP globally enabled, test soft -> erect -> soft, a camera switch,
   equipment changes and Repair. Confirm unrelated hair/clothing/body physics
   remain active and inspect fresh SPS/FSMP/SMPFixes logs.
3. If ordinary switching works, resume receiving/penetrating scene entry,
   stage changes, cleanup and post-scene equipment/repair checks from the SPS
   regression gate. If it fails or destabilises other physics, close Skyrim,
   disable only the dedicated override and restart to restore original FSMP.
