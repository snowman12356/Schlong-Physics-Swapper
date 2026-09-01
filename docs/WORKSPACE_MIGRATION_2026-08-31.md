# Workspace migration audit - 2026-08-31

The former OneDrive workspace and the new `D:` workspace were inventoried
recursively before cleanup, excluding Git's internal object database.

- Old workspace files: 9,088
- New workspace files before cleanup: 49,948
- Same relative path, size and timestamp: 9,088
- Old-only files: 0
- Same-path content differences: 0
- New-only files: 40,860

Nothing important failed to migrate. The old workspace is an exact retained
subset of the new workspace and remains untouched as a migration backup.

The new-only material included the intact Git repository, the
`codex/codebase-rework` worktree, its uncommitted `ActorContext` expansion,
build dependency caches, generated builds, research copies, packages and work
from unrelated Skyrim projects. The rework worktree metadata was repaired, its
uncommitted change was preserved through the handoff, and the branch was made
the active checkout at the main repository path.

No potentially valuable artifact was deleted. Unrelated work, research,
packages and generated output were moved to the sibling dated archive. SPS
build dependencies were separated into the sibling `.sps-deps` directory. The
old root checkout's tracked changes remain recoverable in Git stash
`4c9b115ddb360f33da1b795bfd046347b4d41cb4`; the in-progress rework change also
has a safety copy in stash `fc4473cb1634dc40baa0704593633c36fec49c1a`.
