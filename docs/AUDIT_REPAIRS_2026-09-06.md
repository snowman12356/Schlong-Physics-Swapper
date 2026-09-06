# SPS technical audit repairs — 2026-09-06

Scope: `codex/codebase-rework`, based on `6fd807a` and the existing uncommitted
workload-pressure fix. The user authorized implementation after requesting a
read-only audit. This preserves working role policy, modular boundaries,
settings, installed profiles and player-only scope. No architectural replacement
or speculative engine workaround is included.

The evidence below describes the defective pre-fix paths. The listed fixes are
implemented in the current working tree. A passing local build cannot establish
the in-game regression result.

## Confirmed defects

### 1. High — unknown ownership could permanently block soft recovery

- **Evidence/location:** `src/plugin.cpp`, `Evaluate`'s normal-control relaxing
  branch required `physics.known` before retrying; `PhysicsOwnershipController::Finish`
  explicitly clears it on failure. `HandleOwnershipCompletion` restores the
  relaxing flag after a failed soft transition.
- **Problem/scenario:** after a scene or timed-out soft transition, every
  evaluation returns from the same branch without another ownership attempt.
  Manual Repair reaches that branch too, reproducing the reported lockout.
- **Fix:** retry a finished relaxation when ownership is unknown as well as when
  it is still CBPC. Stop a bend animation when its owner becomes invalid. Keep
  post-scene soft reset work pending when the owner is temporarily unknown.
- **Fix risk:** low; an unknown owner now receives the same ordered handoff used
  by normal reconciliation. Known soft state is not needlessly restarted.
- **Before current gate:** yes; implemented, core failure path covered.

### 2. High — timed-out Papyrus work could overtake its replacement

- **Evidence/location:** `src/runtime/PhysicsOwnershipAdapter.cpp`, ownership
  dispatch/callback; `scripts/Source/SPS_FSMPBridge.psc`, `SetPlayerOwnerV2`.
  Native request generations rejected late callbacks, but the waiting Papyrus
  stack had no generation check before later engine calls.
- **Problem/scenario:** an old SMP request wakes after a newer CBPC request and
  turns SMP back on, despite native state reporting CBPC.
- **Fix:** V3 uses a unique operation token and a native execution lease. Check
  the token before work and after waits. Timeout invalidates the token; a stack
  already executing retains its lease until return/disposal. New requests cannot
  overtake it. Load resets invalidate tokens from the previous session.
- **Fix risk:** medium; the new native/PEX contract must be installed together.
  A script VM that never resumes/disposes a cancelled stack requires loading or
  restarting; forcibly releasing its lease would restore the original race.
- **Before current gate:** yes; implemented, delayed-entry/callback/cancellation
  and load-token cases covered by core tests.

### 3. High — equipment release and SMP rebuild had no completion boundary

- **Evidence/location:** `src/plugin.cpp`, `RunLoadSMPReset`,
  `RunSoftHandoffSMPReset`, `RunSoftAngleRefresh`,
  `RefreshSMPAfterPlayerMeshChange`, and `Tick` equipment reacquisition.
  Separate fire-and-forget bridge calls were followed by 350/750 ms deadlines
  measured from native dispatch. FSMP's reset itself queues a game task.
- **Problem/scenario:** a busy VM executes the release/reset after restoration,
  disconnecting the replacement mesh or leaving its pose stretched.
- **Fix:** preparation, settling and final ownership use one V3 transaction.
  A FIFO SKSE task barrier follows FSMP's rebuild before the settling wait starts.
  Bend/settle work is scheduled by completion. Obsolete dispatch-based restore
  timers and unused native wrapper functions are removed.
- **Fix risk:** medium; live FSMP/CBPC timing and mesh changes require testing.
  Local FSMP 4.1.1 and SKSE queue implementation support the ordering used.
- **Before current gate:** yes; implemented, reset serialization/barrier cases
  tested; visible resting length and reconnection remain in-game checks.

### 4. High — SOFTBODY stage recovery listened to the wrong native events

- **Evidence/location:** `src/plugin.cpp`, `ModEventSink::ProcessEvent`;
  installed P+ `sslThreadModel.SetupThreadEvent` sends extended Papyrus `Hook*`
  events separately from native `SendModEvent` with the bare name/thread string.
- **Problem/scenario:** SOFTBODY responds to `HookStageStart` and reloads XML,
  while SPS never schedules the matching native ownership repair.
