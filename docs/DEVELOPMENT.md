# SPS development environment

The repository owns the build workflow. A global CMake version, a permanent
drive mapping, and a Visual Studio developer prompt are not required.
Run the commands below from PowerShell 7.4 or newer (`pwsh`). The environment
check reports an older shell as a blocking problem, and the local build refuses
to start under legacy Windows PowerShell with a clear version requirement.

## Check the machine

```powershell
.\tools\Test-Environment.ps1
```

This checks PowerShell, Visual Studio C++ tools, CommonLibSSE-NG, the installed
vcpkg packages, the pinned CMake executable, SKSE Menu Framework source, Git and
GitHub CLI. It does not contact GitHub or print authentication tokens.

CommonLibSSE-NG must be `alandtse/CommonLibSSE-NG` v15.4.0 or newer; the
`openvr` submodule must be initialised for the VR target. The build enforces the
minimum version and stops with a clear message when the checkout is older.

Local build dependencies live outside the repository in the sibling
`.sps-deps` directory. The scripts also accept `COMMONLIB_SSE_FOLDER`,
`VCPKG_ROOT`, `SPS_MENU_FRAMEWORK_SOURCE` and `SPS_REFERENCE_ROOT` overrides.
`SPS_SKYRIM_GAME_ROOT` can point release builds at a non-default Skyrim
installation for the official Papyrus compiler.
The shared Skyrim reference library remains a sibling at `codex-references` and
is never copied into SPS.

## Build the DLL

```powershell
.\tools\Build-Local.ps1
```

The script prefers the tested CMake downloaded by vcpkg instead of the older
system CMake. It creates a temporary short drive only while required, reuses an
existing correct mapping safely, and launches CMake/MSBuild with one normalized
`Path` variable. This avoids the Windows path-length and duplicate `PATH`/`Path`
compiler failures seen in restricted development shells.

The build also compiles and runs the game-independent SPS core tests. A failed
mode, threshold or hysteresis regression stops the build before a DLL is copied
to `out\build`.

Use `-Reconfigure` after changing CMake or dependencies. The ready DLL is copied
to `out\build\SchlongPhysicsSwapper.dll`. CMake and compiler intermediates live
under the per-user `SPSBuild` cache rather than in the source tree.

## Build the Papyrus bridges

```powershell
.\tools\Build-PapyrusBridge.ps1 -GameRoot '<Skyrim installation>'
```

This compiles the six bridges (FSMP ownership, SexLab, position, pose angle,
arousal and optional OStim) from `scripts\Source` using only the tracked declarations in
`scripts\BuildStubs`.
The compiler writes the paired PEX files back to `scripts`, where release
validation parses their actual signatures, global/native flags and version
return values, then verifies them against the paired DLL/script build manifest. OStim runtime support remains optional and
experimental even though its bridge is built reproducibly with the package.

## Build and verify a release ZIP

Update the version in `CMakeLists.txt`, `src/plugin.cpp`, `fomod/info.xml` and
`fomod/ModuleConfig.xml`, then run:

```powershell
.\tools\Build-Release.ps1 -Version <version>
```

The release command checks all version declarations and the environment,
recompiles every Papyrus bridge, builds the DLL, creates the FOMOD ZIP, expands
the ZIP into a temporary folder, validates compiled bridge contracts and contents,
and verifies all thirteen DLL/PEX/PSC hashes against `SPSBuildManifest.json`.
Failure-case tests reject incompatible version values, signatures/native flags,
and mismatched DLL, compiled script or source files. `New-ReleasePackage.ps1`
requires the receipt from `Build-Release.ps1`; changing any recorded input
requires another complete paired build. The receipt is package metadata and
is not installed into the game by the FOMOD.
The ZIP uses sorted entries and fixed timestamps, so identical inputs produce
the same archive hash on repeated runs. Local DLLs are placed in `out\build`;
release staging and ZIP files are placed in `out\release`.

### Optional script checks (pex)

`D:\Modding\codex-references\tools\skqe-pex\pex.exe` (a locally built copy of
github.com/cecell/skqe-papyrus-decompiler, GPL-3.0; commit in `SOURCE_COMMIT.txt`).
Run it on copies only. It is a second opinion, not a replacement for the official
compiler or the package guards.

```powershell
pex audit scripts\SPS_*.pex          # loops, update timers, None calls; 0 findings on 2026-11-16
pex diff <installed>.pex scripts\<new>.pex   # what a rebuilt bridge changes (functions, properties)
pex info scripts\<name>.pex          # shows the compile user and computer stored in the header
```

Use `pex diff` against the installed PEX whenever a bridge script changes, to confirm
only the intended functions changed and to judge save safety. Compiled PEX headers
store the Windows user and computer name; `pex recompile <file> --user <name>
--computer <name>` rewrites them, but it changes the PEX hash the package guards pair
with the DLL, so rebuild through `Build-Release.ps1` afterwards.

GitHub publication remains a separate deliberate step so running a local build
cannot accidentally push a commit, tag or release.

After the release commit and tag have been pushed, preview the GitHub upload:

```powershell
.\tools\Publish-GitHubRelease.ps1 -Version 2.0.0
```

When the preview is correct, publish it explicitly:

```powershell
.\tools\Publish-GitHubRelease.ps1 -Version 2.0.0 -Publish
```

The publisher refuses to run from an untagged commit, refuses to publish a tag
that is not on GitHub's `main` branch, and never overwrites an existing release.

## Code structure

New ownership policy should go into the testable core instead of adding more global decisions to `plugin.cpp`.
