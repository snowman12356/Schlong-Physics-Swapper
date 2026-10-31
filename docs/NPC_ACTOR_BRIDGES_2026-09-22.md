# NPC arousal and position bridge increment

Date: 2026-09-22. Branch: `codex/codebase-rework`.
Continues the authorized [NPC workflow](NPC_SUPPORT_WORKFLOW.md) and
[registry foundation](NPC_SUPPORT_FOUNDATION_2026-09-21.md). Baseline HEAD
remains `103e402`; this increment is uncommitted development work.

## Result and boundaries

Actor-specific arousal reads and SOS positioning now have guarded runtime and
Papyrus entry points. Each request carries its original actor lifetime and
live reference handle; callbacks return through the game task queue before a
caller can update actor controllers. Player calls and decisions are preserved.

This is a partial Stage 1 increment. The live plugin registers the new native
functions and resets their transient state at the existing save/new-game
boundary, but does not instantiate the NPC registry, discover actors, enroll
NPCs or schedule these calls. No NPC physics handoff, scene policy, membership
storage, controls or basic arousal-to-SOS scheduler is active. A package passing
validation is not an in-game NPC result.

The user's 2026-09-21 confirmation that player disable/re-enable, Repair and
state changes/save loading during pending handoffs passed remains accepted.
It does not establish live coverage of these new actor entry points.

## Implementation

- `ManagedActorRegistry` now issues a small worker-readable lifetime lease.
  Unload, removal, session reset or registry destruction invalidates that view
  before destroying the actor context. Delayed work retains no controller or
  context pointer. Callers still resolve the returned ticket on the game thread.
- `ActorRequestLedger` bounds outstanding work to 64 records internally, with
  one request per actor and kind. This is not the eventual managed-NPC limit.
  Requests expire after 10 seconds of active execution time. Expired/cancelled
  stacks keep their slot until callback/disposal, or the real session boundary;
  elapsed time and remove/re-add cannot create an unbounded stack backlog.
- `ActorBridgeAdapter` queries the existing OSL native arousal API or a guarded
  Papyrus fallback. SLO/classic reads require actual membership in the verified
  `sla_Arousal` faction. Invalid/missing values remain unknown. Provider routing
  is source-backed implementation, not a completed live compatibility matrix.
- Outgoing VM calls contain only primitive values such as operation tokens,
  event names, booleans and integers. Registered native `ResolveActorRequest`
  functions resolve a stored actor handle inside the bridge. Guards check the
  token, action, original FormID, handle, lifetime, deadline and loaded adult
  actor availability. Failed resolution never falls back to the player.
- `SPS_ArousalBridge` and `SPS_PositionBridge` are version 2. Their existing
  player functions are preserved. Actor position accepts only the existing SOS
  event family and valid native bend range, and checks permission before each
  side effect. The FSMP bridge remains version 6.
- Actor position uses the existing global physics operation gate. Scoped
  cancellation invalidates only its matching token; it cannot cancel a later
  player operation or release an executing stack. Stack return/disposal retires
  that lease. Completion means the commands ran, not ownership read-back or
  observed mesh motion.
- New bridge-version probes retain one outstanding slot until return/disposal
  or a real session reset. Their generation checks and updates share a mutex,
  so a callback cannot write old-session state between a check and reset.
- CMake and the existing local build run the expanded actor lifetime/request
  test executable alongside physics-decision and diagnostics tests. Package
  contracts now require both version-2 actor functions and correct native flags.

## API evidence and save implications

The configured build uses
`D:\Modding\codex\.sps-deps\CommonLibSSE-NG`, whose headers differ from the
general reference copy. Verified against that dependency:

| API | Source evidence |
| --- | --- |
| Actor handle resolution | `include/RE/A/Actor.h:512`, `LookupByHandle(RefHandle)` returns `NiPointer<Actor>`; `GetHandle()` at line 577 |
| NPC faction arousal | `Actor.h:464`, `IsInFaction`; line 574, `GetFactionRank(TESFaction*, bool)` with the NPC flag false |
| Registered native Actor return | `include/RE/N/NativeFunction.h:104` and `:107` pack the result through the registered return type; `include/RE/P/PackUnpack.h:155` uses form-type handle packing |
| OSL and SOS output | Existing project adapters and installed provider sources recorded in the foundation API map; no new relocation or guessed native ABI |

