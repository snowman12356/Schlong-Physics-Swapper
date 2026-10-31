# OSL arousal test controls

Requested on 2026-10-10 after the first 2.1.0.2 live log review. This increment
continues the existing basic NPC runtime and V2 compatibility API without
starting managed NPC physics.

## Scope

The SPS NPC page gains separate test sliders for:

- the player; and
- each eligible actor currently tracked by the bounded basic NPC pool.

Releasing a slider writes the real OSL Aroused value. This is intentionally not
an SPS-only simulation: OSL remains the owner of the value and can subsequently
change it through its normal time, scene or event rules. SPS does not edit OSL
files or settings.

The installed OSL source declares
`OSLArousedNative.SetArousal(Actor, Float)` and its own Puppeteer MCM uses that
function for the same 0-100 operation. SPS calls it through the paired
`SPS_ArousalBridge` rather than inventing a native export or passing an unsafe
actor pointer across the native-to-Papyrus boundary.

## Safety and lifecycle

Slider changes are committed once when editing ends, not on every rendered
frame. The request is held while the Journal/MCM is open and dispatched only
after normal Papyrus work resumes.

NPC writes are accepted only for an actor already in the basic pool. They carry
the existing lifetime generation, handle and root-3D identity, are bounded to
one arousal write per actor, and are rejected after removal, unload or a save
change. The player uses an explicit player-only test lifetime; the default
actor-work constructor still rejects the player. Session reset clears queued
bridge records and pending UI writes.

The arousal bridge advances from version 2 to version 3 and adds guarded write
resolution plus `SetActorArousal`. Its compiled PEX, source and DLL must remain
paired. No setting, saved Papyrus variable, ESP record, serialization record,
physics XML or public SPS API layout changes.

## Validation gate

The 2.1.0.3 SE/AE/VR native build and all three native test executables passed.
All five Papyrus bridges compiled with zero errors and warnings. The compiled
arousal bridge reports version 3 with the guarded write signatures. DLL V1/V2
exports, the paired 11-file receipt, release-package checks, negative package
guards and a repeated deterministic ZIP build also passed.

Candidate ZIP SHA-256:
`DAC86495951825411CDC1E773695C4553BBC2FC743D157DF5B0EE9BA8EC5A27B`.
Candidate DLL SHA-256:
`F45436EE3626E2AA02BB8E29A32123CD4D35D92EDEDECE87F14BBFAAA4130B1B`.
Candidate arousal PEX SHA-256:
`BDDCBCE686E08B07F1D4F50A0378E343F6309AA8B2CF403E6824A0D3814D5224`.

The paired candidate was installed into the dedicated MO2 test mod on
2026-10-10 with Skyrim/SKSE closed. A verified full backup was taken first. The
ten selected candidate files match, while the other thirteen files retain their
pre-install hashes, including the INI, `meta.ini`, CBPC configuration and the
three selected SMP XMLs. Optional OStim remains unselected. Rollback:
`out/backups/testmod-before-osl-controls-2.1.0.3-20261010-194143`. Installation
evidence is under `out/diagnostics/osl-arousal-controls-20261010/`.

Live validation was reported by the user as passing on 2026-10-31: the player
slider at values below and above the SPS threshold, two tracked NPCs with
opposite OSL values posing independently, NPC counters, scene deferral,
unload/reload, save loading and disable/re-enable. This is a user-observed
result; no log, capture or per-check notes were attached, so provider-specific,
per-runtime and non-OSL paths remain unverified. Successful OSL writes and SPS
commands are still not treated as proof of motion without observation.
