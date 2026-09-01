# Workspace migration audit - 2026-08-31

The former OneDrive workspace and the new `D:` workspace were inventoried
recursively before cleanup, excluding Git's internal object database.

- Old workspace files: 9,088
- New workspace files before cleanup: 49,948
- Same relative path, size and timestamp: 9,088
- Old-only files: 0
- Same-path content differences: 0
- New-only files: 40,860

Nothing important failed to migrate. The old workspace was an exact retained
subset of the new workspace during the migration audit.

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

The final object-store audit found 60 incomplete `tmp_obj_*` files left by old
interrupted Git jobs. They were moved intact to the archive's
`git-temporary-objects` category instead of being deleted. Git now reports zero
garbage; valid unreachable objects were retained because they may still contain
recoverable historical work.

## Follow-up cleanup - 2026-09-01

After the active repository, test installation and preservation copies were
checked again, the obsolete OneDrive workspace was permanently removed at the
user's request. Its `.git` directory was empty, it contained no unique refs or
commits, and its 9,088 non-Git files had already been accounted for by the
migration inventory.

Ignored `out` artifacts and the per-user CMake/MSBuild cache were also removed.
The external vcpkg dependency was reduced to the installed libraries, vcpkg
source/tooling and the pinned CMake required by the supported build; duplicate
package trees, buildtrees, source archives, partial downloads and unused helper
tools were removed. `Test-Environment.ps1` passed after this reduction.

The broad dated archive and pre-install mod backup remain outside the active
workspace pending separate explicit approval, because they contain unrelated
historical task material and the only recovery copy of the former merged test
installation respectively.
