# Predator SMP Head / TNG HIMBO compatibility check

## Scope and result

Inspected the user's two archives against SPS 2.0.0 on
`codex/codebase-rework`. The existing SPS physics profiles already contain
the supplied TNG/HIMBO genital collision shape and tag filters. This does not
establish runtime compatibility. A subsequently supplied Nexus report says
the lips and throat do not react to the penis with SPS installed. Treat this
as an unresolved reported compatibility failure, not a supported/tested setup.
The released files are unchanged. A separate XML-only test is now available
for the confirmed SOFTBODY selection, as described below.

Inputs:

- `[Predator] SMP Head.7z`: installer metadata identifies version 1.4 by
  PredatorRJ; SHA-256
  `723D4EDD1142BC226E185A208382905DB0BE35608944894323FE96A4CFC544DC`.
- `TNG_Himbo_SMP_XMLs.7z`: SHA-256
  `AD22D54AA08D5AD2821B8D9727D10C0461D9BD4F95DBEE6BF373792C051D5015`.

The extracted configuration copies and validation record are under
`out/diagnostics/predator-himbo-20260914`. The source archives and installed
mods were not changed.

## Evidence

1. The incoming `MaleGenitals.xml` defines one active `MaleGenitals`
   per-triangle collision shape, with tag `Genitals`, margin `0.1`,
   penetration `0.5`, and eight collision-tag exclusions. Its scrotum
   dynamics are commented out and it has no active Gen01-Gen06 dynamics.
   If it wins the file conflict and no other XML supplies shaft dynamics,
   SPS cannot create a floppy simulation by enabling those bones.
2. `compat/Personal Physics/.../MaleGenitals.xml` already contains that shape
   with matching values and exclusions, alongside six positive-mass shaft
   bones. All three `compat/SOFTBODY SPS` profiles also retain those values
   and exclusions, with SOFTBODY's additional filters and weight thresholds.
   Each SPS XML has exactly one `MaleGenitals` shape.
3. The supplied `HIMBO.xml` controls breast, belly and butt bones and defines
   body collision shapes. Predator's HP and UBE head XMLs control named lip,
   cheek, nose and throat bones. These bone names do not overlap SPS's
   `GetPhysicsBones()` list in `scripts/Source/SPS_FSMPBridge.psc`.
4. Predator's head shapes use the tag `HighPolyHead`. The SPS genital shapes
   do not exclude that tag; the head shapes do not exclude `Genitals`.
   Their tag filters therefore permit this collision pair. This does not
   prove that the required meshes and XMLs are loaded in a particular game.
5. The local FSMP source's `TogglePhysicsImpl` changes the requested bones'
   kinematic flags; it does not remove the genital collision shape or switch
   off the entire actor. `SkinnedMeshBody::canCollideWith` allows a kinematic
   shape to interact with a dynamic shape subject to the tag filters.
   These details support the expected erect-mode head interaction, but do
   not establish live motion. The inspected source copy is the earlier
   isolated FSMP comparison tree, not a newly installed FSMP build.
6. SPS's full player reset is broader than its ordinary six-bone toggle:
   `SetPlayerOwnerV3` calls `DynamicHDT.ResetPhysics` during pose repair.
   Same-player head physics recovery after this reload needs explicit testing.
   The supplied Predator PPA configuration defines facial effects; SPS does
   not need to replace it.

## Follow-up: reported missing lip/throat collisions

The screenshot `Screenshot 2026-09-14 192721.png` reports that an earlier
problem was fixed by the new release but head collisions fail with SPS
installed. It does not identify the active physics profile, whose head and
genital mesh are involved, the loaded FSMP/SMP Fixes versions, or whether
the failure occurs in both soft and erect states. Do not infer those details.

The earlier tag-only conclusion omitted an important SPS-side difference:

- All four bundled SPS profiles explicitly set `margin-multiplier` to zero
  on Gen01-Gen06. The supplied collision-only TNG/HIMBO XML has no active
  bone overrides and uses the FSMP default of one for created skin bones.
- In the local FSMP source, `SkinnedMeshBody::internalUpdate` propagates each
  bone's multiplier and scale into the skinned vertices. The triangle
  collision checker multiplies **both** the shape margin and penetration by
  the average triangle-vertex multiplier. The same shape XML values therefore
  need not produce the same contacts after adding SPS's bone definitions.
