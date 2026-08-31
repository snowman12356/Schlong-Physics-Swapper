# SPS development environment

The repository owns the build workflow. A global CMake version, a permanent
drive mapping, and a Visual Studio developer prompt are not required.

## Check the machine

```powershell
.\tools\Test-Environment.ps1
```

This checks PowerShell, Visual Studio C++ tools, CommonLibSSE-NG, the installed
vcpkg packages, the pinned CMake executable, SKSE Menu Framework source, Git and
GitHub CLI. It does not contact GitHub or print authentication tokens.

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
to `build-output`.

Use `-Reconfigure` after changing CMake or dependencies. The ready DLL is copied
to `build-output\SchlongPhysicsSwapper.dll`.

## Build and verify a release ZIP

Update the version in `CMakeLists.txt`, `src/plugin.cpp`, `fomod/info.xml` and
`fomod/ModuleConfig.xml`, then run:

```powershell
.\tools\Build-Release.ps1 -Version 1.9.4
```

The release command checks all version declarations, checks the environment,
builds the DLL, creates the FOMOD ZIP, expands the ZIP into a temporary folder,
validates its contents and confirms the packaged DLL hash matches the build.
The ZIP uses sorted entries and fixed timestamps, so identical inputs produce
the same archive hash on repeated runs.

GitHub publication remains a separate deliberate step so running a local build
cannot accidentally push a commit, tag or release.

After the release commit and tag have been pushed, preview the GitHub upload:

```powershell
.\tools\Publish-GitHubRelease.ps1 -Version 1.9.4
```

When the preview is correct, publish it explicitly:

```powershell
.\tools\Publish-GitHubRelease.ps1 -Version 1.9.4 -Publish
```

The publisher refuses to run from an untagged commit, refuses to publish a tag
that is not on GitHub's `main` branch, and never overwrites an existing release.

## Code structure

The staged modularisation plan and its regression rules are documented in
[`CODEBASE_REWORK.md`](CODEBASE_REWORK.md). New ownership policy should go into
the testable core instead of adding more global decisions to `plugin.cpp`.
