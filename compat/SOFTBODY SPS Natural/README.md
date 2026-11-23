# SPS SOFTBODY natural-hang XML

This optional compatibility component is a looser variant of the SPS SOFTBODY
combined genital XML. It is for GT SOFTBODY 3.37.2 together with a compatible
SOS, TNG or UBE six-bone genital mesh using `NPC Genitals01 [Gen01]` through
`NPC Genitals06 [Gen06]`. Choose it instead of "Use my SOFTBODY physics", not
in addition to it.

What differs from the standard SOFTBODY profile:

- **Balls:** `GenScrot`, `LGenScrot` and `RGenScrot` hang under full gravity
  with wide travel and soft springs, so they follow gravity when the character
  bends over. Moderate stiffness keeps them from sagging or swinging far.
- **Shaft:** the base joint keeps its firm limits. `Gen02` to `Gen06` each have
  their own joint, getting looser toward the tip, so the shaft curves along its
  length instead of moving as one rigid piece. Pitch limits are tight enough to
  keep the shaft out of the balls.
- Collision shapes, exclusions and weight thresholds are the same as the
  standard SPS SOFTBODY profile. Each file contains exactly one `MaleGenitals`
  shape. There is no shaft-to-thigh or ground collision (SMP needs a collider
  shape on the mesh for that).

These values were tuned by feel in game by the SPS author for one large shaft
mesh. If the shaft droops too far standing, raise the `Gen01` stiffness. If the
balls sag, raise the scrotum constraint linear stiffness.

The SPS mod must win conflicts for `MaleGenitals.xml`, `MaleGenitalsSoft.xml`
and `MaleGenitalsToAnus.xml`. In MO2, place SPS below SOFTBODY and other mods
that provide those files.

SOFTBODY: <https://www.nexusmods.com/skyrimspecialedition/mods/152103>

SOFTBODY's Nexus permissions allow modified releases and asset reuse with
credit to the original creator. Its author note prohibits commercial use and
inclusion in directly or indirectly paid compilations. This compatibility
component must remain free.