- For a triangle weighted only to SPS's zero-multiplier shaft bones, effective
  margin and penetration are zero. The other collider's radius can still
  produce contact; this does not prove that all head collisions are disabled.
- A scalar reproduction of FSMP's contact-plane check uses head-vertex radius
  0.05, signed plane distance -0.2, genital margin 0.1 and penetration 0.5 at
  unit scale. Multiplier one accepts the plane contact; multiplier zero rejects
  it. This is a geometry example, not a full solver test or in-game reproduction.
  Results are in `out/diagnostics/predator-himbo-20260914/collision-margin-check.json`.

Source locations: `hdtSkyrimSystem.h:88` and `hdtSkyrimSystem.cpp:691` in
`D:/Modding/codex/codex-references/native/FSMP-4.1.1-selected`; the matching
full source under `out/diagnostics/fsmp-actor-api-20260907/source/` supplies
`src/hdtSkinnedMesh/hdtSkinnedMeshBody.cpp` (`internalUpdate`) and
`src/hdtSkinnedMesh/hdtSkinnedMeshAlgorithm.cpp` (triangle `checkCollide`).
The selected reference and full-copy `hdtSkyrimSystem.h` hashes match.
This does not establish the Nexus user's installed engine behaviour.

This is a concrete collision-behaviour difference and a plausible contributor
to the report. It is not yet a proven root cause. The ordinary erect handoff
does not unconditionally remove the SMP collider, so "CBPC cannot collide
with an SMP head" is also not established. Do not change FSMP or Predator.

The project owner subsequently confirmed that the reporter selected **Use my
SOFTBODY physics**. The actual winning XML, dynamically selected profile,
diagnostic report and soft-versus-erect comparison remain unavailable.
A blanket multiplier change can expand proxy colliders (including margin-4
shapes in the personal profile), so a test must target the selected SOFTBODY
component and must not be shipped as a proven fix without testing.

## SOFTBODY collision test 1

`tools/New-PredatorHeadCollisionTest.ps1` creates an isolated test ZIP from the
hash-verified published SPS 2.0.0 archive. It does not edit `compat`, the release
ZIP, DLLs, bridges, settings, any installed mod or the private override.

The test changes only `margin-multiplier` from zero to one for Gen01-Gen06 in
the three released SOFTBODY XMLs. Static validation confirms exactly six
single-byte edits per XML and no other content or XML-structure changes.
The source SOFTBODY shapes were checked: their margins are 0.1 and their
penetration values are at most 0.5; the personal profile's larger proxy shapes
are absent. All dynamics, filters, weights and profile distinctions remain
unchanged, but effective contact margins on other shaft collisions also change.
Any actor loading these shared XML paths can be affected, not only the player.

Output:
`out/compatibility/predator-head-test-1/SPS-SOFTBODY-Predator-Head-Collision-Test-1.zip`

SHA-256:
`CA3F3CB89300309B35B6A1209DB1045CE3692F54A36A4A40F318CB6A00324EB1`

The ZIP contains three XMLs, the unchanged SOFTBODY credits README and test/
rollback instructions. Every ZIP entry was checked against its prepared bytes;
7-Zip's integrity test passed. The receipt is in the same folder as the ZIP.
No native/Papyrus rebuild was needed because this test contains neither.

With Skyrim closed, the reporter can install it as a temporary SPS override
winning all three genital XML conflicts, then fully restart. Compare the same
failed scene, soft and erect head response, body collisions, resting length,
scene cleanup, equipment recovery and manual Repair. Disable the override and
restart to return to the release profiles. No save cleaning is needed.
The test has not been installed here or validated in-game; do not advertise
Predator compatibility as fixed from the archive/structure checks alone.

## 2026-09-15: partial test result and supplied PPA patch

The new screenshots show pheonex2077 reporting that test 1 "kind of works but
not very well" on Skyrim 1.6.1170 with 3BA, SOFTBODY, OStim, HIMBO and TNG.
Record partial improvement, not a passed compatibility gate. They also offered
their own working XMLs and recommended the SOFTBODY PPA patch as a reference.
The custom working XMLs have not been supplied yet.

