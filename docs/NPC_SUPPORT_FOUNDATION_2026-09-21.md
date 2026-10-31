# NPC support: baseline, API map and first foundation

Branch: `codex/codebase-rework`, based on `103e402` with the existing uncommitted
NPC workflow retained. The user authorized starting that workflow on 2026-09-21.

## Player gate and scope

The user explicitly confirmed that disable/re-enable, manual Repair, and
changing state or loading a save while a handoff is pending all passed on the
current player build. These are user-observed passes, not new instrumented
results. Together with the earlier ordinary switching, soft-length and
scene/equipment reports, they support proceeding with the NPC foundation.

Do not extend that confirmation to every runtime, mesh, provider or optional
integration. The 2.0.1 startup-crash reporter's confirmation, Barbadoza's actual
TRX mesh/configuration and pheonex2077's working XMLs remain outstanding.
The cumulative SPS-010 suggestion is still a separate unfixed diagnostic issue.

This increment supplies native state/lifetime infrastructure only. No NPC
runtime is activated, no actor receives new commands, and no menu, provider,
physics bridge, save record, configuration or XML behaviour changes.

## Source-backed API map

CommonLib must be checked against the build dependency at
`D:\Modding\codex\.sps-deps\CommonLibSSE-NG`. The reference-library copy was
also consulted, but its headers differ from the configured build copy. In
particular, the build copy has a VR-aware `GetActiveTarget()` helper; do not
copy the reference version's flat-runtime field access into multi-runtime code.
Paths below are relative to that dependency unless a full path is given.

| Need | Verified evidence | Implementation consequence |
| --- | --- | --- |
| Select a reference | `include/RE/C/CrosshairPickData.h:36`, `GetActiveTarget()` returns `ObjectRefHandle` with VR handling | Prefer the helper over direct `targetActor` layout assumptions; resolve and verify an Actor before enrollment |
| Keep a live actor identity | `include/RE/A/Actor.h:577`, `GetHandle()`; `include/RE/B/BSPointerHandle.h:155`, `get()` returns `NiPointer<T>` | Store a handle for runtime identity, resolve immediately before work, and pair it with the registry ticket; do not keep raw actor pointers across delayed calls |
| Check availability | `include/RE/T/TESObjectREFR.h:446`, `Is3DLoaded()`; `:451`, `IsDisabled()`; `include/RE/T/TESForm.h:331`, `IsDeleted()` | Membership does not imply available 3D. Eligibility must also exclude the player, children and unsupported actors/meshes |
| Bounded nearby discovery | `include/RE/P/ProcessLists.h:43`, `ForEachHighActor(...)`; `highActorHandles` at `:72` | Suitable source for bounded loaded-actor discovery, not a reason to scan all forms or enroll every actor |
| Resolve primitives inside Papyrus | `D:\Modding\codex\codex-references\vanilla-papyrus\Game.psc:102`, `GetForm(Int)`; `:443`, `GetFormEx(Int)` | An actor-specific bridge can resolve an identifier inside Papyrus, but must validate the intended live handle/ticket before commands; resolving the FormID alone is insufficient |
| Save-specific membership | `include/SKSE/Interfaces.h:96-99`, save/load/revert/form-delete callbacks; `:173`, `ResolveFormID` | Later persistence needs versioned records and remapping; never serialize raw actor pointers or transient ownership/operation state |
| Current OSL numeric reading | Existing `src/runtime/Compatibility.cpp:70`, `ReadOslArousal(RE::Actor*)`; actual `GetArousalExt` call uses Actor pointer | Reuse the ABI corrected during player testing; never substitute a FormID for the Actor pointer |
| OSL Papyrus fallback | `D:\Modding\mods\OSL Aroused\Scripts\Source\OSLArousedNative.psc:8`, `GetArousal(Actor)`; same file declares `IsInScene(Actor)` | Supports an actor-targeted SPS-owned legacy query bridge. Verify the selected old-version declarations before advertising that path; the installed source alone is not every legacy version |
| Cached faction reading | Existing `src/controllers/ArousalController.cpp:83`; `include/RE/A/Actor.h:574`, `GetFactionRank(TESFaction*, bool a_isPlayer)` | NPCs must use the NPC flag. Check membership/invalid readings rather than copying the player path's clamp of a missing rank into zero |
| SOS AE position output | `D:\Modding\mods\Schlongs of Skyrim AE\Source\Scripts\SOSAE_SKSE.psc:55`, `SetSchlongBend(Actor, Int)`; documented range 0-20 | Actor-specific output exists, but the SPS adapter/bridge currently restricts it to the player. Extend the bridge safely; do not bypass it with the old unsafe native Actor-argument dispatch |
| SexLab participant identity | `D:\Modding\mods\SexLab Framework PPLUS\Source\Scripts\SexLabFramework.psc:220`, `IsActorActive(Actor)`; `:1187`, `GetActorController(Actor)` | Actor-scoped queries exist. Current SPS bridge and event routing remain player-specific; validate roles/stage timing independently in the scene stage |
| Physics calls | Existing verified `scripts/BuildStubs/DynamicHDT.psc` and `CBPCPluginScript.psc` declare actor parameters | Preserve their established calling boundary, ordering and completion checks; extending target selection is not evidence of live engine ownership |

