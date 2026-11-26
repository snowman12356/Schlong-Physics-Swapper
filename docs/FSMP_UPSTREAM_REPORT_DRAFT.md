# Draft issue for Faster HDT-SMP (not sent)

Draft for the owner to post on `DaymareOn/hdtSMP64` if they choose. Not sent
by the agent. Written against FSMP 4.1.1 (`e52ad96`, tag `v4.1.1`) and checked
against upstream master on 2026-11-25 (same code).

---

**Title:** `DynamicHDT.TogglePhysics` / `ResetPhysics` can act on the player's
first-person skeleton and do nothing

**Where:** `src/dhdtPapyrusFunctions.cpp`, `TogglePhysicsImpl` and
`ResetPhysicsImpl`.

Both functions loop over `ActorManager::getSkeletons()` and `break` after the
first skeleton whose owner FormID matches the actor. The player has two
skeletons in that list (first and third person). The first-person one never
gets physics (`ActorManager.cpp`, `reloadMeshes` / `softReloadMeshes` skip
`isFirstPersonSkeleton`), so when it comes first the call returns normally
without touching the body: `TogglePhysics` reports every bone as "not found"
and `ResetPhysics` reloads nothing.

The order is not stable. `ActorManager` sorts the list every frame by camera
distance; `calculateDistanceAndOrientationDifferenceFromSource` gives every
player skeleton distance 0, the comparator treats two zero distances as equal,
and `std::ranges::sort` is not stable. Which player skeleton a call reaches can
therefore change from frame to frame.

Observed from a mod that calls these functions for the player: two consecutive
`TogglePhysics(player, bones, true)` read-backs a few seconds apart returned
6/6 and then 0/6 for the same six bones.

**Suggested fix:** in both loops, also skip skeletons for which
`ActorManager::skeletonNeedsParts(skeleton.skeleton.get())` is false (the
existing first-person classification), or select with `get3rdPersonSkeleton`.
Signatures, locks and return values stay the same.

---

Notes for SPS: SPS works around this without changing FSMP (bounded proof
re-checks and bake retries, SPS-022). The owner previously declined to ship a
patched FSMP; this draft only reports the issue upstream.
