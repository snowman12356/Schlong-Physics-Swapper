# In-game check for the paired 2.3.0.0 redeploy (2026-11-04)

## Round 2: rise snap and even-step ramp (DLL `7D6DFDA6…A839B9`)

Round 1 result (user, 2026-11-04): addon off, "snaps to 9, snaps to -9, then
smoothly -9 to 9"; addon on, "snaps to 0, smoothly 0 to -9, then -9 to 9".
The fall was correct.

1. Addon **off**: slider 0 → 100. Expected: no snap; it starts at the lowest
   pose and climbs evenly to the top in exactly **Rise time** (Appearance page).
   Note any small twitch at the moment CBPC takes over.
2. Slider 100 → 0. Expected: an even descent in exactly **Fall time**, then floppy.
3. Change Rise time and Fall time (for example 4 s and 12 s) and repeat 1–2.
   The climb and fall should take those times.
4. Addon **on**: repeat 1–2.
5. Soft state: the resting angle should look the same as before (SMP still hangs it).
6. Send the new log. Look for `Rise primed on SOSBend-9 before the CBPC
   handoff` just before each `Physics switched to CBPC`.

## Round 1

Installed in `D:\Modding\mods\Schlong Physics Swapper`. DLL SHA-256
`AAC4B0D5…6168DC2`. Start from a save outside any scene. Use the SPS NPCs page
player slider to set arousal, and watch in third person.

After the session, send `SchlongPhysicsSwapper.log` from
`Documents\My Games\Skyrim Special Edition\SKSE`. Its timestamps let each
observation be matched to what SPS actually did.

## A. Rise with SPS Retractable Addon disabled (isolates SPS)

Disable `SPSRetractableAddon` in MO2 for this part only.

1. Slider 0 → 100. Expected: CBPC takes over, then the angle climbs smoothly
   from the lowest pose to full over about 8 s (`ErectionDurationMilliseconds`).
   **Note whether it first snaps up, then drops, then climbs.**
2. Slider 100 → 0. Expected: the angle lowers over about 5 s while still stiff,
   then goes floppy (SMP) at the bottom.
3. Repeat 1 and 2 twice.

## B. Same rise with the addon enabled

Re-enable `SPSRetractableAddon` and repeat A1–A3.

Reading the result:
- Snap in B but not in A: the cause is the addon/SPS handoff race recorded in
  `PROJECT_DIRECTION.md`. Fix in the addon (hold SMP *before* the threshold) or
  in how SPS starts a CBPC handoff.
- Snap in both A and B: the cause is inside SPS's own rise. The next suspect is
  the CBPC handoff picking up the `SOSFlaccid` animation pose before the ramp's
  first `SOSBend-9` step.
- No snap in either: record a pass.

## C. Quick regression (once A/B are recorded)

- Arousal near the threshold for 30 s: no flip-flopping.
- Equip/unequip armour while soft, then while erect.
- Troubleshooting page: Test soft, Test erect, Repair current physics.
- Save, reload; load a different save.
- The log should show `SMP proof: N/N managed bones were dynamic after the
  handoff` after each return to soft. `SMP not proven` or no line at all is
  worth reporting.

Command lines in the log only show what was sent. Say what you **saw** for
each step.