The installed SOS AE `GetWinningAddon` is not a harmless eligibility query:
its source comments describe assigning addon data when an actor has none.
Do not use it to discover NPCs while claiming the scan is read-only.

## Compatibility findings and open checks

1. **Current OSL:** the installed `OSLAroused_Main.psc` pushes its global
   `EnableSOSIntegration` value into `OSLArousedNativeConfig.SetSOSIntegrationEnabled`.
   Its old per-actor SOS event driver is explicitly described as moved into
   the DLL. The planned basic SPS bridge supplies ordinary NPC behaviour with
   this external control disabled; SPS will not silently change OSL settings.
2. **Legacy OSL:** the existing bundled override skips only the player and
   otherwise runs NPC SOS output while `EnableSOSIntegration` is true.
   `compat/OSL Aroused/Scripts/Source/OSLAroused_Main.psc:291` uses
   `int(arousal) / 4 - 14`: below -9 sends `SOSFlaccid`, above 9 sends
   `SOSBend9`, otherwise sends `SOSBend` plus the signed position. It skips
   scenes and makes dead actors flaccid. This verifies the legacy mapping,
   not current OSL's exact native mapping or every SOS/TNG backend.
3. **Non-competing legacy scope:** preserve the current optional legacy package
   and its 2.9.0-2.9.2 restriction. When the new SPS NPC bridge supplies the
   whole basic NPC path, external OSL SOS output must be disabled there too.
   The existing player-only legacy mode can keep its present behaviour while
   the new NPC feature is off. No external script replacement is required by
   the foundation; document/test the actual transition before shipping it.
4. **Generic SOS-state fallback:** no independent general state getter was
   identified in the inspected installed SOS AE declarations. Its bend setter,
   addon/rank getters and SPS's own last commanded angle are not an arousal
   read-back. Do not fabricate a getter or feed SPS output back as external
   input. This provider investigation remains open; unavailable input must
   stay unknown. Numeric OSL support can be developed independently without
   claiming the generic fallback is implemented.
5. **Other providers/backends:** the existing player faction-reading path is
   evidence of the current implementation, not proof that every newly loaded
   NPC has a valid SLO/classic value. Their population/refresh semantics, TNG
   signed-event behaviour and supported legacy versions need targeted source
   verification before their NPC paths are advertised.
6. **Scenes:** current SPS OStim code uses player thread 0. It cannot simply be
   reused for another actor. Verify the installed/supplied NPC thread APIs in
   the scene stage; early NPC background output must defer on uncertain scene
   ownership instead of assuming an unknown query means no scene.

## First implementation

`src/controllers/ManagedActorRegistry.{h,cpp}` supplies a bounded, game-thread-only
registry for explicitly selected NPCs. It owns the existing `ActorContext`
type rather than a second physics/position/recovery implementation. The
player remains in its existing context and does not consume an NPC slot.

