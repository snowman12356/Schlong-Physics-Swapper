# Read-only arousal and threshold API

Requested on 2026-10-09. Continues `codex/codebase-rework` without replacing
the NPC work or published 2.0.1. The user stopped another editor that had begun
the V2 header, getters and documentation; this increment finishes that work.

## Contract

The new `SchlongPhysicsSwapper_GetAPI_V2` export returns `InterfaceV2` with:

```cpp
Result GetArousal(std::uint32_t actorFormID, float* arousal);
Result GetErectThreshold(float* threshold);
```

Both outputs use the 0-100 scale and are written only on Success. Null output
pointers are InvalidArgument. Only the player (`0x14`) is supported by the
public actor API; zero and NPC IDs are UnsupportedActor. No new NPC management
or provider-query path was added.

`GetArousal` reads the controller's latest accepted sample and validity under
one short lock. It returns NotReady if that reading is invalid, including after
save reset. It does not wait for a confirmed physics owner. It reports valid
zero as zero, and follows SPS's existing last-good-reading policy during delayed
provider responses. It does not promise a new sample at the instant of a call.
All existing reading writes/reset paths share the lock; evaluation/callbacks
occur after releasing it. Existing switching and sampling rules are unchanged.

`GetErectThreshold` reads `settings.threshold` under `settingsLock`. A readiness
flag set by the existing settings load prevents reporting the constructor
default as the user's configured value before the INI has loaded. After that,
it returns the setting in disabled, forced and scene-controlled modes too. This
is the automatic erect/physics-switch threshold, not the hysteresis-adjusted
soft-return threshold or the gradual erection starting point.

The original V1 export/table/capability mask and payload layouts remain intact.
V2 has its own version, size and readback capability bits. Its reused request,
snapshot and event payloads remain version 1. ABI assertions pin published x64
sizes and offsets, every V1 table slot and the two appended V2 entries.
The [consumer documentation](MOD_AUTHOR_API.md) includes optional runtime
discovery and explicit error/availability semantics.

## Validation and installation

Separate candidate version: **2.1.0.2**, built on 2026-10-09 and installed on
2026-10-10. Native build passed with SE/AE/VR enabled and zero compiler/linker
warnings or errors. All five Papyrus bridges compiled and all three test
executables passed. The compiled ABI checks pin the old payload and table
layouts as well as the appended V2 slots. Inspection of the built DLL confirmed
both V1 and V2 exports. Packaged API header and consumer documentation match
the current source.

Staged and expanded ZIP validation passed: eleven paired DLL/PEX/PSC hashes,
25 FOMOD source entries, four bundled XMLs and package rejection guards.
No in-game consumer pass is claimed.

| Artifact | SHA-256 |
| --- | --- |
| 2.1.0.2 test ZIP | `BE04AA864244E744DB7AE7349155951F1E5835E3CC65FA9CC0B3D6B0A7E48BE9` |
| Installed 2.1.0.2 DLL | `17AD898F6806EBD4ED670D448C94D283F17C2F429B09513E22C5AB8CA4045120` |

Package: `out/release/Schlong-Physics-Swapper-2.1.0.2.zip`.
The dedicated `D:\Modding\mods\Schlong Physics Swapper` installation was backed
up with Skyrim closed, then updated with ten files: DLL, four core PSC/PEX pairs
and README. All installed candidate hashes match; all thirteen remaining files
retain their previous hashes, including INI, meta.ini and physics selections.
Optional OStim remains unselected. The private SOFTBODY override, other mods and
main game installation were not changed.

Rollback backup: `out/backups/testmod-before-api-2.1.0.2-20261010-131042`.
Restore the paired files with Skyrim closed if rollback is needed.
The 2026-10-09 install attempt did not run because automatic approval review
could not complete due to a usage limit. The normal approved retry succeeded
on 2026-10-10; no approval bypass was used.

Before building, the published 2.0.1 ZIP, 2.1.0.1 NPC ZIP and installed 2.1.0.1
DLL matched their recorded hashes. Both earlier ZIPs must remain unchanged.
Both earlier ZIP hashes were checked again after installation and remain
unchanged. Evidence directory: `out/diagnostics/api-readback-20261009/`, with
build log, DLL/manifest, test executables, exported symbols, before-install file
hashes, `installation.json` and `validation.json`.

No ESP, Papyrus function, saved property/variable, alias, quest or serialization
record changes. Existing-save structure is preserved; live startup/load and
consumer checks remain to be observed. Unrelated support issues and NPC test
gates remain outstanding. No publication, push or external posting is included.