An initial `Game.GetFormEx` lookup did not compile against the actual compiler
source set, despite appearing in the reference library. The final bridge uses
the verified registered native handle resolver instead. No game/compiler base
script or other installed mod was changed.

The new functions are stateless global bridge functions: no stored variables,
properties, quests, aliases, attached scripts, event registrations or inherited
types changed. Old player signatures remain available. This is expected to
preserve existing-save structure, but changed bridge stacks and native target
marshalling still require live save/load testing before release. The managed
membership serialization design remains unimplemented. No save cleaning is
part of this work.

## Validation record

- Native Release build passed with the existing SE/AE/VR configuration; final
  log has no compiler/linker errors or warnings. This is compile coverage,
  not live testing on all runtimes.
- All five Papyrus bridges compiled with zero errors and warnings. Unchanged
  bridge PEX files were regenerated as part of the paired build.
- All three test suites passed. New coverage includes high/dynamic FormIDs,
  wrong actor/handle/action, capacity and duplicate work, expired requests,
  unload/reload, remove/re-add, save reset, owner destruction, late completion,
  retained cancellation slots and preservation of a newer player gate token.
- Staged package and freshly expanded ZIP passed all bridge contracts and
  11 paired-file hashes, 25 FOMOD source entries and four bundled XML checks.
- Rejection checks passed for incorrect bridge versions, function signatures,
  native flags and altered DLL/PEX/PSC files. The actual installed version-1
  arousal and position PEX files were separately confirmed incompatible with
  the new package contracts; installed files were read only.
- Published ZIP bytes and all nine installed core DLL/PEX/PSC files still
  match the released baseline. The private override and user-owned links file
  were not modified. No test installation, push or publication occurred.

Evidence directory: `out/diagnostics/npc-actor-bridges-20260921/` (work began
on September 21). It retains the DLL, paired manifest, three test executables,
native/Papyrus logs, expanded package and `validation.json`.
Package validation log: `out/npc-actor-bridges-package.log`.

| Artifact | SHA-256 |
| --- | --- |
| Internal development DLL | `E283146BE9D96D905883865524665735728BEC5AD1F060AD5C08F6278AA1BC1A` |
| Internal validation ZIP | `89AF0822889D7C8BB7CD3BE73EDEC80DC29DD53CDA62827658CFE8AEEF5EC92E` |
| Preserved published 2.0.1 ZIP | `B5056B31BDF2ECB1D85D9067FBEB856266DAC5C54320E7E9522DAFC6C10CEB4A` |
| Preserved installed test DLL | `4A0C1AA600B5C8FF2E97B2ECE7278C5573175A4793C92615C1A3A1AD3D7A21A6` |

The internal ZIP is under the evidence directory's `internal-package` folder,
not `out/release`. It retains baseline 2.0.1 metadata solely to exercise the
unchanged packaging workflow. It is not a live test candidate or replacement
release. Assign a distinct candidate version before offering or installing it.

## Next concrete work

Follow-up: scene-deferral entry points and the basic SOS policy below were
subsequently implemented and validated; see
[`NPC_BASIC_SOS_POLICY_2026-09-22.md`](NPC_BASIC_SOS_POLICY_2026-09-22.md).
The hashes and bridge versions above describe this earlier increment.

Establish actor-specific scene deferral before any background NPC SOS writes,
then implement the verified basic arousal-to-SOS mapping and bounded loaded-actor
scheduling as a separate responsibility from full management. Preserve the
legacy OSL restrictions and unknown-input behavior. No independent generic
SOS-state getter has been established; that fallback remains pending and must
not read SPS's own output as fresh evidence.

Managed NPC physics still needs guarded actor-specific handoff/preparation,
arousal and scene controller ownership, and integration with existing policy,
position/recovery controllers. Later enrollment/removal must coordinate request
cancellation, running-stack retirement and cleanup; removing a registry entry
alone does not restore engine ownership. No live first milestone can be claimed
until those paths, minimal controls and a separately versioned candidate exist.
