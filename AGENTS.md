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

## Second brain vault

This project uses the Obsidian vault at `D:\SecondBrain` as its knowledge
base. `PROJECT_DIRECTION.md` stays the authoritative high-level status in the
repo; the vault holds the project note, dev logs, decisions, debugging notes
and open questions that should survive between sessions.

- At the start of SPS work, read `D:\SecondBrain\_CLAUDE.md` (operating rules),
  then `Projects/Schlong Physics Swapper.md` and the most recent dev log whose
  `project:` frontmatter is `[[Projects/Schlong Physics Swapper]]` for the
  current state. The vault is shared with other projects: only touch notes
  tagged to SPS, and set `project:` on every SPS note you write.
- Search the vault (`index.md`, then search) for earlier decisions and
  debugging notes before re-deriving them.
- At the end of a work session, write one dev log
  (`Dev Logs/YYYY-MM-DD - Description.md`), add a line to the project note's
  Recent Activity, and save bug investigations to `Debugging/` and design
  decisions to `Knowledge/` (ADR) or `Architecture/`. Follow the vault's
  AI-first note rules, and update `index.md` and `Logs/YYYY-MM-DD.md` for every
  note created.
- Do not duplicate `PROJECT_DIRECTION.md` into the vault; link to it and record
  only what changed and why.
- Never store credentials or private personal data in the vault, and ask
  before renaming, merging or deleting an existing note.

