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

This compiles the FSMP ownership, SexLab, position, arousal and optional OStim
bridges from `scripts\Source` using only the tracked declarations in
`scripts\BuildStubs`.
The compiler writes the paired PEX files back to `scripts`, where release
validation checks their ABI marker and required symbols and confirms that they
are packaged with the DLL. OStim runtime support remains optional and
experimental even though its bridge is built reproducibly with the package.

## Build and verify a release ZIP

Update the version in `CMakeLists.txt`, `src/plugin.cpp`, `fomod/info.xml` and
`fomod/ModuleConfig.xml`, then run:

```powershell
.\tools\Build-Release.ps1 -Version 2.0.0
```

The release command checks all version declarations and the environment,
recompiles every Papyrus bridge, builds the DLL, creates the FOMOD ZIP, expands
the ZIP into a temporary folder, validates the bridge ABI symbols and contents,
and confirms the packaged DLL hash matches the build.
The ZIP uses sorted entries and fixed timestamps, so identical inputs produce
the same archive hash on repeated runs. Local DLLs are placed in `out\build`;
release staging and ZIP files are placed in `out\release`.

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

The staged modularisation plan and its regression rules are documented in
[`CODEBASE_REWORK.md`](CODEBASE_REWORK.md). New ownership policy should go into
the testable core instead of adding more global decisions to `plugin.cpp`.