An enrollment begins unloaded. A verified availability transition creates a
fresh context and a work ticket containing FormID and a monotonically increasing
generation. Duplicate enrollment or same-state availability notifications
preserve state. Unload, removal/re-add and session reset invalidate old tickets;
the counter is not reset across saves. New loaded 3D starts with unknown
ownership and no old animation/recovery timers. Snapshots use a stable FormID
order. No arbitrary release cap is selected: the constructor accepts the bound
that the later runtime will supply, and tests exercise zero and two slots.

Callbacks must capture the issued ticket, return to the game thread and resolve
that ticket before accessing state. They must not look up a new ticket by FormID
and substitute it for the old one. Context pointers are borrowed only until the
next lifetime change; they cannot be captured by delayed callbacks.

The registry is not an engine-operation scheduler or cleanup mechanism.
It does not alter `PapyrusOperationGate`, cancel a running stack or restore
physics on removal. The runtime layer must coordinate cancellation, retirement
and release using the existing shared gate before treating user-facing removal
as complete. Retiring native state does not release that gate's engine lease.
The current player adapter and its safeguards are unchanged.

This is a partial Stage 1 implementation. The registry is not instantiated in
`plugin.cpp`; no new live NPC path exists yet. Arousal and scene controller
lifetime, actor-targeted Papyrus entry points, callback routing, enrollment UI
and saved membership remain to be connected in their workflow stages.

## Validation and risk

- The existing environment check passed.
- The full native Release build passed with SE, AE and VR targets enabled and
  no compiler/linker warnings or errors in the saved build log.
- Existing physics-decision and diagnostics tests passed.
- New `SPSActorRegistryTests` passed: capacity, duplicate enrollment, independent
  opposite owners, position/recovery isolation, unload/reload, stale actor/save
  callbacks, remove/re-add, stable snapshots and the retained engine lease.
- CMake/CTest register the new executable. `tools/Build-Local.ps1` runs it with
  the existing suites, so a failure prevents the ready-DLL copy.
- No Papyrus source or PEX changed, so no bridge recompilation was needed for
  this isolated native foundation. No installable package was produced.

The normal build initially could not create its temporary drive mapping under
the restricted shell. The same build succeeded with approved build access;
this did not install anything or require a different build workflow.

Build log: `out/npc-stage1-build.log`; retained validation artifacts:
`out/diagnostics/npc-stage1-20260921/`.
Intermediate build DLL SHA-256:
`056FBD20AFB48CDB7194D750CF6FC485883725B709AA28593738E598FD23676F`.
This is a development artifact with the unchanged baseline version metadata,
not an update to install or a replacement 2.0.1 release. Assign a distinct
candidate identity when there is a live NPC test package.

Published ZIP SHA-256 remains
`B5056B31BDF2ECB1D85D9067FBEB856266DAC5C54320E7E9522DAFC6C10CEB4A`.
Installed dedicated-test DLL SHA-256 remains
`4A0C1AA600B5C8FF2E97B2ECE7278C5573175A4793C92615C1A3A1AD3D7A21A6`.
All installed files, the private SOFTBODY override and the user-owned links
file were left untouched. Nothing was committed, pushed or published.

Save risk for this increment: no new saved state or active runtime path.
Future lifecycle/bridge wiring still needs live testing; a passing registry
test cannot establish safe engine cleanup or actual mesh motion.

## Next concrete work

Follow-up: the arousal/position routing described below was built and validated
on 2026-09-22. See
[`NPC_ACTOR_BRIDGES_2026-09-22.md`](NPC_ACTOR_BRIDGES_2026-09-22.md) for the
current artifact identity and remaining work. The evidence above describes
the earlier registry-only increment.

Continue Stage 1 with actor-specific arousal/position request routing. Replace
the proposed NPC callback ownership pattern with tickets/verified actor handles;
the existing player arousal callback's retained controller reference is safe
only for its current long-lived owner and must not be copied into removable
NPC controllers. Keep player calls and signatures intact. Add matching
Papyrus/version/package-contract changes together when those entry points
change. Leave physics execution serialized through the existing gate.

Use the first live scope of one explicitly selected compatible adult NPC plus
the player. The basic SOS bridge has a separate bounded loaded-actor set;
ordinary NPCs are not enrolled in the full-management registry. Resolve the
remaining provider/fallback evidence before claiming those paths supported.
