# SPS pose angle: replacing the removed SOS AE bend (2026-11-01)

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
  menu.
- `GenBase` is never the lever. If a genuinely continuous angle is wanted later
  it has to go through an animation clip, a CBPC config, or a native that the
  owning mod exposes - not a bone write.
