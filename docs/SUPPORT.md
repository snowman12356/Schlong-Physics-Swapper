# Getting help

Please include enough information to reproduce the problem:

1. Open **SKSE Menu Framework > Schlong Physics Swapper > Help and reports**.
2. Press **Check my setup again**.
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

Please also say whether **Physics Editor** or **Auto Physics Reset** is installed.
Physics Editor controls the same SMP/CBPC systems and should be disabled while
using SPS. Auto Physics Reset is optional, but its load, cell or scene triggers
may overlap SPS's own player reset if physics changes unexpectedly.

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
- `SPS-014`: Physics Editor is loaded and conflicts with SPS physics control.

## Suggested pinned Nexus post

If something is not working, please open the mod's **Help and reports** page,
check the setup and save a report. For problems you can repeat, record the next
30 seconds and reproduce the issue before the timer ends.
Attach the resulting text file with your schlong addon, Skyrim version and a
short description of what happened. Screenshots are welcome, but the report is
usually much more useful.
