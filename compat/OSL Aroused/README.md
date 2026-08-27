# Legacy OSL Aroused player-position compatibility override

Upstream: <https://github.com/ozooma10/OSLAroused>

Base source: `contrib/Distribution/PapyrusSources/OSLAroused_Main.psc` from the
public `2.9.0` tag. The installed binary was validated with OSL Aroused 2.9.2.
Use this override only with OSL Aroused 2.9.0 through 2.9.2. OSL 2.9.3 moved
SOS position control into its native DLL, so this script cannot exclude the
player on that version and must not be installed with it.

The patch changes `UpdateSOSPosition` from:

```papyrus
if(act == none || !EnableSOSIntegration)
```

to:

```papyrus
if(act == none || act == PlayerRef || !EnableSOSIntegration)
```

This leaves all NPC SOS behavior unchanged and prevents OSL from competing with
Schlong Physics Swapper for the player's SOS position. See
`LICENSE.OSLAroused-Unlicense.txt` for the upstream terms.
