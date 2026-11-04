# SPS pose angle: replacing the removed SOS AE bend (2026-11-01)

## Evidence
Schlongs of Skyrim AE updated to `d2026.10.18` (reported as 4.0.0). Checked
against the previous version kept at
`D:\Modding\usefulmods\Schlongs of Skyrim AE old`.

**The native bend call is gone.** `SOSAE_SKSE.SetSchlongBend` is absent from
both the new `Scripts\SOSAE_SKSE.pex` and `SKSE\Plugins\SchlongsOfSkyrim.dll`.
The new script exposes seven natives only:

`GetActorAddonName`, `GetActorRank`, `GetActorAddonArmor`, `SetActorAddonName`,
`SetActorRank`, `SetActorAddonArmor`, and the `SOSAE_GetAPIVersion` string the
DLL registers.

Also removed: `ExecuteErectionCycle`, `ShowSchlong`, `GetActorUnderCrosshair`,
`ScaleSchlongBonesAE`, `GetWinningAddon`, `PrepareForShutdown` (still present),
and the whole `SOSAE_MCM.pex` / `SOSAE_PlayerAliasScript.pex` pair. The mod moved
its own controls to SKSE input keys and the SKSE Menu Framework.

**The skeleton layer is untouched.** All 23 files under `meshes\auxbones\SOS`
are byte-identical to the old version (SHA-256 compared). The behaviour graph
still binds:

- events `SOSBend0`-`SOSBend9`, `SOSFlaccid`, `SOSFastErect`, `SOSSlowErect`,
  `SOSErect`, `SOSBendUp`/`SOSBendDown`
- AnimObjects `SOS_Bend0`-`SOS_Bend9`, `SOS_Custom00`-`SOS_Custom09`

The new DLL contains the format string `SOSBend{}`, so its own keys drive the
same graph events SPS can send. This is why the mod still works after deleting
an entire MCM system.

**New hard dependency.** The new DLL requires `po3_Tweaks` with the *Load
EditorIDs* tweak for addon lookup (`SOS AE - Error: po3_Tweaks is not loaded,
editor IDs are not available and no addon will be found`). The SKSE Menu
Framework is optional. SexLab compatibility is still referenced.

## Why a native replacement was not available

The bend value used to be applied by the DLL. With that entry point removed
there is no supported way to ask SOS for an arbitrary angle, and the graph
events are both coarser (ten stages) and relative (re-sending repeats the blend,
so a polling tick produces mush rather than a held angle).

## Design

A new `Position/Method` value, `3 = SPS pose angle`, implemented in
`src/runtime/SosPoseAngle.cpp` with the pure math in
`src/controllers/PoseAngle.h`.

- Writes `NPC GenitalsBase [GenBase]` only. That bone is declared without a rigid
  body in the shipped SMP profiles, so it is kinematic and Faster HDT-SMP does
  not absorb a direct write, unlike `Gen01`-`Gen06`. `Gen01` and `GenScrot` are
  constrained to it, so the assembly follows.
- Composes a rotation offset onto a base orientation captured before the first
  SPS write. The write is therefore a pure function of bend: repeating it is
  harmless, which is what allows the mode to run from a polling tick. `Clear`
  restores the captured base exactly; `Recapture` re-reads whatever pose another
  mod has established.
- Never writes `Gen01`-`Gen06`. CBPC drives them directly and FSMP absorbs
  direct writes to dynamic bones.
- Needs no Papyrus dispatch and no SOS script. It is exposed to Papyrus through
  `SPS_PoseAngle` for the physics bridge's soft-angle step.

## Dispatch rules

`ApplyPosition` in `src/runtime/PositionBackendAdapter.cpp`:

1. Method 3, or method 2 with SOS AE present, tries the pose write first and
   returns immediately on success.
2. If the pose could not be written (no actor, no 3D yet, missing base bone) the
   call falls through to the graph events. The caller is never told a pose was
   applied when one was not.
3. When neither channel exists the result is `-1`, meaning "no backend
   required", rather than a failure.

## Save safety

Safe for existing saves. No forms, quests, aliases or scripts were removed, no
serialized state was added, and the bone write is transient: it is re-derived
from settings every tick and is not persisted. `SPS_PoseAngle.pex` is new, so the
Papyrus VM resolves it on load; nothing references it from a save.

## Still unverified

The pitch axis and its sign. `GenBase`'s local axes come from the skeleton, so
`PoseAngleDegrees` and the axis need one in-game pass. The plugin logs the
resolved frame (`LogActorPoseFrame`, also a button under *Pose angle
diagnostics*) and the angle can be tuned in the INI without a rebuild.

## Outcome: the premise was wrong (verified in game 2026-11-01)

Reported by the user: **the angle does not change.** The cause is not the axis
or the sign, and no amount of tuning would have fixed it.

The design above assumed `GenBase` was safe to write because it has no rigid
body in the SMP profiles. That part is true, but it is the wrong test. Every
SOS and TNG bend clip binds `GenBase`:

| clip | binds GenBase |
|---|---|
| `SOSFlaccid.hkx` | yes |
| `SOSErect.hkx` | yes |
| `SOSBendUp.HKX` / `SOSBendDown.HKX` | yes |
| `SOSFastErect.hkx` / `SOSSlowErect.HKX` | yes |

The behaviour graph therefore writes that bone every frame, and CBPC separately
owns `Gen01`-`Gen06`. A rotation set from an SKSE task survives until the next
graph evaluation and is then replaced, which is why nothing moved.

"Kinematic with respect to Faster HDT-SMP" is not the same as "not animated".
FSMP ignoring the bone means nothing is fighting the write on the physics side;
the animation graph was still overwriting it.

### What changed as a result

- Automatic mode (2) no longer routes to the pose angle. It uses graph events
  for every backend, including SOS AE 4.0.0, whose graph still binds
  `SOSBend0-9` even though its native call was removed.
- The gradual-erection ramp now drives graph events for SOS AE too, instead of
  calling the pose write from its worker thread.
- Method 3 remains as an opt-in diagnostic and is labelled experimental in the
  MCP panel.
- `GenBase` is never the lever. If a genuinely continuous angle is wanted later
  it has to go through an animation clip, a CBPC config, or a native that the
  owning mod exposes - not a bone write.
