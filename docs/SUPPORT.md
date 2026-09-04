# Getting help

Please include enough information to reproduce the problem:

1. Open **SKSE Menu Framework > Schlong Physics Swapper > Troubleshooting**.
2. Press **Check setup again**.
3. If the problem happens on demand, press **Record the next 30 seconds** and
   reproduce it before the timer finishes.
4. Attach `SchlongPhysicsSwapper_DebugCapture.txt` from
   `Data/SKSE/Plugins`. If no capture was needed, attach
   `SchlongPhysicsSwapper_Diagnostics.txt` instead.
5. Say which schlong addon, Skyrim runtime and mod manager you use, what you
   expected, what actually happened, and the shortest steps that reproduce it.

If Skyrim crashed, also attach the newest crash log. Crash Logger SSE AE VR is
recommended for this, but it is not required for SPS to run. Do not attach an
old crash log from a different play session.

Please also say whether **Physics Editor**, **Auto Physics Reset**, or
**SOFTBODY** is installed.
Physics Editor may stay installed; if physics changes unexpectedly, disable its
schlong controls so it does not change the same bones as SPS. Auto Physics Reset
is optional, but its load, cell or scene triggers may overlap SPS's own player
reset if physics changes unexpectedly.

When using an arousal mod, leave its own SOS position control disabled so it
does not compete with SPS: turn off **Enable SOS** in OSL Aroused or classic
SexLab Aroused Redux, and turn off **Use SOS** in SLO Aroused NG. OSL's switch
is currently global, so this also disables its automatic NPC angles outside
scenes; SexLab and OStim scene animations are unaffected. Users remaining on
OSL 2.9.0 through 2.9.2 may select the legacy OSL FOMOD option instead; never install
that script override with OSL 2.9.3 or newer.

SOFTBODY is detected automatically. SPS waits for its SexLab/OStim genital XML
reloads to finish and then restores the selected physics owner. Most users
should keep **I have my own compatible physics**. Alternatively choose **Use my
personal physics** for the supplied general six-bone profile, or **Use my
SOFTBODY physics** when GT SOFTBODY 3.37.2 is also installed. Both supplied
choices support compatible SOS, TNG and UBE six-bone meshes. For the SOFTBODY
choice, make SPS win MO2 conflicts for
`MaleGenitals.xml`, `MaleGenitalsSoft.xml` and `MaleGenitalsToAnus.xml`; another
winning copy can make the soft shaft rigid or prevent the erect angle from
appearing. The diagnostic report records whether SOFTBODY was found but cannot
identify which MO2 mod supplied the winning loose file.

The reports contain mod state, versions, settings and relevant filenames. They
do not contain your Windows username, save name, or full computer paths. Please
check any file yourself before uploading it.

## Error codes

- `SPS-001`: SKSE Menu Framework is missing.
- `SPS-002`: The selected arousal provider (OSL Aroused, SLO Aroused NG, or
  classic SexLab Aroused Redux) is missing, timed out, or returned invalid data.
- `SPS-003`: Faster HDT-SMP is missing.
- `SPS-004`: CBPC is missing.
- `SPS-005`: One or more of the six player schlong bones are missing.
- `SPS-006`: No compatible six-bone SMP XML was found.
- `SPS-007`: The CBPC six-bone map is missing or overwritten.
- `SPS-008`: The bundled CBPC physics values are missing or overwritten.
- `SPS-009`: No supported SOS AE, legacy SOS, or TNG position backend is
  available.
- `SPS-010`: A physics handoff failed.
- `SPS-011`: A position update failed or recovery stopped.
- `SPS-012`: A report or capture file could not be saved.
- `SPS-013`: The SPS SexLab role bridge is missing.
- `SPS-014`: Physics Editor is loaded. It may stay installed, but its schlong
  controls can overlap SPS.
- `SPS-015`: OStim is installed but the optional SPS OStim bridge is missing.
- `SPS-016`: SOS Physics Manager is enabled and can fight SPS for control.
- `SPS-017`: The optional delayed player SMP reset could not run.
- `SPS-018`: The optional custom soft-angle SMP refresh could not run.
- `SPS-019`: The player-only SMP refresh after a soft handoff could not run.
- `SPS-020`: A startup or queued quick-fix action timed out while the game was
  still loading.

## Suggested pinned Nexus post

If something is not working, please open the mod's **Troubleshooting** page,
check the setup and save a report. For problems you can repeat, record the next
30 seconds and reproduce the issue before the timer ends.
Attach the resulting text file with your schlong addon, Skyrim version and a
short description of what happened. Screenshots are welcome, but the report is
usually much more useful.