- **Fix:** recognize the actual native names and compare their thread ID with
  `SPS_SexLabBridge.GetPlayerThreadID`. The existing player query detects entry
  before the ID is cached. Unrelated NPC stages cannot reset player role state
  or schedule player physics repair. OStim thread events also require player ID 0.
- **Fix risk:** medium; actual P+ event timing needs live coverage. Existing P+
  role-query delays and role priority are retained.
- **Before current gate:** yes; implemented with native-event classification,
  matching player/NPC IDs, unknown IDs, malformed/overflowing ID tests and a
  paired updated SexLab bridge.

### 5. Medium — Test erect could immediately lose its pose at low arousal

- **Evidence/location:** `src/plugin.cpp`, `Tick`'s `targetStillWantsCBPC`
  ignored `activeManualPhysicsTest` although `Evaluate` preserves the test.
- **Problem/scenario:** the test acquires CBPC at arousal zero; maintenance
  clears its pending erect bend before it executes.
- **Fix:** maintenance applies the same active manual-test override, after API
  and scene priorities. Normal control resumes when the test expires.
- **Fix risk:** low; bounded to the existing ten-second test.
- **Before current gate:** yes; zero-arousal erect/high-arousal soft/expiry tests.

### 6. Medium — paused menus could expire executable work

- **Evidence/location:** `src/runtime/PapyrusGateway.cpp`, `NowMs`/readiness;
  `src/plugin.cpp`, `Tick`/`StartBendAnimation`; `PositionBackendAdapter::SendPositionEvent`.
  Native deadlines used wall time while Papyrus `Utility.Wait` excludes menu
  time. The legacy graph path bypassed readiness.
- **Problem/scenario:** opening a menu during a handoff causes a timeout and
  replacement while the old stack is simply paused; gradual motion can jump.
- **Fix:** one execution clock excludes paused/inactive/frozen periods. Tick,
  queries and animation use it; graph dispatch observes the readiness gate.
  Only one periodic game task can be queued during long frames.
- **Fix risk:** medium; nonstandard pause/menu behavior needs live validation.
  Uses verified NG runtime accessors and UI APIs, without offsets.
- **Before current gate:** yes; a minute-long simulated pause preserves the
  original ownership deadline and allows completion after resume.

### 7. Medium — equipment follow-up was unbounded

- **Evidence/location:** `src/plugin.cpp`, `Tick` node-follow-up branch scheduled
  another attempt every second with no terminal deadline. Related confirmation
  callbacks could recreate a budget cleared at dispatch.
- **Problem/scenario:** persistent missing bones/failed scripts cause repeated
  recovery traffic and can keep competing with another owner's changes.
- **Fix:** equipment follow-up stops after 12 seconds. Confirmation/reconnect
  budgets survive dispatch and are cleared on completion, not recreated on
  failure. A genuinely new equipment/reset event can start a new episode.
- **Fix risk:** low; after exhaustion the user may need Repair, or a new real
  equipment event, rather than endless retries.
- **Before current gate:** yes; deadline exhaustion/non-rearming/new-episode
  cases covered. Normal unknown-owner policy remains available.

### 8. Medium — scene identity could leak across saves

- **Evidence/location:** `src/plugin.cpp`, `OnMessage`; `SceneController` result
  callbacks; `SceneState`. Load reset cleared roles/validity but left active and
  ended timestamps. Callback mutation could interleave with load invalidation.
- **Problem/scenario:** loading a save outside a scene is misread as a scene
  ending, or a loaded active scene misses entry capture. A late previous-save
  result can repopulate state.
- **Fix:** clear cached scene identity/timestamps/entry state and invalidate
  generations at load boundaries. Commit scene and legacy arousal callback
  results on the game thread, after generation checks. Unknown entry ownership
  explicitly uses existing configured fallback behavior.
- **Fix risk:** low to medium; loading during a scene needs testing because SPS
  cannot reconstruct an owner from before a save was made.
- **Before current gate:** yes; scene-reset/generation tests added.

### 9. Low — invalid arousal looked like a valid zero

- **Evidence/location:** `src/controllers/ArousalController.cpp`, `AcceptResult`;
  the legacy bridge returns -1 on failure, which was clamped to zero. Nonfinite
  external readings were not rejected consistently.
- **Problem/scenario:** a delayed provider response spuriously softens the player.
- **Fix:** reject negative/nonfinite readings; preserve the existing last-valid
  reading/retry policy. Genuine zero is still accepted; above 100 is clamped.