The project owner supplied `PPA(Penetration Physics) Softbody patch
v3.0-152103-v3-0-1771932150.zip`. SHA-256:
`71E574876350F6E1605A54E705A4417A66EA33FA48B47C68C06A128BCAA9B8AB`.
It contains only `MaleGenitals.xml` and `MaleGenitalsToAnus.xml` under
`PPA/skse/plugins/hdtSkinnedMeshConfigs`; there is no soft profile, DLL, script
or head configuration. Archive integrity and both XML parses passed.
Extracted copies and structural comparisons are retained under
`out/diagnostics/ppa-softbody-v3-20260915`.

Both profiles were compared against the corresponding SPS combined profiles:

- All 58 bone names and all 11 collision-shape names match.
- Ten of the eleven shape definitions match after ignoring formatting.
- In `MaleGenitals`, SPS retains every PPA exclusion and weight threshold.
  SPS uses penetration `0.5` instead of `0.1` and adds six exclusion values:
  `MaleHands`, `Malehands`, `penis`, `VirtualFeet`, `VirtualLegs`, `VirtualPenis`.
  Each additional exclusion occurs ten times in the existing SPS shape.
  These repetitions were recorded, not changed as part of this comparison.
- Both sources exclude `3BCA_Head`; that is not the `HighPolyHead` tag used by
  the supplied Predator HP and UBE shapes. This archive does not establish a
  missing head-tag allowance in SPS.
- The PPA files declare Gen01-Gen06 as empty bones before their scrotum
  `bone-default` block. The local FSMP `BoneTemplate` constructor initializes
  mass zero and margin multiplier one; the parser applies templates as it
  encounters each bone (`hdtSkyrimSystem.h:84`, `.cpp:296`, `.cpp:681`). Thus
  these files do not provide SPS's dynamic shaft chain. Their sole generic
  constraint attaches the scrotum to GenBase. SPS supplies positive-mass shaft
  bones and six shaft constraints, with additional scrotum dynamics.
- Test 1 retains SPS's shape definitions and dynamics, changing only the six
  shaft margin multipliers to one. The supplied PPA patch does not identify
  the additional changes in the reporter's subsequently edited working XMLs.

Use this archive as the PPA baseline when the custom working XMLs arrive.
Replacing the SPS profiles wholesale would remove their shaft dynamics and
leave the soft profile from another provider; that is not an established fix.
Compare the custom files against both this baseline and SPS, then select the
smallest evidenced SPS-only change. Head motion, profile selection, actual
mesh weights and recovery still require live testing. No runtime, profile,
package or installed-mod file changed during this comparison. The 2.0.1
startup-crash candidate remains unchanged and separate.

## Existing profile selection

The following explains the existing file relationship; it is not a validated
workaround for the reported head-collision failure.

- Install the matching Predator head option and its own dependencies. Its
  installer specifically calls out SMP Fixes; the supplied archive does not
  establish which dependency version the user's installation has.
- Retain the TNG/HIMBO component's `HIMBO.xml` and the matching body meshes.
- Without SOFTBODY, select SPS's **Use my personal physics** option.
- With SOFTBODY 3.37.2, select **Use my SOFTBODY physics** instead.
- Let the chosen SPS profile win `MaleGenitals.xml`; with SOFTBODY, also
  let SPS win `MaleGenitalsSoft.xml` and `MaleGenitalsToAnus.xml`.
  The incoming collision-only genital XML must not replace that SPS profile.
- The active genital mesh must still have SPS's six supported shaft bones.
  The TNG/HIMBO archive supplies XMLs, not the genital mesh, so it cannot
  establish the actual mesh's bone layout.

## Remaining in-game gate and risk

1. With SMP enabled globally, use Test soft and Test erect in third person.
   Confirm both shaft states and the erect angle.
2. With the matching Predator head and collision meshes active, verify its
   facial collision response while the player's shaft is in each state.
3. If the player wears the Predator head, verify its response again after
   Test soft, Repair current physics, and ordinary erect-to-soft recovery.
4. Test scene cleanup and equipment changes. Confirm normal resting length,
   continued head response, and no persistent SPS-010 recovery failure.

No DLL, bridge, saved state or settings changed. The separate test alters only
shaft collision-margin multipliers and requires testing with the actual meshes.
No release, installation or modification of another mod was performed.
