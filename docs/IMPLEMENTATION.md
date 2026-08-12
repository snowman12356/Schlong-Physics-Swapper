# Implementation and validation

## Why the switch works

FSMP's `TogglePhysics(actor, bones, on)` changes the selected Bullet rigid
bodies between dynamic and kinematic state. With `on = false`, FSMP stops
simulating those bodies and follows the skeleton pose, allowing CBPC to supply
that pose. With `on = true`, FSMP resumes simulation and clears stale velocity
to avoid an explosive transition.

The controller explicitly calls CBPC's per-node `StopPhysics` before enabling
SMP and `StartPhysics` after disabling SMP. The order prevents a transition
frame in which both engines own the same dynamic chain.

The bundled late-loading CBPC map assigns aliases `UBEPS01` through `UBEPS06`
to:

1. `NPC Genitals01 [Gen01]`
2. `NPC Genitals02 [Gen02]`
3. `NPC Genitals03 [Gen03]`
4. `NPC Genitals04 [Gen04]`
5. `NPC Genitals05 [Gen05]`
6. `NPC Genitals06 [Gen06]`

`NPC GenitalsBase [GenBase]` is mass-zero in the installed SMP XML and is not
toggled. Scrotum nodes are also excluded because the active CBPC SOS mapping
does not drive them.

## Mesh/XML requirements

A compatible schlong mesh must have one consistent bone chain shared by its SMP
XML and CBPC config. The native controller toggles Gen01 through Gen06. Parent
collision-only bodies should not be added unless both physics engines drive
them.

Do not ship a generic XML over the user's UBE addon. Mass, constraints, bone
names and collision groups must match that exact mesh.

## Test procedure

1. Disable CBPC handling of the chain temporarily and verify visible SMP motion
   below the threshold.
2. Re-enable CBPC. Set manual mode to `Force CBPC` and verify that movement is
   controlled only by CBPC.
3. Set manual mode to `Force SMP` and verify SMP resumes without a jump.
4. Return to `Automatic`; set arousal just below and above the threshold.
5. Stand near the boundary for at least 30 seconds and confirm hysteresis
   prevents repeated switching.
6. Save in each mode, reload, and confirm the first update reapplies the mode.

If only part of the chain moves, a rigid-body name is missing. If the chain
vibrates or stretches violently, both engines still own at least one dynamic
bone or the two configs use different parent transforms.
