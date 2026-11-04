# SPS project instructions

The permanent SPS workspace is `D:\Modding\codex\schlong smp`.

Before planning or changing this project, read `PROJECT_DIRECTION.md` and keep
all work aligned with it. Continue the existing repository and active branch;
do not restart or replace working subsystems simply to make them look cleaner.

The shared Skyrim reference library is `D:\Modding\codex\codex-references`.
Follow the parent Skyrim modding instructions and verify unfamiliar APIs against
the local reference library before relying on memory.

When a dependency removes or changes an API we call, read
`docs/DEPENDENCY_API_REMOVAL_PLAYBOOK.md` before changing any code. The short
version: find the upstream source first, and do not assume the surviving code
path is a correct replacement — a fallback that never took effect was never
validated.