- **Fix risk:** low; limited to invalid data handling.
- **Before current gate:** yes; sentinel, NaN, infinity, zero and clamp tested.

## Possible risks, not established engine defects

### 10. Medium — command success does not prove visible ownership

- **Evidence/location:** `SPS_FSMPBridge.SetPlayerOwnerV3`, native completion,
  diagnostics. FSMP returns previous body states; false is ambiguous for
  absent/kinematic bodies. CBPC per-node start/stop returns void. Local FSMP code
  can stop on its first matching skeleton.
- **Possible scenario:** SPS executes the right calls but an active first-person
  skeleton or another mesh/config keeps moving under the wrong simulation.
- **Recommended action:** diagnostics now distinguish command execution from
  observed movement. Test first/third person and equipment swaps. Collect the
  exact mesh/FSMP evidence before changing engine integration.
- **Fix risk:** low for accurate wording; high for speculative skeleton hooks.
- **Before current gate:** diagnostic clarification implemented; no speculative
  engine workaround. The actual motion check is part of the gate.

### 11. Medium — weak package/version checks could admit a mixed build

- **Evidence/location:** `tools/Test-ReleasePackage.ps1` previously checked only
  symbol strings. The prior installed DLL/scripts matched; no mixed deployment
  was established in the inspected test mod.
- **Possible scenario:** stale PEX still contains required names but returns an
  older version or has an incompatible signature; independent packaging combines
  outputs from different builds.
- **Fix:** require runtime FSMP bridge version 3. Validate actual PEX signatures,
  global/native flags and version literals. Build-Release creates an eleven-file
  hash receipt; staging and expanded ZIP validate it. Negative tests exercise
  wrong versions/ABI/native flags and modified DLL/PEX/PSC files.
- **Fix risk:** low; direct packaging now requires a completed paired build.
- **Before current gate:** yes; implemented as a preventive build safeguard.

## Compatibility and validation boundaries

- **Saves:** probably safe for an existing save, requires testing. No persistent
  properties, variables, inheritance, quests, aliases, records, registrations or
  serialization format changed. Existing global bridge entry points remain.
  Already-saved older unguarded stacks do not acquire new token checks. Update
  with Skyrim closed; start testing outside an active scene. No save cleaning.
- **SE/AE/VR:** native builds retain all three CommonLibSSE-NG runtime targets.
  New calls were checked against the actual compiled NG headers, local SKSE,
  installed P+, and FSMP source. Compilation does not establish runtime support
  for every third-party dependency/version, especially optional OStim/VR.
- **Scope:** no new NPC management, XML changes, settings migration, public API
  redesign, main-install writes or release publication. User-owned links and
  private SOFTBODY tuning are retained.

## Validation completed

The final 2026-09-06 paired release build passed native compilation with SE/AE/VR
targets, all core tests, five Papyrus compiles (zero warnings/errors), compiled
bridge signatures/version values, eleven DLL/PEX/PSC hashes, 25 FOMOD references,
four XMLs, expanded-ZIP checks and six deliberate package/ABI rejection cases.
With Skyrim closed, 18 core files and three combined SOFTBODY XMLs were installed
into `D:\Modding\mods\Schlong Physics Swapper` and hash-verified. INI/meta,
private XMLs and user links were unchanged. The prior installation and logs are
backed up under `out/diagnostics/audit-repairs-20260906`; its JSON verification
record contains installed and preserved hashes. Current artifact hashes are in
`PROJECT_DIRECTION.md`. The in-game gate is still open.

## Remaining prioritised plan

1. Test receiving scene entry, stage changes and exit: remain soft throughout;
   return to normal resting length without manual SMP reset. Confirm ordinary
   arousal switching and Repair still work afterward.
2. Test a penetrating scene, then equipment/schlong swaps in each state. Check
   no new SPS-010 and repeat in first/third person. Run an unrelated NPC scene
   and confirm it does not trigger player recovery.
3. Test erect at zero arousal, soft at high arousal, paused menu during a handoff,
   disable/re-enable, and loading saves inside/outside scenes. Compare new P+
   thread-not-found errors with the previous log, rather than counting old lines.
4. Address an engine attachment problem only if this gate reproduces it with
   matching logs/mesh evidence. Keep managed NPC work blocked until the player
   gate passes. No further architecture changes are justified by this audit.
