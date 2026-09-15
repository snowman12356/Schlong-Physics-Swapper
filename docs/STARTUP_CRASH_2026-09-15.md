# Startup filename conversion crash

## Evidence

Report: dacama, supplied `Schlong Physics Swapper crash-2026-09-14-20-43-40.log`.
Skyrim AE 1.6.1170, crash before the main menu with SPS enabled. The exception
is `std::system_error`, Windows error 1113: no mapping for a Unicode character
in the target multi-byte code page. This is a native crash log, not a Papyrus log.

The retained release DLL matches the published 2.0.0 SHA-256:
`470575B47C18664007888B3777C6B0A846B252DEC5C116058FE8FE2DA9C5DFBC`.
No release PDB exists. Relinking the retained objects with a map, without
recompiling or installing them, produced an identical `.text` section:
`5B10AFCBB37453B222F7280ECAC9FA15BB9345C4D2F958B92D227B64CD48D347`.
This makes the map usable for these exact release code addresses:

| DLL offset | Function |
| --- | --- |
| `+001D12A` | `Mod::OnMessage +0x50A` |
| `+001E23B` | `Mod::RefreshDiagnostics +0x3B` |
| `+003D31E` | `SPS::Diagnostics::Scan +0x104E` |
| `+0002C15` | `std::filesystem::_Convert_wide_to<char> +0xE5` |
| `+002FE18` | `std::_Throw_system_error_from_std_win_error +0x38` |

Disassembly at `+003D319` calls the wide-to-narrow converter after extracting
the filename. The log also contains the `.txt` filter and previously populated
XML/CBPC summaries. The source converts every TXT filename to `std::string`
before deciding whether it is a CBPC configuration. A non-representable name
can therefore crash startup even when the file is unrelated to SPS.

The original scanner, extracted unchanged except for supplying its root path,
was compiled with MSVC and run against a temporary plugin directory containing
one unrelated Unicode TXT file. It failed with the same error 1113. The supplied
log does not identify the reporter's exact filename; it is unnecessary to
guess which mod supplied it or ask them to rename/delete unrelated files.

## Fix and risk

Severity: **High**, confirmed SPS startup defect. Address before further features.

`src/diagnostics/PhysicsFileScan.cpp::ScanPhysicsFiles` now compares native path
strings using ASCII-only case folding for the ASCII keys. Display summaries
explicitly use UTF-8. Windows filenames containing unpaired UTF-16 surrogates
retain detection but use `<filename unavailable>` if conversion fails.
The file-only scan moved out of `Diagnostics.cpp` to allow tests to execute
the production scanner; `Diagnostics::Scan` calls it at the original location.
The scan roots, recursion rules, content checks, limits and counts are retained.

No physics controller, Papyrus bridge API, event lifecycle, runtime relocation,
XML or INI behaviour changes. The candidate is numbered 2.0.1 to distinguish
it from public 2.0.0. Save compatibility: safe for existing saves by design;
the change reads filenames and does not change serialized or script state.
Runtime risk is limited to diagnostic file detection/display. Menu fonts may
lack glyphs for some valid UTF-8 names, but that is separate from conversion.

## Validation and next gate

`tests/DiagnosticsTests.cpp` covers unrelated Unicode TXT files, non-ASCII
extensions, valid ASCII/Unicode XML and CBPC configurations, nested XMLs,
non-recursive CBPC scanning, missing folders, incomplete configs, UTF-8
summaries and malformed UTF-16 filenames. Original code fails; fixed code passes.
The new executable is registered with CTest and the normal local build workflow.

The native Release build with SE/AE/VR enabled and both test executables passed.
All five Papyrus bridges compiled without errors or warnings; their source,
API and required version 6 are unchanged. The stage and expanded ZIP passed
validation of 25 FOMOD references, four XML profiles, bridge ABIs/versions and
11 paired-build hashes. Package rejection tests passed. Comparing the old and
new archives found only the DLL, recompiled PEX files, manifest, FOMOD version
labels and the already pending README compatibility documentation changed.
All XMLs, INIs, CBPC configuration files and Papyrus sources match 2.0.0.

Skyrim was confirmed closed before installation. Five changed files (DLL and
four core PEXs) were installed in the dedicated SPS test mod after a verified
backup. Twenty selected package files and all 23 installed files were checked;
the existing INI, metadata, XMLs, private override, FSMP DLL and user links file
are preserved. No other mod, public release or main Skyrim installation changed.

Candidate ZIP: `out/release/Schlong-Physics-Swapper-2.0.1.zip`.

- ZIP SHA-256: `B5056B31BDF2ECB1D85D9067FBEB856266DAC5C54320E7E9522DAFC6C10CEB4A`
- DLL SHA-256: `4A0C1AA600B5C8FF2E97B2ECE7278C5573175A4793C92615C1A3A1AD3D7A21A6`

The reporter must confirm reaching the main menu, loading an existing save and
ordinary switching. A local regression test is not an in-game confirmation.
Do not publish automatically or combine this with the pending Predator XML test.

Evidence and reproduction artifacts: `out/diagnostics/unicode-startup-20260915`.

With explicit user authorization, the unchanged candidate was published as
2.0.1 on GitHub on 2026-09-15 from commit
`7aa9f80267fcea662cdd0d270396e65d499bc366`, tag `v2.0.1`. The uploaded ZIP and
changelog hashes were verified against the local files. The release notes
retain the pending in-game confirmation; publication does not close that gate.
Publication records are in `out/diagnostics/publish-201-20260915`.
