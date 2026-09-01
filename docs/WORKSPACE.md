# SPS workspace layout

The active repository contains only SPS-owned source and project metadata:

- `src` - C++ core, runtime adapters, controllers, diagnostics and public API
- `scripts/Source` - Papyrus source owned by SPS
- `scripts/BuildStubs` - compile-only dependency declarations; never packaged
- `scripts/*.pex` - compiled bridge components paired with the DLL
- `compat` - the supported optional OSL legacy compatibility component
- `config` and `fomod` - runtime configuration and installer declarations
- `tests` - game-independent C++ regression tests and API smoke-test source
- `tools` - environment, build, Papyrus, package and publication commands
- `docs` - architecture, development, compatibility and support records
- `out` - ignored local DLL and release-package output

CMake intermediates are generated in the per-user `SPSBuild` cache and may be
removed between builds. Required build dependencies are stored in the sibling
`.sps-deps` directory. Skyrim source and API references are read from the
sibling `codex-references` library; they must not be copied into this
repository.

Temporary worktrees, research downloads, extracted mods, repair staging,
packages and old generated builds do not belong in the repository. The items
found during the 2026-08-31 migration were audited and temporarily preserved,
then permanently removed with explicit approval on 2026-09-01 after the active
source, build dependencies and test installation were verified.
